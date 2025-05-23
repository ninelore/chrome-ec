/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI PPM Driver */

#include "charge_manager.h"
#include "cros_board_info.h"
#include "ec_commands.h"
#include "ppm_common.h"
#include "usb_pd.h"
#include "usbc/pdc_power_mgmt.h"
#include "util.h"

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>

#include <drivers/pdc.h>
#include <usbc/ppm.h>

LOG_MODULE_REGISTER(ppm, LOG_LEVEL_INF);

#define DT_DRV_COMPAT ucsi_ppm
#define UCSI_7BIT_PORTMASK(p) ((p) & 0x7F)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
	     "Exactly one instance of ucsi-ppm should be defined.");

K_EVENT_DEFINE(ppm_event);
#define PPM_EVENT_CMD_COMPLETE BIT(0)
#define PPM_EVENT_CMD_ERROR BIT(1)
#define PPM_EVENT_ALL (PPM_EVENT_CMD_COMPLETE | PPM_EVENT_CMD_ERROR)

struct ucsi_commands_t {
	uint8_t command;
	uint8_t command_copy_length;
};

#define UCSI_ENTRY(cmd, length)                \
	[cmd] = {                              \
		.command = cmd,                \
		.command_copy_length = length, \
	}

struct ucsi_commands_t ucsi_commands[] = {
	UCSI_ENTRY(UCSI_PPM_RESET, 0),
	UCSI_ENTRY(UCSI_CANCEL, 0),
	UCSI_ENTRY(UCSI_CONNECTOR_RESET, 1),
	UCSI_ENTRY(UCSI_ACK_CC_CI, 1),
	UCSI_ENTRY(UCSI_SET_NOTIFICATION_ENABLE, 3),
	UCSI_ENTRY(UCSI_GET_CAPABILITY, 0),
	UCSI_ENTRY(UCSI_GET_CONNECTOR_CAPABILITY, 1),
	UCSI_ENTRY(UCSI_SET_CCOM, 2),
	UCSI_ENTRY(UCSI_SET_UOR, 2),
	UCSI_ENTRY(UCSI_SET_PDR, 2),
	UCSI_ENTRY(UCSI_GET_ALTERNATE_MODES, 4),
	UCSI_ENTRY(UCSI_GET_CAM_SUPPORTED, 1),
	UCSI_ENTRY(UCSI_GET_CURRENT_CAM, 1),
	UCSI_ENTRY(UCSI_SET_NEW_CAM, 6),
	UCSI_ENTRY(UCSI_GET_PDOS, 3),
	UCSI_ENTRY(UCSI_GET_CABLE_PROPERTY, 1),
	UCSI_ENTRY(UCSI_GET_CONNECTOR_STATUS, 1),
	UCSI_ENTRY(UCSI_GET_ERROR_STATUS, 1),
	UCSI_ENTRY(UCSI_SET_POWER_LEVEL, 6),
	UCSI_ENTRY(UCSI_GET_PD_MESSAGE, 4),
	UCSI_ENTRY(UCSI_GET_ATTENTION_VDO, 1),
	UCSI_ENTRY(UCSI_GET_CAM_CS, 2),
	UCSI_ENTRY(UCSI_LPM_FW_UPDATE_REQUEST, 4),
	UCSI_ENTRY(UCSI_SECURITY_REQUEST, 5),
	UCSI_ENTRY(UCSI_SET_RETIMER_MODE, 5),
	UCSI_ENTRY(UCSI_SET_SINK_PATH, 1),
	UCSI_ENTRY(UCSI_SET_PDOS, 3),
	UCSI_ENTRY(UCSI_READ_POWER_LEVEL, 3),
	UCSI_ENTRY(UCSI_CHUNKING_SUPPORT, 1),
	UCSI_ENTRY(UCSI_VENDOR_DEFINED_COMMAND, 6),
	UCSI_ENTRY(UCSI_SET_USB, 6),
	UCSI_ENTRY(UCSI_GET_LPM_PPM_INFO, 1),
};

BUILD_ASSERT(ARRAY_SIZE(ucsi_commands) == UCSI_CMD_MAX,
	     "Not all UCSI commands are handled.");
struct ppm_data {
	struct ucsi_ppm_device *ppm_dev;
	union connector_status_t
		port_status[CONFIG_USB_PD_PORT_MAX_COUNT] __aligned(4);
	struct pdc_callback cc_cb;
	struct pdc_callback ci_cb;
	union cci_event_t cci_event;
	/** References to the PDC drivers for each port. */
	const struct device *lpm[CONFIG_USB_PD_PORT_MAX_COUNT];
	/** Number of active USB-C ports on this system. */
	uint8_t active_port_count;
};
static struct ppm_data ppm_data;

