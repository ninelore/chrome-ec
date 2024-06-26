/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_fuel_gauge.h"
#include "board_config.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "emul/tcpc/emul_tcpci.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "led_onoff_states.h"
#include "led_pwm.h"
#include "mock/isl923x.h"
#include "motionsense_sensors.h"
#include "nissa_sub_board.h"
#include "riven.h"
#include "tablet_mode.h"
#include "tcpm/tcpci.h"
#include "thermal.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <drivers/vivaldi_kbd.h>
#include <dt-bindings/gpio_defines.h>
#include <typec_control.h>

#define TCPC0 EMUL_DT_GET(DT_NODELABEL(tcpc_port0))
#define TCPC1 EMUL_DT_GET(DT_NODELABEL(tcpc_port1))

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VALUE_FUNC(int, cbi_get_ssfc, uint32_t *);
FAKE_VALUE_FUNC(enum nissa_sub_board_type, nissa_get_sb_type);
FAKE_VOID_FUNC(usb_interrupt_c1, enum gpio_signal);
FAKE_VOID_FUNC(bmi3xx_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(lsm6dso_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(bma4xx_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(lis2dw12_interrupt, enum gpio_signal);

FAKE_VALUE_FUNC(enum ec_error_list, raa489000_is_acok, int, bool *);
FAKE_VOID_FUNC(raa489000_hibernate, int, bool);
FAKE_VALUE_FUNC(int, raa489000_enable_asgate, int, bool);
FAKE_VALUE_FUNC(int, raa489000_set_output_current, int, enum tcpc_rp_value);
FAKE_VALUE_FUNC(int, chipset_in_state, int);
FAKE_VOID_FUNC(usb_charger_task_set_event_sync, int, uint8_t);
FAKE_VALUE_FUNC(enum ec_error_list, charger_discharge_on_ac, int);
FAKE_VOID_FUNC(set_pwm_led_color, enum pwm_led_id, int);

FAKE_VALUE_FUNC(enum battery_present, battery_is_present);
FAKE_VOID_FUNC(lpc_keyboard_resume_irq);

static void test_before(void *fixture)
{
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(cbi_get_ssfc);
	RESET_FAKE(nissa_get_sb_type);
	RESET_FAKE(bmi3xx_interrupt);
	RESET_FAKE(lsm6dso_interrupt);
	RESET_FAKE(bma4xx_interrupt);
	RESET_FAKE(lis2dw12_interrupt);
	RESET_FAKE(raa489000_is_acok);
	RESET_FAKE(raa489000_hibernate);
	RESET_FAKE(raa489000_enable_asgate);
	RESET_FAKE(raa489000_set_output_current);
	RESET_FAKE(chipset_in_state);
	RESET_FAKE(charger_discharge_on_ac);
	RESET_FAKE(set_pwm_led_color);

	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_absent;

	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC1),
		I2C_COMMON_EMUL_NO_FAIL_REG);
}

ZTEST_SUITE(riven, NULL, NULL, test_before, NULL, NULL);

int clock_get_freq(void)
{
	return 16000000;
}

static bool clamshell_mode;

static int cbi_get_form_factor_config(enum cbi_fw_config_field_id field,
				      uint32_t *value)
{
	if (field == FORM_FACTOR)
		*value = clamshell_mode ? CLAMSHELL : CONVERTIBLE;

	return 0;
}

ZTEST(riven, test_convertible)
{
	const struct device *tablet_mode_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_tablet_mode_l), gpios));
	const gpio_port_pins_t tablet_mode_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_tablet_mode_l), gpios);
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_imu_int_l), gpios);
	int interrupt_count;

	/* reset tablet mode for initialize status.
	 * enable int_imu and int_tablet_mode before clashell_init
	 * for the priorities of sensor_enable_irqs and
	 * gmr_tablet_switch_init is earlier.
	 */
	tablet_reset();
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_tablet_mode));
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));

	cros_cbi_get_fw_config_fake.custom_fake = cbi_get_form_factor_config;

	clamshell_mode = false;
	clamshell_init();

	/* Verify gmr_tablet_switch is enabled, by checking the side effects
	 * of calling tablet_set_mode, and setting gpio_tablet_mode_l.
	 */
	zassert_ok(gpio_emul_input_set(tablet_mode_gpio, tablet_mode_pin, 0),
		   NULL);
	k_sleep(K_MSEC(100));
	tablet_set_mode(1, TABLET_TRIGGER_LID);
	zassert_equal(1, tablet_get_mode(), NULL);
	zassert_ok(gpio_emul_input_set(tablet_mode_gpio, tablet_mode_pin, 1),
		   NULL);
	k_sleep(K_MSEC(100));
	tablet_set_mode(0, TABLET_TRIGGER_LID);
	zassert_equal(0, tablet_get_mode(), NULL);
	zassert_ok(gpio_emul_input_set(tablet_mode_gpio, tablet_mode_pin, 0),
		   NULL);
	k_sleep(K_MSEC(100));
	tablet_set_mode(1, TABLET_TRIGGER_LID);
	zassert_equal(1, tablet_get_mode(), NULL);

	/* Clear base_imu_irq call count before test */
	bmi3xx_interrupt_fake.call_count = 0;
	lsm6dso_interrupt_fake.call_count = 0;

	/* Verify base_imu_irq is enabled. Interrupt is configured
	 * GPIO_INT_EDGE_FALLING, so set high, then set low.
	 */
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));
	interrupt_count = bmi3xx_interrupt_fake.call_count +
			  lsm6dso_interrupt_fake.call_count;
	zassert_equal(interrupt_count, 1);
}

