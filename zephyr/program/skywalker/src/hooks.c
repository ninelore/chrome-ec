/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_charge.h"

#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_PLATFORM_EC_CHARGER_BQ25720
#include "driver/charger/bq257x0_regs.h"
#endif

static void skywalker_common_init(void)
{
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_ap_xhci_init_done));

#ifdef CONFIG_PLATFORM_EC_CHARGER_BQ25720
	/* b/353712228:
	 * Skywalker's HW sets external current limit (ILIM_HIZ) to 0.9A.
	 * FW need to disable EXTILIM on boot to allow larger current.
	 */
	i2c_update16(chg_chips[CHARGER_SOLO].i2c_port,
		     chg_chips[CHARGER_SOLO].i2c_addr_flags,
		     BQ25710_REG_CHARGE_OPTION_2,
		     1 << BQ257X0_CHARGE_OPTION_2_EN_EXTILIM_SHIFT, 0);
#endif
}
DECLARE_HOOK(HOOK_INIT, skywalker_common_init, HOOK_PRIO_PRE_DEFAULT);

/* USB-A */
void xhci_interrupt(enum gpio_signal signal)
{
#ifdef USB_PORT_ENABLE_COUNT
	enum usb_charge_mode mode = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(
					    gpio_ap_xhci_init_done_r)) ?
					    USB_CHARGE_MODE_ENABLED :
					    USB_CHARGE_MODE_DISABLED;

	for (int i = 0; i < USB_PORT_ENABLE_COUNT; i++) {
		usb_charge_set_mode(i, mode, USB_ALLOW_SUSPEND_CHARGE);
	}
#endif
}
