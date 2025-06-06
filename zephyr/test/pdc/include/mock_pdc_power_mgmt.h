/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/ucsi_v3.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/fff.h>

/* FFF fake declarations for select functions in `pdc_power_mgmt.c` */

DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_connector_reset, int,
			enum connector_reset);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_cable_prop, int,
			union cable_property_t *);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_connector_status, int,
			union connector_status_t *);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_info, int, struct pdc_info_t *,
			bool);
DECLARE_FAKE_VALUE_FUNC(bool, pdc_power_mgmt_get_partner_data_swap_capable,
			int);
DECLARE_FAKE_VALUE_FUNC(enum pd_power_role, pdc_power_mgmt_get_power_role, int);
DECLARE_FAKE_VALUE_FUNC(const char *, pdc_power_mgmt_get_task_state_name, int);
DECLARE_FAKE_VALUE_FUNC(bool, pdc_power_mgmt_is_connected, int);
DECLARE_FAKE_VALUE_FUNC(enum pd_data_role, pdc_power_mgmt_pd_get_data_role,
			int);
DECLARE_FAKE_VALUE_FUNC(enum tcpc_cc_polarity, pdc_power_mgmt_pd_get_polarity,
			int);
DECLARE_FAKE_VOID_FUNC(pdc_power_mgmt_request_data_swap, int);
DECLARE_FAKE_VOID_FUNC(pdc_power_mgmt_request_power_swap, int);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_reset, int);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_set_comms_state, bool);
DECLARE_FAKE_VOID_FUNC(pdc_power_mgmt_set_dual_role, int,
		       enum pd_dual_role_states);
DECLARE_FAKE_VALUE_FUNC(enum pd_dual_role_states, pdc_power_mgmt_get_dual_role,
			int);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_set_trysrc, int, bool);
DECLARE_FAKE_VOID_FUNC(pdc_power_mgmt_request_source_voltage, int, int);
DECLARE_FAKE_VALUE_FUNC(unsigned int, pdc_power_mgmt_get_max_voltage);
DECLARE_FAKE_VALUE_FUNC(uint8_t, pdc_power_mgmt_get_src_cap_cnt, int);
DECLARE_FAKE_VALUE_FUNC(const uint32_t *const, pdc_power_mgmt_get_src_caps,
			int);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_lpm_ppm_info, int,
			struct lpm_ppm_info_t *);
DECLARE_FAKE_VALUE_FUNC(bool, pdc_power_mgmt_check_hpd_wake, int);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_pch_data_status, int,
			uint8_t *);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_rdo, int, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_drp_mode, int,
			enum drp_mode_t *);
DECLARE_FAKE_VALUE_FUNC(bool, pdc_power_mgmt_get_vconn_state, int);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_get_sbu_mux_mode,
			enum pdc_sbu_mux_mode *, int *);
DECLARE_FAKE_VALUE_FUNC(int, pdc_power_mgmt_set_sbu_mux_mode,
			enum pdc_sbu_mux_mode);

/**
 * @brief Reset the above set of fakes
 */
void helper_reset_pdc_power_mgmt_fakes(void);
