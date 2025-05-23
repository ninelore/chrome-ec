/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lantis board-specific configuration */

#include "adc_chip.h"
#include "button.h"
#include "cbi_fw_config.h"
#include "cbi_ssfc.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "cros_board_info.h"
#include "driver/accel_bma2x2.h"
#include "driver/accel_bma422.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/bc12/pi3usb9201.h"
#include "driver/charger/sm5803.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/temp_sensor/thermistor.h"
#include "driver/usb_mux/it5205.h"
#include "gpio.h"
#include "hooks.h"
#include "intc.h"
#include "keyboard_8042.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "switch.h"
#include "system.h"
#include "tablet_mode.h"
#include "task.h"
#include "tcpm/tcpci.h"
#include "temp_sensor.h"
#include "uart.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "watchdog.h"

#define CPRINTUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)

#define INT_RECHECK_US 5000

uint32_t board_version;

/* GPIO to enable/disable the USB Type-A port. */
const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_USB_A_5V,
};

/* Keyboard scan setting */
__override struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 50,
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.stable_scan_period_us = 9 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x1c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfe, 0xff, 0xff, 0xff,  /* full set */
	},
};

__override void board_process_pd_alert(int port)
{
	/*
	 * PD_INT task will process this alert, and that task is only needed on
	 * C1.
	 */
	if (port != 1)
		return;

	if (gpio_get_level(GPIO_USB_C1_INT_ODL))
		return;

	sm5803_handle_interrupt(port);

	/*
	 * b:273208597: There are some peripheral display docks will
	 * issue HPDs in the short time. TCPM must wake up pd_task
	 * continually to service the events. They may cause the
	 * watchdog to reset. This patch placates watchdog after
	 * receiving dp_attention.
	 */
	watchdog_reload();
}

/* C0 interrupt line shared by BC 1.2 and charger */
static void check_c0_line(void);
DECLARE_DEFERRED(check_c0_line);

static void notify_c0_chips(void)
{
	usb_charger_task_set_event(0, USB_CHG_EVENT_BC12);
	sm5803_interrupt(0);
}

static void check_c0_line(void)
{
	/*
	 * If line is still being held low, see if there's more to process from
	 * one of the chips
	 */
	if (!gpio_get_level(GPIO_USB_C0_INT_ODL)) {
		notify_c0_chips();
		hook_call_deferred(&check_c0_line_data, INT_RECHECK_US);
	}
}

static void usb_c0_interrupt(enum gpio_signal s)
{
	/* Cancel any previous calls to check the interrupt line */
	hook_call_deferred(&check_c0_line_data, -1);

	/* Notify all chips using this line that an interrupt came in */
	notify_c0_chips();

	/* Check the line again in 5ms */
	hook_call_deferred(&check_c0_line_data, INT_RECHECK_US);
}

/* C1 interrupt line shared by BC 1.2, TCPC, and charger */
static void check_c1_line(void);
DECLARE_DEFERRED(check_c1_line);

static void notify_c1_chips(void)
{
	schedule_deferred_pd_interrupt(1);
	usb_charger_task_set_event(1, USB_CHG_EVENT_BC12);
}

static void check_c1_line(void)
{
	/*
	 * If line is still being held low, see if there's more to process from
	 * one of the chips.
	 */
	if (!gpio_get_level(GPIO_USB_C1_INT_ODL)) {
		notify_c1_chips();
		hook_call_deferred(&check_c1_line_data, INT_RECHECK_US);
	}
}

static void usb_c1_interrupt(enum gpio_signal s)
{
	/* Cancel any previous calls to check the interrupt line */
	hook_call_deferred(&check_c1_line_data, -1);

	/* Notify all chips using this line that an interrupt came in */
	notify_c1_chips();

	/* Check the line again in 5ms */
	hook_call_deferred(&check_c1_line_data, INT_RECHECK_US);
}

static void button_sub_hdmi_hpd_interrupt(enum gpio_signal s)
{
	enum fw_config_db db = get_cbi_fw_config_db();
	int hdmi_hpd = gpio_get_level(GPIO_VOLUP_BTN_ODL_HDMI_HPD);

	if (db == DB_1A_HDMI || db == DB_LTE_HDMI || db == DB_1A_HDMI_LTE)
		gpio_set_level(GPIO_EC_AP_USB_C1_HDMI_HPD, hdmi_hpd);
	else
		button_interrupt(s);
}

