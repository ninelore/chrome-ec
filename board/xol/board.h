/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Xol board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "compile_time_macros.h"

/* Baseboard features */
#include "baseboard.h"

/*
 * This will happen automatically on NPCX9 ES2 and later. Do not remove
 * until we can confirm all earlier chips are out of service.
 */
#define CONFIG_HIBERNATE_PSL_VCC1_RST_WAKEUP

/* Chipset */
#define CONFIG_CHIPSET_RESUME_INIT_HOOK

#define CONFIG_MP2964
#define CONFIG_KEYBOARD_STRAUSS
#define CONFIG_KEYBOARD_REFRESH_ROW3

#define CONFIG_CMD_ACCEL_INFO
#define CONFIG_CMD_ACCELS

/* ALS */
#define CONFIG_ALS
#define ALS_COUNT 1
#define CONFIG_ALS_VEML3328

/* Sensors without hardware FIFO are in forced mode */
#define CONFIG_ACCEL_FORCE_MODE_MASK (BIT(CLEAR_ALS) | BIT(RGB_ALS))

/* LED */
#define CONFIG_LED_COMMON
#define CONFIG_LED_ONOFF_STATES
#define GPIO_BAT_LED_RED_L GPIO_LED_R_ODL
#define GPIO_BAT_LED_GREEN_L GPIO_LED_G_ODL
#define GPIO_PWR_LED_BLUE_L GPIO_LED_B_ODL

/* USB Type A Features */
#define USB_PORT_COUNT 1
#define CONFIG_USB_PORT_POWER_DUMB

#define CONFIG_IO_EXPANDER
#define CONFIG_IO_EXPANDER_NCT38XX
#define CONFIG_IO_EXPANDER_PORT_COUNT 2

/* I2C speed console command */
#define CONFIG_CMD_I2C_SPEED

/* I2C control host command */
#define CONFIG_HOSTCMD_I2C_CONTROL

#define CONFIG_USBC_PPC_NX20P3483
#define CONFIG_USBC_NX20P348X_RCP_5VSRC_MASK_ENABLE
#define CONFIG_USBC_NX20P348X_VBUS_DISCHARGE_BY_SRC_EN

/* TODO: b/177608416 - measure and check these values on brya */
#define PD_POWER_SUPPLY_TURN_ON_DELAY 30000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 30000 /* us */
#define PD_VCONN_SWAP_DELAY 5000 /* us */

/* PD */
#define CONFIG_USB_PD_OPERATING_POWER_MW 15000
#define CONFIG_USB_PD_MAX_POWER_MW 60000
#define CONFIG_USB_PD_MAX_CURRENT_MA 3000
#define CONFIG_USB_PD_MAX_VOLTAGE_MV 20000
#define CONFIG_USB_PD_PREFER_HIGH_VOLTAGE

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_AC_PRESENT GPIO_ACOK_OD
#define GPIO_CPU_PROCHOT GPIO_EC_PROCHOT_ODL
#define GPIO_EC_INT_L GPIO_EC_PCH_INT_ODL
#define GPIO_ENTERING_RW GPIO_EC_ENTERING_RW
#define GPIO_KBD_KSO2 GPIO_EC_KSO_02_INV
#define GPIO_PACKET_MODE_EN GPIO_EC_GSC_PACKET_MODE
#define GPIO_PCH_PWRBTN_L GPIO_EC_PCH_PWR_BTN_ODL
#define GPIO_PCH_RSMRST_L GPIO_EC_PCH_RSMRST_L
#define GPIO_PCH_RTCRST GPIO_EC_PCH_RTCRST
#define GPIO_PCH_SLP_S0_L GPIO_SYS_SLP_S0IX_L
#define GPIO_PCH_SLP_S3_L GPIO_SLP_S3_L
#define GPIO_TEMP_SENSOR_POWER GPIO_SEQ_EC_DSW_PWROK

/*
 * GPIO_EC_PCH_INT_ODL is used for MKBP events as well as a PCH wakeup
 * signal.
 */
#define GPIO_PCH_WAKE_L GPIO_EC_PCH_INT_ODL
#define GPIO_PG_EC_ALL_SYS_PWRGD GPIO_SEQ_EC_ALL_SYS_PG
#define GPIO_PG_EC_DSW_PWROK GPIO_SEQ_EC_DSW_PWROK
#define GPIO_PG_EC_RSMRST_ODL GPIO_SEQ_EC_RSMRST_ODL
#define GPIO_POWER_BUTTON_L GPIO_GSC_EC_PWR_BTN_ODL
#define GPIO_SYS_RESET_L GPIO_SYS_RST_ODL
#define GPIO_WP_L GPIO_EC_WP_ODL

