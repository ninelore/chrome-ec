/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Boten board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Select Baseboard features */
#define VARIANT_DEDEDE_EC_IT8320
#include "baseboard.h"

/* Battery */
#define CONFIG_BATTERY_FUEL_GAUGE
#define CONFIG_BATTERY_V2
#define CONFIG_BATTERY_COUNT 1
#define CONFIG_HOSTCMD_BATTERY_V2

/* BC 1.2 */
#define CONFIG_BC12_DETECT_PI3USB9201

/* Charger */
#define CONFIG_CHARGE_RAMP_HW
#define CONFIG_CHARGER_RAA489000
#define CONFIG_USB_PD_MAX_VOLTAGE_MV 20000
#define CONFIG_CHARGER_SENSE_RESISTOR_AC 10
#define CONFIG_CHARGER_SENSE_RESISTOR 10
#define CONFIG_OCPC_DEF_RBATT_MOHMS               \
	22 /* R_DS(on) 11.6mOhm + 10mOhm sns rstr \
	    */
#undef CONFIG_USB_PD_TCPC_LPM_EXIT_DEBOUNCE
#define CONFIG_USB_PD_TCPC_LPM_EXIT_DEBOUNCE (100 * MSEC)
/*
 * b/147463641: The charger IC seems to overdraw ~4%, therefore we
 * reduce our target accordingly.
 */
#undef CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT
#define CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT 4

/* DAC for PSYS */
#define CONFIG_DAC

/* LED */
#define CONFIG_LED_ONOFF_STATES

/* PWM */
#define CONFIG_PWM

/* Sensors */
#define CONFIG_ACCEL_LIS2DWL /* Lid accel */
#define CONFIG_ACCELGYRO_LSM6DSM /* Base accel */
#define CONFIG_ACCEL_LSM6DSM_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
/* Sensors without hardware FIFO are in forced mode */
#define CONFIG_ACCEL_FORCE_MODE_MASK BIT(LID_ACCEL)

/* Enable sensor fifo, must also define the _SIZE and _THRES */
#define CONFIG_ACCEL_FIFO
/* Power of 2 - Too large of a fifo causes too much timestamp jitter */
#define CONFIG_ACCEL_FIFO_SIZE 256
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO_SIZE / 3)

#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_UPDATE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL

#define CONFIG_TABLET_MODE
#define CONFIG_TABLET_MODE_SWITCH
#define CONFIG_GMR_TABLET_MODE

/* TCPC */
#define CONFIG_USB_PD_PORT_MAX_COUNT 1
#define CONFIG_USB_PD_TCPM_RAA489000

/* USB defines specific to external TCPCs */
#define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USB_PD_DISCHARGE_TCPC
#define CONFIG_USB_PD_TCPC_LOW_POWER

/* EC console commands */
#define CONFIG_CMD_TCPC_DUMP
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO

/* Keyboard */
#define CONFIG_KEYBOARD_STRICT_DEBOUNCE

/* Variant references the TCPCs to determine Vbus sourcing */
#define CONFIG_USB_PD_5V_EN_CUSTOM

/* Thermistors */
#define CONFIG_TEMP_SENSOR
#define CONFIG_THERMISTOR
#define CONFIG_STEINHART_HART_3V3_51K1_47K_4050B

/* USB Mux */
#define CONFIG_USB_MUX_IT5205

/* USB Type A Features */
#define USB_PORT_COUNT 1
#define CONFIG_USB_PORT_POWER_DUMB

#define CONFIG_DYNAMIC_MOTION_SENSOR_COUNT
#define CONFIG_BUTTONS_RUNTIME_CONFIG

#undef CONFIG_SYSTEM_SAFE_MODE
#undef CONFIG_PANIC_ON_WATCHDOG_WARNING

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum pwm_channel {
	PWM_CH_KBLIGHT,
	PWM_CH_LED_RED,
	PWM_CH_LED_GREEN,
	PWM_CH_LED_WHITE,
	PWM_CH_COUNT,
};

/* Motion sensors */
enum sensor_id { LID_ACCEL, BASE_ACCEL, BASE_GYRO, SENSOR_COUNT };

/* ADC channels */
enum adc_channel {
	ADC_VSNS_PP3300_A, /* ADC0 */
	ADC_TEMP_SENSOR_1, /* ADC2 */
	ADC_TEMP_SENSOR_2, /* ADC3 */
	ADC_CH_COUNT
};

enum temp_sensor_id { TEMP_SENSOR_1, TEMP_SENSOR_2, TEMP_SENSOR_COUNT };

/* List of possible batteries */
enum battery_type {
	BATTERY_SMP,
	BATTERY_LGC,
	BATTERY_SUNWODA,
	BATTERY_TYPE_COUNT,
};

void pen_detect_interrupt(enum gpio_signal s);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
