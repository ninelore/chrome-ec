/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Mushu board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Reduce flash usage */
#undef CONFIG_ACCEL_SPOOF_MODE
#undef CONFIG_CONSOLE_CMDHELP
#undef CONFIG_CONSOLE_HISTORY
#define CONFIG_USB_PD_DEBUG_LEVEL 0
#undef CONFIG_CMD_ACCELSPOOF
#undef CONFIG_CMD_ACCELS
#undef CONFIG_CMD_APTHROTTLE
#undef CONFIG_CMD_BATTFAKE
#undef CONFIG_CMD_CHARGE_SUPPLIER_INFO
#undef CONFIG_CMD_CHARGER_DUMP
#undef CONFIG_CMD_HCDEBUG
#undef CONFIG_CMD_I2C_SCAN
#undef CONFIG_CMD_I2C_XFER
#undef CONFIG_CMD_PPC_DUMP
#undef CONFIG_CMD_SYSLOCK
#undef CONFIG_CMD_TIMERINFO

#define CONFIG_POWER_BUTTON
#define CONFIG_KEYBOARD_PROTOCOL_8042
#define CONFIG_LED_COMMON
#define CONFIG_LOW_POWER_IDLE

#define CONFIG_HOST_INTERFACE_ESPI
#undef CONFIG_CMD_MFALLOW

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/* Keyboard features */
#define CONFIG_PWM_KBLIGHT

/* Sensors */
/* BMI160 Base accel/gyro */
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#define CONFIG_ACCELGYRO_BMI160_INT2_OUTPUT
/* BMA253 Lid accel */
#define CONFIG_ACCEL_BMA255
#define CONFIG_ACCEL_FORCE_MODE_MASK (BIT(LID_ACCEL) | BIT(CLEAR_ALS))
#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL
#define CONFIG_LID_ANGLE_UPDATE
/* TC3400 ALS */
#define CONFIG_ALS
#define ALS_COUNT 1
#define CONFIG_ALS_TCS3400
#define CONFIG_ALS_TCS3400_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(CLEAR_ALS)
#define I2C_PORT_ALS I2C_PORT_SENSOR
#define CONFIG_TEMP_SENSOR
/* AMD SMBUS Temp sensors */
#define CONFIG_TEMP_SENSOR_AMD_R19ME4070
/* F75303 on I2C bus */
#define CONFIG_TEMP_SENSOR_F75303
/* Temp sensor is on port 4 on baseboard but on port 0 on Mushu */
#undef I2C_PORT_THERMAL
#define I2C_PORT_THERMAL I2C_PORT_SENSOR

/* GPU features */
#define I2C_PORT_GPU NPCX_I2C_PORT4_1

/* USB Type C and USB PD defines */
#undef CONFIG_USB_PD_TCPMV1
/*
 * Enable TCPMv2. Use default PD 2.0 operation because we have a
 * parade PS8751 TCPC
 */
#define CONFIG_USB_PD_TCPMV2
#undef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
#define CONFIG_USB_PID 0x5047
#define CONFIG_USB_PD_DECODE_SOP
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_DRP_ACC_TRYSRC
#define CONFIG_USB_PD_COMM_LOCKED
#define CONFIG_USB_PD_TCPM_ANX7447
#define CONFIG_USB_PD_TCPM_ANX7447_AUX_PU_PD
#define CONFIG_USB_PD_TCPM_PS8751
#define BOARD_TCPC_C0_RESET_HOLD_DELAY ANX74XX_RESET_HOLD_MS
#define BOARD_TCPC_C0_RESET_POST_DELAY ANX74XX_RESET_HOLD_MS
#define BOARD_TCPC_C1_RESET_HOLD_DELAY PS8XXX_RESET_DELAY_MS
#define BOARD_TCPC_C1_RESET_POST_DELAY 0
#define GPIO_USB_C1_TCPC_RST GPIO_USB_C1_TCPC_RST_ODL

/* USB Type A Features */
#define CONFIG_USB_PORT_POWER_SMART
#undef CONFIG_USB_PORT_POWER_SMART_PORT_COUNT
#define CONFIG_USB_PORT_POWER_SMART_PORT_COUNT 1
#define CONFIG_USB_PORT_POWER_SMART_CDP_SDP_ONLY
#define GPIO_USB1_ILIM_SEL GPIO_EN_USB_A_LOW_PWR_OD

/* BC 1.2 */
#define CONFIG_BC12_DETECT_PI3USB9201

