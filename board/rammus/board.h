/* Copyright 2018 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rammus board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*
 * By default, enable all console messages excepted HC, ACPI and event:
 * The sensor stack is generating a lot of activity.
 */
#define CC_DEFAULT (CC_ALL & ~(CC_MASK(CC_EVENTS) | CC_MASK(CC_LPC)))
#undef CONFIG_HOSTCMD_DEBUG_MODE
#define CONFIG_HOSTCMD_DEBUG_MODE HCDEBUG_OFF

/* EC */
#define CONFIG_ADC
#define CONFIG_BACKLIGHT_LID
#define CONFIG_BOARD_VERSION_CBI
#define CONFIG_BOARD_FORCE_RESET_PIN
#define CONFIG_CRC8
#define CONFIG_CBI_EEPROM
#define CONFIG_DPTF
#define CONFIG_FLASH_SIZE_BYTES 0x80000
#define CONFIG_FPU
#define CONFIG_I2C
#define CONFIG_I2C_CONTROLLER
#define CONFIG_KEYBOARD_COL2_INVERTED
#undef CONFIG_KEYBOARD_VIVALDI
#define CONFIG_KEYBOARD_PROTOCOL_8042
#define CONFIG_LED_COMMON
#define CONFIG_LID_SWITCH
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_LTO
#define CONFIG_CHIP_PANIC_BACKUP
#define CONFIG_PWM
#define CONFIG_PWM_KBLIGHT
#define CONFIG_SPI_FLASH_REGS
#define CONFIG_SPI_FLASH_W25X40
#define CONFIG_VBOOT_HASH
#define CONFIG_SHA256_UNROLLED
#define CONFIG_VOLUME_BUTTONS
#define CONFIG_VSTORE
#define CONFIG_VSTORE_SLOT_COUNT 1
#define CONFIG_WATCHDOG_HELP
#define CONFIG_WIRELESS
#define CONFIG_WIRELESS_SUSPEND \
	(EC_WIRELESS_SWITCH_WLAN | EC_WIRELESS_SWITCH_WLAN_POWER)
#define WIRELESS_GPIO_WLAN GPIO_WLAN_OFF_L
#define WIRELESS_GPIO_WLAN_POWER GPIO_EN_PP3300_DX_WLAN

/* EC console commands */
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO
#define CONFIG_CMD_BUTTON

/* Port80 */
#undef CONFIG_PORT80_HISTORY_LEN
#define CONFIG_PORT80_HISTORY_LEN 256

/* SOC */
#define CONFIG_CHIPSET_SKYLAKE
#define CONFIG_CHIPSET_HAS_PLATFORM_PMIC_RESET
#define CONFIG_CHIPSET_RESET_HOOK
#define CONFIG_HOST_INTERFACE_ESPI
#define CONFIG_HOST_INTERFACE_ESPI_VW_SLP_S3
#define CONFIG_HOST_INTERFACE_ESPI_VW_SLP_S4
#define CONFIG_HOSTCMD_FLASH_SPI_INFO

/* Battery */
#define CONFIG_BATTERY_CUT_OFF
#define CONFIG_BATTERY_HW_PRESENT_CUSTOM
#define CONFIG_BATTERY_DEVICE_CHEMISTRY "LION"
#define CONFIG_BATTERY_SMART
#undef CONFIG_BATT_HOST_FULL_FACTOR
#define CONFIG_BATT_HOST_FULL_FACTOR 94

/* Charger */
#define CONFIG_CHARGE_MANAGER
#define CONFIG_CHARGE_RAMP_HW /* This, or just RAMP? */