ZTEST(riven, test_clamshell)
{
	const struct device *tablet_mode_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_tablet_mode_l), gpios));
	const gpio_port_pins_t tablet_mode_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_tablet_mode_l), gpios);
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_imu_int_l), gpios);
	int interrupt_count;

	/* reset tablet mode for initialize status.
	 * enable int_imu and int_tablet_mode before clashell_init
	 * for the priorities of sensor_enable_irqs and
	 * gmr_tablet_switch_init is earlier.
	 */
	tablet_reset();
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_tablet_mode));
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));

	cros_cbi_get_fw_config_fake.custom_fake = cbi_get_form_factor_config;

	clamshell_mode = true;
	clamshell_init();

	/* Verify gmr_tablet_switch is disabled, by checking the side effects
	 * of calling tablet_set_mode, and setting gpio_tablet_mode_l.
	 */
	zassert_ok(gpio_emul_input_set(tablet_mode_gpio, tablet_mode_pin, 0),
		   NULL);
	k_sleep(K_MSEC(100));
	tablet_set_mode(1, TABLET_TRIGGER_LID);
	zassert_equal(0, tablet_get_mode(), NULL);
	zassert_ok(gpio_emul_input_set(tablet_mode_gpio, tablet_mode_pin, 1),
		   NULL);
	k_sleep(K_MSEC(100));
	tablet_set_mode(0, TABLET_TRIGGER_LID);
	zassert_equal(0, tablet_get_mode(), NULL);
	zassert_ok(gpio_emul_input_set(tablet_mode_gpio, tablet_mode_pin, 0),
		   NULL);
	k_sleep(K_MSEC(100));
	tablet_set_mode(1, TABLET_TRIGGER_LID);
	zassert_equal(0, tablet_get_mode(), NULL);

	/* Clear base_imu_irq call count before test */
	bmi3xx_interrupt_fake.call_count = 0;
	lsm6dso_interrupt_fake.call_count = 0;

	/* Verify base_imu_irq is disabled. */
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));
	interrupt_count = bmi3xx_interrupt_fake.call_count +
			  lsm6dso_interrupt_fake.call_count;
	zassert_equal(interrupt_count, 0);
}

static int ssfc_data;

static int cbi_get_ssfc_mock(uint32_t *ssfc)
{
	*ssfc = ssfc_data;
	return 0;
}

ZTEST(riven, test_alt_sensor_base_lsm6dso)
{
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_imu_int_l), gpios);

	/* Initial ssfc data for LSM6DSO base sensor. */
	cbi_get_ssfc_fake.custom_fake = cbi_get_ssfc_mock;
	ssfc_data = 0x00;
	cros_cbi_ssfc_init();

	/* sensor_enable_irqs enable the interrupt int_imu */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));

	alt_sensor_init();

	/* Clear base_imu_irq call count before test */
	bmi3xx_interrupt_fake.call_count = 0;
	lsm6dso_interrupt_fake.call_count = 0;
	bma4xx_interrupt_fake.call_count = 0;

	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(bmi3xx_interrupt_fake.call_count, 0);
	zassert_equal(lsm6dso_interrupt_fake.call_count, 1);
	zassert_equal(bma4xx_interrupt_fake.call_count, 0);
}

