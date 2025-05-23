/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Endeavour board configuration */

#include "adc.h"
#include "battery.h"
#include "bd99992gw.h"
#include "board_config.h"
#include "button.h"
#include "chipset.h"
#include "console.h"
#include "cros_board_info.h"
#include "driver/pmic_tps650x30.h"
#include "driver/temp_sensor/tmp432.h"
#include "espi.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "math_util.h"
#include "pi3usb9281.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "temp_sensor.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ##args)

static uint8_t board_version;
static uint32_t oem;
static uint32_t sku;

/* Must come after other header files and interrupt handler declarations */
#include "gpio_list.h"

/* Hibernate wake configuration */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* Vbus sensing (1/10 voltage divider). */
	[ADC_VBUS] = { "VBUS", NPCX_ADC_CH2, ADC_MAX_VOLT * 10,
		       ADC_READ_MAX + 1, 0 },
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* TODO: Verify fan control and mft */
const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = MFT_CH_0, /* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = GPIO_FAN_PWR_EN,
};

const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 2500,
	.rpm_start = 2500,
	.rpm_max = 5400,
};

const struct fan_t fans[] = {
	[FAN_CH_0] = { .conf = &fan_conf_0, .rpm = &fan_rpm_0, },
};
BUILD_ASSERT(ARRAY_SIZE(fans) == FAN_CH_COUNT);

const struct mft_t mft_channels[] = {
	[MFT_CH_0] = { NPCX_MFT_MODULE_2, TCKC_LFCLK, PWM_CH_FAN },
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);

const struct i2c_port_t i2c_ports[] = {
	{ .name = "pse",
	  .port = I2C_PORT_PSE,
	  .kbps = 400,
	  .scl = GPIO_I2C0_0_SCL,
	  .sda = GPIO_I2C0_0_SDA },
	{ .name = "eeprom",
	  .port = I2C_PORT_EEPROM,
	  .kbps = 400,
	  .scl = GPIO_I2C0_1_SCL,
	  .sda = GPIO_I2C0_1_SDA },
	{ .name = "pmic",
	  .port = I2C_PORT_PMIC,
	  .kbps = 400,
	  .scl = GPIO_I2C2_SCL,
	  .sda = GPIO_I2C2_SDA },
	{ .name = "thermal",
	  .port = I2C_PORT_THERMAL,
	  .kbps = 400,
	  .scl = GPIO_I2C3_SCL,
	  .sda = GPIO_I2C3_SDA },
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_USB_C0_5V_EN,
	GPIO_USB_FP0_5V_EN,
	GPIO_USB_FP1_5V_EN,
	GPIO_USB_FP3_5V_EN,
};

/*
 * TMP431 has one local and one remote sensor.
 *
 * Temperature sensors data; must be in same order as enum temp_sensor_id.
 * Sensor index and name must match those present in coreboot:
 *     src/mainboard/google/${board}/acpi/dptf.asl
 */
