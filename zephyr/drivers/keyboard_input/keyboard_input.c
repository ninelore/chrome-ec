/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "keyboard_protocol.h"
#include "keyboard_scan.h"
#include "system.h"

#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_kbd_matrix.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(kbd_input, CONFIG_INPUT_LOG_LEVEL);

#define CROS_EC_KEYBOARD_NODE DT_CHOSEN(cros_ec_keyboard)

static const struct device *const kbd_dev =
	DEVICE_DT_GET(CROS_EC_KEYBOARD_NODE);

static atomic_t disable_scan_mask;

uint8_t keyboard_get_cols(void)
{
	const struct input_kbd_matrix_common_config *cfg = kbd_dev->config;

	return cfg->col_size;
}

uint8_t keyboard_get_rows(void)
{
	const struct input_kbd_matrix_common_config *cfg = kbd_dev->config;

	return cfg->row_size;
}

void keyboard_scan_enable(int enable, enum kb_scan_disable_masks mask)
{
	int prev_mask;

	if (enable) {
		prev_mask = atomic_and(&disable_scan_mask, ~mask);
	} else {
		prev_mask = atomic_or(&disable_scan_mask, mask);
	}

	if (!pm_device_runtime_is_enabled(kbd_dev)) {
		LOG_WRN("device %s does not support runtime PM", kbd_dev->name);
		return;
	}

	if (prev_mask == 0 && atomic_get(&disable_scan_mask) != 0) {
		pm_device_runtime_put(kbd_dev);
	} else if (prev_mask != 0 && atomic_get(&disable_scan_mask) == 0) {
		pm_device_runtime_get(kbd_dev);
	}
}

static int keyboard_input_init(void)
{
	struct input_kbd_matrix_common_data *data = kbd_dev->data;

	/* Initialize as active */
	pm_device_runtime_get(kbd_dev);

	/* fix up the device thread priority */
	k_thread_priority_set(&data->thread,
			      EC_TASK_PRIORITY(EC_TASK_KEYSCAN_PRIO));

	return 0;
}

SYS_INIT(keyboard_input_init, APPLICATION, 0);

static void keyboard_input_cb(struct input_event *evt, void *user_data)
{
	static int row;
	static int col;
	static bool pressed;

	switch (evt->code) {
	case INPUT_ABS_X:
		col = evt->value;
		break;
	case INPUT_ABS_Y:
		row = evt->value;
		break;
	case INPUT_BTN_TOUCH:
		pressed = evt->value;
		break;
	}

	if (evt->sync) {
		LOG_DBG("keyboard_state_changed %d %d %d", row, col, pressed);
		keyboard_state_changed(row, col, pressed);
	}
}
INPUT_CALLBACK_DEFINE(kbd_dev, keyboard_input_cb, NULL);

/* referenced in common/keyboard_8042.c */
uint8_t keyboard_cols = DT_PROP(CROS_EC_KEYBOARD_NODE, col_size);

static int cmd_ksstate(const struct shell *sh, size_t argc, char **argv)
{
	shell_fprintf(sh, SHELL_NORMAL, "Keyboard scan disable mask: 0x%08lx\n",
		      atomic_get(&disable_scan_mask));

	return 0;
}

SHELL_CMD_REGISTER(ksstate, NULL, "Show keyboard scan state", cmd_ksstate);

/* Keys simulated-pressed */
static uint8_t simulated_key[KEYBOARD_COLS_MAX];

static void simulate_key(int row, int col, int pressed)
{
	simulated_key[col] &= ~BIT(row);
	if (pressed)
		simulated_key[col] |= BIT(row);

	input_report_abs(kbd_dev, INPUT_ABS_X, col, false, K_FOREVER);
	input_report_abs(kbd_dev, INPUT_ABS_Y, row, false, K_FOREVER);
	input_report_key(kbd_dev, INPUT_BTN_TOUCH, pressed, true, K_FOREVER);
}

static int cmd_kbpress(const struct shell *sh, size_t argc, char **argv)
{
	int err = 0;
	uint32_t row, col, val;

	if (argc < 2) {
		shell_print(sh, "Simulated keys:");
		for (col = 0; col < keyboard_cols; ++col) {
			if (simulated_key[col] == 0)
				continue;
			for (row = 0; row < KEYBOARD_ROWS; ++row)
				if (simulated_key[col] & BIT(row))
					shell_print(sh, "\t%d %d", col, row);
		}
		return 0;
	}
	if (!strcmp(argv[1], "clear")) {
		for (col = 0; col < keyboard_cols; ++col) {
			if (simulated_key[col] == 0)
				continue;
			for (row = 0; row < KEYBOARD_ROWS; ++row)
				if (simulated_key[col] & BIT(row))
					simulate_key(row, col, 0);
		}
		return 0;
	}

	if (argc != 4) {
		shell_error(sh, "Usage: %s [clear | col row [0 | 1]]", argv[0]);
		return -EINVAL;
	}
	col = shell_strtoul(argv[1], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[1]);
		return err;
	}

	row = shell_strtoul(argv[2], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[1]);
		return err;
	}

	val = shell_strtoul(argv[3], 0, &err);
	if (err) {
		shell_error(sh, "Invalid argument: %s", argv[1]);
		return err;
	}
	simulate_key(row, col, val);
	return 0;
}

SHELL_CMD_ARG_REGISTER(kbpress, NULL,
		       "Simulate keypress: kbpress [clear | col row [0 | 1]]",
		       cmd_kbpress, 1, 3);

static enum ec_status
mkbp_command_simulate_key(struct host_cmd_handler_args *args)
{
	const struct ec_params_mkbp_simulate_key *p = args->params;
	const struct input_kbd_matrix_common_config *cfg = kbd_dev->config;

	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;

	if (p->col >= cfg->col_size || p->row >= cfg->row_size)
		return EC_RES_INVALID_PARAM;

	simulate_key(p->row, p->col, p->pressed);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_MKBP_SIMULATE_KEY, mkbp_command_simulate_key,
		     EC_VER_MASK(0));