static void c0_ccsbu_ovp_interrupt(enum gpio_signal s)
{
	cprints(CC_USBPD, "C0: CC OVP, SBU OVP, or thermal event");
	pd_handle_cc_overvoltage(0);
}

static void pen_detect_interrupt(enum gpio_signal s)
{
	int pen_detect = !gpio_get_level(GPIO_PEN_DET_ODL);

	gpio_set_level(GPIO_EN_PP5000_PEN, pen_detect);
}

/* Must come after other header files and interrupt handler declarations */
#include "gpio_list.h"

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_VSNS_PP3300_A] = { .name = "PP3300_A_PGOOD",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH0 },
	[ADC_TEMP_SENSOR_1] = { .name = "TEMP_SENSOR1",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH2 },
	[ADC_TEMP_SENSOR_2] = { .name = "TEMP_SENSOR2",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH3 },
	[ADC_SUB_ANALOG] = { .name = "SUB_ANALOG",
			     .factor_mul = ADC_MAX_MVOLT,
			     .factor_div = ADC_READ_MAX + 1,
			     .shift = 0,
			     .channel = CHIP_ADC_CH13 },
	[ADC_TEMP_SENSOR_3] = { .name = "TEMP_SENSOR3",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH15 },
	[ADC_TEMP_SENSOR_4] = { .name = "TEMP_SENSOR4",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH16 },
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* BC 1.2 chips */
const struct pi3usb9201_config_t pi3usb9201_bc12_chips[] = {
	{
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
		.flags = PI3USB9201_ALWAYS_POWERED,
	},
	{
		.i2c_port = I2C_PORT_SUB_USB_C1,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
		.flags = PI3USB9201_ALWAYS_POWERED,
	},
};

/* Charger chips */
const struct charger_config_t chg_chips[] = {
	[CHARGER_PRIMARY] = {
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
	[CHARGER_SECONDARY] = {
		.i2c_port = I2C_PORT_SUB_USB_C1,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
};

/* TCPCs */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &it83xx_tcpm_drv,
	},
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_SUB_USB_C1,
			.addr_flags = PS8XXX_I2C_ADDR1_FLAGS,
		},
		.drv = &ps8xxx_tcpm_drv,
	},
};

/* USB Muxes */
const struct usb_mux_chain usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.mux =
			&(const struct usb_mux){
				.usb_port = 0,
				.i2c_port = I2C_PORT_USB_C0,
				.i2c_addr_flags = IT5205_I2C_ADDR1_FLAGS,
				.driver = &it5205_usb_mux_driver,
			},
	},
	{
		.mux =
			&(const struct usb_mux){
				.usb_port = 1,
				.i2c_port = I2C_PORT_SUB_USB_C1,
				.i2c_addr_flags = PS8XXX_I2C_ADDR1_FLAGS,
				.driver = &tcpci_tcpm_usb_mux_driver,
				.hpd_update = &ps8xxx_tcpc_update_hpd_status,
			},
	},
};

/* Sensor Mutexes */
static struct mutex g_lid_mutex;
static struct mutex g_base_mutex;

/* Sensor Data */
static struct accelgyro_saved_data_t g_bma253_data;
static struct accelgyro_saved_data_t g_bma422_data;
static struct lsm6dsm_data lsm6dsm_data = LSM6DSM_DATA;

/* Matrix to rotate accelrator into standard reference frame */
static const mat33_fp_t base_standard_ref = { { 0, FLOAT_TO_FP(-1), 0 },
					      { FLOAT_TO_FP(1), 0, 0 },
					      { 0, 0, FLOAT_TO_FP(1) } };

static const mat33_fp_t lid_standard_ref = { { FLOAT_TO_FP(-1), 0, 0 },
					     { 0, FLOAT_TO_FP(-1), 0 },
					     { 0, 0, FLOAT_TO_FP(1) } };

