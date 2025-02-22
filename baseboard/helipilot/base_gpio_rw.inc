/*
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SECTION_IS_RW
#error "This file should only be included in RW."
#endif

/* Interrupts */
/* FP_MCU_INT_L in the schematic. */
#if defined(CONFIG_FP_SENSOR_INT_ACTIVE_LOW)
GPIO_INT(FPS_INT,              PIN(B, 0), GPIO_INT_FALLING, fps_event)
#else
GPIO_INT(FPS_INT,              PIN(B, 0), GPIO_INT_RISING, fps_event)
#endif

/* Inputs */
GPIO(FP_SENSOR_SEL,            PIN(4, 4), GPIO_INPUT)

/* Outputs */
GPIO(FP_RST_ODL,               PIN(9, 6),  GPIO_OUT_HIGH)
/* SPI_MCU_CS_FP_L in the schematic. */
GPIO(FP_SPI_CS,                PIN(A, 6),  GPIO_OUT_HIGH)
GPIO(USER_PRES_L,              PIN(A, 5),  GPIO_ODR_HIGH)

/* SPIP - to fingerprint sensor */
/* SPIP_MOSI/SPIP_SCLK GPIOA3/A1 */
ALTERNATE(PIN_MASK(A, 0x0A), 1, MODULE_SPI, 0)
/* SPIP_MISO GPIO95    */
ALTERNATE(PIN_MASK(9, 0x20), 1, MODULE_SPI, 0)