#define CONFIG_CHARGER
#define CONFIG_CHARGER_ISL9238
#define CONFIG_CHARGER_DISCHARGE_ON_AC
#define CONFIG_CHARGER_DEFAULT_CURRENT_LIMIT 512
#define CONFIG_CHARGER_MIN_INPUT_CURRENT_LIMIT 512
#define CONFIG_CHARGER_PSYS
#define CONFIG_CHARGER_SENSE_RESISTOR 10
#define CONFIG_CHARGER_SENSE_RESISTOR_AC 20
#undef CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT
#define CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT 4 /* charger margin */
#define CONFIG_CMD_CHARGER_ADC_AMON_BMON
#define CONFIG_HOSTCMD_PD_CONTROL
#define CONFIG_EXTPOWER_GPIO
#undef CONFIG_EXTPOWER_DEBOUNCE_MS
#define CONFIG_EXTPOWER_DEBOUNCE_MS 1000
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_X86
#define CONFIG_POWER_COMMON
#define CONFIG_POWER_SIGNAL_INTERRUPT_STORM_DETECT_THRESHOLD 30
#define CONFIG_POWER_S0IX
#define CONFIG_POWER_TRACK_HOST_SLEEP_STATE

/* USB-A config */
#define CONFIG_USB_PORT_POWER_SMART
#define CONFIG_USB_PORT_POWER_SMART_CDP_SDP_ONLY
#define CONFIG_USB_PORT_POWER_SMART_DEFAULT_MODE USB_CHARGE_MODE_CDP
#define CONFIG_USB_PORT_POWER_SMART_INVERTED
#undef CONFIG_USB_PORT_POWER_SMART_PORT_COUNT
#define CONFIG_USB_PORT_POWER_SMART_PORT_COUNT 1
#define GPIO_USB1_ILIM_SEL GPIO_USB_A_CHARGE_EN_L

/* Sensor */
#define CONFIG_TEMP_SENSOR
#define CONFIG_TEMP_SENSOR_BD99992GW
#define CONFIG_THERMISTOR_NCP15WB

#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_HOST_EVENT
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#define CONFIG_ACCELGYRO_ICM426XX
#define CONFIG_ACCELGYRO_ICM426XX_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#define CONFIG_ACCELGYRO_BMI160_INT2_OUTPUT
#define CONFIG_ACCEL_BMA255
#define CONFIG_ACCEL_KX022
#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL
#define CONFIG_LID_ANGLE_UPDATE

#undef CONFIG_SENSOR_TIGHT_TIMESTAMPS

/* Enable sensor fifo, must also define the _SIZE and _THRES */
#define CONFIG_ACCEL_FIFO
/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO_SIZE 512
/* Depends on how fast the AP boots and typical ODRs */
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO_SIZE / 3)

#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 1024

#define CONFIG_TABLET_MODE
#define CONFIG_TABLET_MODE_SWITCH
#define CONFIG_GMR_TABLET_MODE
#define GPIO_TABLET_MODE_L GPIO_TABLET_MODE

/* USB */
#define CONFIG_USB_CHARGER
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_COMM_LOCKED
#define CONFIG_USB_PD_DISCHARGE_TCPC
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT TYPEC_RP_3A0
#define CONFIG_USB_PD_PORT_MAX_COUNT 2
#define CONFIG_USB_PD_VBUS_DETECT_GPIO
#define CONFIG_USB_PD_TCPC_LOW_POWER
/* TODO(b:121222079): Remove the config before FSI */
#define CONFIG_USB_PD_TCPC_RUNTIME_CONFIG
#define CONFIG_USB_PD_TCPM_MUX
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_TCPM_ANX7447
#define CONFIG_USB_PD_TCPM_ANX7447_OCM_ERASE_COMMAND
#define CONFIG_USB_PD_TCPM_PS8751
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_TCPMV1
#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_SS_MUX_DFP_ONLY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP

/* BC 1.2 charger */
#define CONFIG_BC12_DETECT_PI3USB9281
#define CONFIG_BC12_DETECT_PI3USB9281_CHIP_COUNT 2

/* Optional feature to configure npcx chip */
#define NPCX_UART_MODULE2 1 /* 1:GPIO64/65 as UART */
#define NPCX_JTAG_MODULE2 0 /* 0:GPIO21/17/16/20 as JTAG */
#define NPCX_TACH_SEL2 0 /* 0:GPIO40/73 as TACH */