ZTEST(riven, test_alt_sensor_base_bmi323)
{
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_imu_int_l), gpios);

	/* Initial ssfc data for BMI323 base sensor. */
	cbi_get_ssfc_fake.custom_fake = cbi_get_ssfc_mock;
	ssfc_data = 0x10;
	cros_cbi_ssfc_init();

	/* sensor_enable_irqs enable the interrupt int_imu */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));

	alt_sensor_init();

	/* Clear base_imu_irq call count before test */
	bmi3xx_interrupt_fake.call_count = 0;
	lsm6dso_interrupt_fake.call_count = 0;
	bma4xx_interrupt_fake.call_count = 0;

	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(bmi3xx_interrupt_fake.call_count, 1);
	zassert_equal(lsm6dso_interrupt_fake.call_count, 0);
	zassert_equal(bma4xx_interrupt_fake.call_count, 0);
}

ZTEST(riven, test_alt_sensor_base_bma422)
{
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_imu_int_l), gpios);

	/* Initial ssfc data for BMA422 base sensor. */
	cbi_get_ssfc_fake.custom_fake = cbi_get_ssfc_mock;
	ssfc_data = 0x20;
	cros_cbi_ssfc_init();

	/* sensor_enable_irqs enable the interrupt int_imu */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));

	alt_sensor_init();

	/* Clear base_imu_irq call count before test */
	bmi3xx_interrupt_fake.call_count = 0;
	lsm6dso_interrupt_fake.call_count = 0;
	bma4xx_interrupt_fake.call_count = 0;

	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(bmi3xx_interrupt_fake.call_count, 0);
	zassert_equal(lsm6dso_interrupt_fake.call_count, 0);
	zassert_equal(bma4xx_interrupt_fake.call_count, 1);
}

ZTEST(riven, test_alt_sensor_lid_lis2dw12)
{
	const struct device *lid_accel_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_acc_int_l), gpios));
	const gpio_port_pins_t lid_accel_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_acc_int_l), gpios);

	/* Initial ssfc data for LIS2DW12 lid sensor. */
	cbi_get_ssfc_fake.custom_fake = cbi_get_ssfc_mock;
	ssfc_data = 0x00;
	cros_cbi_ssfc_init();

	/* sensor_enable_irqs enable the interrupt int_lid_accel */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_lid_accel));

	alt_sensor_init();

	/* Clear base_imu_irq call count before test */
	lis2dw12_interrupt_fake.call_count = 0;
	bma4xx_interrupt_fake.call_count = 0;

	zassert_ok(gpio_emul_input_set(lid_accel_gpio, lid_accel_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(lid_accel_gpio, lid_accel_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(lis2dw12_interrupt_fake.call_count, 1);
	zassert_equal(bma4xx_interrupt_fake.call_count, 0);
}

ZTEST(riven, test_alt_sensor_lid_bma422)
{
	const struct device *lid_accel_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_acc_int_l), gpios));
	const gpio_port_pins_t lid_accel_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_acc_int_l), gpios);

	/* Initial ssfc data for BMA422 lid sensor. */
	cbi_get_ssfc_fake.custom_fake = cbi_get_ssfc_mock;
	ssfc_data = 0x04;
	cros_cbi_ssfc_init();

	/* sensor_enable_irqs enable the interrupt int_lid_accel */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_lid_accel));

	alt_sensor_init();

	/* Clear base_imu_irq call count before test */
	lis2dw12_interrupt_fake.call_count = 0;
	bma4xx_interrupt_fake.call_count = 0;

	zassert_ok(gpio_emul_input_set(lid_accel_gpio, lid_accel_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(lid_accel_gpio, lid_accel_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(lis2dw12_interrupt_fake.call_count, 0);
	zassert_equal(bma4xx_interrupt_fake.call_count, 1);
}

static int extpower_handle_update_call_count;

static void call_extpower_handle_update(void)
{
	extpower_handle_update_call_count++;
}
DECLARE_HOOK(HOOK_AC_CHANGE, call_extpower_handle_update, HOOK_PRIO_DEFAULT);

ZTEST(riven, test_board_check_extpower)
{
	/* Clear call count before testing. */
	extpower_handle_update_call_count = 0;

	/* Update with no change does nothing. */
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 0);

	/* Becoming present updates */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_present;
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 1);

	/* Errors are treated as not plugged in */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_error;
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 2);
}