/* Drivers */
struct motion_sensor_t motion_sensors[] = {
	[LID_ACCEL] = {
		.name = "Lid Accel",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_BMA255,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &bma2x2_accel_drv,
		.mutex = &g_lid_mutex,
		.drv_data = &g_bma253_data,
		.port = I2C_PORT_SENSOR,
		.i2c_spi_addr_flags = BMA2x2_I2C_ADDR1_FLAGS,
		.rot_standard_ref = &lid_standard_ref,
		.default_range = 2,
		.min_frequency = BMA255_ACCEL_MIN_FREQ,
		.max_frequency = BMA255_ACCEL_MAX_FREQ,
		.config = {
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 10000 | ROUND_UP_FLAG,
			},
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 10000 | ROUND_UP_FLAG,
			},
		},
	},
	[BASE_ACCEL] = {
		.name = "Base Accel",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_LSM6DSM,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lsm6dsm_drv,
		.mutex = &g_base_mutex,
		.drv_data = LSM6DSM_ST_DATA(lsm6dsm_data,
				MOTIONSENSE_TYPE_ACCEL),
		.port = I2C_PORT_SENSOR,
		.i2c_spi_addr_flags = LSM6DSM_ADDR0_FLAGS,
		.rot_standard_ref = &base_standard_ref,
		.default_range = 4,  /* g */
		.min_frequency = LSM6DSM_ODR_MIN_VAL,
		.max_frequency = LSM6DSM_ODR_MAX_VAL,
		.config = {
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 13000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
		},
	},
	[BASE_GYRO] = {
		.name = "Base Gyro",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_LSM6DSM,
		.type = MOTIONSENSE_TYPE_GYRO,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &lsm6dsm_drv,
		.mutex = &g_base_mutex,
		.drv_data = LSM6DSM_ST_DATA(lsm6dsm_data,
				MOTIONSENSE_TYPE_GYRO),
		.port = I2C_PORT_SENSOR,
		.i2c_spi_addr_flags = LSM6DSM_ADDR0_FLAGS,
		.default_range = 1000 | ROUND_UP_FLAG, /* dps */
		.rot_standard_ref = &base_standard_ref,
		.min_frequency = LSM6DSM_ODR_MIN_VAL,
		.max_frequency = LSM6DSM_ODR_MAX_VAL,
	},
};

unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

struct motion_sensor_t bma422_lid_accel = {
	.name = "Lid Accel",
	.active_mask = SENSOR_ACTIVE_S0_S3,
	.chip = MOTIONSENSE_CHIP_BMA422,
	.type = MOTIONSENSE_TYPE_ACCEL,
	.location = MOTIONSENSE_LOC_LID,
	.drv = &bma4_accel_drv,
	.mutex = &g_lid_mutex,
	.drv_data = &g_bma422_data,
	.port = I2C_PORT_SENSOR,
	.i2c_spi_addr_flags = BMA4_I2C_ADDR_PRIMARY,
	.rot_standard_ref = &lid_standard_ref,
	.default_range = 2,
	.min_frequency = BMA4_ACCEL_MIN_FREQ,
	.max_frequency = BMA4_ACCEL_MAX_FREQ,
	.config = {
		/* EC use accel for angle detection */
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 12500 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		},
		/* Sensor on in S3 */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 12500 | ROUND_UP_FLAG,
			.ec_rate = 0,
		},
	},
};

static void board_update_motion_sensor_config(void)
{
	if (get_cbi_ssfc_lid_sensor() == SSFC_SENSOR_BMA422)
		motion_sensors[LID_ACCEL] = bma422_lid_accel;
}

static const struct ec_response_keybd_config lantis_keybd_backlight = {
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_FORWARD,		/* T2 */
		TK_REFRESH,		/* T3 */
		TK_FULLSCREEN,		/* T4 */
		TK_OVERVIEW,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	/* No function keys, no numeric keypad and no screenlock key */
};

static const struct ec_response_keybd_config landia_keybd = {
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_FORWARD,		/* T2 */
		TK_REFRESH,		/* T3 */
		TK_FULLSCREEN,		/* T4 */
		TK_OVERVIEW,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	.capabilities = KEYBD_CAP_SCRNLOCK_KEY,
	/* No function keys and no numeric keypad */
};

static const struct ec_response_keybd_config landrid_keybd_backlight = {
	.num_top_row_keys = 13,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_KBD_BKLIGHT_TOGGLE,	/* T8 */
		TK_PLAY_PAUSE,		/* T9 */
		TK_MICMUTE,		/* T10 */
		TK_VOL_MUTE,		/* T11 */
		TK_VOL_DOWN,		/* T12 */
		TK_VOL_UP,		/* T13 */
	},
	.capabilities = KEYBD_CAP_NUMERIC_KEYPAD,
	/* No function keys and no screenlock key */
};