static int ucsi_ppm_init(const struct device *device)
{
	struct ppm_data *data = (struct ppm_data *)device->data;

	return ucsi_ppm_init_and_wait(data->ppm_dev);
}

static struct ucsi_ppm_device *ucsi_ppm_get_ppm_dev(const struct device *device)
{
	struct ppm_data *data = (struct ppm_data *)device->data;

	return data->ppm_dev;
}

static int ucsi_get_active_port_count(const struct device *dev)
{
	struct ppm_data *data = (struct ppm_data *)dev->data;

	return data->active_port_count;
}

#define SYNC_CMD_TIMEOUT_MSEC 2000
#define RETRY_INTERVAL_MS 20

static int execute_cmd_with_pdc_power_mgmt(const struct device *device,
					   struct ucsi_control_t *control,
					   uint8_t *lpm_data_out)
{
	uint8_t conn = UCSI_7BIT_PORTMASK(control->command_specific[0]);
	uint8_t ucsi_command = control->command;
	struct ppm_data *data = (struct ppm_data *)device->data;
	union set_sink_path_t set_sink_path;
	int charge_port;
	int rv;

	/* Handled commands must return. */
	switch (ucsi_command) {
	case UCSI_SET_SINK_PATH:
		/*
		 * Intercept UCSI_SET_SINK_PATH. This command will be sent by
		 * the ucsi kernel driver with enable set or cleared. If the
		 * enable bit in the command is set, then use the port number
		 * for the override port. If the enable bit is clear, then pass
		 * OVERIDE_OFF to the charge manager, disabling any previous
		 * override.
		 *
		 * If this requires a change to the charging port, then the
		 * charge_manager will call into the PDM which in turn will
		 * cause SET_SINK_PATH to get sent the PDC. So this command
		 * should not be passed directly to the PDC from the PPM.
		 */
		set_sink_path.raw_value = control->command_specific[0];
		conn = set_sink_path.connector_number - 1;
		charge_port = set_sink_path.sink_path_enable ? conn :
							       OVERRIDE_OFF;

		if (charge_port == OVERRIDE_OFF ||
		    (pdc_power_mgmt_get_power_role(charge_port) ==
			     PD_ROLE_SINK &&
		     pdc_power_mgmt_is_connected(charge_port))) {
			rv = charge_manager_set_override(charge_port);
			return rv == EC_SUCCESS ? 0 : -EINVAL;
		} else {
			return -EINVAL;
		}

	/* We intercept both GET_CONNECTOR_STATUS and ACK_CC_CI by
	 * forwarding it to the PDM to do caching.
	 */
	case UCSI_GET_CONNECTOR_STATUS: {
		int rv = pdc_power_mgmt_get_connector_status_for_ppm(
			conn - 1, (union connector_status_t *)lpm_data_out);
		if (rv == 0)
			rv = sizeof(union connector_status_t);

		return rv;
	}
	case UCSI_ACK_CC_CI: {
		union connector_status_t *conn_status;
		union conn_status_change_bits_t ci;
		union ack_cc_ci_t *cmd =
			(union ack_cc_ci_t *)control->command_specific;

		if (!cmd->connector_change_ack) {
			/* This ACK is only for CC. Internally handle it. */
			return 0;
		}
		/* This ACK includes only CI or both CC and CI. */
		if (!ucsi_ppm_get_next_connector_status(data->ppm_dev, &conn,
							&conn_status)) {
			LOG_ERR("Cx: Found no port with CI to ack.");
			return -EINVAL;
		}

		ci.raw_value = conn_status->raw_conn_status_change_bits;
		return pdc_power_mgmt_ppm_ack_status_change(conn - 1, ci);
	}
	default:
		break;
	}

	return -EINVAL;
}

