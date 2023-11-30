/* -*- mode:c -*-
 *
 * Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Power State interrupts */
GPIO_INT(SLP_S4_L,		PIN(I, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S3_L,		PIN(H, 3), GPIO_INT_BOTH, baseboard_all_sys_pgood_interrupt)
GPIO_INT(SLP_S0_L,		PIN(E, 4), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_SUS_L,		PIN(G, 2), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(VCCIN_AUX_VID0,	PIN(D, 0), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(RSMRST_PWRGD_L,	PIN(E, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(CPU_C10_GATE_L,	PIN(G, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PG_DRAM_OD,		PIN(D, 3), GPIO_INT_BOTH, baseboard_all_sys_pgood_interrupt)
GPIO_INT(PG_PP1050_ST_OD,	PIN(L, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(VCCIN_AUX_VID1,	PIN(K, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PG_VCCIO_EXT_OD,	PIN(D, 7), GPIO_INT_BOTH, baseboard_all_sys_pgood_interrupt)
GPIO_INT(ESPI_RESET_L, 	 PIN(D, 2), GPIO_INT_FALLING | GPIO_SEL_1P8V, espi_reset_pin_asserted_interrupt)

GPIO_INT(H1_EC_PWR_BTN_ODL,	PIN(E, 2), GPIO_INT_BOTH | GPIO_PULL_UP, power_button_interrupt)
#ifdef CONFIG_LOW_POWER_IDLE
/* Used to wake up the EC from Deep Doze mode when writing to console */
GPIO_INT(UART1_RX,	 PIN(B, 0), GPIO_INT_BOTH, uart_deepsleep_interrupt)	/* UART_DBG_TX_EC_RX */
#endif

/* USB-C interrupts */
GPIO_INT(USB_C0_INT_ODL,	PIN(K, 0), GPIO_INT_FALLING | GPIO_PULL_UP, usb_c0_interrupt) /* charger */
GPIO_INT(USB_C0_CCSBU_OVP_ODL,	PIN(K, 6), GPIO_INT_FALLING | GPIO_PULL_UP, c0_ccsbu_ovp_interrupt) /* Fault protection */

/* Other interrupts */
GPIO_INT(EC_WP_OD,		PIN(A, 6), GPIO_INT_BOTH, switch_interrupt)
GPIO_INT(BJ_ADP_PRESENT_L,	PIN(J, 1), GPIO_INT_BOTH | GPIO_PULL_UP, adp_connect_interrupt)
GPIO_INT(PANEL_BACKLIGHT_EN,	PIN(J, 3), GPIO_INT_RISING, oz554_interrupt)
GPIO_INT(EC_RECOVERY_BTN_ODL,	PIN(F, 1), GPIO_INT_BOTH, button_interrupt)
GPIO_INT(H1_EC_RECOVERY_BTN_ODL,	PIN(KSI, 5), GPIO_INT_BOTH, button_interrupt)

/* Power sequence GPIOs */
GPIO(EC_AP_RTCRST,		PIN(K, 2), GPIO_OUT_LOW)
GPIO(EC_AP_PWR_BTN_ODL,		PIN(B, 6), GPIO_ODR_HIGH)
GPIO(EC_AP_DPWROK,		PIN(L, 7), GPIO_OUT_LOW)
GPIO(EC_AP_RSMRST_L,		PIN(H, 0), GPIO_OUT_LOW)
GPIO(EC_AP_WAKE_ODL,		PIN(D, 5), GPIO_ODR_HIGH)
GPIO(SYS_RST_ODL,		PIN(D, 1), GPIO_ODR_HIGH)
GPIO(EC_AP_SYS_PWROK,		PIN(F, 2), GPIO_OUT_LOW)
GPIO(PG_PP5000_U_OD,		PIN(E, 3), GPIO_INPUT)
GPIO(EN_PP3300_A,		PIN(C, 5), GPIO_OUT_LOW)
GPIO(EC_AP_PCH_PWROK_OD,	PIN(D, 6), GPIO_ODR_LOW)
GPIO(EN_PP5000_U,		PIN(K, 5), GPIO_OUT_LOW)
/* TODO(b:149775160) - Modify if needed if we ever use this signal. */
GPIO(EN_VCCST,			PIN(D, 4), GPIO_INPUT)
GPIO(EN_VCCIO_EXT,		PIN(B, 2), GPIO_OUT_LOW)
GPIO(EC_PROCHOT_ODL,		PIN(I, 1), GPIO_ODR_HIGH | GPIO_SEL_1P8V)
GPIO(EC_AP_VCCST_PWRGD_OD,	PIN(E, 5), GPIO_ODR_LOW)
GPIO(ALL_SYS_PWRGD,		PIN(B, 7), GPIO_OUT_LOW)
GPIO(EN_SLP_Z,			PIN(K, 3), GPIO_OUT_LOW)

/* Required for icelake chipset code, but implemented through other means for dedede */
UNIMPLEMENTED(AC_PRESENT)
UNIMPLEMENTED(PG_EC_DSW_PWROK)
UNIMPLEMENTED(PG_EC_ALL_SYS_PWRGD)

/* I2C pins - Alternate function below configures I2C module on these pins */
GPIO(EC_I2C_EEPROM_SCL,		PIN(B, 3), GPIO_INPUT)
GPIO(EC_I2C_EEPROM_SDA,		PIN(B, 4), GPIO_INPUT)
GPIO(EC_I2C_BATTERY_SCL,	PIN(C, 1), GPIO_INPUT)
GPIO(EC_I2C_BATTERY_SDA,	PIN(C, 2), GPIO_INPUT)
GPIO(EC_I2C_SENSOR_SCL,		PIN(F, 6), GPIO_INPUT | GPIO_SEL_1P8V)
GPIO(EC_I2C_SENSOR_SDA,		PIN(F, 7), GPIO_INPUT | GPIO_SEL_1P8V)
GPIO(EC_I2C_SUB_USB_C1_SCL,	PIN(E, 0), GPIO_INPUT)
GPIO(EC_I2C_SUB_USB_C1_SDA,	PIN(E, 7), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_SCL,		PIN(A, 4), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_SDA,		PIN(A, 5), GPIO_INPUT)

/* USB pins */
GPIO(EN_USB_A_5V,			PIN(L, 6), GPIO_OUT_LOW)
GPIO(EN_USB_C0_CC1_VCONN,	PIN(H, 4), GPIO_OUT_LOW)
GPIO(EN_USB_C0_CC2_VCONN,	PIN(H, 6), GPIO_OUT_LOW)
GPIO(EC_AP_USB_C0_HPD,		PIN(L, 4), GPIO_OUT_LOW)
GPIO(USB_C0_FRS,		PIN(C, 4), GPIO_OUT_LOW)
GPIO(HDMI_SEL_L,		PIN(C, 6), GPIO_OUT_HIGH)

/* MKBP event synchronization */
GPIO(EC_AP_MKBP_INT_L,	PIN(L, 5), GPIO_ODR_HIGH)

/* Misc */
GPIO(EC_ENTERING_RW,		PIN(G, 0), GPIO_OUT_LOW)
GPIO(EC_ENTERING_RW2,		PIN(C, 7), GPIO_OUT_LOW)
GPIO(CCD_MODE_ODL,		PIN(H, 5), GPIO_ODR_HIGH)
GPIO(ECH1_PACKET_MODE,		PIN(H, 1), GPIO_OUT_LOW)
GPIO(EN_PPVAR_BJ_ADP_L,		PIN(J, 0), GPIO_OUT_LOW)
GPIO(EN_VBUS_PWR,		PIN(F, 3), GPIO_OUT_HIGH)
GPIO(PANEL_ID0,			PIN(K, 4), GPIO_INPUT)
GPIO(PANEL_ID1,			PIN(J, 4), GPIO_INPUT)
GPIO(PANEL_ID2,			PIN(J, 5), GPIO_INPUT)
GPIO(PANEL_ID3,			PIN(J, 6), GPIO_INPUT)
GPIO(BL_OZ554_ID,		PIN(A, 7), GPIO_INPUT)
GPIO(EC_CRTL,			PIN(F, 0), GPIO_OUT_LOW)
GPIO(EC_EDID_WRITE_EN_L,	PIN(B, 5), GPIO_ODR_HIGH)

/* NC pins, enable internal pull-down to avoid floating state. */
GPIO(GPIOA0_NC, PIN(A, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOA2_NC, PIN(A, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOA3_NC, PIN(A, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOC0_NC, PIN(C, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOC3_NC, PIN(C, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOE6_NC, PIN(E, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOG3_NC, PIN(G, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOG4_NC, PIN(G, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOG5_NC, PIN(G, 5), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOG6_NC, PIN(G, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOG7_NC, PIN(G, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOI4_NC, PIN(I, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOI6_NC, PIN(I, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOI7_NC, PIN(I, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOK7_NC, PIN(K, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(GPIOM6_NC, PIN(M, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI0_NC, 	PIN(KSI, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI1_NC, 	PIN(KSI, 1), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI2_NC, 	PIN(KSI, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI3_NC, 	PIN(KSI, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI4_NC, 	PIN(KSI, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI6_NC, 	PIN(KSI, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSI7_NC, 	PIN(KSI, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO0_NC, 	PIN(KSO_L, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO1_NC, 	PIN(KSO_L, 1), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO3_NC, 	PIN(KSO_L, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO4_NC, 	PIN(KSO_L, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO5_NC, 	PIN(KSO_L, 5), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO6_NC, 	PIN(KSO_L, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO7_NC, 	PIN(KSO_L, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO8_NC, 	PIN(KSO_H, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO9_NC, 	PIN(KSO_H, 1), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO10_NC, 	PIN(KSO_H, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO11_NC, 	PIN(KSO_H, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO12_NC, 	PIN(KSO_H, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO13_NC, 	PIN(KSO_H, 5), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO14_NC, 	PIN(KSO_H, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(KSO15_NC, 	PIN(KSO_H, 7), GPIO_INPUT | GPIO_PULL_DOWN)

/* LED */
GPIO(PWR_LED_WHITE_L,	 PIN(A, 1), GPIO_OUT_HIGH)

/* Alternate functions GPIO definitions */
GPIO(EC_KSO_02_INV, PIN(KSO_L, 2), GPIO_OUT_HIGH)			/* KSO2 inverted */

/* UART */
ALTERNATE(PIN_MASK(B, BIT(0) | BIT(1)), 0, MODULE_UART, 0)		/* UART for debug */

/* I2C */
ALTERNATE(PIN_MASK(B, BIT(3) | BIT(4)), 0, MODULE_I2C, 0)		/* I2C0 */
ALTERNATE(PIN_MASK(C, BIT(1) | BIT(2)), 0, MODULE_I2C, 0)		/* I2C1 */
ALTERNATE(PIN_MASK(F, BIT(6) | BIT(7)), 0, MODULE_I2C, GPIO_SEL_1P8V)	/* I2C2 - 1.8V */
ALTERNATE(PIN_MASK(E, BIT(0) | BIT(7)), 0, MODULE_I2C, 0)		/* I2C4 */
ALTERNATE(PIN_MASK(A, BIT(4) | BIT(5)), 0, MODULE_I2C, 0)		/* I2C5 */

/* ADC */
ALTERNATE(PIN_MASK(L, BIT(0) | BIT(2) | BIT(3)), 0, MODULE_ADC, 0)	/* ADC13: EC_SUB_ANALOG, ADC15: TEMP_SENSOR_3, ADC16: TEMP_SENSOR_4 */
ALTERNATE(PIN_MASK(I, BIT(0) | BIT(2) | BIT(3)), 0, MODULE_ADC, 0)	/* ADC0: EC_VSNS_PP3300_A, ADC2: TEMP_SENSOR_1, ADC3: TEMP_SENSOR_2 */