static const struct ec_response_keybd_config landrid_keybd = {
	.num_top_row_keys = 13,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_PLAY_PAUSE,		/* T8 */
		TK_MICMUTE,		/* T9 */
		TK_VOL_MUTE,		/* T10 */
		TK_VOL_DOWN,		/* T11 */
		TK_VOL_UP,		/* T12 */
		TK_MENU,		/* T13 */
	},
	.capabilities = KEYBD_CAP_NUMERIC_KEYPAD,
	/* No function keys and no screenlock key */
};

BUILD_ASSERT(IS_ENABLED(CONFIG_KEYBOARD_VIVALDI));
__override const struct ec_response_keybd_config *
board_vivaldi_keybd_config(void)
{
	if (get_cbi_fw_config_numeric_pad()) {
		if (get_cbi_fw_config_kblight())
			return &landrid_keybd_backlight;
		else
			return &landrid_keybd;
	} else {
		if (get_cbi_fw_config_tablet_mode())
			return &landia_keybd;
		else
			return &lantis_keybd_backlight;
	}
}

#define LANTIS_KEYBOARD_COL_DOWN 11
#define LANTIS_KEYBOARD_ROW_DOWN 6
#define LANTIS_KEYBOARD_COL_ESC 1
#define LANTIS_KEYBOARD_ROW_ESC 1
#define LANTIS_KEYBOARD_COL_KEY_H 6
#define LANTIS_KEYBOARD_ROW_KEY_H 1
#define LANTIS_KEYBOARD_COL_KEY_R 3
#define LANTIS_KEYBOARD_ROW_KEY_R 7
#define LANTIS_KEYBOARD_COL_LEFT_ALT 10
#define LANTIS_KEYBOARD_ROW_LEFT_ALT 6
#define LANTIS_KEYBOARD_COL_REFRESH 2
#define LANTIS_KEYBOARD_ROW_REFRESH 2
#define LANTIS_KEYBOARD_COL_RIGHT_ALT 10
#define LANTIS_KEYBOARD_ROW_RIGHT_ALT 0
#define LANTIS_KEYBOARD_COL_LEFT_SHIFT 7
#define LANTIS_KEYBOARD_ROW_LEFT_SHIFT 5

struct boot_key_entry boot_key_list[] = {
	[BOOT_KEY_ESC] = { LANTIS_KEYBOARD_COL_ESC, LANTIS_KEYBOARD_ROW_ESC },
	[BOOT_KEY_DOWN_ARROW] = { LANTIS_KEYBOARD_COL_DOWN,
				  LANTIS_KEYBOARD_ROW_DOWN },
	[BOOT_KEY_LEFT_SHIFT] = { LANTIS_KEYBOARD_COL_LEFT_SHIFT,
				  LANTIS_KEYBOARD_ROW_LEFT_SHIFT },
	[BOOT_KEY_REFRESH] = { LANTIS_KEYBOARD_COL_REFRESH,
			       LANTIS_KEYBOARD_ROW_REFRESH },
};
BUILD_ASSERT(ARRAY_SIZE(boot_key_list) == BOOT_KEY_COUNT);

struct keyboard_type key_typ = {
	.col_esc = LANTIS_KEYBOARD_COL_ESC,
	.row_esc = LANTIS_KEYBOARD_ROW_ESC,
	.col_down = LANTIS_KEYBOARD_COL_DOWN,
	.row_down = LANTIS_KEYBOARD_ROW_DOWN,
	.col_left_shift = LANTIS_KEYBOARD_COL_LEFT_SHIFT,
	.row_left_shift = LANTIS_KEYBOARD_ROW_LEFT_SHIFT,
	.col_refresh = LANTIS_KEYBOARD_COL_REFRESH,
	.row_refresh = LANTIS_KEYBOARD_ROW_REFRESH,
	.col_right_alt = LANTIS_KEYBOARD_COL_RIGHT_ALT,
	.row_right_alt = LANTIS_KEYBOARD_ROW_RIGHT_ALT,
	.col_left_alt = LANTIS_KEYBOARD_COL_LEFT_ALT,
	.row_left_alt = LANTIS_KEYBOARD_ROW_LEFT_ALT,
	.col_key_r = LANTIS_KEYBOARD_COL_KEY_R,
	.row_key_r = LANTIS_KEYBOARD_ROW_KEY_R,
	.col_key_h = LANTIS_KEYBOARD_COL_KEY_H,
	.row_key_h = LANTIS_KEYBOARD_ROW_KEY_H,
};

