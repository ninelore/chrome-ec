/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "keyboard_scan.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_kbd_matrix.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

DEFINE_FFF_GLOBALS;

#define TEST_KBD_SCAN_NODE DT_NODELABEL(fake_input_device)

INPUT_KBD_MATRIX_DT_DEFINE(TEST_KBD_SCAN_NODE);

FAKE_VOID_FUNC(test_drive_column, const struct device *, int);
FAKE_VALUE_FUNC(kbd_row_t, test_read_row, const struct device *);
FAKE_VOID_FUNC(test_set_detect_mode, const struct device *, bool);

static const struct input_kbd_matrix_api test_api = {
	.drive_column = test_drive_column,
	.read_row = test_read_row,
	.set_detect_mode = test_set_detect_mode,
};

static const struct input_kbd_matrix_common_config kbd_cfg =
	INPUT_KBD_MATRIX_DT_COMMON_CONFIG_INIT(TEST_KBD_SCAN_NODE, &test_api);

static struct input_kbd_matrix_common_data kbd_data;

static const struct device *const fake_dev = DEVICE_DT_GET(TEST_KBD_SCAN_NODE);

static int vnd_keyboard_pm_action(const struct device *dev,
				  enum pm_device_action action)
{
	return 0;
}

#define VND_KEYBOARD_NODE DT_INST(0, vnd_keyboard_input_device)

PM_DEVICE_DT_DEFINE(VND_KEYBOARD_NODE, vnd_keyboard_pm_action);

DEVICE_DT_DEFINE(VND_KEYBOARD_NODE, input_kbd_matrix_common_init,
		 PM_DEVICE_DT_GET(VND_KEYBOARD_NODE), &kbd_data, &kbd_cfg,
		 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

FAKE_VALUE_FUNC(int, system_is_locked);
FAKE_VOID_FUNC(keyboard_state_changed, int, int, int);

ZTEST(keyboard_input, test_keyboard_input_events)
{
	zassert_equal(keyboard_state_changed_fake.call_count, 0);

	input_report_abs(fake_dev, INPUT_ABS_X, 10, false, K_FOREVER);
	input_report_abs(fake_dev, INPUT_ABS_Y, 11, false, K_FOREVER);
	input_report_key(fake_dev, INPUT_BTN_TOUCH, 1, true, K_FOREVER);

	input_report_abs(fake_dev, INPUT_ABS_X, 10, false, K_FOREVER);
	input_report_abs(fake_dev, INPUT_ABS_Y, 11, false, K_FOREVER);
	input_report_key(fake_dev, INPUT_BTN_TOUCH, 0, true, K_FOREVER);

	zassert_equal(keyboard_state_changed_fake.call_count, 2);

	zassert_equal(keyboard_state_changed_fake.arg0_history[0], 11);
	zassert_equal(keyboard_state_changed_fake.arg1_history[0], 10);
	zassert_equal(keyboard_state_changed_fake.arg2_history[0], 1);

	zassert_equal(keyboard_state_changed_fake.arg0_history[1], 11);
	zassert_equal(keyboard_state_changed_fake.arg1_history[1], 10);
	zassert_equal(keyboard_state_changed_fake.arg2_history[1], 0);
}

ZTEST(keyboard_input, test_keyboard_input_enable_disable)
{
	enum pm_device_state state;

	pm_device_state_get(fake_dev, &state);
	zassert_equal(state, PM_DEVICE_STATE_ACTIVE);

	/* disable A */
	keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_CLOSED);

	pm_device_state_get(fake_dev, &state);
	zassert_equal(state, PM_DEVICE_STATE_SUSPENDED);

	/* disable B */
	keyboard_scan_enable(0, KB_SCAN_DISABLE_POWER_BUTTON);

	pm_device_state_get(fake_dev, &state);
	zassert_equal(state, PM_DEVICE_STATE_SUSPENDED);

	/* enable A */
	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_CLOSED);

	pm_device_state_get(fake_dev, &state);
	zassert_equal(state, PM_DEVICE_STATE_SUSPENDED);

	/* enable B */
	keyboard_scan_enable(1, KB_SCAN_DISABLE_POWER_BUTTON);

	pm_device_state_get(fake_dev, &state);
	zassert_equal(state, PM_DEVICE_STATE_ACTIVE);
}