ZTEST(riven, test_charger_hibernate)
{
	/* board_hibernate() asks the chargers to hibernate. */
	board_hibernate();

	zassert_equal(raa489000_hibernate_fake.call_count, 2);
	zassert_equal(raa489000_hibernate_fake.arg0_history[0],
		      CHARGER_SECONDARY);
	zassert_true(raa489000_hibernate_fake.arg1_history[0]);
	zassert_equal(raa489000_hibernate_fake.arg0_history[1],
		      CHARGER_PRIMARY);
	zassert_true(raa489000_hibernate_fake.arg1_history[1]);
}

ZTEST(riven, test_get_leave_safe_mode_delay_ms)
{
	/* Not cosmx battery would use defaut delay time 500ms. */
	battery_conf = &board_battery_info[0];
	zassert_equal(board_get_leave_safe_mode_delay_ms(), 500);

	/* cosmx battery should delay 2000ms to leave safe mode. */
	battery_conf = &board_battery_info[1];
	zassert_equal(board_get_leave_safe_mode_delay_ms(), 2000);

	battery_conf = &board_battery_info[2];
	zassert_equal(board_get_leave_safe_mode_delay_ms(), 2000);
}

ZTEST(riven, test_board_is_sourcing_vbus)
{
	tcpci_emul_set_reg(TCPC0, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SOURCING_VBUS |
				   TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_true(board_is_sourcing_vbus(0));

	tcpci_emul_set_reg(TCPC1, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SINKING_VBUS |
				   TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_false(board_is_sourcing_vbus(1));
}

ZTEST(riven, test_set_active_charge_port_none)
{
	uint16_t reg;

	/* Setting CHARGE_PORT_NONE disables sinking on all ports */
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE));
	zassert_equal(raa489000_enable_asgate_fake.call_count, 2);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[0], 0);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[0], false);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[1], 1);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[1], false);
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	tcpci_emul_get_reg(TCPC1, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
}

ZTEST(riven, test_set_active_charge_port_invalid_port)
{
	zassert_equal(board_set_active_charge_port(4), EC_ERROR_INVAL,
		      "port 4 doesn't exist, should return error");
}

ZTEST(riven, test_set_active_charge_port_currently_sourcing)
{
	/* Attempting to sink on a port that's sourcing is an error */
	tcpci_emul_set_reg(TCPC1, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SOURCING_VBUS);
	zassert_equal(board_set_active_charge_port(1), EC_ERROR_INVAL);
}

ZTEST(riven, test_set_active_charge_port)
{
	uint16_t reg;

	/* Setting old_port to a port not CHARGE_PORT_NONE. */
	charge_port = 1;
	/* We can successfully start sinking on a port */
	zassert_ok(board_set_active_charge_port(0));
	zassert_equal(raa489000_enable_asgate_fake.call_count, 2);
	zassert_equal(charger_discharge_on_ac_fake.call_count, 2);

	/* Requested charging stop initially */
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[0], 1);
	/* Sinking on the other port was disabled */
	tcpci_emul_get_reg(TCPC1, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[0], 1);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[0], false);
	/* Sinking was enabled on the new port */
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SNK_CTRL_HIGH);
	zassert_equal(raa489000_enable_asgate_fake.arg0_history[1], 0);
	zassert_equal(raa489000_enable_asgate_fake.arg1_history[1], true);
	/* Resumed charging */
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[1], 0);
}

ZTEST(riven, test_set_active_charge_port_enable_fail)
{
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	zassert_equal(board_set_active_charge_port(0), EC_ERROR_UNKNOWN);
}

ZTEST(riven, test_set_active_charge_port_disable_fail)
{
	/* Failing to disable sinking on the other port isn't fatal */
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC1),
		TCPC_REG_COMMAND);
	zassert_ok(board_set_active_charge_port(0));
}

ZTEST(riven, test_tcpc_get_alert_status)
{
	const struct gpio_dt_spec *c0_int =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c0_int_odl);
	const struct gpio_dt_spec *c1_int =
		GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl);

	/* Sub-board IO configuration is handled by other inits */
	gpio_pin_configure_dt(c1_int, GPIO_INPUT_PULL_UP);

	/* Both IRQs are asserted */
	gpio_emul_input_set(c0_int->port, c0_int->pin, 0);
	gpio_emul_input_set(c1_int->port, c1_int->pin, 0);

	tcpci_emul_set_reg(TCPC0, TCPC_REG_ALERT, 1);
	zassert_equal(tcpc_get_alert_status(), PD_STATUS_TCPC_ALERT_0);

	/* Bit 14 is ignored */
	tcpci_emul_set_reg(TCPC0, TCPC_REG_ALERT, 0x4000);
	zassert_equal(tcpc_get_alert_status(), 0);

	/* Port 1 works too */
	tcpci_emul_set_reg(TCPC1, TCPC_REG_ALERT, 0x8000);
	zassert_equal(tcpc_get_alert_status(), PD_STATUS_TCPC_ALERT_1);
}