/* TODO(b/219051027): Add assert to check that key_typ.{row,col}_refresh == the
 * row/col in the tables above. */

static void board_update_no_keypad_by_fwconfig(void)
{
	if (!get_cbi_fw_config_numeric_pad()) {
		/* Disable scanning KSO13 & 14 if keypad isn't present. */
		keyboard_raw_set_cols(KEYBOARD_COLS_NO_KEYPAD);
		keyscan_config.actual_key_mask[11] = 0xfa;
		keyscan_config.actual_key_mask[12] = 0xca;
	}
}

static void board_update_keyboard_layout(void)
{
	if (get_cbi_fw_config_keyboard() == KB_LAYOUT_1) {
		/*
		 * If keyboard is UK(KB_LAYOUT_1), we need translate right ctrl
		 * to backslash(\|) key.
		 */
		set_scancode_set2(4, 0, get_scancode_set2(2, 7));
	}
	if (gpio_get_level(GPIO_EC_VIVALDIKEYBOARD_ID)) {
		key_typ.row_refresh = 3;
		boot_key_list[BOOT_KEY_REFRESH].row = 3;
	} else {
		key_typ.row_refresh = 2;
		boot_key_list[BOOT_KEY_REFRESH].row = 2;
	}
}

void board_init(void)
{
	int on;
	enum fw_config_db db = get_cbi_fw_config_db();

	if (db == DB_1A_HDMI || db == DB_LTE_HDMI || db == DB_1A_HDMI_LTE) {
		/* Select HDMI option */
		gpio_set_level(GPIO_HDMI_SEL_L, 0);
	} else {
		/* Select AUX option */
		gpio_set_level(GPIO_HDMI_SEL_L, 1);
	}

	gpio_enable_interrupt(GPIO_USB_C0_INT_ODL);
	gpio_enable_interrupt(GPIO_USB_C1_INT_ODL);

	/* Store board version for use in determining charge limits */
	cbi_get_board_version(&board_version);

	/*
	 * If interrupt lines are already low, schedule them to be processed
	 * after inits are completed.
	 */
	if (!gpio_get_level(GPIO_USB_C0_INT_ODL))
		hook_call_deferred(&check_c0_line_data, 0);
	if (!gpio_get_level(GPIO_USB_C1_INT_ODL))
		hook_call_deferred(&check_c1_line_data, 0);

	gpio_enable_interrupt(GPIO_USB_C0_CCSBU_OVP_ODL);

	if (get_cbi_fw_config_tablet_mode() == TABLET_MODE_PRESENT) {
		motion_sensor_count = ARRAY_SIZE(motion_sensors);
		/* Enable Base Accel interrupt */
		gpio_enable_interrupt(GPIO_BASE_SIXAXIS_INT_L);

		board_update_motion_sensor_config();
	} else {
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
		/* Base accel is not stuffed, don't allow line to float */
		gpio_set_flags(GPIO_BASE_SIXAXIS_INT_L,
			       GPIO_INPUT | GPIO_PULL_DOWN);
	}

	gpio_enable_interrupt(GPIO_PEN_DET_ODL);

	/* Charger on the MB will be outputting PROCHOT_ODL and OD CHG_DET */
	sm5803_configure_gpio0(CHARGER_PRIMARY, GPIO0_MODE_PROCHOT, 1);
	sm5803_configure_chg_det_od(CHARGER_PRIMARY, 1);

	if (board_get_charger_chip_count() > 1) {
		/* Charger on the sub-board will be a push-pull GPIO */
		sm5803_configure_gpio0(CHARGER_SECONDARY, GPIO0_MODE_OUTPUT, 0);
	}

	/* Turn on 5V if the system is on, otherwise turn it off */
	on = chipset_in_state(CHIPSET_STATE_ON | CHIPSET_STATE_ANY_SUSPEND |
			      CHIPSET_STATE_SOFT_OFF);
	board_power_5v_enable(on);

	board_update_no_keypad_by_fwconfig();
	board_update_keyboard_layout();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static void board_resume(void)
{
	sm5803_disable_low_power_mode(CHARGER_PRIMARY);
	if (board_get_charger_chip_count() > 1)
		sm5803_disable_low_power_mode(CHARGER_SECONDARY);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_resume, HOOK_PRIO_DEFAULT);

static void board_suspend(void)
{
	sm5803_enable_low_power_mode(CHARGER_PRIMARY);
	if (board_get_charger_chip_count() > 1)
		sm5803_enable_low_power_mode(CHARGER_SECONDARY);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_suspend, HOOK_PRIO_DEFAULT);

void board_hibernate(void)
{
	/*
	 * Put all charger ICs present into low power mode before entering
	 * z-state.
	 */
	sm5803_hibernate(CHARGER_PRIMARY);
	if (board_get_charger_chip_count() > 1)
		sm5803_hibernate(CHARGER_SECONDARY);
}

__override void board_ocpc_init(struct ocpc_data *ocpc)
{
	/* There's no provision to measure Isys */
	ocpc->chg_flags[CHARGER_SECONDARY] |= OCPC_NO_ISYS_MEAS_CAP;
}

__override void board_pulse_entering_rw(void)
{
	/*
	 * On the ITE variants, the EC_ENTERING_RW signal was connected to a pin
	 * which is active high by default.  This causes Cr50 to think that the
	 * EC has jumped to its RW image even though this may not be the case.
	 * The pin is changed to GPIO_EC_ENTERING_RW2.
	 */
	gpio_set_level(GPIO_EC_ENTERING_RW, 1);
	gpio_set_level(GPIO_EC_ENTERING_RW2, 1);
	crec_usleep(MSEC);
	gpio_set_level(GPIO_EC_ENTERING_RW, 0);
	gpio_set_level(GPIO_EC_ENTERING_RW2, 0);
}

void board_reset_pd_mcu(void)
{
	/*
	 * Nothing to do.  TCPC C0 is internal, TCPC C1 reset pin is not
	 * connected to the EC.
	 */
}

__override void board_power_5v_enable(int enable)
{
	/*
	 * Motherboard has a GPIO to turn on the 5V regulator, but the sub-board
	 * sets it through the charger GPIO.
	 */
	gpio_set_level(GPIO_EN_PP5000, !!enable);

	if (board_get_charger_chip_count() > 1) {
		if (sm5803_set_gpio0_level(1, !!enable))
			CPRINTUSB("Failed to %sable sub rails!",
				  enable ? "en" : "dis");
	}
}

__override uint8_t board_get_usb_pd_port_count(void)
{
	enum fw_config_db db = get_cbi_fw_config_db();

	if (db == DB_1A_HDMI || db == DB_NONE || db == DB_LTE_HDMI ||
	    db == DB_1A_HDMI_LTE)
		return CONFIG_USB_PD_PORT_MAX_COUNT - 1;
	else if (db == DB_1C || db == DB_1C_LTE || db == DB_1C_1A ||
		 db == DB_1C_1A_LTE)
		return CONFIG_USB_PD_PORT_MAX_COUNT;

	ccprints("Unhandled DB configuration: %d", db);
	return 0;
}

__override uint8_t board_get_charger_chip_count(void)
{
	enum fw_config_db db = get_cbi_fw_config_db();

	if (db == DB_1A_HDMI || db == DB_NONE || db == DB_LTE_HDMI ||
	    db == DB_1A_HDMI_LTE)
		return CHARGER_NUM - 1;
	else if (db == DB_1C || db == DB_1C_LTE || db == DB_1C_1A ||
		 db == DB_1C_1A_LTE)
		return CHARGER_NUM;

	ccprints("Unhandled DB configuration: %d", db);
	return 0;
}

uint16_t tcpc_get_alert_status(void)
{
	/*
	 * TCPC 0 is embedded in the EC and processes interrupts in the chip
	 * code (it83xx/intc.c)
	 */

	uint16_t status = 0;
	int regval;

	/* Check whether TCPC 1 pulled the shared interrupt line */
	if (!gpio_get_level(GPIO_USB_C1_INT_ODL)) {
		if (!tcpc_read16(1, TCPC_REG_ALERT, &regval)) {
			if (regval)
				status = PD_STATUS_TCPC_ALERT_1;
		}
	}

	return status;
}

int board_set_active_charge_port(int port)
{
	int is_valid_port = (port >= 0 && port < board_get_usb_pd_port_count());

	if (!is_valid_port && port != CHARGE_PORT_NONE)
		return EC_ERROR_INVAL;

	if (port == CHARGE_PORT_NONE) {
		CPRINTUSB("Disabling all charge ports");

		sm5803_vbus_sink_enable(CHARGER_PRIMARY, 0);

		if (board_get_charger_chip_count() > 1)
			sm5803_vbus_sink_enable(CHARGER_SECONDARY, 0);

		return EC_SUCCESS;
	}

	CPRINTUSB("New chg p%d", port);

	/*
	 * Ensure other port is turned off, then enable new charge port
	 */
	if (port == 0) {
		if (board_get_charger_chip_count() > 1)
			sm5803_vbus_sink_enable(CHARGER_SECONDARY, 0);
		sm5803_vbus_sink_enable(CHARGER_PRIMARY, 1);

	} else {
		sm5803_vbus_sink_enable(CHARGER_PRIMARY, 0);
		sm5803_vbus_sink_enable(CHARGER_SECONDARY, 1);
	}

	return EC_SUCCESS;
}

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/* Vconn control is only for port 0 */
	if (port)
		return;

	if (cc_pin == USBPD_CC_PIN_1)
		gpio_set_level(GPIO_EN_USB_C0_CC1_VCONN, !!enabled);
	else
		gpio_set_level(GPIO_EN_USB_C0_CC2_VCONN, !!enabled);
}