ZTEST(keyboard_input, test_kso_gpio)
{
	const struct device *gpio_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(kso_gpio), col_gpios));
	gpio_pin_t pin = DT_GPIO_PIN(DT_NODELABEL(kso_gpio), col_gpios);
	int col_num = DT_PROP(DT_NODELABEL(kso_gpio), col_num);

	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 1);

	input_kbd_matrix_drive_column_hook(NULL, col_num);
	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 1);

	input_kbd_matrix_drive_column_hook(fake_dev, col_num + 1);
	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 0);

	input_kbd_matrix_drive_column_hook(fake_dev, col_num);
	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 1);

	input_kbd_matrix_drive_column_hook(fake_dev, col_num + 1);
	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 0);

	input_kbd_matrix_drive_column_hook(fake_dev,
					   INPUT_KBD_MATRIX_COLUMN_DRIVE_ALL);
	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 1);

	input_kbd_matrix_drive_column_hook(fake_dev,
					   INPUT_KBD_MATRIX_COLUMN_DRIVE_NONE);
	zassert_equal(gpio_emul_output_get(gpio_dev, pin), 0);
}

uint8_t keyboard_get_cols(void);

ZTEST(keyboard_input, test_get_cols)
{
	zassert_equal(keyboard_get_cols(), 10);
}

uint8_t keyboard_get_rows(void);

ZTEST(keyboard_input, test_get_rows)
{
	zassert_equal(keyboard_get_rows(), 8);
}

extern uint8_t keyboard_cols;

ZTEST(keyboard_input, test_keyboard_cols)
{
	zassert_equal(keyboard_cols, 10);
}

