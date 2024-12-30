/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/usb_common.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(select_best_pdo);

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

ZTEST_SUITE(pd_select_best_pdo, NULL, NULL, NULL, NULL, NULL);

/* Test that a non-fixed PDO will never be selected by pd_select_best_pdo. */
ZTEST(pd_select_best_pdo, test_fixed_only)
{
	/* Note - this set of PDOs violates the PD specification. */
	const uint32_t partner_src_pdo[] = {
		PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
		PDO_VAR(4750, CONFIG_USB_PD_MAX_VOLTAGE_MV,
			CONFIG_USB_PD_MAX_CURRENT_MA),
		PDO_BATT(4750, CONFIG_USB_PD_MAX_VOLTAGE_MV,
			 CONFIG_USB_PD_MAX_POWER_MW),
		PDO_FIXED(9000, 3000, PDO_FIXED_FLAGS),
		PDO_FIXED(12000, 3000, PDO_FIXED_FLAGS),
		PDO_FIXED(20000, 3000, PDO_FIXED_FLAGS),
	};
	uint32_t pdo;

	struct {
		int max_mv;
		int expected_index;
	} test[] = {
		{
			.max_mv = 5000,
			.expected_index = 0,
		},
		{
			.max_mv = 9000,
			.expected_index = 3,
		},
		{
			.max_mv = 10000,
			.expected_index = 3,
		},
		{
			.max_mv = 12000,
			.expected_index = 4,
		},
		{
			.max_mv = 15000,
			.expected_index = 4,
		},
		{
			.max_mv = 20000,
			.expected_index = 5,
		},
	};

	if (!IS_ENABLED(CONFIG_PLATFORM_EC_USB_PD_ONLY_FIXED_PDOS)) {
		ztest_test_skip();
		return;
	}

	for (int i = 0; i < ARRAY_SIZE(test); i++) {
		LOG_INF("Find best PDO, max %d mV, expect %d mV",
			test[i].max_mv,
			PDO_FIXED_VOLTAGE(
				partner_src_pdo[test[i].expected_index]));
		zassert_equal(pd_select_best_pdo(ARRAY_SIZE(partner_src_pdo),
						 partner_src_pdo,
						 test[i].max_mv, &pdo),
			      test[i].expected_index,
			      "Max %d mV - failed to select fixed PDO index %d",
			      test[i].max_mv, test[i].expected_index);
	}
}