__override void typec_set_source_current_limit(int port, enum tcpc_rp_value rp)
{
	int current;

	if (port < 0 || port > board_get_usb_pd_port_count())
		return;

	current = (rp == TYPEC_RP_3A0) ? 3000 : 1500;

	charger_set_otg_current_voltage(port, current, 5000);
}

__override uint16_t board_get_ps8xxx_product_id(int port)
{
	/* Lantis variant doesn't have ps8xxx product in the port 0 */
	if (port == 0)
		return 0;

	switch (get_cbi_ssfc_tcpc_p1()) {
	case SSFC_TCPC_P1_PS8805:
		return PS8805_PRODUCT_ID;
	case SSFC_TCPC_P1_DEFAULT:
	case SSFC_TCPC_P1_PS8705:
	default:
		return PS8705_PRODUCT_ID;
	}
}

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = { [PWM_CH_KBLIGHT] = {
					      .channel = 0,
					      .flags = PWM_CONFIG_DSLEEP,
					      .freq_hz = 100,
				      } };
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/* Thermistors */
const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_1] = { .name = "Memory",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_1 },
	[TEMP_SENSOR_2] = { .name = "Ambient",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_2 },
	[TEMP_SENSOR_3] = { .name = "Charger",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_3 },
	[TEMP_SENSOR_4] = { .name = "5V regular",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_4 },
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