static int ucsi_ppm_execute_cmd_sync(const struct device *device,
				     struct ucsi_control_t *control,
				     uint8_t *lpm_data_out)
{
	struct ppm_data *data = (struct ppm_data *)device->data;
	const struct device *lpm;
	uint8_t ucsi_command = control->command;
	uint8_t conn; /* 1:port=0, 2:port=1, ... */
	uint8_t data_size;
	uint32_t events;
	k_timepoint_t timeout;
	int rv;

	if (ucsi_command == 0 || ucsi_command >= UCSI_CMD_MAX) {
		LOG_ERR("Invalid command 0x%x", ucsi_command);
		return -1;
	}

	/*
	 * Most commands pass the connector number starting at bit 16 - which
	 * aligns to command_specific[0] but GET_ALTERNATE_MODE moves this to
	 * bit 24 and some commands don't use a connector number at all
	 */
	switch (ucsi_command) {
	case UCSI_PPM_RESET:
	case UCSI_SET_NOTIFICATION_ENABLE:
		return 0;
	case UCSI_CONNECTOR_RESET:
	case UCSI_GET_CONNECTOR_CAPABILITY:
	case UCSI_GET_CAM_SUPPORTED:
	case UCSI_GET_CURRENT_CAM:
	case UCSI_GET_PDOS:
	case UCSI_GET_CABLE_PROPERTY:
	case UCSI_GET_CONNECTOR_STATUS:
	case UCSI_GET_ERROR_STATUS:
	case UCSI_GET_PD_MESSAGE:
	case UCSI_GET_ATTENTION_VDO:
	case UCSI_GET_CAM_CS:
	case UCSI_SET_CCOM:
	case UCSI_SET_UOR:
	case UCSI_SET_PDR:
	case UCSI_SET_POWER_LEVEL:
	case UCSI_SET_RETIMER_MODE:
	case UCSI_SET_SINK_PATH:
	case UCSI_SET_PDOS:
	case UCSI_SET_NEW_CAM:
	case UCSI_SET_USB:
		conn = UCSI_7BIT_PORTMASK(control->command_specific[0]);
		break;
	case UCSI_GET_ALTERNATE_MODES:
		conn = UCSI_7BIT_PORTMASK(control->command_specific[1]);
		break;
	default:
		conn = 1;
	}

	/* `conn` is a 1-based port number. C0 = 1, C1 = 2, etc */
	if (conn == 0 || conn > data->active_port_count) {
		LOG_ERR("Invalid conn=%d", conn);
		return -ERANGE;
	}

	lpm = data->lpm[conn - 1];

	/* Ensure the PDC for this port is fully initialized. */
	if (!pdc_is_init_done(lpm)) {
		LOG_ERR("Cannot execute UCSI command 0x%02x: PDC not active (conn=%d)",
			ucsi_command, conn);

		return -ENOTCONN;
	}

	/* Some commands should get intercepted and handled directly via the PDC
	 * power mgmt apis.
	 */
	switch (ucsi_command) {
	case UCSI_GET_CONNECTOR_STATUS:
	case UCSI_ACK_CC_CI:
	case UCSI_SET_SINK_PATH:
		rv = execute_cmd_with_pdc_power_mgmt(device, control,
						     lpm_data_out);
		goto done;
	}

	data_size = ucsi_commands[ucsi_command].command_copy_length;
	LOG_DBG("%s: Executing conn=%u cmd=0x%02x data_size=%d", __func__, conn,
		ucsi_command, data_size);

	timeout = sys_timepoint_calc(K_MSEC(SYNC_CMD_TIMEOUT_MSEC));
	k_event_clear(&ppm_event, PPM_EVENT_ALL);
	do {
		rv = pdc_execute_ucsi_cmd(lpm, ucsi_command, data_size,
					  control->command_specific,
					  lpm_data_out, &data->cc_cb);

		if (rv == 0) {
			/* Command posted but not finished. */
			break;
		}
		if (rv != -EBUSY) {
			/* Failed to post command not because of contention. */
			return rv;
		}

		/* Command can't be posted due to contention. Wait and retry. */
		if (sys_timepoint_expired(timeout)) {
			LOG_DBG("%s: Timed out before posting cmd", __func__);
			return -ETIMEDOUT;
		}
		k_sleep(K_MSEC(RETRY_INTERVAL_MS));
	} while (true);

	LOG_DBG("C%d: Posted command. Waiting for completion.", conn - 1);
	/* Wait for command completion, error, or timeout. */
	events = k_event_wait(&ppm_event, PPM_EVENT_ALL, false,
			      sys_timepoint_timeout(timeout));

	if (events == 0) {
		rv = -ETIMEDOUT;
	} else if (events & PPM_EVENT_CMD_ERROR) {
		rv = -EIO;
	} else if (events & PPM_EVENT_CMD_COMPLETE) {
		rv = data->cci_event.data_len;
	}

done:
	if (rv >= 0) {
		/* Certain SET_* commands require sychronizing the pdc power
		 * mgmt api so it can respond to subsequent calls. Do that here
		 * and wait for it to sync.
		 */
		switch (ucsi_command) {
		case UCSI_SET_CCOM:
		case UCSI_SET_PDR:
		case UCSI_SET_UOR:
		case UCSI_SET_PDOS:
		case UCSI_SET_SINK_PATH:
			pdc_power_mgmt_wait_for_sync(conn - 1, -1);
			break;
		}

		/* Intercept and override some values. */
		switch (ucsi_command) {
		case UCSI_GET_CAPABILITY: {
			/* Override the number of supported ports with what's
			 * defined in device tree.
			 */
			struct capability_t *caps =
				(struct capability_t *)lpm_data_out;
			caps->bNumConnectors =
				ucsi_get_active_port_count(device);
			break;
		}
		default:
			break;
		}
	}

	return rv;
}