ZTEST(riven, test_pd_power_supply_reset)
{
	uint16_t reg;

	/* Stops any active sourcing on the given port */
	pd_power_supply_reset(0);
	tcpci_emul_get_reg(TCPC0, TCPC_REG_COMMAND, &reg);
	zassert_equal(reg, TCPC_REG_COMMAND_SRC_CTRL_LOW);
}

ZTEST(riven, test_set_source_current_limit)
{
	/* Args pass through raa489000_set_output_current() */
	typec_set_source_current_limit(0, TYPEC_RP_3A0);
	zassert_equal(raa489000_set_output_current_fake.call_count, 1);
	zassert_equal(raa489000_set_output_current_fake.arg0_val, 0);
	zassert_equal(raa489000_set_output_current_fake.arg1_val, TYPEC_RP_3A0);

	/* A port that doesn't exist does nothing */
	typec_set_source_current_limit(3, TYPEC_RP_USB);
	zassert_equal(raa489000_set_output_current_fake.call_count, 1);
}

static int chipset_in_state_break_tcpc_command(int state_mask)
{
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	return 0;
}

ZTEST(riven, test_pd_set_power_supply_ready)
{
	uint16_t reg;

	/* Initially sinking VBUS so we can see that gets disabled */
	tcpci_emul_set_reg(TCPC0, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_SINKING_VBUS);

	zassert_ok(pd_set_power_supply_ready(0));
	tcpci_emul_get_reg(TCPC0, TCPC_REG_POWER_STATUS, &reg);
	zassert_equal(reg, TCPC_REG_POWER_STATUS_SOURCING_VBUS);
	zassert_equal(raa489000_enable_asgate_fake.call_count, 1);
	zassert_equal(raa489000_enable_asgate_fake.arg0_val, 0);
	zassert_equal(raa489000_enable_asgate_fake.arg1_val, true);

	/* Assorted errors are propagated: enable_asgate() fails */
	raa489000_enable_asgate_fake.return_val = EC_ERROR_UNIMPLEMENTED;
	zassert_not_equal(pd_set_power_supply_ready(0), EC_SUCCESS);

	/* Write to enable VBUS fails */
	chipset_in_state_fake.custom_fake = chipset_in_state_break_tcpc_command;
	zassert_not_equal(pd_set_power_supply_ready(0), EC_SUCCESS);
	chipset_in_state_fake.custom_fake = NULL;

	/* Write to disable sinking fails */
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		TCPC_REG_COMMAND);
	zassert_not_equal(pd_set_power_supply_ready(0), EC_SUCCESS);
	i2c_common_emul_set_write_fail_reg(
		emul_tcpci_generic_get_i2c_common_data(TCPC0),
		I2C_COMMON_EMUL_NO_FAIL_REG);

	/* AP is off */
	chipset_in_state_fake.return_val = 1;
	zassert_equal(pd_set_power_supply_ready(0), EC_ERROR_NOT_POWERED);

	/* Invalid port number requested */
	zassert_equal(pd_set_power_supply_ready(2), EC_ERROR_INVAL);
}

ZTEST(riven, test_reset_pd_mcu)
{
	/* Doesn't do anything */
	board_reset_pd_mcu();
}

ZTEST(riven, test_process_pd_alert)
{
	const struct gpio_dt_spec *c0_int =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c0_int_odl);
	const struct gpio_dt_spec *c1_int =
		GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl);

	gpio_emul_input_set(c0_int->port, c0_int->pin, 0);
	board_process_pd_alert(0);
	/* We ran BC1.2 processing inline */
	zassert_equal(usb_charger_task_set_event_sync_fake.call_count, 1);
	zassert_equal(usb_charger_task_set_event_sync_fake.arg0_val, 0);
	zassert_equal(usb_charger_task_set_event_sync_fake.arg1_val,
		      USB_CHG_EVENT_BC12);
	/*
	 * This should also call schedule_deferred_pd_interrupt() again, but
	 * there's no good way to verify that.
	 */

	/* Port 1 also works */
	gpio_emul_input_set(c1_int->port, c1_int->pin, 0);
	board_process_pd_alert(1);
	zassert_equal(usb_charger_task_set_event_sync_fake.call_count, 2);
	zassert_equal(usb_charger_task_set_event_sync_fake.arg0_val, 1);
	zassert_equal(usb_charger_task_set_event_sync_fake.arg1_val,
		      USB_CHG_EVENT_BC12);
}