__override void ocpc_get_pid_constants(int *kp, int *kp_div, int *ki,
				       int *ki_div, int *kd, int *kd_div)
{
	*kp = 3;
	*kp_div = 20;

	*ki = 3;
	*ki_div = 125;

	*kd = 4;
	*kd_div = 40;
}

#ifdef CONFIG_KEYBOARD_FACTORY_TEST
/*
 * Map keyboard connector pins to EC GPIO pins for factory test.
 * Pins mapped to {-1, -1} are skipped.
 * The connector has 24 pins total, and there is no pin 0.
 */
const int keyboard_factory_scan_pins[][2] = {
	{ -1, -1 },	   { GPIO_KSO_H, 4 }, { GPIO_KSO_H, 0 },
	{ GPIO_KSO_H, 1 }, { GPIO_KSO_H, 3 }, { GPIO_KSO_H, 2 },
	{ GPIO_KSO_L, 5 }, { GPIO_KSO_L, 6 }, { GPIO_KSO_L, 3 },
	{ GPIO_KSO_L, 2 }, { GPIO_KSI, 0 },   { GPIO_KSO_L, 1 },
	{ GPIO_KSO_L, 4 }, { GPIO_KSI, 3 },   { GPIO_KSI, 2 },
	{ GPIO_KSO_L, 0 }, { GPIO_KSI, 5 },   { GPIO_KSI, 4 },
	{ GPIO_KSO_L, 7 }, { GPIO_KSI, 6 },   { GPIO_KSI, 7 },
	{ GPIO_KSI, 1 },   { -1, -1 },	      { -1, -1 },
	{ -1, -1 },
};

const int keyboard_factory_scan_pins_used =
	ARRAY_SIZE(keyboard_factory_scan_pins);
#endif