/*
 * Callback for command completion. It's shared by all the connectors because
 * the PPM executes only one command at a time.
 */
static void ppm_cc_cb(const struct device *dev,
		      const struct pdc_callback *callback,
		      union cci_event_t cci_event)
{
	struct ppm_data *data = CONTAINER_OF(callback, struct ppm_data, cc_cb);
	uint32_t events = 0;

	LOG_DBG("%s called", __func__);

	data->cci_event = cci_event;

	if (cci_event.command_completed) {
		events |= PPM_EVENT_CMD_COMPLETE;
	}
	if (cci_event.error) {
		events |= PPM_EVENT_CMD_ERROR;
	}

	if (events) {
		k_event_post(&ppm_event, events);
	}
}

/*
 * Callback for connector change events. It's shared by all the connectors.
 */
static void ppm_ci_cb(const struct device *dev,
		      const struct pdc_callback *callback,
		      union cci_event_t cci_event)
{
	struct ppm_data *data = CONTAINER_OF(callback, struct ppm_data, ci_cb);

	LOG_DBG("%s: CCI=0x%08x", __func__, cci_event.raw_value);

	if (cci_event.connector_change == 0 ||
	    cci_event.connector_change > data->active_port_count) {
		LOG_WRN("%s: Received CI on invalid connector = %u (port_count=%u)",
			__func__, cci_event.connector_change,
			data->active_port_count);
		return;
	}

	ucsi_ppm_lpm_alert(data->ppm_dev, cci_event.connector_change);
}

static struct ucsi_pd_driver ppm_drv = {
	.init_ppm = ucsi_ppm_init,
	.get_ppm_dev = ucsi_ppm_get_ppm_dev,
	.execute_cmd = ucsi_ppm_execute_cmd_sync,
	.get_active_port_count = ucsi_get_active_port_count,
};

test_export_static int ppm_init(const struct device *device)
{
	struct ppm_data *data = (struct ppm_data *)device->data;
	const struct ucsi_pd_driver *drv = device->api;

	/* Check that referenced PDC (LPM) drivers are ready. If none are ready,
	 * fail initialization of this driver. The PPM can continue to function
	 * if only a subset of the PDC drivers are ready.
	 */

	uint8_t port_count = pdc_power_mgmt_get_usb_pd_port_count();

	if (port_count == 0) {
		LOG_ERR("No USB-C ports active. Cannot initialize PPM.");
		return -ENODEV;
	}

	for (uint8_t i = 0; i < port_count; i++) {
		data->lpm[i] = pdc_power_mgmt_get_port_pdc_driver(i);

		/* It is pdc_power_mgmt's responsibility to prepare the PDC
		 * drivers. The PPM initializes after pdc_power_mgmt. */
		__ASSERT(device_is_ready(data->lpm[i]),
			 "PDC driver for C%u is not ready or NULL", i);
	}

	LOG_INF("PPM driver found %u USB-C ports", port_count);
	data->active_port_count = port_count;

	/* Initialize the PPM. */
	data->ppm_dev = ppm_data_init(drv, device, data->port_status,
				      data->active_port_count);
	if (!data->ppm_dev) {
		LOG_ERR("Failed to open PPM");
		return -ENODEV;
	}

	/*
	 * Register connector change callback. Command completion callback will
	 * be registered on every command execution. This is intercepted and
	 * returned by the PDM.
	 */
	data->ci_cb.handler = ppm_ci_cb;
	pdc_power_mgmt_register_ppm_callback(&data->ci_cb);

	data->cc_cb.handler = ppm_cc_cb;

	k_event_init(&ppm_event);

	return 0;
}
DEVICE_DT_INST_DEFINE(0, &ppm_init, NULL, &ppm_data, NULL, POST_KERNEL,
		      CONFIG_UCSI_PPM_INIT_PRIORITY, &ppm_drv);

/* Enforce initialization order constraints. The PPM driver depends on
 * pdc_power_mgmt drivers having finished initialization to ensure the
 * underlying PDC device(s) have been fully configured and are stable.
 */

BUILD_ASSERT(CONFIG_UCSI_PPM_INIT_PRIORITY >
		     CONFIG_PDC_POWER_MGMT_INIT_PRIORITY,
	     "PPM must init after pdc_power_mgmt");
