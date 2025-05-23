/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_TPS6699X_H_
#define __EMUL_TPS6699X_H_

#include "drivers/ucsi_v3.h"
#include "emul/emul_pdc_pdo.h"
#include "include/usb_pd.h"
#include "tps6699x_reg.h"

#include <stdint.h>

#include <zephyr/drivers/gpio.h>

#define TPS6699X_MAX_REG 0xa4
#define TPS6699X_REG_SIZE 64

struct ti_ccom {
	uint16_t connector_number : 7;
	uint16_t cc_operation_mode : 3;
	uint16_t reserved : 6;
} __packed;

struct ti_get_pdos {
	uint8_t connector_number : 7;
	uint8_t partner_pdo : 1;
	uint8_t pdo_offset : 8;
	uint8_t num_pdos : 2;
	uint8_t source : 1;
	uint8_t source_caps : 2;
	uint8_t reserved : 1;
} __packed;

enum switch_select {
	PP_5V1 = 0,
	PP_5V2 = 1,
	PP_EXT1 = 2,
	PP_EXT2 = 3,
};

struct ti_task_srdy {
	uint8_t switch_select : 3;
	uint8_t reserved : 5;
} __packed;

struct tps6699x_response {
	uint8_t result : 4;
	uint8_t reserved : 4;
	union {
		struct {
			uint8_t length;
			union {
				union error_status_t error;
				struct ti_ccom ccom;
				uint32_t pdos[4];
			};
		} __packed;
		union connector_status_t connector_status;
		struct capability_t capability;
		union connector_capability_t connector_capability;
		union cable_property_t cable_property;
	} data;
} __packed;

struct tps6699x_emul_pdc_data {
	struct gpio_dt_spec irq_gpios;
	uint32_t delay_ms;
	struct k_work_delayable delay_work;
	/* The register address currently being read or written. */
	uint8_t reg_addr;
	/* The stated length of the current read or write. */
	uint8_t transaction_bytes;
	/* There are 0xa4 registers, and the biggest is 512 bits long.
	 * TODO(b/345292002): Define a real data structure for registers.
	 */
	uint8_t reg_val[TPS6699X_MAX_REG][TPS6699X_REG_SIZE];

	union connector_status_t connector_status;
	union connector_reset_t reset_cmd;
	union error_status_t error;
	struct capability_t capability;
	union connector_capability_t connector_capability;
	union uor_t uor;
	union pdr_t pdr;
	enum ccom_t ccom;
	union cable_property_t cable_property;
	union reg_port_control port_control;
	bool frs_configured;

	struct tps6699x_response response;

	struct emul_pdc_pdo_t pdo;
	bool cmd_error;
	struct k_work_delayable aneg_delay_work;
	/** PDC feature flags */
	ATOMIC_DEFINE(features, EMUL_PDC_FEATURE_COUNT);
};

#endif /* __EMUL_TPS6699X_H_ */