ZTEST(keyboard_input, test_ksstate)
{
	const struct shell *shell_zephyr = shell_backend_dummy_get_ptr();
	const char *outbuffer;
	size_t buffer_size;

	/* Give the backend time to initialize */
	k_sleep(K_MSEC(100));

	shell_backend_dummy_clear_output(shell_zephyr);

	zassert_ok(shell_execute_cmd(shell_zephyr, "ksstate"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);

	zassert_not_null(
		strstr(outbuffer, "Keyboard scan disable mask: 0x00000000"));

	shell_backend_dummy_clear_output(shell_zephyr);

	keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_CLOSED);

	zassert_ok(shell_execute_cmd(shell_zephyr, "ksstate"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);

	zassert_not_null(
		strstr(outbuffer, "Keyboard scan disable mask: 0x00000001"));

	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_CLOSED);

	zassert_ok(shell_execute_cmd(shell_zephyr, "ksstate"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);

	zassert_not_null(
		strstr(outbuffer, "Keyboard scan disable mask: 0x00000000"));
}

struct {
	int x;
	int y;
	int touch;
	int count;
} last_evt;

static void test_input_cb_handler(struct input_event *evt, void *user_data)
{
	if (evt->type == INPUT_EV_ABS && evt->code == INPUT_ABS_X) {
		last_evt.x = evt->value;
	} else if (evt->type == INPUT_EV_ABS && evt->code == INPUT_ABS_Y) {
		last_evt.y = evt->value;
	} else if (evt->type == INPUT_EV_KEY && evt->code == INPUT_BTN_TOUCH) {
		last_evt.touch = evt->value;
	}
	last_evt.count++;
}

INPUT_CALLBACK_DEFINE(fake_dev, test_input_cb_handler, NULL);

ZTEST(keyboard_input, test_kbpress)
{
	const struct shell *shell_zephyr = shell_backend_dummy_get_ptr();
	const char *outbuffer;
	size_t buffer_size;

	/* Give the shell time to initialize */
	k_sleep(K_MSEC(100));

	shell_backend_dummy_clear_output(shell_zephyr);
	zassert_ok(shell_execute_cmd(shell_zephyr, "kbpress"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);
	zassert_ok(strcmp(outbuffer, "\r\nSimulated keys:\r\n"),
		   "outbuffer = '%s'", outbuffer);

	zassert_equal(shell_execute_cmd(shell_zephyr, "kbpress x 2 3"),
		      -EINVAL);
	zassert_equal(shell_execute_cmd(shell_zephyr, "kbpress 1 x 3"),
		      -EINVAL);
	zassert_equal(shell_execute_cmd(shell_zephyr, "kbpress 1 2 x"),
		      -EINVAL);

	zassert_ok(shell_execute_cmd(shell_zephyr, "kbpress 3 5 1"));

	zassert_equal(last_evt.x, 3);
	zassert_equal(last_evt.y, 5);
	zassert_equal(last_evt.touch, 1);
	zassert_equal(last_evt.count, 3);

	shell_backend_dummy_clear_output(shell_zephyr);
	zassert_ok(shell_execute_cmd(shell_zephyr, "kbpress"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);
	zassert_ok(strcmp(outbuffer, "\r\nSimulated keys:\r\n\t3 5\r\n"),
		   "outbuffer = '%s'", outbuffer);

	zassert_ok(shell_execute_cmd(shell_zephyr, "kbpress clear"));

	shell_backend_dummy_clear_output(shell_zephyr);
	zassert_ok(shell_execute_cmd(shell_zephyr, "kbpress"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);
	zassert_ok(strcmp(outbuffer, "\r\nSimulated keys:\r\n"),
		   "outbuffer = '%s'", outbuffer);
}

ZTEST(keyboard_input, test_mkbp_command_simulate_key)
{
	struct ec_params_mkbp_simulate_key param;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_MKBP_SIMULATE_KEY, 0, param);

	param.col = 9;
	param.row = 7;
	param.pressed = 1;

	zassert_ok(host_command_process(&args));

	zassert_equal(last_evt.x, 9);
	zassert_equal(last_evt.y, 7);
	zassert_equal(last_evt.touch, 1);
	zassert_equal(last_evt.count, 3);

	param.pressed = 0;

	zassert_ok(host_command_process(&args));

	zassert_equal(last_evt.x, 9);
	zassert_equal(last_evt.y, 7);
	zassert_equal(last_evt.touch, 0);
	zassert_equal(last_evt.count, 6);
}

ZTEST(keyboard_input, test_mkbp_command_simulate_key_denied)
{
	struct ec_params_mkbp_simulate_key param;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_MKBP_SIMULATE_KEY, 0, param);

	system_is_locked_fake.return_val = 1;

	zassert_equal(host_command_process(&args), EC_RES_ACCESS_DENIED);

	zassert_equal(last_evt.count, 0);
}

ZTEST(keyboard_input, test_mkbp_command_simulate_key_invalid_param)
{
	struct ec_params_mkbp_simulate_key param;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_MKBP_SIMULATE_KEY, 0, param);

	param.col = kbd_cfg.col_size;
	param.row = 0;

	zassert_equal(host_command_process(&args), EC_RES_INVALID_PARAM);

	zassert_equal(last_evt.count, 0);

	param.col = 0;
	param.row = kbd_cfg.row_size;
}

ZTEST(keyboard_input, test_keyboard_input_priority)
{
	zassert_equal(k_thread_priority_get(&kbd_data.thread),
		      EC_TASK_PRIORITY(EC_TASK_KEYSCAN_PRIO));
}

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(keyboard_state_changed);
	RESET_FAKE(system_is_locked);

	memset(&last_evt, 0, sizeof(last_evt));
}

ZTEST_SUITE(keyboard_input, NULL, NULL, reset, reset, NULL);
