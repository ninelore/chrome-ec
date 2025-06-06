/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI host command */

#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "ppm_common.h"
#include "usb_pd.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include <usbc/ppm.h>

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_INF);

static struct ucsi_ppm_device *ppm_dev;

static void opm_notify(void *context)
{
	pd_send_host_event(PD_EVENT_PPM);
}

/* Sort of main */
int eppm_init(void)
{
	const struct ucsi_pd_driver *drv;
	const struct device *pdc_dev;

	ppm_dev = NULL;

	pdc_dev = DEVICE_DT_GET(DT_INST(0, ucsi_ppm));
	if (!device_is_ready(pdc_dev)) {
		LOG_ERR("device %s not ready", pdc_dev->name);
		return -ENODEV;
	}

	/* Start a PPM task. */
	drv = pdc_dev->api;
	if (drv->init_ppm(pdc_dev)) {
		LOG_ERR("Failed to init PPM");
		return -ENODEV;
	}

	ppm_dev = drv->get_ppm_dev(pdc_dev);
	LOG_INF("Initialized PPM num_ports=%u",
		drv->get_active_port_count(pdc_dev));
	ucsi_ppm_register_notify(ppm_dev, opm_notify, NULL);

	/* Signal the OPM that the PPM just initialized */
	pd_send_host_event(PD_EVENT_INIT);

	return 0;
}

static enum ec_status hc_ucsi_ppm_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_set *p = args->params;

	if (!ppm_dev)
		return EC_RES_UNAVAILABLE;

	if (ucsi_ppm_write(ppm_dev, p->offset, p->data,
			   args->params_size - sizeof(p->offset)))
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_SET, hc_ucsi_ppm_set, EC_VER_MASK(0));

static enum ec_status hc_ucsi_ppm_get(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_get *p = args->params;
	int len;

	if (!ppm_dev)
		return EC_RES_UNAVAILABLE;

	len = ucsi_ppm_read(ppm_dev, p->offset, args->response, p->size);
	if (len < 0)
		return EC_RES_ERROR;

	args->response_size = len;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_GET, hc_ucsi_ppm_get, EC_VER_MASK(0));
