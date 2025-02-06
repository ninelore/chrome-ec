/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power_events.h"
#include "charge_manager.h"
#include "emul/tcpc/emul_tcpci.h"
#include "extpower.h"
#include "hooks.h"
#include "keyboard_protocol.h"
#include "nissa_hdmi.h"
#include "system.h"
#include "tcpm/tcpci.h"
#include "typec_control.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

extern const struct ec_response_keybd_config nereid_kb_legacy;

FAKE_VOID_FUNC(nissa_configure_hdmi_rails);
FAKE_VOID_FUNC(nissa_configure_hdmi_vcc);
FAKE_VALUE_FUNC(int, cbi_get_board_version, uint32_t *);

FAKE_VALUE_FUNC(int, ppc_is_sourcing_vbus, int);
FAKE_VALUE_FUNC(int, ppc_vbus_source_enable, int, int);
FAKE_VOID_FUNC(pd_set_vbus_discharge, int, int);
FAKE_VALUE_FUNC(int, ppc_vbus_sink_enable, int, int);
FAKE_VALUE_FUNC(int, ppc_is_vbus_present, int);
FAKE_VOID_FUNC(syv682x_interrupt, enum gpio_signal)

uint8_t board_get_charger_chip_count(void)
{
	return 2;
}

static void test_before(void *fixture)
{
	RESET_FAKE(nissa_configure_hdmi_rails);
	RESET_FAKE(nissa_configure_hdmi_vcc);
	RESET_FAKE(cbi_get_board_version);

	RESET_FAKE(ppc_is_sourcing_vbus);
	RESET_FAKE(ppc_vbus_source_enable);
	RESET_FAKE(pd_set_vbus_discharge);
	RESET_FAKE(ppc_vbus_sink_enable);
	RESET_FAKE(ppc_is_vbus_present);
	RESET_FAKE(syv682x_interrupt);
}

ZTEST_SUITE(dirks, NULL, NULL, test_before, NULL, NULL);

static int cbi_get_board_version_1(uint32_t *version)
{
	*version = 1;
	return 0;
}

static int cbi_get_board_version_2(uint32_t *version)
{
	*version = 2;
	return 0;
}

ZTEST(dirks, test_hdmi_power)
{
	/* Board version less than 2 configures both */
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_1;
	nissa_configure_hdmi_power_gpios();
	zassert_equal(nissa_configure_hdmi_vcc_fake.call_count, 1);
	zassert_equal(nissa_configure_hdmi_rails_fake.call_count, 1);

	/* Later versions only enable core rails */
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_2;
	nissa_configure_hdmi_power_gpios();
	zassert_equal(nissa_configure_hdmi_vcc_fake.call_count, 1);
	zassert_equal(nissa_configure_hdmi_rails_fake.call_count, 2);
}

ZTEST(dirks, test_extpower_is_present)
{
	/* AC is always present */
	zassert_true(extpower_is_present());
}

ZTEST(dirks, test_ppc_interrupt)
{
	const struct gpio_dt_spec *usb_c0_int =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c0_fault_l);

	hook_notify(HOOK_INIT);
	zassert_ok(gpio_emul_input_set(usb_c0_int->port, usb_c0_int->pin, 1),
		   NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(usb_c0_int->port, usb_c0_int->pin, 0),
		   NULL);
	zassert_equal(syv682x_interrupt_fake.call_count, 1);
}

ZTEST(dirks, test_board_vbus_source_enabled)
{
	/* check C0 */
	board_vbus_source_enabled(0);
	zassert_equal(ppc_is_sourcing_vbus_fake.call_count, 1);

	/* Errors cause early return */
	ppc_is_sourcing_vbus_fake.return_val = EC_ERROR_UNKNOWN;
	zassert_equal(board_vbus_source_enabled(0), EC_ERROR_UNKNOWN);

	/* Invalid port always return 0 */
	zassert_equal(board_vbus_source_enabled(1), 0);
}

ZTEST(dirks, test_pd_power_supply_reset)
{
	ppc_is_sourcing_vbus_fake.return_val = 1;

	/* Disables sourcing and discharges VBUS on active port */
	pd_power_supply_reset(0);
	zassert_equal(ppc_vbus_source_enable_fake.call_count, 1);
	zassert_equal(ppc_vbus_source_enable_fake.arg0_val, 0);
	zassert_equal(ppc_vbus_source_enable_fake.arg1_val, 0);
	zassert_equal(pd_set_vbus_discharge_fake.call_count, 1);
	zassert_equal(pd_set_vbus_discharge_fake.arg0_val, 0);
	zassert_equal(pd_set_vbus_discharge_fake.arg1_val, 1);

	/* Invalid port does nothing */
	pd_power_supply_reset(1);
	zassert_equal(ppc_is_sourcing_vbus_fake.call_count, 1);
}

ZTEST(dirks, test_pd_set_power_supply_ready)
{
	zassert_ok(pd_set_power_supply_ready(0));
	/* Disable charging */
	zassert_equal(ppc_vbus_sink_enable_fake.call_count, 1);
	zassert_equal(ppc_vbus_sink_enable_fake.arg0_val, 0);
	zassert_false(ppc_vbus_sink_enable_fake.arg1_val);
	/* Disabled VBUS discharge */
	zassert_equal(pd_set_vbus_discharge_fake.call_count, 1);
	zassert_equal(pd_set_vbus_discharge_fake.arg0_val, 0);
	zassert_false(pd_set_vbus_discharge_fake.arg1_val);
	/* Enabled sourcing */
	zassert_equal(ppc_vbus_source_enable_fake.call_count, 1);
	zassert_equal(ppc_vbus_source_enable_fake.arg0_val, 0);
	zassert_true(ppc_vbus_source_enable_fake.arg1_val);

	/* Errors cause early return */
	ppc_vbus_source_enable_fake.return_val = EC_ERROR_UNKNOWN;
	zassert_equal(pd_set_power_supply_ready(0), EC_ERROR_UNKNOWN);

	ppc_vbus_sink_enable_fake.return_val = EC_ERROR_UNKNOWN;
	zassert_equal(pd_set_power_supply_ready(0), EC_ERROR_UNKNOWN);

	zassert_equal(pd_set_power_supply_ready(31), EC_ERROR_INVAL);
}

ZTEST(dirks, test_pd_snk_is_vbus_provided)
{
	/* pd_snk_is_vbus_provided just delegates to ppc_is_vbus_present */
	pd_snk_is_vbus_provided(0);
	zassert_equal(ppc_is_vbus_present_fake.call_count, 1);

	/* Invalid port */
	zassert_equal(pd_snk_is_vbus_provided(1), 0);
}
