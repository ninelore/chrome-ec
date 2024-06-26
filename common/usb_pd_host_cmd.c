/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host commands for USB-PD module.
 */

#include "atomic.h"
#include "battery.h"
#include "charge_manager.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "mkbp_event.h"
#include "tcpm/tcpm.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#include <string.h>
#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)
#else /* CONFIG_COMMON_RUNTIME */
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif /* CONFIG_COMMON_RUNTIME */

#ifdef HAS_TASK_HOSTCMD

static enum ec_status hc_pd_ports(struct host_cmd_handler_args *args)
{
	struct ec_response_usb_pd_ports *r = args->response;

	r->num_ports = board_get_usb_pd_port_count();
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_PORTS, hc_pd_ports, EC_VER_MASK(0));

#if defined(CONFIG_HOSTCMD_RWHASHPD) && defined(CONFIG_COMMON_RUNTIME)
static enum ec_status
hc_remote_rw_hash_entry(struct host_cmd_handler_args *args)
{
	int i, idx = 0, found = 0;
	const struct ec_params_usb_pd_rw_hash_entry *p = args->params;
	static int rw_hash_next_idx;

	if (!p->dev_id)
		return EC_RES_INVALID_PARAM;

	for (i = 0; i < RW_HASH_ENTRIES; i++) {
		if (p->dev_id == rw_hash_table[i].dev_id) {
			idx = i;
			found = 1;
			break;
		}
	}

	if (!found) {
		idx = rw_hash_next_idx;
		rw_hash_next_idx = rw_hash_next_idx + 1;
		if (rw_hash_next_idx == RW_HASH_ENTRIES)
			rw_hash_next_idx = 0;
	}
	memcpy(&rw_hash_table[idx], p, sizeof(*p));

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_RW_HASH_ENTRY, hc_remote_rw_hash_entry,
		     EC_VER_MASK(0));
#endif /* CONFIG_HOSTCMD_RWHASHPD && CONFIG_COMMON_RUNTIME */

#if defined(CONFIG_HOSTCMD_PD_CHIP_INFO) && !defined(CONFIG_USB_PD_TCPC)
static enum ec_status hc_remote_pd_chip_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_pd_chip_info *p = args->params;
	struct ec_response_pd_chip_info_v1 info;

	if (p->port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	if (tcpm_get_chip_info(p->port, p->live, &info))
		return EC_RES_ERROR;

	/*
	 * Take advantage of the fact that v0 and v1 structs have the
	 * same layout for v0 data. (v1 just appends data)
	 */
	args->response_size =
		args->version ? sizeof(struct ec_response_pd_chip_info_v1) :
				sizeof(struct ec_response_pd_chip_info);

	memcpy(args->response, &info, args->response_size);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_CHIP_INFO, hc_remote_pd_chip_info,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
#endif /* CONFIG_HOSTCMD_PD_CHIP_INFO && !CONFIG_USB_PD_TCPC */

#ifdef CONFIG_COMMON_RUNTIME
/*
 * Combines the following information into a single byte
 * Bit 0: Active/Passive cable
 * Bit 1: Optical/Non-optical cable
 * Bit 2: Legacy Thunderbolt adapter
 * Bit 3: Active Link Uni-Direction/Bi-Direction
 * Bit 4: Retimer/Rediriver cable
 */
uint8_t get_pd_control_flags(int port)
{
	union tbt_mode_resp_cable cable_resp;
	union tbt_mode_resp_device device_resp;
	uint8_t control_flags = 0;

	if (!IS_ENABLED(CONFIG_USB_PD_ALT_MODE_DFP) ||
	    !IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE))
		return 0;

	cable_resp.raw_value = pd_get_tbt_mode_vdo(port, TCPCI_MSG_SOP_PRIME);
	device_resp.raw_value = pd_get_tbt_mode_vdo(port, TCPCI_MSG_SOP);

	/*
	 * Ref: USB Type-C Cable and Connector Specification
	 * Table F-11 TBT3 Cable Discover Mode VDO Responses
	 * For Passive cables, Active Cable Plug link training is set to 0
	 */
	control_flags |= (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE ||
			  cable_resp.tbt_active_passive == TBT_CABLE_ACTIVE) ?
				 USB_PD_CTRL_ACTIVE_CABLE :
				 0;
	control_flags |= cable_resp.tbt_cable == TBT_CABLE_OPTICAL ?
				 USB_PD_CTRL_OPTICAL_CABLE :
				 0;
	control_flags |= device_resp.tbt_adapter == TBT_ADAPTER_TBT2_LEGACY ?
				 USB_PD_CTRL_TBT_LEGACY_ADAPTER :
				 0;
	control_flags |= cable_resp.lsrx_comm == UNIDIR_LSRX_COMM ?
				 USB_PD_CTRL_ACTIVE_LINK_UNIDIR :
				 0;
	control_flags |= cable_resp.retimer_type == USB_RETIMER ?
				 USB_PD_CTRL_RETIMER_CABLE :
				 0;
	return control_flags;
}
#endif
#endif /* HAS_TASK_HOSTCMD */