/* Charger features */
/*
 * The IDCHG current limit is set in 512 mA steps. The value set here is
 * somewhat specific to the battery pack being currently used. The limit here
 * was set based on the battery's discharge current limit and what was tested to
 * prevent the AP rebooting with low charge level batteries.
 *
 * TODO(b/133447140): Revisit this threshold once peak power consumption tuning
 * for the AP is completed.
 */
#define CONFIG_CHARGER_BQ25710_IDCHG_LIMIT_MA 8192

/* Volume Button feature */
#define CONFIG_VOLUME_BUTTONS
#define GPIO_VOLUME_UP_L GPIO_EC_VOLUP_BTN_ODL
#define GPIO_VOLUME_DOWN_L GPIO_EC_VOLDN_BTN_ODL

/* Fan features */
#define CONFIG_FANS 2
#define CONFIG_CUSTOM_FAN_CONTROL
#undef CONFIG_FAN_INIT_SPEED
#define CONFIG_FAN_INIT_SPEED 50
#define CONFIG_TEMP_SENSOR_POWER
#define CONFIG_THERMISTOR
#define CONFIG_THROTTLE_AP
#define CONFIG_STEINHART_HART_3V3_30K9_47K_4050B

/* MST */
/*
 * TDOD (b/124068003): This inherently assumes the MST chip is connected to only
 * one Type C port. This will need to be chagned to support 2 Type C ports
 * connected to the same MST chip.
 */
#define USB_PD_PORT_TCPC_MST USB_PD_PORT_TCPC_1

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_PCH_RSMRST_L GPIO_EC_PCH_RSMRST_L
#define GPIO_PCH_SLP_S0_L GPIO_SLP_S0_L
#define GPIO_CPU_PROCHOT GPIO_EC_PROCHOT_ODL
#define GPIO_AC_PRESENT GPIO_ACOK_OD
#define GPIO_PG_EC_RSMRST_ODL GPIO_PG_EC_RSMRST_L
#define GPIO_PCH_SYS_PWROK GPIO_EC_PCH_SYS_PWROK
#define GPIO_PCH_SLP_S3_L GPIO_SLP_S3_L
#define GPIO_PCH_SLP_S4_L GPIO_SLP_S4_L
#define GPIO_TEMP_SENSOR_POWER GPIO_EN_A_RAILS
#define GPIO_EN_PP5000 GPIO_EN_PP5000_A

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

/* GPIO signals updated base on board version. */
#define GPIO_EN_PP5000_A gpio_en_pp5000_a
extern enum gpio_signal gpio_en_pp5000_a;

enum adc_channel {
	ADC_TEMP_SENSOR_1, /* ADC0 */
	ADC_TEMP_SENSOR_2, /* ADC1 */
	ADC_CH_COUNT
};

enum sensor_id {
	LID_ACCEL = 0,
	BASE_ACCEL,
	BASE_GYRO,
	CLEAR_ALS,
	RGB_ALS,
	SENSOR_COUNT,
};

enum pwm_channel { PWM_CH_KBLIGHT, PWM_CH_FAN, PWM_CH_FAN2, PWM_CH_COUNT };

enum fan_channel {
	FAN_CH_0 = 0,
	FAN_CH_1,
	/* Number of FAN channels */
	FAN_CH_COUNT,
};

enum mft_channel {
	MFT_CH_0 = 0,
	MFT_CH_1,
	/* Number of MFT channels */
	MFT_CH_COUNT,
};

enum temp_sensor_id {
	TEMP_CHARGER,
	TEMP_5V,
	TEMP_GPU,
	TEMP_F75303_LOCAL,
	TEMP_F75303_GPU,
	TEMP_F75303_GPU_POWER,
	TEMP_SENSOR_COUNT
};

/* List of possible batteries */
enum battery_type {
	BATTERY_POWER_TECH,
	BATTERY_SMP_SDI,
	BATTERY_TYPE_COUNT,
};

#undef CONFIG_USB_PD_OPERATING_POWER_MW
#define CONFIG_USB_PD_OPERATING_POWER_MW 15000
#undef CONFIG_USB_PD_MAX_POWER_MW
#define CONFIG_USB_PD_MAX_POWER_MW 100000
#undef CONFIG_USB_PD_MAX_CURRENT_MA
#define CONFIG_USB_PD_MAX_CURRENT_MA 5000
#undef CONFIG_USB_PD_MAX_VOLTAGE_MV
#define CONFIG_USB_PD_MAX_VOLTAGE_MV 20000

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