#undef CONFIG_VOLUME_BUTTONS
#undef CONFIG_BC12_DETECT_PI3USB9201
#undef CONFIG_BACKLIGHT_LID
#undef CONFIG_TABLET_MODE
#undef CONFIG_TABLET_MODE_SWITCH
#undef CONFIG_GMR_TABLET_MODE
#undef CONFIG_USB_CHARGER

#define CONFIG_CHARGER_PROFILE_OVERRIDE
#define CONFIG_BATTERY_CHECK_CHARGE_TEMP_LIMITS
#undef CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT
#define CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT 5

/* System has back-lit keyboard */
#define CONFIG_PWM_KBLIGHT

/* I2C Bus Configuration */

#define I2C_PORT_SENSOR NPCX_I2C_PORT0_0

#define I2C_PORT_USB_C0_C2_TCPC NPCX_I2C_PORT1_0

#define I2C_PORT_USB_C0_C2_PPC NPCX_I2C_PORT2_0

#define I2C_PORT_BATTERY NPCX_I2C_PORT5_0
#define I2C_PORT_CHARGER NPCX_I2C_PORT7_0
#define I2C_PORT_EEPROM NPCX_I2C_PORT7_0
#define I2C_PORT_MP2964 NPCX_I2C_PORT7_0

#define I2C_ADDR_EEPROM_FLAGS 0x50

#define I2C_ADDR_MP2964_FLAGS 0x20

/* Thermal features */
#define CONFIG_THERMISTOR
#define CONFIG_TEMP_SENSOR
#define CONFIG_TEMP_SENSOR_POWER
#define CONFIG_STEINHART_HART_3V3_51K1_47K_4050B

#define CONFIG_FANS FAN_CH_COUNT
#define CONFIG_CUSTOM_FAN_CONTROL
#define RPM_DEVIATION 1

/* Charger defines */
#define CONFIG_CHARGER_BQ25710
#define CONFIG_CHARGER_BQ25710_SENSE_RESISTOR 10
#define CONFIG_CHARGER_BQ25710_SENSE_RESISTOR_AC 10
#define CONFIG_CHARGER_BQ25710_PSYS_SENSING
#define CONFIG_CHARGER_BQ25710_IDCHG_LIMIT_MA 5632
#define CONFIG_CHARGER_BQ25710_PP_INOM

/* Enable sensor fifo, must also define the _SIZE and _THRES */
#define CONFIG_ACCEL_FIFO
/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO_SIZE 256
/* Depends on how fast the AP boots and typical ODRs */
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO_SIZE / 3)

/* Keyboard */
#define KEYBOARD_COLS_MAX 18
#define CONFIG_KEYBOARD_KEYPAD
#define KEYBOARD_DEFAULT_COL_VOL_UP 12

/* Remove bringup features */
#undef CONFIG_CMD_POWERINDEBUG

#ifndef __ASSEMBLER__

#include "gpio_signal.h" /* needed by registers.h */
#include "registers.h"
#include "usbc_config.h"

/* I2C access in polling mode before task is initialized */
#define CONFIG_I2C_BITBANG_CROS_EC

enum banshee_bitbang_i2c_channel {
	I2C_BITBANG_CHAN_BRD_ID,
	I2C_BITBANG_CHAN_COUNT
};
#define I2C_BITBANG_PORT_COUNT I2C_BITBANG_CHAN_COUNT

enum adc_channel {
	ADC_TEMP_SENSOR_1,
	ADC_TEMP_SENSOR_2,
	ADC_TEMP_SENSOR_3,
	ADC_CH_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_1,
	TEMP_SENSOR_2,
	TEMP_SENSOR_3,
	TEMP_SENSOR_COUNT
};

enum ioex_port { IOEX_C0_NCT38XX = 0, IOEX_C2_NCT38XX, IOEX_PORT_COUNT };

enum battery_type { BATTERY_SDI, BATTERY_SWD, BATTERY_TYPE_COUNT };

enum pwm_channel {
	PWM_CH_KBLIGHT = 0, /* PWM3 */
	PWM_CH_FAN, /* PWM5 */
	PWM_CH_COUNT
};

enum sensor_id { CLEAR_ALS = 0, RGB_ALS, SENSOR_COUNT };

enum fan_channel { FAN_CH_0 = 0, FAN_CH_COUNT };

enum mft_channel { MFT_CH_0 = 0, MFT_CH_COUNT };

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