/* I2C ports */
#define I2C_PORT_TCPC0 NPCX_I2C_PORT0_0
#define I2C_PORT_TCPC1 NPCX_I2C_PORT0_1
#define I2C_PORT_USB_CHARGER_1 NPCX_I2C_PORT0_1
#define I2C_PORT_USB_CHARGER_0 NPCX_I2C_PORT1
#define I2C_PORT_CHARGER NPCX_I2C_PORT1
#define I2C_PORT_EEPROM NPCX_I2C_PORT0_0
#define I2C_PORT_BATTERY NPCX_I2C_PORT1
#define I2C_PORT_PMIC NPCX_I2C_PORT2
#define I2C_PORT_MP2949 NPCX_I2C_PORT2
#define I2C_PORT_GYRO NPCX_I2C_PORT3
#define I2C_PORT_ACCEL I2C_PORT_GYRO
#define I2C_PORT_THERMAL I2C_PORT_PMIC

/* I2C addresses */
#define I2C_ADDR_BD99992_FLAGS 0x30
#define I2C_ADDR_MP2949_FLAGS 0x20
#define I2C_ADDR_EEPROM_FLAGS 0x50

/* Rename GPIOs */
#define GPIO_PCH_SLP_S0_L GPIO_SLP_S0_L
#define GPIO_PCH_SLP_SUS_L GPIO_SLP_SUS_L_PCH
#define GPIO_PG_EC_RSMRST_ODL GPIO_ROP_EC_RSMRST_L
#define GPIO_PMIC_DPWROK GPIO_ROP_DSW_PWROK
#define GPIO_POWER_BUTTON_L GPIO_PWR_BTN_ODL
#define GPIO_VOLUME_DOWN_L GPIO_VOLDN_BTN
#define GPIO_VOLUME_UP_L GPIO_VOLUP_BTN
#define GPIO_AC_PRESENT GPIO_ROP_EC_ACOK
#define GPIO_ENABLE_BACKLIGHT GPIO_BL_DISABLE_L
#define GPIO_CPU_PROCHOT GPIO_PCH_PROCHOT
#define GPIO_PCH_PWRBTN_L GPIO_PCH_PWR_BTN_L
#define GPIO_EC_PLATFORM_RST GPIO_PLATFORM_RST
#define GPIO_PMIC_SLP_SUS_L GPIO_SLP_SUS_L_PMIC
#define GPIO_USB_C0_5V_EN GPIO_EN_USB_C0_5V_OUT
#define GPIO_USB_C1_5V_EN GPIO_EN_USB_C1_5V_OUT

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum temp_sensor_id {
	TEMP_SENSOR_BATTERY, /* Smart Battery Temperature */
	TEMP_SENSOR_AMBIENT, /* BD99992GW SYSTHERM0 */
	TEMP_SENSOR_CHARGER, /* BD99992GW SYSTHERM1 */
	TEMP_SENSOR_DRAM, /* BD99992GW SYSTHERM2 */
	TEMP_SENSOR_EMMC, /* BD99992GW SYSTHERM3 */
	TEMP_SENSOR_COUNT
};

/*
 * Motion sensors:
 * When reading through IO memory is set up for sensors (LPC is used),
 * the first 2 entries must be accelerometers, then gyroscope.
 * For BMI160, accel, gyro and compass sensors must be next to each other.
 */
enum sensor_id {
	LID_ACCEL = 0,
	BASE_ACCEL,
	BASE_GYRO,
	SENSOR_COUNT,
};

enum adc_channel { ADC_VBUS, ADC_AMON_BMON, ADC_CH_COUNT };

enum pwm_channel {
	PWM_CH_KBLIGHT,
	/* Number of PWM channels */
	PWM_CH_COUNT
};

/* TODO(crosbug.com/p/61098): Verify the numbers below. */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY 30000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* delay to turn on/off vconn */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW 45000
#define PD_MAX_CURRENT_MA 3000
#define PD_MAX_VOLTAGE_MV 20000

/* Board specific handlers */
void board_reset_pd_mcu(void);
void board_set_tcpc_power_mode(int port, int mode);
void motion_interrupt(enum gpio_signal signal);

/* Sensors without hardware FIFO are in forced mode */
#define CONFIG_ACCEL_FORCE_MODE_MASK BIT(LID_ACCEL)

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */