/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"

#include <stdbool.h>

#include <zephyr/ztest.h>

ZTEST_USER(i2c_policy, test_deny_all)
{
	/* Some implausible I2C target */
	const struct i2c_cmd_desc_t cmd_desc_99 = {
		.port = 99,
		.addr_flags = 0x99,
	};

	zassert_equal(board_allow_i2c_passthru(&cmd_desc_99), false);
}

ZTEST_SUITE(i2c_policy, NULL, NULL, NULL, NULL, NULL);