ZTEST(riven, test_led_pwm)
{
	led_set_color_battery(EC_LED_COLOR_RED);
	zassert_equal(set_pwm_led_color_fake.arg0_val, PWM_LED0);
	zassert_equal(set_pwm_led_color_fake.arg1_val, EC_LED_COLOR_RED);

	led_set_color_battery(EC_LED_COLOR_BLUE);
	zassert_equal(set_pwm_led_color_fake.arg0_val, PWM_LED0);
	zassert_equal(set_pwm_led_color_fake.arg1_val, EC_LED_COLOR_BLUE);

	led_set_color_battery(EC_LED_COLOR_AMBER);
	zassert_equal(set_pwm_led_color_fake.arg0_val, PWM_LED0);
	zassert_equal(set_pwm_led_color_fake.arg1_val, EC_LED_COLOR_AMBER);

	/* Craask unsupport green */
	led_set_color_battery(EC_LED_COLOR_GREEN);
	zassert_equal(set_pwm_led_color_fake.arg0_val, PWM_LED0);
	zassert_equal(set_pwm_led_color_fake.arg1_val, -1);
}

static bool cbi_touch_en;
static bool cbi_read_fail;

static int cbi_get_touch_en_config(enum cbi_fw_config_field_id field,
				   uint32_t *value)
{
	if (field != FW_TOUCH_EN)
		return -EINVAL;

	if (cbi_read_fail)
		return -1;

	*value = cbi_touch_en ? FW_TOUCH_EN_ENABLE : FW_TOUCH_EN_DISABLE;
	return 0;
}

#define TEST_DELAY_MS 1
#define TOUCH_ENABLE_DELAY_MS (500 + TEST_DELAY_MS)
#define TOUCH_DISABLE_DELAY_MS (0 + TEST_DELAY_MS)

ZTEST(riven, test_touch_enable)
{
	const struct gpio_dt_spec *bl_en =
		GPIO_DT_FROM_NODELABEL(gpio_soc_edp_bl_en);
	const struct gpio_dt_spec *touch_en =
		GPIO_DT_FROM_NODELABEL(gpio_ec_touch_en);

	cbi_touch_en = true;
	cbi_read_fail = false;
	cros_cbi_get_fw_config_fake.custom_fake = cbi_get_touch_en_config;

	hook_notify(HOOK_INIT);

	/* touch_en become high after TOUCH_ENABLE_DELAY_MS delay */
	zassert_ok(gpio_emul_input_set(bl_en->port, bl_en->pin, 1), NULL);
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);

	k_sleep(K_MSEC(TOUCH_ENABLE_DELAY_MS));
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 1);

	/* touch_en become low after TOUCH_DISABLE_DELAY_MS delay */
	zassert_ok(gpio_emul_input_set(bl_en->port, bl_en->pin, 0), NULL);
	k_sleep(K_MSEC(TOUCH_DISABLE_DELAY_MS));
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);

	/* touch_en keep low if fw_config is not enabled */
	cbi_touch_en = false;
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_soc_edp_bl_en));
	hook_notify(HOOK_INIT);

	zassert_ok(gpio_emul_input_set(bl_en->port, bl_en->pin, 1), NULL);
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);

	k_sleep(K_MSEC(TOUCH_ENABLE_DELAY_MS));
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);

	/* touch_en keep low if fw_config read fail */
	cbi_read_fail = true;
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_soc_edp_bl_en));
	hook_notify(HOOK_INIT);

	zassert_ok(gpio_emul_input_set(bl_en->port, bl_en->pin, 0), NULL);
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);

	zassert_ok(gpio_emul_input_set(bl_en->port, bl_en->pin, 1), NULL);
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);

	k_sleep(K_MSEC(TOUCH_ENABLE_DELAY_MS));
	zassert_equal(gpio_emul_output_get(touch_en->port, touch_en->pin), 0);
}