const struct temp_sensor_t temp_sensors[] = {
	{ "TMP431_Internal", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
	  TMP432_IDX_LOCAL },
	{ "TMP431_Sensor_1", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
	  TMP432_IDX_REMOTE1 },
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/*
 * Thermal limits for each temp sensor.  All temps are in degrees K.  Must be in
 * same order as enum temp_sensor_id.  To always ignore any temp, use 0.
 */
static const int temp_fan_off = C_TO_K(0);
static const int temp_fan_max = C_TO_K(56);
struct ec_thermal_config thermal_params[] = {
	/* {Twarn, Thigh, Thalt}, <on>
	 * {Twarn, Thigh, X    }, <off>
	 * fan_off, fan_max
	 */
	{ { 0, C_TO_K(81), C_TO_K(82) },
	  { 0, C_TO_K(77), 0 },
	  temp_fan_off,
	  temp_fan_max }, /* TMP431_Internal */
	{ { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 }, /* TMP431_Sensor_1 */
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);

/* Initialize PMIC */
#define I2C_PMIC_READ(reg, data) \
	i2c_read8(I2C_PORT_PMIC, TPS650X30_I2C_ADDR1_FLAGS, (reg), (data))

#define I2C_PMIC_WRITE(reg, data) \
	i2c_write8(I2C_PORT_PMIC, TPS650X30_I2C_ADDR1_FLAGS, (reg), (data))

static void board_pmic_init(void)
{
	int err;
	int error_count = 0;
	static uint8_t pmic_initialized = 0;

	if (pmic_initialized)
		return;

	/* Read vendor ID */
	while (1) {
		int data;
		err = I2C_PMIC_READ(TPS650X30_REG_VENDORID, &data);
		if (!err && data == TPS650X30_VENDOR_ID)
			break;
		else if (error_count > 5)
			goto pmic_error;
		error_count++;
	}

	/*
	 * VCCIOCNT register setting
	 * [6] : CSDECAYEN
	 * otherbits: default
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_VCCIOCNT, 0x4A);
	if (err)
		goto pmic_error;

	/*
	 * VRMODECTRL:
	 * [4] : VCCIOLPM clear
	 * otherbits: default
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_VRMODECTRL, 0x2F);
	if (err)
		goto pmic_error;

	/*
	 * PGMASK1 : Exclude VCCIO from Power Good Tree
	 * [7] : MVCCIOPG clear
	 * otherbits: default
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_PGMASK1, 0x80);
	if (err)
		goto pmic_error;

	/*
	 * PWFAULT_MASK1 Register settings
	 * [7] : 1b V4 Power Fault Masked
	 * [4] : 1b V7 Power Fault Masked
	 * [2] : 1b V9 Power Fault Masked
	 * [0] : 1b V13 Power Fault Masked
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_PWFAULT_MASK1, 0x95);
	if (err)
		goto pmic_error;

	/*
	 * Discharge control 4 register configuration
	 * [7:6] : 00b Reserved
	 * [5:4] : 01b V3.3S discharge resistance (V6S), 100 Ohm
	 * [3:2] : 01b V18S discharge resistance (V8S), 100 Ohm
	 * [1:0] : 01b V100S discharge resistance (V11S), 100 Ohm
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_DISCHCNT4, 0x15);
	if (err)
		goto pmic_error;

	/*
	 * Discharge control 3 register configuration
	 * [7:6] : 01b V1.8U_2.5U discharge resistance (V9), 100 Ohm
	 * [5:4] : 01b V1.2U discharge resistance (V10), 100 Ohm
	 * [3:2] : 01b V100A discharge resistance (V11), 100 Ohm
	 * [1:0] : 01b V085A discharge resistance (V12), 100 Ohm
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_DISCHCNT3, 0x55);
	if (err)
		goto pmic_error;

	/*
	 * Discharge control 2 register configuration
	 * [7:6] : 01b V5ADS3 discharge resistance (V5), 100 Ohm
	 * [5:4] : 01b V33A_DSW discharge resistance (V6), 100 Ohm
	 * [3:2] : 01b V33PCH discharge resistance (V7), 100 Ohm
	 * [1:0] : 01b V18A discharge resistance (V8), 100 Ohm
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_DISCHCNT2, 0x55);
	if (err)
		goto pmic_error;

	/*
	 * Discharge control 1 register configuration
	 * [7:2] : 00b Reserved
	 * [1:0] : 01b VCCIO discharge resistance (V4), 100 Ohm
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_DISCHCNT1, 0x01);
	if (err)
		goto pmic_error;

	/*
	 * Increase Voltage
	 *  [7:0] : 0x2a default
	 *  [5:4] : 10b default
	 *  [5:4] : 01b 5.1V (0x1a)
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_V5ADS3CNT, 0x1a);
	if (err)
		goto pmic_error;

	/*
	 * PBCONFIG Register configuration
	 *   [7] :      1b Power button debounce, 0ms (no debounce)
	 *   [6] :      0b Power button reset timer logic, no action (default)
	 * [5:0] : 011111b Force an Emergency reset time, 31s (default)
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_PBCONFIG, 0x9F);
	if (err)
		goto pmic_error;

	/*
	 * V3.3A_DSW (VR3) control. Default: 0x2A.
	 * [7:6] : 00b Disabled
	 * [5:4] : 00b Vnom + 3%. (default: 10b 0%)
	 */
	err = I2C_PMIC_WRITE(TPS650X30_REG_V33ADSWCNT, 0x0A);
	if (err)
		goto pmic_error;

	CPRINTS("PMIC init done");
	pmic_initialized = 1;
	return;

pmic_error:
	CPRINTS("PMIC init failed");
}

void chipset_pre_init_callback(void)
{
	board_pmic_init();
}

/**
 * Notify PCH of the AC presence.
 */
static void board_extpower(void)
{
	gpio_set_level(GPIO_PCH_ACPRESENT, extpower_is_present());
}
DECLARE_HOOK(HOOK_AC_CHANGE, board_extpower, HOOK_PRIO_DEFAULT);

int64_t get_time_dsw_pwrok(void)
{
	/* DSW_PWROK is turned on before EC was powered. */
	return -20 * MSEC;
}

const struct pwm_t pwm_channels[] = {
	[PWM_CH_LED_RED] = { 3, PWM_CONFIG_DSLEEP, 100 },
	[PWM_CH_LED_WHITE] = { 5, PWM_CONFIG_DSLEEP, 100 },
	[PWM_CH_FAN] = { 4, PWM_CONFIG_OPEN_DRAIN, 25000 },
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

static const struct fan_step_1_1 fan_table0[] = {
	{ .decreasing_temp_ratio_threshold = TEMP_TO_RATIO(30),
	  .increasing_temp_ratio_threshold = TEMP_TO_RATIO(33),
	  .rpm = 3200 },
	{ .decreasing_temp_ratio_threshold = TEMP_TO_RATIO(32),
	  .increasing_temp_ratio_threshold = TEMP_TO_RATIO(38),
	  .rpm = 3800 },
	{ .decreasing_temp_ratio_threshold = TEMP_TO_RATIO(37),
	  .increasing_temp_ratio_threshold = TEMP_TO_RATIO(42),
	  .rpm = 4200 },
	{ .decreasing_temp_ratio_threshold = TEMP_TO_RATIO(41),
	  .increasing_temp_ratio_threshold = TEMP_TO_RATIO(46),
	  .rpm = 4800 },
	{ .decreasing_temp_ratio_threshold = TEMP_TO_RATIO(45),
	  .increasing_temp_ratio_threshold = TEMP_TO_RATIO(56),
	  .rpm = 5200 },
};
#define NUM_FAN_LEVELS ARRAY_SIZE(fan_table0)

static const struct fan_step_1_1 *fan_table = fan_table0;

static void cbi_init(void)
{
	uint32_t val;
	if (cbi_get_board_version(&val) == EC_SUCCESS && val <= UINT8_MAX)
		board_version = val;
	CPRINTS("Board Version: 0x%02x", board_version);

	if (cbi_get_oem_id(&val) == EC_SUCCESS && val < OEM_COUNT)
		oem = val;
	CPRINTS("OEM: %d", oem);

	if (cbi_get_sku_id(&val) == EC_SUCCESS)
		sku = val;
	CPRINTS("SKU: 0x%08x", sku);
}
DECLARE_HOOK(HOOK_INIT, cbi_init, HOOK_PRIO_INIT_I2C + 1);

static void board_init(void)
{
	board_extpower();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

int fan_percent_to_rpm(int fan, int temp_ratio)
{
	return temp_ratio_to_rpm_hysteresis(fan_table, NUM_FAN_LEVELS, fan,
					    temp_ratio, NULL);
}
