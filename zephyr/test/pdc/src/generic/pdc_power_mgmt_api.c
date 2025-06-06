/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "chipset.h"
#include "drivers/intel_altmode.h"
#include "drivers/ucsi_v3.h"
#include "emul/emul_pdc.h"
#include "hooks.h"
#include "test/util.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(pdc_power_mgmt_api, LOG_LEVEL_INF);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#define TYPEC_ONLY_SINK_DEBOUNCE_TIME_MS 1000

#if DT_NODE_EXISTS(ZEPHYR_USER_NODE)
#define PDC_TEST_TIMEOUT DT_PROP_OR(ZEPHYR_USER_NODE, test_timeout, 2000)
#else
#define PDC_TEST_TIMEOUT 2000
#endif
/* Time needed for chipset power to stabilize
 * (PDC_POWER_STATE_DEBOUNCE_S * 2) defined in pdc_power_mgmt.c
 */
#define PDC_POWER_STABLE_TIMEOUT (4000)
#define RTS5453P_NODE DT_NODELABEL(pdc_emul1)

#define USBC0_NODE DT_NODELABEL(usbc0)
#define USBC0_UNA_DRP_MODE \
	DT_STRING_TOKEN(DT_PROP(USBC0_NODE, policy), unattached_try)

static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);
#define TEST_PORT 0

bool pdc_power_mgmt_is_pd_attached(int port);

bool test_pdc_power_mgmt_is_snk_typec_attached_run(int port);
bool test_pdc_power_mgmt_is_src_typec_attached_run(int port);

typedef void (*set_connector_status_t)(
	const struct emul *target, union connector_status_t *connector_status);

/* Test-specific FFF fakes */
FAKE_VALUE_FUNC(int, system_jumped_late);
FAKE_VALUE_FUNC(int, chipset_in_state, int);
FAKE_VOID_FUNC(board_unattached_cb_stub, int);
FAKE_VOID_FUNC(board_dp_attention_cb_stub, int, uint32_t);

static enum chipset_state_mask fake_chipset_state = CHIPSET_STATE_ON;

static int custom_fake_chipset_in_state(int mask)
{
	LOG_DBG("MOCK: chipset_in_state");
	return !!(fake_chipset_state & mask);
}

static void reset_fakes(void)
{
	RESET_FAKE(system_jumped_late);
	RESET_FAKE(chipset_in_state);

	fake_chipset_state = CHIPSET_STATE_ON;
	chipset_in_state_fake.custom_fake = custom_fake_chipset_in_state;
}

static void clear_partner_pdos(const struct emul *e, enum pdo_type_t type)
{
	uint32_t clear_pdos[PDO_MAX_OBJECTS] = { 0 };

	emul_pdc_set_pdos(e, type, PDO_OFFSET_0, ARRAY_SIZE(clear_pdos),
			  PARTNER_PDO, clear_pdos);
}

static void *pdc_power_mgmt_setup(void)
{
	zassume(TEST_PORT < CONFIG_USB_PD_PORT_MAX_COUNT,
		"TEST_PORT is invalid");

	return NULL;
}

static void pdc_power_mgmt_before(void *fixture)
{
	emul_pdc_reset(emul);
	emul_pdc_set_response_delay(emul, 0);
	emul_pdc_disconnect(emul);
	emul_pdc_reset_feature_flags(emul);

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	reset_fakes();
}

static void pdc_power_mgmt_after(void *fixture)
{
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	reset_fakes();
}

ZTEST_SUITE(pdc_power_mgmt_api, NULL, pdc_power_mgmt_setup,
	    pdc_power_mgmt_before, pdc_power_mgmt_after, NULL);

ZTEST_USER(pdc_power_mgmt_api, test_get_usb_pd_port_count)
{
	zassert_equal(CONFIG_USB_PD_PORT_MAX_COUNT,
		      pdc_power_mgmt_get_usb_pd_port_count());
}

ZTEST_USER(pdc_power_mgmt_api, test_connector_reset)
{
	union connector_status_t connector_status = {};

	zassert_equal(-ERANGE,
		      pdc_power_mgmt_connector_reset(
			      CONFIG_USB_PD_PORT_MAX_COUNT, PD_HARD_RESET));

	zassert_ok(pdc_power_mgmt_connector_reset(TEST_PORT, PD_HARD_RESET));
	zassert_ok(pdc_power_mgmt_connector_reset(TEST_PORT, PD_DATA_RESET));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_ok(pdc_power_mgmt_connector_reset(TEST_PORT, PD_HARD_RESET));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_ok(pdc_power_mgmt_connector_reset(TEST_PORT, PD_DATA_RESET));
}

ZTEST_USER(pdc_power_mgmt_api, test_is_connected)
{
	union connector_status_t connector_status = {};
	bool frs_enabled;

	/* Verify that the emulator tracks whether FRS enable/disable
	 * has been configured.
	 */
	zassert_ok(emul_pdc_reset(emul));
	zassert_equal(emul_pdc_get_frs(emul, &frs_enabled), -EIO);

	/* Invalid port number */
	zassert_false(pd_is_connected(CONFIG_USB_PD_PORT_MAX_COUNT));
	zassert_equal(pd_get_task_state(CONFIG_USB_PD_PORT_MAX_COUNT),
		      PDC_INVALID);

	zassert_false(pd_is_connected(TEST_PORT));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	emul_pdc_disconnect(emul);
	zassert_true(
		TEST_WAIT_FOR(!pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	if (!IS_ENABLED(CONFIG_PLATFORM_EC_USB_PD_FRS)) {
		/* FRS should be disabled after connecting a partner source. */
		zassert_ok(emul_pdc_get_frs(emul, &frs_enabled));
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_comm_is_enabled)
{
	union connector_status_t connector_status = {};

	zassert_false(pd_comm_is_enabled(CONFIG_USB_PD_PORT_MAX_COUNT));

	zassert_false(pd_comm_is_enabled(TEST_PORT));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_comm_is_enabled(TEST_PORT), PDC_TEST_TIMEOUT));

	emul_pdc_disconnect(emul);
	zassert_true(TEST_WAIT_FOR(!pd_comm_is_enabled(TEST_PORT),
				   PDC_TEST_TIMEOUT));

	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_comm_is_enabled(TEST_PORT), PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_get_polarity)
{
	union connector_status_t connector_status = {};

	zassert_false(
		pdc_power_mgmt_is_connected(CONFIG_USB_PD_PORT_MAX_COUNT));

	connector_status.orientation = 1;
	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(POLARITY_CC2 == pd_get_polarity(TEST_PORT),
				   PDC_TEST_TIMEOUT));

	connector_status.orientation = 0;
	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(POLARITY_CC1 == pd_get_polarity(TEST_PORT),
				   PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_get_data_role)
{
	union connector_status_t connector_status = {};

	zassert_equal(PD_ROLE_DISCONNECTED,
		      pd_get_data_role(CONFIG_USB_PD_PORT_MAX_COUNT));

	connector_status.conn_partner_type = DFP_ATTACHED;
	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(PD_ROLE_UFP == pd_get_data_role(TEST_PORT),
				   PDC_TEST_TIMEOUT));

	connector_status.conn_partner_type = UFP_ATTACHED;
	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(PD_ROLE_DFP == pd_get_data_role(TEST_PORT),
				   PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_get_power_role)
{
	union connector_status_t connector_status = {};
	zassert_equal(PD_ROLE_SINK,
		      pd_get_power_role(CONFIG_USB_PD_PORT_MAX_COUNT));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(PD_ROLE_SOURCE == pd_get_power_role(TEST_PORT),
			      PDC_TEST_TIMEOUT));

	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(PD_ROLE_SINK == pd_get_power_role(TEST_PORT),
				   PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_get_task_cc_state)
{
	int i;
	struct {
		enum conn_partner_type_t in;
		enum pd_cc_states out;
	} test[] = {
		{ .in = DFP_ATTACHED, .out = PD_CC_DFP_ATTACHED },
		{ .in = UFP_ATTACHED, .out = PD_CC_UFP_ATTACHED },
		{ .in = POWERED_CABLE_NO_UFP_ATTACHED, .out = PD_CC_NONE },
		{ .in = POWERED_CABLE_UFP_ATTACHED, .out = PD_CC_UFP_ATTACHED },
		{ .in = DEBUG_ACCESSORY_ATTACHED, .out = PD_CC_UFP_DEBUG_ACC },
		{ .in = AUDIO_ADAPTER_ACCESSORY_ATTACHED,
		  .out = PD_CC_UFP_AUDIO_ACC },
	};

	zassert_equal(PD_CC_NONE,
		      pd_get_task_cc_state(CONFIG_USB_PD_PORT_MAX_COUNT));

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		union connector_status_t connector_status = {};

		connector_status.conn_partner_type = test[i].in;
		emul_pdc_configure_src(emul, &connector_status);
		emul_pdc_connect_partner(emul, &connector_status);
		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

		zassert_equal(test[i].out, pd_get_task_cc_state(TEST_PORT),
			      "[%d] expecting=%d, returned=%d", i, test[i].out,
			      pd_get_task_cc_state(TEST_PORT));

		emul_pdc_disconnect(emul);
		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_capable)
{
	union connector_status_t connector_status = {};
	zassert_equal(false, pd_capable(CONFIG_USB_PD_PORT_MAX_COUNT));

	emul_pdc_disconnect(emul);
	zassert_false(TEST_WAIT_FOR(pd_capable(TEST_PORT), PDC_TEST_TIMEOUT));

	connector_status.power_operation_mode = USB_DEFAULT_OPERATION;
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_false(TEST_WAIT_FOR(pd_capable(TEST_PORT), PDC_TEST_TIMEOUT));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(pd_capable(TEST_PORT), PDC_TEST_TIMEOUT));
}

K_THREAD_STACK_DEFINE(test_toggle_stack, 256);
static bool test_toggle_done;
static union connector_status_t test_toggle_status;

static void test_thread_toggle(void *a, void *b, void *c)
{
	union conn_status_change_bits_t status_change_bits;

	memset(&status_change_bits, 0, sizeof(union conn_status_change_bits_t));
	test_toggle_status.raw_conn_status_change_bits =
		status_change_bits.raw_value;

	LOG_INF("Emul PDC disconnect partner");
	emul_pdc_connect_partner(emul, &test_toggle_status);

	while (!test_toggle_done) {
		k_msleep(50);

		/* Toggle attention on each pass to keep the PDC busy */
		status_change_bits.attention ^= status_change_bits.attention;
		test_toggle_status.raw_conn_status_change_bits =
			status_change_bits.raw_value;

		LOG_INF("Emul PDC toggle attention");
		emul_pdc_connect_partner(emul, &test_toggle_status);
	}
}

static k_tid_t start_toggle_thread(struct k_thread *thread,
				   union connector_status_t *connector_status)
{
	memcpy(&test_toggle_status, connector_status,
	       sizeof(union connector_status_t));
	test_toggle_done = false;

	return k_thread_create(thread, test_toggle_stack,
			       K_THREAD_STACK_SIZEOF(test_toggle_stack),
			       test_thread_toggle, NULL, NULL, NULL, -1, 0,
			       K_NO_WAIT);
}

static int join_toggle_thread(k_tid_t thread)
{
	test_toggle_done = true;
	return k_thread_join(thread, K_MSEC(100));
}

static void run_toggle_test(union connector_status_t *connector_status)
{
	struct pdc_info_t pdc_info;
	struct k_thread test_thread_data;
	int ret;

	/* Slow down driver response to give -EBUSY response */
	emul_pdc_set_response_delay(emul, 20);

	LOG_INF("Emul PDC disconnect partner");
	emul_pdc_disconnect(emul);
	zassert_false(TEST_WAIT_FOR(pd_capable(TEST_PORT), PDC_TEST_TIMEOUT));

	/*
	 * Create a new thread to toggle keep the PDC busy with interrupts.
	 * Thread priority set to cooperative to ensure it preempts the PDC
	 * subsystem.
	 */
	memset(connector_status, 0, sizeof(union connector_status_t));
	k_tid_t test_thread =
		start_toggle_thread(&test_thread_data, connector_status);

	/* Allow the test thread some cycles to run. */
	k_msleep(100);

	LOG_INF("Sending GET INFO");
	ret = pdc_power_mgmt_get_info(TEST_PORT, &pdc_info, true);
	zassert_equal(-EBUSY, ret,
		      "pdc_power_mgmt_get_info() returned %d (expected %d)",
		      ret, -EBUSY);

	/* Allow the test thread to exit. */
	zassert_ok(join_toggle_thread(test_thread));

	/* All the PDC subsystem to settle. */
	k_msleep(250);

	/* Public API command should now succeed. */
	ret = pdc_power_mgmt_get_info(TEST_PORT, &pdc_info, true);
	zassert_false(ret, "pdc_power_mgmt_get_info() failed (%d)", ret);
}

/* Verify that public commands complete when a non PD partner is connected */
ZTEST_USER(pdc_power_mgmt_api, test_non_pd_snk_public_cmd)
{
	union connector_status_t connector_status;

	memset(&connector_status, 0, sizeof(union connector_status_t));
	connector_status.power_operation_mode = USB_TC_CURRENT_5A;
	connector_status.power_direction = 0;

	run_toggle_test(&connector_status);
}

ZTEST_USER(pdc_power_mgmt_api, test_non_pd_src_public_cmd)
{
	union connector_status_t connector_status;

	memset(&connector_status, 0, sizeof(union connector_status_t));
	connector_status.power_operation_mode = USB_TC_CURRENT_5A;
	connector_status.power_direction = 1;

	run_toggle_test(&connector_status);
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_snk_public_cmd)
{
	union connector_status_t connector_status;

	memset(&connector_status, 0, sizeof(union connector_status_t));
	connector_status.power_operation_mode = PD_OPERATION;
	connector_status.power_direction = 0;

	run_toggle_test(&connector_status);
}

ZTEST_USER(pdc_power_mgmt_api, test_pd_src_public_cmd)
{
	union connector_status_t connector_status;

	memset(&connector_status, 0, sizeof(union connector_status_t));
	connector_status.power_operation_mode = PD_OPERATION;
	connector_status.power_direction = 1;

	run_toggle_test(&connector_status);
}

ZTEST_USER(pdc_power_mgmt_api, test_unattached_public_cmd)
{
	union connector_status_t connector_status;

	memset(&connector_status, 0, sizeof(union connector_status_t));

	run_toggle_test(&connector_status);
}

static void pdc_power_mgmt_connectionless_before(void *fixture)
{
	pdc_power_mgmt_before(fixture);
	zassert_false(TEST_WAIT_FOR(pd_capable(TEST_PORT), PDC_TEST_TIMEOUT));
}

ZTEST_SUITE(pdc_power_mgmt_api_connectionless, NULL, pdc_power_mgmt_setup,
	    pdc_power_mgmt_connectionless_before, pdc_power_mgmt_after, NULL);

ZTEST_USER(pdc_power_mgmt_api_connectionless, test_reset)
{
	/* These commands are expected to succeed without a connection. */
	LOG_INF("Sending PDC RESET");
	zassert_ok(pdc_power_mgmt_reset(TEST_PORT));

	emul_pdc_disconnect(emul);
	zassert_false(TEST_WAIT_FOR(pd_capable(TEST_PORT), PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api_connectionless, test_get_info)
{
	struct pdc_info_t pdc_info;

	LOG_INF("Sending GET INFO");
	zassert_ok(pdc_power_mgmt_get_info(TEST_PORT, &pdc_info, true));
}

ZTEST_USER(pdc_power_mgmt_api_connectionless, test_get_pch_data_status)
{
	union data_status_reg status;

	LOG_INF("Sending GET PCH DATA_STATUS");
	zassert_ok(pdc_power_mgmt_get_pch_data_status(TEST_PORT,
						      status.raw_value));
}

ZTEST_USER(pdc_power_mgmt_api_connectionless, test_set_trysrc)
{
	LOG_INF("Sending SET DRP");
	zassert_ok(pdc_power_mgmt_set_trysrc(TEST_PORT, true));
}

ZTEST_USER(pdc_power_mgmt_api_connectionless, test_get_drp)
{
	enum drp_mode_t drp_mode = DRP_NORMAL;

	LOG_INF("Sending GET DRP");
	zassert_ok(pdc_power_mgmt_get_drp_mode(TEST_PORT, &drp_mode));
}

ZTEST_USER(pdc_power_mgmt_api_connectionless, test_get_lpm_ppm_info)
{
	struct lpm_ppm_info_t lpm_ppm_info;
	int rv;

	LOG_INF("Sending GET LPM PPM INFO");
	rv = pdc_power_mgmt_get_lpm_ppm_info(TEST_PORT, &lpm_ppm_info);
	if (rv == -ENOSYS) {
		ztest_test_skip();
	}
	zassert_ok(rv, "rv=%d", rv);
}

ZTEST_USER(pdc_power_mgmt_api, test_get_partner_usb_comm_capable)
{
	int i;
	union connector_status_t connector_status = { 0 };
	struct {
		union connector_capability_t ccap;
		bool expected;
	} test[] = {
		{ .ccap = { .raw_value = 0 }, .expected = false },
		{ .ccap = { .op_mode_usb2 = 1 }, .expected = true },
		{ .ccap = { .op_mode_usb3 = 1 }, .expected = true },
		{ .ccap = { .ext_op_mode_usb4_gen2 = 1 }, .expected = true },
		{ .ccap = { .ext_op_mode_usb4_gen3 = 1 }, .expected = true },
		{ .ccap = { .op_mode_debug_acc = 1 }, .expected = false },
		{ .ccap = { .op_mode_analog_audio = 1 }, .expected = false },
		{ .ccap = { .op_mode_rp_only = 1 }, .expected = false },
		{ .ccap = { .op_mode_rd_only = 1 }, .expected = false },
	};

	zassert_false(
		pd_get_partner_usb_comm_capable(CONFIG_USB_PD_PORT_MAX_COUNT));

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		emul_pdc_set_connector_capability(emul, &test[i].ccap);
		emul_pdc_configure_src(emul, &connector_status);
		emul_pdc_connect_partner(emul, &connector_status);
		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
		zassert_true(
			test[i].expected ==
				pd_get_partner_usb_comm_capable(TEST_PORT), );

		emul_pdc_disconnect(emul);
		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
		zassert_true(!pd_is_connected(TEST_PORT));
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_get_partner_data_swap_capable)
{
	int i;
	union connector_status_t connector_status = {};
	struct {
		enum pd_power_role power_role;
		uint32_t pdo;
		bool expected;
	} test[] = {
		{ .power_role = PD_ROLE_SINK, .pdo = 0, .expected = false },
		{ .power_role = PD_ROLE_SOURCE, .pdo = 0, .expected = false },
		{ .power_role = PD_ROLE_SINK,
		  .pdo = PDO_FIXED(5000, 3000,
				   PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP),
		  .expected = true },
		{ .power_role = PD_ROLE_SOURCE,
		  .pdo = PDO_FIXED(5000, 3000,
				   PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP),
		  .expected = true },
		{ .power_role = PD_ROLE_SINK,
		  .pdo = PDO_FIXED(5000, 3000, PDO_FIXED_UNCONSTRAINED),
		  .expected = false },
		{ .power_role = PD_ROLE_SOURCE,
		  .pdo = PDO_FIXED(5000, 3000, PDO_FIXED_UNCONSTRAINED),
		  .expected = false },
		{ .power_role = PD_ROLE_SINK,
		  .pdo = PDO_VAR(5000, 3000, 15000),
		  .expected = false },
		{ .power_role = PD_ROLE_SOURCE,
		  .pdo = PDO_VAR(5000, 3000, 15000),
		  .expected = false },
	};

	zassert_false(
		pd_get_partner_data_swap_capable(CONFIG_USB_PD_PORT_MAX_COUNT));

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		if (test[i].power_role == PD_ROLE_SINK)
			emul_pdc_configure_snk(emul, &connector_status);
		else
			emul_pdc_configure_src(emul, &connector_status);
		clear_partner_pdos(emul, (test[i].power_role == PD_ROLE_SINK ?
						  SOURCE_PDO :
						  SINK_PDO));
		emul_pdc_set_pdos(emul,
				  (test[i].power_role == PD_ROLE_SINK ?
					   SOURCE_PDO :
					   SINK_PDO),
				  PDO_OFFSET_0, 1, PARTNER_PDO, &test[i].pdo);
		emul_pdc_connect_partner(emul, &connector_status);

		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
		LOG_INF("[%d] connection settled", i);

		zassert_equal(test[i].expected,
			      pd_get_partner_data_swap_capable(TEST_PORT),
			      "[%d] expected=%d, pdo=0x%X", i, test[i].expected,
			      test[i].pdo);

		emul_pdc_disconnect(emul);
		zassert_true(TEST_WAIT_FOR(!pd_is_connected(TEST_PORT),
					   PDC_TEST_TIMEOUT));
		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
		LOG_INF("[%d] disconnection settled", i);
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_get_info)
{
#if DT_NODE_EXISTS(ZEPHYR_USER_NODE)
	static const struct pdc_info_t in1 = {
		.fw_version = 0x001a2b3c,
		.pd_version = DT_PROP(ZEPHYR_USER_NODE, pd_version),
		.pd_revision = DT_PROP(ZEPHYR_USER_NODE, pd_revision),
		.vid = 0x1234,
		.pid = 0x5678,
		.project_name = DT_PROP(ZEPHYR_USER_NODE, project_name),
	};
	static const struct pdc_info_t in2 = {
		.fw_version = 0x002a3b4c,
		.pd_version = DT_PROP(ZEPHYR_USER_NODE, pd_version),
		.pd_revision = DT_PROP(ZEPHYR_USER_NODE, pd_revision),
		.vid = 0x9abc,
		.pid = 0xdef0,
		.project_name = DT_PROP(ZEPHYR_USER_NODE, project_name),
	};
#else
	struct pdc_info_t in1 = {
		.fw_version = 0x001a2b3c,
		.pd_version = 0xabcd,
		.pd_revision = 0x1234,
		.vid = 0x1234,
		.pid = 0x5678,
		.project_name = "ProjectName",
	};
	struct pdc_info_t in2 = {
		.fw_version = 0x002a3b4c,
		.pd_version = 0xef01,
		.pd_revision = 0x5678,
		.vid = 0x9abc,
		.pid = 0xdef0,
		.project_name = "MyProj",
	};
#endif
	struct pdc_info_t out = { 0 };
	union connector_status_t connector_status = {};

	zassert_equal(-ERANGE,
		      pdc_power_mgmt_get_info(CONFIG_USB_PD_PORT_MAX_COUNT,
					      &out, true));
	zassert_equal(-EINVAL, pdc_power_mgmt_get_info(TEST_PORT, NULL, true));

	emul_pdc_set_info(emul, &in1);
	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_ok(pdc_power_mgmt_get_info(TEST_PORT, &out, true));
	zassert_equal(in1.fw_version, out.fw_version, "in=0x%X, out=0x%X",
		      in1.fw_version, out.fw_version);
	zassert_equal(in1.pd_version, out.pd_version);
	zassert_equal(in1.pd_revision, out.pd_revision);
	zassert_equal(in1.vid, out.vid, "in=0x%X, out=0x%X", in1.vid, out.vid);
	zassert_equal(in1.pid, out.pid, "in=0x%X, out=0x%X", in1.pid, out.pid);
	zassert_mem_equal(in1.project_name, out.project_name,
			  sizeof(in1.project_name));

	/* Repeat but non-live. The cached info should match the original
	 * read instead of `in2`.
	 */
	emul_pdc_set_info(emul, &in2);
	zassert_ok(pdc_power_mgmt_get_info(TEST_PORT, &out, false));
	zassert_equal(in1.fw_version, out.fw_version, "in=0x%X, out=0x%X",
		      in1.fw_version, out.fw_version);
	zassert_equal(in1.pd_version, out.pd_version);
	zassert_equal(in1.pd_revision, out.pd_revision);
	zassert_equal(in1.vid, out.vid, "in=0x%X, out=0x%X", in1.vid, out.vid);
	zassert_equal(in1.pid, out.pid, "in=0x%X, out=0x%X", in1.pid, out.pid);
	zassert_mem_equal(in1.project_name, out.project_name,
			  sizeof(in1.project_name));

	/* Live read again. This time we should get `in2`. */
	zassert_ok(pdc_power_mgmt_get_info(TEST_PORT, &out, true));
	zassert_equal(in2.fw_version, out.fw_version, "in=0x%X, out=0x%X",
		      in2.fw_version, out.fw_version);
	zassert_equal(in2.pd_version, out.pd_version);
	zassert_equal(in2.pd_revision, out.pd_revision);
	zassert_equal(in2.vid, out.vid, "in=0x%X, out=0x%X", in2.vid, out.vid);
	zassert_equal(in2.pid, out.pid, "in=0x%X, out=0x%X", in2.pid, out.pid);
	zassert_mem_equal(in2.project_name, out.project_name,
			  sizeof(in2.project_name));

	emul_pdc_disconnect(emul);
	zassert_true(
		TEST_WAIT_FOR(!pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_lpm_ppm_info)
{
	struct lpm_ppm_info_t out = { 0 };
	struct lpm_ppm_info_t in = {
		.vid = 0x1234,
		.pid = 0x5678,
		.xid = 0xa1b2c3d4,
		.fw_ver = 123,
		.fw_ver_sub = 456,
		.hw_ver = 0xa5b6c7de,
	};

	/* Bad params */
	zassert_equal(-ERANGE, pdc_power_mgmt_get_lpm_ppm_info(
				       CONFIG_USB_PD_PORT_MAX_COUNT, &out));
	zassert_equal(-EINVAL,
		      pdc_power_mgmt_get_lpm_ppm_info(TEST_PORT, NULL));

	if (pdc_power_mgmt_get_lpm_ppm_info(TEST_PORT, &out) == -ENOSYS) {
		ztest_test_skip();
	}

	/* Successful */
	emul_pdc_set_lpm_ppm_info(emul, &in);
	zassert_equal(EC_SUCCESS,
		      pdc_power_mgmt_get_lpm_ppm_info(TEST_PORT, &out));

	zassert_equal(in.vid, out.vid, "Got $%04x, expected $%04x", out.vid,
		      in.vid);
	zassert_equal(in.pid, out.pid, "Got $%04x, expected $%04x", out.pid,
		      in.pid);
	zassert_equal(in.xid, out.xid, "Got $%08x, expected $%08x", out.xid,
		      in.xid);
	zassert_equal(in.fw_ver, out.fw_ver, "Got %u, expected %u", out.fw_ver,
		      in.fw_ver);
	zassert_equal(in.fw_ver_sub, out.fw_ver_sub, "Got %u, expected %u",
		      out.fw_ver_sub, in.fw_ver_sub);
	zassert_equal(in.hw_ver, out.hw_ver, "Got %08x, expected $%08x",
		      out.hw_ver, in.hw_ver);
}

ZTEST_USER(pdc_power_mgmt_api, test_request_power_swap)
{
	int i;
	struct setup_t {
		enum conn_partner_type_t conn_partner_type;
		set_connector_status_t configure;
	};
	struct expect_t {
		union pdr_t pdr;
	};
	struct {
		struct setup_t s;
		struct expect_t e;
	} test[] = {
		{ .s = { .conn_partner_type = DFP_ATTACHED,
			 .configure = emul_pdc_configure_snk },
		  .e = { .pdr = { .swap_to_src = 1,
				  .swap_to_snk = 0,
				  .accept_pr_swap = 1 } } },
		{ .s = { .conn_partner_type = DFP_ATTACHED,
			 .configure = emul_pdc_configure_src },
		  .e = { .pdr = { .swap_to_src = 0,
				  .swap_to_snk = 1,
				  .accept_pr_swap = 1 } } },
		{ .s = { .conn_partner_type = UFP_ATTACHED,
			 .configure = emul_pdc_configure_snk },
		  .e = { .pdr = { .swap_to_src = 1,
				  .swap_to_snk = 0,
				  .accept_pr_swap = 1 } } },
		{ .s = { .conn_partner_type = UFP_ATTACHED,
			 .configure = emul_pdc_configure_src },
		  .e = { .pdr = { .swap_to_src = 0,
				  .swap_to_snk = 1,
				  .accept_pr_swap = 1 } } },
	};

	union connector_status_t connector_status = { 0 };
	union pdr_t pdr;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;

	/* Set the pdr.allow_pr_swap bit to 1 by enabling the TOGGLE_ON DRP
	 * mode. This provides a consistent value for the pdr.accept_pr_swap
	 * assertions below. */
	pdc_power_mgmt_set_dual_role(TEST_PORT, PD_DRP_TOGGLE_ON);
	TEST_WORKING_DELAY(PDC_TEST_TIMEOUT);

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		memset(&connector_status, 0, sizeof(connector_status));
		connector_status.conn_partner_type =
			test[i].s.conn_partner_type;

		test[i].s.configure(emul, &connector_status);
		emul_pdc_connect_partner(emul, &connector_status);
		zassert_true(
			TEST_WAIT_FOR(pdc_power_mgmt_is_pd_attached(TEST_PORT),
				      PDC_TEST_TIMEOUT),
			"PD not connected in time (i=%d)", i);

		pd_request_power_swap(TEST_PORT);

		start = k_cycle_get_32();
		while (k_cycle_get_32() - start < timeout) {
			k_msleep(TEST_WAIT_FOR_INTERVAL_MS);

			emul_pdc_get_pdr(emul, &pdr);

			if (pdr.swap_to_src != test[i].e.pdr.swap_to_src)
				continue;

			if (pdr.swap_to_snk != test[i].e.pdr.swap_to_snk)
				continue;

			if (pdr.accept_pr_swap != test[i].e.pdr.accept_pr_swap)
				continue;

			break;
		}

		zassert_equal(pdr.swap_to_src, test[i].e.pdr.swap_to_src,
			      "Got %u, expected %u (i=%d)", pdr.swap_to_src,
			      test[i].e.pdr.swap_to_src, i);
		zassert_equal(pdr.swap_to_snk, test[i].e.pdr.swap_to_snk,
			      "Got %u, expected %u (i=%d)", pdr.swap_to_snk,
			      test[i].e.pdr.swap_to_snk, i);
		zassert_equal(pdr.accept_pr_swap, test[i].e.pdr.accept_pr_swap,
			      "Got %u, expected %u (i=%d)", pdr.accept_pr_swap,
			      test[i].e.pdr.accept_pr_swap, i);

		emul_pdc_disconnect(emul);
		zassert_true(TEST_WAIT_FOR(!pd_is_connected(TEST_PORT),
					   PDC_TEST_TIMEOUT),
			     "PD not disconnected in time (i=%d)", i);
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_request_data_swap)
{
	int i;
	struct setup_t {
		enum conn_partner_type_t conn_partner_type;
		set_connector_status_t configure;
	};
	struct expect_t {
		union uor_t uor;
	};
	struct {
		struct setup_t s;
		struct expect_t e;
	} test[] = {
		{ .s = { .conn_partner_type = DFP_ATTACHED,
			 .configure = emul_pdc_configure_src },
		  .e = { .uor = { .swap_to_dfp = 1,
				  .swap_to_ufp = 0,
				  .accept_dr_swap = 0 } } },
		{ .s = { .conn_partner_type = DFP_ATTACHED,
			 .configure = emul_pdc_configure_snk },
		  .e = { .uor = { .swap_to_dfp = 1,
				  .swap_to_ufp = 0,
				  .accept_dr_swap = 0 } } },
		{ .s = { .conn_partner_type = UFP_ATTACHED,
			 .configure = emul_pdc_configure_src },
		  .e = { .uor = { .swap_to_dfp = 0,
				  .swap_to_ufp = 1,
				  .accept_dr_swap = 1 } } },
		{ .s = { .conn_partner_type = UFP_ATTACHED,
			 .configure = emul_pdc_configure_snk },
		  .e = { .uor = { .swap_to_dfp = 0,
				  .swap_to_ufp = 1,
				  .accept_dr_swap = 1 } } },
	};

	union connector_status_t connector_status = { 0 };
	union uor_t uor;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		memset(&connector_status, 0, sizeof(connector_status));
		connector_status.conn_partner_type =
			test[i].s.conn_partner_type;

		test[i].s.configure(emul, &connector_status);
		emul_pdc_connect_partner(emul, &connector_status);
		zassert_true(
			TEST_WAIT_FOR(pdc_power_mgmt_is_pd_attached(TEST_PORT),
				      PDC_TEST_TIMEOUT));

		pd_request_data_swap(TEST_PORT);
		start = k_cycle_get_32();
		while (k_cycle_get_32() - start < timeout) {
			k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
			emul_pdc_get_uor(emul, &uor);

			if (uor.swap_to_ufp != test[i].e.uor.swap_to_ufp)
				continue;

			if (uor.swap_to_dfp != test[i].e.uor.swap_to_dfp)
				continue;

			if (uor.accept_dr_swap != test[i].e.uor.accept_dr_swap)
				continue;

			break;
		}

		emul_pdc_get_uor(emul, &uor);
		zassert_equal(uor.swap_to_ufp, test[i].e.uor.swap_to_ufp);
		zassert_equal(uor.swap_to_dfp, test[i].e.uor.swap_to_dfp);
		zassert_equal(uor.accept_dr_swap, test[i].e.uor.accept_dr_swap);

		emul_pdc_disconnect(emul);
		zassert_true(TEST_WAIT_FOR(!pd_is_connected(TEST_PORT),
					   PDC_TEST_TIMEOUT));
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_get_partner_unconstr_power_src)
{
	union connector_status_t connector_status = {};
	const uint32_t pdos_up[] = {
		PDO_FIXED(5000, 3000,
			  PDO_FIXED_DUAL_ROLE |
				  PDO_FIXED_GET_UNCONSTRAINED_PWR),
	};

	zassert_false(
		pd_get_partner_unconstr_power(CONFIG_USB_PD_PORT_MAX_COUNT));

	/* If the port is not in Attached.SNK, unconstrained power is considered
	 * to be false.
	 */
	emul_pdc_configure_src(emul, &connector_status);
	clear_partner_pdos(emul, SOURCE_PDO);
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, PARTNER_PDO,
			  pdos_up);
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_false(TEST_WAIT_FOR(pd_get_partner_unconstr_power(TEST_PORT),
				    PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_partner_unconstr_power_snk_no_up)
{
	union connector_status_t connector_status = {};
	const uint32_t pdos_no_up[] = {
		PDO_FIXED(5000, 3000, PDO_FIXED_DUAL_ROLE),
	};

	emul_pdc_configure_snk(emul, &connector_status);
	clear_partner_pdos(emul, SOURCE_PDO);
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, PARTNER_PDO,
			  pdos_no_up);
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_false(TEST_WAIT_FOR(pd_get_partner_unconstr_power(TEST_PORT),
				    PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_partner_unconstr_power_snk_up)
{
	union connector_status_t connector_status = { 0 };
	const uint32_t pdos_up[] = {
		PDO_FIXED(5000, 3000,
			  PDO_FIXED_DUAL_ROLE |
				  PDO_FIXED_GET_UNCONSTRAINED_PWR),
	};

	/* If the port is in Attached.SNK, unconstrained power should be the
	 * partner's advertised capability.
	 */
	emul_pdc_configure_snk(emul, &connector_status);
	clear_partner_pdos(emul, SOURCE_PDO);
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, PARTNER_PDO,
			  pdos_up);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(TEST_WAIT_FOR(pd_get_partner_unconstr_power(TEST_PORT),
				   PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_vbus_voltage)
{
/* Keep in line with |pdc_power_mgmt_api.c|. */
#define VBUS_READ_CACHE_MS 500

	union connector_status_t connector_status = {};
	union conn_status_change_bits_t change_bits = { 0 };
	uint32_t mv_units = 50;
	const uint32_t expected_voltage_mv = 5000;
	uint32_t next_expected_voltage_mv = 6000;
	uint16_t out;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;

	zassert_equal(0, pdc_power_mgmt_get_vbus_voltage(TEST_PORT));

	connector_status.voltage_scale = 10; /* 50 mv units*/
	connector_status.voltage_reading = expected_voltage_mv / mv_units;
	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);

	start = k_cycle_get_32();
	while (k_cycle_get_32() - start < timeout) {
		k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
		out = pdc_power_mgmt_get_vbus_voltage(TEST_PORT);
		if (out != expected_voltage_mv)
			continue;

		break;
	}

	zassert_equal(expected_voltage_mv, out, "expected=%d, out=%d",
		      expected_voltage_mv, out);

	/*
	 * Change the voltage and expect that we keep getting cached value until
	 * 500ms has passed.
	 */
	connector_status.voltage_reading = next_expected_voltage_mv / mv_units;
	emul_pdc_set_connector_status(emul, &connector_status);
	k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
	zassert_equal(expected_voltage_mv,
		      pdc_power_mgmt_get_vbus_voltage(TEST_PORT));

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_true(TEST_WAIT_FOR(
		next_expected_voltage_mv ==
			pdc_power_mgmt_get_vbus_voltage(TEST_PORT),
		VBUS_READ_CACHE_MS));

	/*
	 * Connector status change bits can also immediately trigger vbus reads.
	 */
	change_bits.raw_value = 0;
	change_bits.negotiated_power_level = 1;
	next_expected_voltage_mv += 100;
	connector_status.voltage_reading = next_expected_voltage_mv / mv_units;
	connector_status.raw_conn_status_change_bits = change_bits.raw_value;
	emul_pdc_set_connector_status(emul, &connector_status);
	emul_pdc_pulse_irq(emul);
	k_msleep(TEST_WAIT_FOR_INTERVAL_MS);

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_equal(next_expected_voltage_mv,
		      pdc_power_mgmt_get_vbus_voltage(TEST_PORT));

	emul_pdc_disconnect(emul);
	zassert_true(
		TEST_WAIT_FOR(!pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_set_dual_role)
{
	int i;
	struct setup_t {
		enum pd_dual_role_states state;
		set_connector_status_t configure;
	};
	struct expect_t {
		bool check_cc_mode;
		enum ccom_t cc_mode;
		bool check_pdr;
		union pdr_t pdr;
	};
	struct {
		struct setup_t s;
		struct expect_t e;
	} test[] = {
		/* Unattached tests */
		{ .s = { .state = PD_DRP_TOGGLE_ON, .configure = NULL },
		  .e = { .check_cc_mode = true, .cc_mode = CCOM_DRP } },
		{ .s = { .state = PD_DRP_TOGGLE_OFF, .configure = NULL },
		  .e = { .check_cc_mode = true, .cc_mode = CCOM_RD } },
		{ .s = { .state = PD_DRP_FREEZE, .configure = NULL },
		  .e = { .check_cc_mode = true, .cc_mode = CCOM_RD } },
		/* Freeze while a sink */
		{ .s = { .state = PD_DRP_FREEZE,
			 .configure = emul_pdc_configure_snk },
		  .e = { .check_cc_mode = true, .cc_mode = CCOM_RD } },
		{ .s = { .state = PD_DRP_FREEZE,
			 .configure = emul_pdc_configure_src },
		  .e = { .check_cc_mode = true, .cc_mode = CCOM_RP } },
		/* Force sink while a source */
		{ .s = { .state = PD_DRP_FORCE_SINK,
			 .configure = emul_pdc_configure_src },
		  .e = { .check_pdr = true, /* Validate PDR as SNK */
			 .pdr = { .swap_to_src = 0,
				  .swap_to_snk = 1,
				  .accept_pr_swap = 0 },
			 .check_cc_mode = true,
			 .cc_mode = CCOM_RD } },
		/* Force source while a sink */
		{ .s = { .state = PD_DRP_FORCE_SOURCE,
			 .configure = emul_pdc_configure_snk },
		  .e = { .check_pdr = true, /* Validate PDR as SRC */
			 .pdr = { .swap_to_src = 1,
				  .swap_to_snk = 0,
				  .accept_pr_swap = 0 } } },
		/* Force sink while already a sink */
		{ .s = { .state = PD_DRP_FORCE_SINK,
			 .configure = emul_pdc_configure_snk },
		  .e = { .check_pdr = true,
			 .pdr = { .swap_to_src = 0,
				  .swap_to_snk = 1,
				  /* No external swaps allowed because we are
				   * already in the desired role. */
				  .accept_pr_swap = 0 },
			 .check_cc_mode = true,
			 .cc_mode = CCOM_RD } },
		/* Force source while already a source */
		{ .s = { .state = PD_DRP_FORCE_SOURCE,
			 .configure = emul_pdc_configure_src },
		  .e = { .check_pdr = true,
			 .pdr = { .swap_to_src = 1,
				  .swap_to_snk = 0,
				  /* No external swaps allowed because we are
				   * already in the desired role. */
				  .accept_pr_swap = 0 } } },
		/* Toggling on while a source */
		{ .s = { .state = PD_DRP_TOGGLE_ON,
			 .configure = emul_pdc_configure_src },
		  .e = { .check_pdr = true,
			 .pdr = {
				  /* Don't initiate a swap but allow external
				   * swaps. */
				  .swap_to_src = 1,
				  .swap_to_snk = 0,
				  .accept_pr_swap = 1 } } },
		/* Toggling on while a sink */
		{ .s = { .state = PD_DRP_TOGGLE_ON,
			 .configure = emul_pdc_configure_snk },
		  .e = { .check_pdr = true,
			 .pdr = {
				  /* Don't initiate a swap but allow external
				   * swaps. */
				  .swap_to_src = 0,
				  .swap_to_snk = 1,
				  .accept_pr_swap = 1 } } },
		/* Toggling off while a source */
		{ .s = { .state = PD_DRP_TOGGLE_OFF,
			 .configure = emul_pdc_configure_src },
		  .e = { .check_pdr = true,
			 .pdr = {
				  /* Remain a source but allow a swap to
				   * sink */
				  .swap_to_src = 1,
				  .swap_to_snk = 0,
				  .accept_pr_swap = 1 } } },
		/* Toggling off while a sink */
		{ .s = { .state = PD_DRP_TOGGLE_OFF,
			 .configure = emul_pdc_configure_snk },
		  .e = { .check_pdr = true,
			 .pdr = {
				  /* Remain a sink and don't allow an external
				   * swap. */
				  .swap_to_src = 0,
				  .swap_to_snk = 1,
				  .accept_pr_swap = 0 } } },
	};

	union connector_status_t connector_status = { 0 };
	enum ccom_t ccom;
	union pdr_t pdr;
	uint32_t timeout = k_ms_to_cyc_ceil32(4000);
	uint32_t start;

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		char *pd_config = "UNA";
		if ((void *)test[i].s.configure ==
		    (void *)emul_pdc_configure_src) {
			pd_config = "SRC";
		} else if ((void *)test[i].s.configure ==
			   (void *)emul_pdc_configure_snk) {
			pd_config = "SNK";
		}
		LOG_INF("Testing [%d]: DRP=%d PD=%s", i, test[i].s.state,
			pd_config);
		/* Reset CCOM in emulator to defaults */
		zassert_ok(emul_pdc_reset(emul));

		memset(&connector_status, 0, sizeof(connector_status));
		if (test[i].s.configure) {
			test[i].s.configure(emul, &connector_status);
			emul_pdc_connect_partner(emul, &connector_status);
			zassert_true(TEST_WAIT_FOR(
				pdc_power_mgmt_is_pd_attached(TEST_PORT),
				PDC_TEST_TIMEOUT));
		}

		pd_set_dual_role(TEST_PORT, test[i].s.state);

		zassert_equal(test[i].s.state, pd_get_dual_role(TEST_PORT));

		start = k_cycle_get_32();

		while (k_cycle_get_32() - start < timeout) {
			k_msleep(TEST_WAIT_FOR_INTERVAL_MS);

			if (test[i].e.check_cc_mode) {
				zassert_ok(emul_pdc_get_ccom(emul, &ccom),
					   "Invalid CCOM value in emul");

				if (test[i].e.cc_mode != ccom)
					continue;
			}

			if (test[i].e.check_pdr) {
				emul_pdc_get_pdr(emul, &pdr);

				if (test[i].e.pdr.swap_to_snk !=
					    pdr.swap_to_snk ||
				    test[i].e.pdr.swap_to_src !=
					    pdr.swap_to_src ||
				    test[i].e.pdr.accept_pr_swap !=
					    pdr.accept_pr_swap)
					continue;
			}

			break;
		}
		if (test[i].e.check_pdr) {
			zassert_ok(emul_pdc_get_pdr(emul, &pdr));
			zassert_equal(test[i].e.pdr.swap_to_snk,
				      pdr.swap_to_snk,
				      "Expected %u, got %u (i=%d)",
				      test[i].e.pdr.swap_to_snk,
				      pdr.swap_to_snk, i);
			zassert_equal(test[i].e.pdr.swap_to_src,
				      pdr.swap_to_src,
				      "Expected %u, got %u (i=%d)",
				      test[i].e.pdr.swap_to_src,
				      pdr.swap_to_src, i);
			zassert_equal(test[i].e.pdr.accept_pr_swap,
				      pdr.accept_pr_swap,
				      "Expected %u, got %u (i=%d)",
				      test[i].e.pdr.accept_pr_swap,
				      pdr.accept_pr_swap, i);
		}

		emul_pdc_disconnect(emul);

		zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

		if (test[i].e.check_cc_mode) {
			zassert_ok(emul_pdc_get_ccom(emul, &ccom),
				   "Invalid CCOM value in emul");
			zassert_equal(test[i].e.cc_mode, ccom,
				      "[%d] expected=%d, received=%d", i,
				      test[i].e.cc_mode, ccom);
		}
	}
}

ZTEST_USER(pdc_power_mgmt_api, test_chipset_suspend)
{
	union connector_status_t connector_status = {};
	enum ccom_t ccom;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	fake_chipset_state = CHIPSET_STATE_SUSPEND;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	TEST_WORKING_DELAY(PDC_POWER_STABLE_TIMEOUT);

	emul_pdc_disconnect(emul);

	start = k_cycle_get_32();
	while (k_cycle_get_32() - start < timeout) {
		k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
		zassert_ok(emul_pdc_get_ccom(emul, &ccom),
			   "Invalid CCOM value in emul");

		if (ccom != CCOM_RD)
			continue;

		break;
	}

	zassert_equal(CCOM_RD, ccom);
}

ZTEST_USER(pdc_power_mgmt_api, test_chipset_resume_no_partner)
{
	enum ccom_t ccom;

	fake_chipset_state = CHIPSET_STATE_ON;
	hook_notify(HOOK_CHIPSET_RESUME);
	TEST_WORKING_DELAY(PDC_POWER_STABLE_TIMEOUT);

	zassert_ok(emul_pdc_get_ccom(emul, &ccom),
		   "Invalid CCOM value in emul");
	zassert_equal(CCOM_DRP, ccom);
}

ZTEST_USER(pdc_power_mgmt_api, test_chipset_resume_drp_partner)
{
	union connector_status_t connector_status = {};
	union pdr_t pdr;
	const uint32_t pdos[] = {
		PDO_FIXED(5000, 3000, PDO_FIXED_DUAL_ROLE),
	};

	emul_pdc_configure_snk(emul, &connector_status);
	clear_partner_pdos(emul, SOURCE_PDO);
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 1, PARTNER_PDO, pdos);
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	fake_chipset_state = CHIPSET_STATE_ON;
	hook_notify(HOOK_CHIPSET_RESUME);
	TEST_WORKING_DELAY(PDC_POWER_STABLE_TIMEOUT);

	zassert_ok(emul_pdc_get_pdr(emul, &pdr), "Invalid PDR value in emul");
	zassert_equal(pdr.swap_to_src, 1);
	zassert_equal(pdr.accept_pr_swap, 1);

	zassert_true(pd_is_connected(TEST_PORT));
}

ZTEST_USER(pdc_power_mgmt_api, test_chipset_resume_up_drp_partner)
{
	union connector_status_t connector_status = {};
	union pdr_t pdr;
	const uint32_t pdos[] = {
		PDO_FIXED(5000, 3000,
			  PDO_FIXED_DUAL_ROLE |
				  PDO_FIXED_GET_UNCONSTRAINED_PWR),
	};

	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, PARTNER_PDO, pdos);
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	hook_notify(HOOK_CHIPSET_RESUME);
	TEST_WORKING_DELAY(PDC_POWER_STABLE_TIMEOUT);

	zassert_ok(emul_pdc_get_pdr(emul, &pdr), "Invalid PDR value in emul");
	zassert_equal(pdr.swap_to_src, 0);
}

ZTEST_USER(pdc_power_mgmt_api, test_chipset_on)
{
	union connector_status_t connector_status = {};
	enum ccom_t ccom;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;
	const enum drp_mode_t expected = USBC0_UNA_DRP_MODE;
	enum drp_mode_t drp_mode = DRP_INVALID;

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	fake_chipset_state = CHIPSET_STATE_ON;
	hook_notify(HOOK_CHIPSET_STARTUP);
	hook_notify(HOOK_CHIPSET_RESUME);
	TEST_WORKING_DELAY(PDC_POWER_STABLE_TIMEOUT);

	emul_pdc_disconnect(emul);

	start = k_cycle_get_32();
	while (k_cycle_get_32() - start < timeout) {
		k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
		zassert_ok(emul_pdc_get_ccom(emul, &ccom),
			   "Invalid CCOM value in emul");

		if (ccom != CCOM_DRP)
			continue;

		break;
	}

	zassert_equal(CCOM_DRP, ccom);
	zassert_ok(pdc_power_mgmt_get_drp_mode(TEST_PORT, &drp_mode));
	zassert_equal(drp_mode, expected);
}

ZTEST_USER(pdc_power_mgmt_api, test_chipset_shutdown)
{
	union connector_status_t connector_status = {};
	union pdr_t pdr;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;
	const enum drp_mode_t expected = DRP_NORMAL;
	enum drp_mode_t drp_mode = DRP_INVALID;

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));

	fake_chipset_state = CHIPSET_STATE_HARD_OFF;
#ifdef CONFIG_PLATFORM_EC_CHIPSET_RESUME_INIT_HOOK
	hook_notify(HOOK_CHIPSET_SHUTDOWN_COMPLETE);
#else
	hook_notify(HOOK_CHIPSET_SHUTDOWN);
#endif
	TEST_WORKING_DELAY(PDC_POWER_STABLE_TIMEOUT);

	emul_pdc_disconnect(emul);

	start = k_cycle_get_32();
	while (k_cycle_get_32() - start < timeout) {
		k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
		emul_pdc_get_pdr(emul, &pdr);

		if (pdr.swap_to_snk != 1)
			continue;

		if (pdr.swap_to_src != 0)
			continue;

		break;
	}

	zassert_equal(1, pdr.swap_to_snk);
	zassert_equal(0, pdr.swap_to_src);
	zassert_ok(pdc_power_mgmt_get_drp_mode(TEST_PORT, &drp_mode));
	zassert_equal(drp_mode, expected);
}

static bool wait_state_name(int port, const uint8_t target_state,
			    const char *target_name)
{
	const uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start = k_cycle_get_32();
	const char *state_name = pd_get_task_state_name(port);
	uint8_t state = pd_get_task_state(port);

	while (k_cycle_get_32() - start < timeout) {
		k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
		state_name = pd_get_task_state_name(port);
		state = pd_get_task_state(port);

		if (strcmp(state_name, target_name) != 0)
			continue;

		return target_state == state;
	}

	return false;
}

ZTEST_USER(pdc_power_mgmt_api, test_get_task_state_name_typec_snk_attached)
{
	union connector_status_t connector_status = { 0 };

	zassert_true(wait_state_name(TEST_PORT, PDC_UNATTACHED, "Unattached"));

	memset(&connector_status, 0, sizeof(connector_status));
	emul_pdc_configure_snk(emul, &connector_status);
	connector_status.power_operation_mode = USB_DEFAULT_OPERATION;
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_true(wait_state_name(TEST_PORT, PDC_SNK_TYPEC_ONLY,
				     "TypeCSnkAttached"));

	/* Allow for debouncing time. */
	TEST_WORKING_DELAY(PD_T_SINK_WAIT_CAP / USEC_PER_MSEC);
	zassert_true(test_pdc_power_mgmt_is_snk_typec_attached_run(TEST_PORT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_task_state_name_typec_src_attached)
{
	union connector_status_t connector_status = { 0 };

	zassert_true(wait_state_name(TEST_PORT, PDC_UNATTACHED, "Unattached"));

	memset(&connector_status, 0, sizeof(connector_status));
	emul_pdc_configure_src(emul, &connector_status);
	connector_status.power_operation_mode = USB_DEFAULT_OPERATION;
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_true(wait_state_name(TEST_PORT, PDC_SRC_TYPEC_ONLY,
				     "TypeCSrcAttached"));

	/* Allow for debouncing time. */
	TEST_WORKING_DELAY(TYPEC_ONLY_SINK_DEBOUNCE_TIME_MS);
	zassert_true(test_pdc_power_mgmt_is_src_typec_attached_run(TEST_PORT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_task_state_name_attached_snk)
{
	union connector_status_t connector_status = { 0 };

	zassert_true(wait_state_name(TEST_PORT, PDC_UNATTACHED, "Unattached"));

	memset(&connector_status, 0, sizeof(connector_status));
	emul_pdc_configure_snk(emul, &connector_status);
	connector_status.power_operation_mode = PD_OPERATION;
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_true(
		wait_state_name(TEST_PORT, PDC_SNK_ATTACHED, "Attached.SNK"));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_task_state_name_attached_src)
{
	union connector_status_t connector_status = { 0 };

	zassert_true(wait_state_name(TEST_PORT, PDC_UNATTACHED, "Unattached"));

	memset(&connector_status, 0, sizeof(connector_status));
	emul_pdc_configure_src(emul, &connector_status);
	connector_status.power_operation_mode = PD_OPERATION;
	emul_pdc_connect_partner(emul, &connector_status);

	zassert_true(
		wait_state_name(TEST_PORT, PDC_SRC_ATTACHED, "Attached.SRC"));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_connector_status)
{
	union connector_status_t in = {}, out = {};
	union conn_status_change_bits_t in_conn_status_change_bits = { 0 };
	union conn_status_change_bits_t out_conn_status_change_bits = { 0 };

	zassert_equal(-ERANGE, pdc_power_mgmt_get_connector_status(
				       CONFIG_USB_PD_PORT_MAX_COUNT, &out));
	zassert_equal(-EINVAL,
		      pdc_power_mgmt_get_connector_status(TEST_PORT, NULL));

	in_conn_status_change_bits.external_supply_change = 1;
	in_conn_status_change_bits.connector_partner = 1;
	in_conn_status_change_bits.connect_change = 1;
	in.raw_conn_status_change_bits = in_conn_status_change_bits.raw_value;

	in.conn_partner_flags = 1;
	in.conn_partner_type = UFP_ATTACHED;
	in.rdo = 0x01234567;

	emul_pdc_configure_snk(emul, &in);
	emul_pdc_connect_partner(emul, &in);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_ok(pdc_power_mgmt_get_connector_status(TEST_PORT, &out));
	zassert_ok(pdc_power_mgmt_get_last_status_change(
		TEST_PORT, &out_conn_status_change_bits));

	zassert_equal(out_conn_status_change_bits.external_supply_change,
		      in_conn_status_change_bits.external_supply_change);
	zassert_equal(out_conn_status_change_bits.connector_partner,
		      in_conn_status_change_bits.connector_partner);
	zassert_equal(out_conn_status_change_bits.connect_change,
		      in_conn_status_change_bits.connect_change);
	zassert_equal(out.power_operation_mode, in.power_operation_mode);
	zassert_equal(out.connect_status, in.connect_status);
	zassert_equal(out.power_direction, in.power_direction);
	zassert_equal(out.conn_partner_flags, in.conn_partner_flags,
		      "out=0x%X != in=0x%X", out.conn_partner_flags,
		      in.conn_partner_flags);
	zassert_equal(out.conn_partner_type, in.conn_partner_type);
	zassert_equal(out.rdo, in.rdo);

	emul_pdc_disconnect(emul);
	zassert_true(TEST_WAIT_FOR(!pdc_power_mgmt_is_connected(TEST_PORT),
				   PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_cable_prop)
{
	union cable_property_t in, out, exp;
	union connector_status_t in_conn_status = {}, out_conn_status = {};
	union conn_status_change_bits_t in_conn_status_change_bits = { 0 };

	zassert_equal(-ERANGE, pdc_power_mgmt_get_cable_prop(
				       CONFIG_USB_PD_PORT_MAX_COUNT, &out));
	zassert_equal(-EINVAL, pdc_power_mgmt_get_cable_prop(TEST_PORT, NULL));

	in.raw_value[0] = 0x1a2b3c4d;
	in.raw_value[1] = 0x5a6b7c8d;
	emul_pdc_set_cable_property(emul, in);

	in_conn_status_change_bits.external_supply_change = 1;
	in_conn_status_change_bits.connector_partner = 1;
	in_conn_status_change_bits.connect_change = 1;
	in_conn_status.raw_conn_status_change_bits =
		in_conn_status_change_bits.raw_value;

	in_conn_status.conn_partner_flags = 1;
	in_conn_status.conn_partner_type = UFP_ATTACHED;
	in_conn_status.rdo = 0x01234567;

	emul_pdc_configure_snk(emul, &in_conn_status);
	emul_pdc_connect_partner(emul, &in_conn_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_ok(pdc_power_mgmt_get_connector_status(TEST_PORT,
						       &out_conn_status));

	zassert_ok(pdc_power_mgmt_get_cable_prop(TEST_PORT, &out));

	/*
	 * The RTS54xx only returns 5 bytes of cable property.
	 */
	zassert_mem_equal(in.raw_value, out.raw_value, 5,
			  "Returned cable property did not match input "
			  "in 0x%08X:%08X != out 0x%08X:%08X",
			  in.raw_value[0], in.raw_value[1], out.raw_value[0],
			  out.raw_value[1]);

	exp.raw_value[0] = in.raw_value[0];
	exp.raw_value[1] = in.raw_value[1] & 0xff;
	zassert_mem_equal(exp.raw_value, out.raw_value, sizeof(exp),
			  "Returned cable property included extra data "
			  "exp 0x%08X:%08X != out 0x%08X:%08X",
			  exp.raw_value[0], exp.raw_value[1], out.raw_value[0],
			  out.raw_value[1]);

	emul_pdc_disconnect(emul);
	zassert_true(TEST_WAIT_FOR(!pdc_power_mgmt_is_connected(TEST_PORT),
				   PDC_TEST_TIMEOUT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_identity_discovery)
{
	struct setup_t {
		enum tcpci_msg_type type;
		bool cable_type;
		bool mode_support;
	};
	struct expect_t {
		bool check_cc_mode;
		enum ccom_t cc_mode;
		bool check_pdr;
		union pdr_t pdr;
	};
	struct {
		char *description;
		struct setup_t s;
		enum pd_discovery_state expected_state;
	} test[] = {
		{
			.description = "SOP with alt mode support",
			.s = { .type = TCPCI_MSG_SOP,
			       .cable_type = false,
			       .mode_support = true, },
			.expected_state = PD_DISC_COMPLETE,
		},
		{
			.description = "SOP without alt mode support",
			.s = { .type = TCPCI_MSG_SOP,
			       .cable_type = false,
			       .mode_support = false, },
			.expected_state = PD_DISC_FAIL,
		},
		{
			.description = "SOP' with alt mode support",
			.s = { .type = TCPCI_MSG_SOP_PRIME,
			       .cable_type = true,
			       .mode_support = true, },
			.expected_state = PD_DISC_COMPLETE,
		},
		{
			.description = "SOP' without alt mode support",
			.s = { .type = TCPCI_MSG_SOP_PRIME,
			       .cable_type = true,
			       .mode_support = false, },
			.expected_state = PD_DISC_FAIL,
		},
		{
			/* SOP'' not supported and should always fail. */
			.description = "SOP'' with alt mode support",
			.s = { .type = TCPCI_MSG_SOP_PRIME_PRIME,
			       .cable_type = true,
			       .mode_support = true, },
			.expected_state = PD_DISC_FAIL,
		},
	};

	union cable_property_t in;
	union connector_status_t in_conn_status = {};
	union conn_status_change_bits_t in_conn_status_change_bits = { 0 };
	enum pd_discovery_state actual_state;

	in_conn_status_change_bits.external_supply_change = 1;
	in_conn_status_change_bits.connector_partner = 1;
	in_conn_status_change_bits.connect_change = 1;
	in_conn_status.raw_conn_status_change_bits =
		in_conn_status_change_bits.raw_value;

	in_conn_status.conn_partner_type = UFP_ATTACHED;
	in_conn_status.rdo = 0x01234567;

	for (int i = 0; i < ARRAY_SIZE(test); i++) {
		LOG_INF("Testing %s", test[i].description);

		emul_pdc_configure_snk(emul, &in_conn_status);

		if (test[i].s.mode_support) {
			in_conn_status.conn_partner_flags =
				CONNECTOR_PARTNER_FLAG_ALTERNATE_MODE;
		} else {
			in_conn_status.conn_partner_flags =
				CONNECTOR_PARTNER_FLAG_USB;
		}
		in.cable_type = test[i].s.cable_type;
		in.mode_support = test[i].s.mode_support;

		emul_pdc_set_cable_property(emul, in);

		emul_pdc_connect_partner(emul, &in_conn_status);
		zassert_true(
			TEST_WAIT_FOR(pdc_power_mgmt_is_pd_attached(TEST_PORT),
				      PDC_TEST_TIMEOUT));

		actual_state = pdc_power_mgmt_get_identity_discovery(
			TEST_PORT, test[i].s.type);
		zassert_equal(test[i].expected_state, actual_state,
			      "%s: expected state %d, actual %d",
			      test[i].description, test[i].expected_state,
			      actual_state);

		emul_pdc_disconnect(emul);
		zassert_true(
			TEST_WAIT_FOR(!pdc_power_mgmt_is_connected(TEST_PORT),
				      PDC_TEST_TIMEOUT));
	}

	zassert_equal(pdc_power_mgmt_get_identity_discovery(TEST_PORT,
							    TCPCI_MSG_SOP),
		      PD_DISC_NEEDED);
};

ZTEST_USER(pdc_power_mgmt_api, test_get_identity_vid)
{
	uint32_t vid = VDO_IDH(
		/* USB host */ false, /* USB device */ true, IDH_PTYPE_HUB,
		/* modal operation */ true, USB_VID_GOOGLE);
	uint32_t product = VDO_PRODUCT(0xBEAD, 0x1001);
	uint32_t vdo[] = { vid, product };
	union connector_status_t conn_status = {};

	zassert_equal(0, pdc_power_mgmt_get_identity_vid(
				 CONFIG_USB_PD_PORT_MAX_COUNT));

	if (-ENOSYS == emul_pdc_set_vdo(emul, 2, vdo)) {
		ztest_test_skip();
	}

	emul_pdc_configure_snk(emul, &conn_status);
	emul_pdc_connect_partner(emul, &conn_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_equal(USB_VID_GOOGLE,
		      pdc_power_mgmt_get_identity_vid(TEST_PORT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_identity_pid)
{
	uint32_t vid = VDO_IDH(
		/* USB host */ false, /* USB device */ true, IDH_PTYPE_HUB,
		/* modal operation */ true, USB_VID_GOOGLE);
	uint32_t product = VDO_PRODUCT(0xBEAD, 0x1001);
	uint32_t vdo[] = { vid, product };
	union connector_status_t conn_status = { 0 };

	zassert_equal(0, pdc_power_mgmt_get_identity_pid(
				 CONFIG_USB_PD_PORT_MAX_COUNT));

	if (-ENOSYS == emul_pdc_set_vdo(emul, 2, vdo)) {
		ztest_test_skip();
	}

	emul_pdc_configure_snk(emul, &conn_status);
	emul_pdc_connect_partner(emul, &conn_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_equal(0xBEAD, pdc_power_mgmt_get_identity_pid(TEST_PORT));
	zassert_equal(IDH_PTYPE_HUB,
		      pdc_power_mgmt_get_product_type(TEST_PORT));
}

ZTEST_USER(pdc_power_mgmt_api, test_get_product_type)
{
	uint32_t vid = VDO_IDH(
		/* USB host */ false, /* USB device */ true, IDH_PTYPE_HUB,
		/* modal operation */ true, USB_VID_GOOGLE);
	uint32_t product = VDO_PRODUCT(0xBEAD, 0x1001);
	uint32_t vdo[] = { vid, product };
	union connector_status_t conn_status = {};

	zassert_equal(0, pdc_power_mgmt_get_product_type(
				 CONFIG_USB_PD_PORT_MAX_COUNT));

	if (-ENOSYS == emul_pdc_set_vdo(emul, 2, vdo)) {
		ztest_test_skip();
	}

	emul_pdc_configure_snk(emul, &conn_status);
	emul_pdc_connect_partner(emul, &conn_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_equal(IDH_PTYPE_HUB,
		      pdc_power_mgmt_get_product_type(TEST_PORT));
}

/*
 * Validate that all possible PDC power management states have a name
 * assigned.  This could possibly be done with some macrobatics, but
 * a runtime unit test is easier to maintain.
 */
ZTEST_USER(pdc_power_mgmt_api, test_names)
{
	for (int i = 0; i < pdc_cmd_types; i++) {
		zassert_not_null(pdc_cmd_names[i],
				 "PDC command %d missing name", i);
	}
}

/**
 * @brief Poll up to PDC_TEST_TIMEOUT milliseconds for the expected CCOM
 *        value to be returned by emul_pdc_get_ccom()
 */
static void helper_wait_for_ccom_mode(enum ccom_t expected)
{
	enum ccom_t ccom;
	uint32_t timeout = k_ms_to_cyc_ceil32(PDC_TEST_TIMEOUT);
	uint32_t start;

	start = k_cycle_get_32();
	while (k_cycle_get_32() - start < timeout) {
		k_msleep(TEST_WAIT_FOR_INTERVAL_MS);
		zassert_ok(emul_pdc_get_ccom(emul, &ccom),
			   "Invalid CCOM value in emul");

		if (ccom != expected)
			continue;

		break;
	}

	zassert_equal(expected, ccom, "Got CCOM %d but expected %d", ccom,
		      expected);
}

ZTEST_USER(pdc_power_mgmt_api, test_sysjump_policy_shutdown)
{
	/* Mock a late sysjump while AP is off. */
	fake_chipset_state = CHIPSET_STATE_HARD_OFF;
	system_jumped_late_fake.return_val = 1;

	/* PDC should go into PD_DRP_FORCE_SINK mode, which means CC operating
	 * mode is CCOM_RD.
	 */

	/* This forces a pass through the init state */
	zassert_ok(pdc_power_mgmt_reset(TEST_PORT));
	TEST_WORKING_DELAY(PDC_TEST_TIMEOUT);

	helper_wait_for_ccom_mode(CCOM_RD);
}

ZTEST_USER(pdc_power_mgmt_api, test_sysjump_policy_suspend)
{
	/* Mock a late sysjump while AP is off. */
	fake_chipset_state = CHIPSET_STATE_SUSPEND;
	system_jumped_late_fake.return_val = 1;

	/* PDC should go into PD_DRP_TOGGLE_OFF mode, which means CC operating
	 * mode is CCOM_RD.
	 */

	/* This forces a pass through the init state */
	zassert_ok(pdc_power_mgmt_reset(TEST_PORT));
	TEST_WORKING_DELAY(PDC_TEST_TIMEOUT);

	helper_wait_for_ccom_mode(CCOM_RD);
}

ZTEST_USER(pdc_power_mgmt_api, test_sysjump_policy_on)
{
	/* Mock a late sysjump while AP is off. */
	fake_chipset_state = CHIPSET_STATE_ON;
	system_jumped_late_fake.return_val = 1;

	/* PDC should go into PD_DRP_TOGGLE_ON mode, which means CC operating
	 * mode is CCOM_DRP.
	 */

	/* This forces a pass through the init state */
	zassert_ok(pdc_power_mgmt_reset(TEST_PORT));
	TEST_WORKING_DELAY(PDC_TEST_TIMEOUT);

	helper_wait_for_ccom_mode(CCOM_DRP);
}

/* TODO(b/345292002): Need Set RDO API for TPS6699x emulator/driver. */
#ifndef CONFIG_TODO_B_345292002
/**
 * @brief Helper function for getting object position in RDO from the emulator
 */
static uint32_t get_rdo()
{
	uint32_t rdo;

	zassert_ok(emul_pdc_get_rdo(emul, &rdo));

	return rdo;
}

ZTEST_USER(pdc_power_mgmt_api, test_set_new_power_request)
{
	union connector_status_t connector_status = {};
	const uint32_t pdos[] = {
		PDO_FIXED(5000, 3000, PDO_FIXED_DUAL_ROLE), /* 15W */
		PDO_FIXED(9000, 3000, PDO_FIXED_DUAL_ROLE), /* 27W */
		PDO_FIXED(20000, 3000, PDO_FIXED_DUAL_ROLE) /* 60W */
	};
	unsigned int max_voltage = pdc_power_mgmt_get_max_voltage();

	/* This should result in no-op */
	zassert_not_ok(pdc_power_mgmt_set_new_power_request(TEST_PORT));

	emul_pdc_configure_snk(emul, &connector_status);
	clear_partner_pdos(emul, SOURCE_PDO);

	/* Set the first two PDOs, make sure 5v RDO is selected */
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, ARRAY_SIZE(pdos) - 1,
			  PARTNER_PDO, pdos);

	pdc_power_mgmt_set_max_voltage(5000);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_true(
		TEST_WAIT_FOR(pd_is_connected(TEST_PORT), PDC_TEST_TIMEOUT));
	LOG_DBG("RDO position before new power request: %d",
		RDO_POS(get_rdo()));

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	/* Set all PDOs, make sure 27w PDO is selected as pdc_power_mgmt
	 * should NOT query for new PDOs although 60w is present now. */

	clear_partner_pdos(emul, SOURCE_PDO);
	emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, ARRAY_SIZE(pdos),
			  PARTNER_PDO, pdos);
	pdc_power_mgmt_set_max_voltage(20000);
	zassert_ok(pdc_power_mgmt_set_new_power_request(TEST_PORT));
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	/* The 27W PDO at position 2 must be selected after the new power
	 * request.
	 */
	zassert_true(TEST_WAIT_FOR(RDO_POS(get_rdo()) == 2, PDC_TEST_TIMEOUT));

	/* RDO should always have the USB communication capable bit set */
	zassert_true(get_rdo() & RDO_COMM_CAP,
		     "RDO (0x%08x) is missing RDO_COMM_CAP bit (bit 25)",
		     get_rdo());

	LOG_DBG("RDO position after new power request: %d", RDO_POS(get_rdo()));
	pdc_power_mgmt_set_max_voltage(max_voltage);
}

ZTEST_USER(pdc_power_mgmt_api, test_request_source_voltage)
{
	uint32_t partner_src_pdos[] = {
		PDO_FIXED(5000, 3000, 0),
		PDO_FIXED(12000, 3000, 0),
		PDO_FIXED(20000, 5000, 0),
	};
	uint32_t rdo = 0;
	union connector_status_t connector_status = { 0 };
	int prev_mv = pdc_power_mgmt_get_max_voltage();
	int source_mv = 5000;

	/* Setup    - Configure as SNK and request 5v
	 * Validate - Confirm 5v PDO selected
	 */
	pdc_power_mgmt_request_source_voltage(TEST_PORT, source_mv);
	emul_pdc_configure_snk(emul, &connector_status);
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0,
				     ARRAY_SIZE(partner_src_pdos), PARTNER_PDO,
				     partner_src_pdos));
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_ok(pdc_power_mgmt_get_rdo(TEST_PORT, &rdo));
	/* Confirm 5v PDO selected */
	zassert_equal(1, RDO_POS(rdo));
	zassert_equal(source_mv, pdc_power_mgmt_get_max_voltage());

	/* Setup    - Configure as SNK and request 12v
	 * Validate - Confirm 12v PDO selected
	 */
	source_mv = 12000;
	pdc_power_mgmt_request_source_voltage(TEST_PORT, source_mv);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_ok(pdc_power_mgmt_get_rdo(TEST_PORT, &rdo));
	/* Confirm 12v PDO selected */
	zassert_equal(2, RDO_POS(rdo));
	zassert_equal(source_mv, pdc_power_mgmt_get_max_voltage());

	emul_pdc_disconnect(emul);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	/* Setup    - Configure as DRP SRC and request 5v
	 * Validate - Confirm 5v PDO selected and switched to SINK
	 */
	memset(&connector_status, 0, sizeof(connector_status));

	pdc_power_mgmt_request_source_voltage(TEST_PORT, prev_mv);
	emul_pdc_reset(emul);
	emul_pdc_configure_src(emul, &connector_status);
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0,
				     ARRAY_SIZE(partner_src_pdos), PARTNER_PDO,
				     partner_src_pdos));
	source_mv = 5000;
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_equal(PD_ROLE_SOURCE, pd_get_power_role(TEST_PORT));

	pdc_power_mgmt_request_source_voltage(TEST_PORT, source_mv);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_equal(source_mv, pdc_power_mgmt_get_max_voltage());

	/* Confirm 5v PDO selected */
	zassert_ok(pdc_power_mgmt_get_rdo(TEST_PORT, &rdo));
	zassert_equal(1, RDO_POS(rdo));

	/* Confirm Power role is SINK */
	zassert_equal(PD_ROLE_SINK, pd_get_power_role(TEST_PORT));

	emul_pdc_disconnect(emul);

	/* Restore source voltage */
	pdc_power_mgmt_request_source_voltage(TEST_PORT, prev_mv);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
}
#endif /* CONFIG_TODO_B_345292002 */

/* Get / set SBU mux mode is only supported on RTK currently */
ZTEST(pdc_power_mgmt_api, test_pdc_power_mgmt_sbu_mux_mode)
{
	enum pdc_sbu_mux_mode mode;
	int ccd_port = -1;
	int rv;

	zassert_ok(emul_pdc_set_feature_flag(
		emul, EMUL_PDC_FEATURE_SBU_MUX_OVERRIDE));

	rv = pdc_power_mgmt_set_sbu_mux_mode(PDC_SBU_MUX_MODE_NORMAL);
	zassert_ok(rv, "Setting SBU mux mode to normal failed: %d", rv);

	rv = pdc_power_mgmt_get_sbu_mux_mode(&mode, &ccd_port);
	zassert_ok(rv, "Getting SBU mux mode failed: %d", rv);

	zassert_equal(PDC_SBU_MUX_MODE_NORMAL, mode);
	zassert_equal(0, ccd_port);

	rv = pdc_power_mgmt_set_sbu_mux_mode(PDC_SBU_MUX_MODE_FORCE_DBG);
	zassert_ok(rv, "Setting SBU mux mode to debug failed: %d", rv);

	/* Null pointer */
	rv = pdc_power_mgmt_get_sbu_mux_mode(NULL, NULL);
	zassert_equal(-EINVAL, rv);

	/* Success */
	rv = pdc_power_mgmt_get_sbu_mux_mode(&mode, &ccd_port);
	zassert_ok(rv, "Getting SBU mux mode failed: %d", rv);

	zassert_equal(PDC_SBU_MUX_MODE_FORCE_DBG, mode);
	zassert_equal(0, ccd_port);
}

/* TODO(b/345292002): Not supported by TI driver currently. */
#ifndef CONFIG_TODO_B_345292002
ZTEST(pdc_power_mgmt_api, test_pdc_power_mgmt_sbu_mux_mode_not_supported_set)
{
	int rv;

	/* Do not enable support for SBU mux override. Commands will fail. */

	rv = pdc_power_mgmt_set_sbu_mux_mode(PDC_SBU_MUX_MODE_NORMAL);
	zassert_equal(-EBUSY, rv, "Expected -EBUSY (%d) but got %d", -EBUSY,
		      rv);
}

ZTEST(pdc_power_mgmt_api, test_pdc_power_mgmt_sbu_mux_mode_not_supported_get)
{
	int rv;
	enum pdc_sbu_mux_mode mode = PDC_SBU_MUX_MODE_NORMAL;

	/* Do not enable support for SBU mux override. We should get an invalid
	 * (PDC_SBU_MUX_MODE_INVALID) response.
	 */

	rv = pdc_power_mgmt_get_sbu_mux_mode(&mode, NULL);
	zassert_ok(rv, "Expected success but got %d", rv);
	zassert_equal(PDC_SBU_MUX_MODE_INVALID, mode,
		      "Expected PDC_SBU_MUX_MODE_INVALID (%d) but got %d",
		      PDC_SBU_MUX_MODE_INVALID, mode);
}

#endif /* CONFIG_TODO_B_345292002 */

/**
 * @brief Helper function for polling sink path status
 */
static bool is_sink_path_enabled()
{
	bool sink_path_en;
	zassert_ok(emul_pdc_get_sink_path(emul, &sink_path_en));
	return sink_path_en;
}

ZTEST_USER(pdc_power_mgmt_api, test_pdc_power_mgmt_set_active_charge_port)
{
	union connector_status_t connector_status = {};

	emul_pdc_configure_snk(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_true(is_sink_path_enabled());

	zassert_ok(charge_manager_set_override(OVERRIDE_DONT_CHARGE));
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	/* Sink path should be disabled because it's not active charge port */
	zassert_false(is_sink_path_enabled());

	zassert_ok(charge_manager_set_override(TEST_PORT));
	/* Sink path should be enabled after activating TEST_PORT */
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_true(is_sink_path_enabled());
}

ZTEST_USER(pdc_power_mgmt_api, test_get_vconn_state)
{
	union connector_status_t connector_status = { 0 };

	if (emul_pdc_set_vconn_sourcing(emul, false) == -ENOSYS) {
		ztest_test_skip();
	}

	zassert_false(pdc_power_mgmt_get_vconn_state(TEST_PORT));

	emul_pdc_configure_src(emul, &connector_status);
	emul_pdc_connect_partner(emul, &connector_status);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	emul_pdc_set_vconn_sourcing(emul, false);
	zassert_false(pdc_power_mgmt_get_vconn_state(TEST_PORT));
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	emul_pdc_set_vconn_sourcing(emul, true);
	zassert_true(pdc_power_mgmt_get_vconn_state(TEST_PORT));
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
}

/* TODO(b/345292002): The tests below fail with the TPS6699x emulator/driver. */
#ifndef CONFIG_TODO_B_345292002
ZTEST_USER(pdc_power_mgmt_api, test_hpd_wake)
{
	union get_attention_vdo_t attention_vdo;
	union connector_status_t in_conn_status = {};
	union conn_status_change_bits_t in_conn_status_change_bits = { 0 };

	/* Connect (DP) alternate mode partner. */
	in_conn_status_change_bits.connect_change = 1;
	in_conn_status.raw_conn_status_change_bits =
		in_conn_status_change_bits.raw_value;
	in_conn_status.power_operation_mode = PD_OPERATION;
	in_conn_status.conn_partner_flags =
		CONNECTOR_PARTNER_FLAG_ALTERNATE_MODE;
	emul_pdc_configure_src(emul, &in_conn_status);
	emul_pdc_connect_partner(emul, &in_conn_status);
	zassert_true(TEST_WAIT_FOR(pdc_power_mgmt_is_connected(TEST_PORT),
				   PDC_TEST_TIMEOUT));

	/* Configure PDC emulator to respond to GET_ATTENTION_VDO with HPD_LVL
	 * low.
	 */
	attention_vdo.vdo = 0x01;
	emul_pdc_set_attention_vdo(emul, attention_vdo);
	k_msleep(TEST_WAIT_FOR_INTERVAL_MS);

	/* Send an attention IRQ for the PDC power manager to update its DP
	 * Status.
	 */
	in_conn_status.raw_conn_status_change_bits = 0x8;
	emul_pdc_set_connector_status(emul, &in_conn_status);
	emul_pdc_pulse_irq(emul);
	k_msleep(TEST_WAIT_FOR_INTERVAL_MS * 2);

	/* Suspend the DUT. */
	fake_chipset_state = CHIPSET_STATE_SUSPEND;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	TEST_WORKING_DELAY(PDC_TEST_TIMEOUT * 2);

	/* Clear any USB mux host event. */
	host_clear_events(EC_HOST_EVENT_MASK(EC_HOST_EVENT_USB_MUX));
	zassert_false(host_is_event_set(EC_HOST_EVENT_USB_MUX));

	/* Configure PDC emulator to respond to GET_VDO with DP Status VDO with
	 * HPD_LVL high.
	 */
	attention_vdo.vdo = 0x81;
	emul_pdc_set_attention_vdo(emul, attention_vdo);
	k_msleep(TEST_WAIT_FOR_INTERVAL_MS);

	/* Send an IRQ for the PDC power manager to update its DP Status. */
	emul_pdc_set_connector_status(emul, &in_conn_status);
	emul_pdc_pulse_irq(emul);
	TEST_WORKING_DELAY(PDC_TEST_TIMEOUT * 2);

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	/* Confirm that the IRQ with HPD_LVL high caused a USB mux host
	 * event.
	 */
	zassert_true(host_is_event_set(EC_HOST_EVENT_USB_MUX));
}

static void test_dp_mode_helper(enum pd_power_role role)
{
	union get_attention_vdo_t attention_vdo;
	union connector_status_t in_conn_status = {};
	union conn_status_change_bits_t in_conn_status_change_bits = { 0 };
	uint32_t vdo[] = { 0x05 | (MODE_DP_PIN_D << 8) };

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	/* PIN_D and mux should be in DOCK mode*/
	zassert_equal(0, pdc_power_mgmt_get_dp_pin_mode(TEST_PORT));

	in_conn_status_change_bits.supported_cam = 1;
	in_conn_status_change_bits.attention = 1;
	in_conn_status.raw_conn_status_change_bits =
		in_conn_status_change_bits.raw_value;
	in_conn_status.power_operation_mode = PD_OPERATION;
	in_conn_status.conn_partner_flags =
		CONNECTOR_PARTNER_FLAG_ALTERNATE_MODE;
	zassert_ok(emul_pdc_set_vdo(emul, 1, vdo));

	attention_vdo.vdo = VDO_DP_STATUS(0, 0, 0, 0, 1 /* mf */, 1, 0, 1);
	emul_pdc_set_attention_vdo(emul, attention_vdo);

	switch (role) {
	case PD_ROLE_SINK:
		emul_pdc_configure_snk(emul, &in_conn_status);
		break;
	case PD_ROLE_SOURCE:
		emul_pdc_configure_src(emul, &in_conn_status);
		break;
		/* LCOV_EXCL_START */
	default:
		zassert_true(0, "Invalid PD role");
		__builtin_unreachable();
		/* LCOV_EXCL_STOP */
	}

	emul_pdc_connect_partner(emul, &in_conn_status);

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_equal(MODE_DP_PIN_D, pdc_power_mgmt_get_dp_pin_mode(TEST_PORT));
	zassert_equal(USB_PD_MUX_DOCK,
		      pdc_power_mgmt_get_dp_mux_mode(TEST_PORT));

	/* PIN_C and mux should be in DP only mode */
	vdo[0] = 0x05 | (MODE_DP_PIN_C << 8);
	zassert_ok(emul_pdc_set_vdo(emul, 1, vdo));

	/* PDC may have consumed the status, set status again. */
	emul_pdc_set_connector_status(emul, &in_conn_status);
	emul_pdc_pulse_irq(emul);

	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));

	zassert_equal(MODE_DP_PIN_C, pdc_power_mgmt_get_dp_pin_mode(TEST_PORT));
	zassert_equal(USB_PD_MUX_DP_ENABLED,
		      pdc_power_mgmt_get_dp_mux_mode(TEST_PORT));
}

ZTEST_USER(pdc_power_mgmt_api, test_dp_mode_src)
{
	test_dp_mode_helper(PD_ROLE_SOURCE);
}

ZTEST_USER(pdc_power_mgmt_api, test_dp_mode_snk)
{
	test_dp_mode_helper(PD_ROLE_SINK);
}

ZTEST_USER(pdc_power_mgmt_api, test_board_callback)
{
	union connector_status_t in_conn_status = {};
	union conn_status_change_bits_t in_conn_status_change_bits = { 0 };
	union get_attention_vdo_t attention_vdo;

	/* Register board callbacks. */
	pdc_power_mgmt_register_board_callback(PDC_BOARD_CB_DP_ATTENTION,
					       board_dp_attention_cb_stub);
	pdc_power_mgmt_register_board_callback(PDC_BOARD_CB_UNATTACH,
					       board_unattached_cb_stub);

	/* Connect (DP) alternate mode partner. */
	in_conn_status_change_bits.attention = 1;
	in_conn_status.raw_conn_status_change_bits =
		in_conn_status_change_bits.raw_value;
	in_conn_status.power_operation_mode = PD_OPERATION;
	in_conn_status.conn_partner_flags =
		CONNECTOR_PARTNER_FLAG_ALTERNATE_MODE;
	emul_pdc_configure_src(emul, &in_conn_status);
	emul_pdc_connect_partner(emul, &in_conn_status);

	zassert_true(TEST_WAIT_FOR(pdc_power_mgmt_is_connected(TEST_PORT),
				   PDC_TEST_TIMEOUT));

	/* Trigger Attention event for callback. */
	attention_vdo.vdo = 0x01;
	emul_pdc_set_attention_vdo(emul, attention_vdo);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_equal(board_dp_attention_cb_stub_fake.call_count, 1);

	/* Trigger unattach event for callback. */
	emul_pdc_disconnect(emul);
	zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_PORT, -1));
	zassert_equal(board_unattached_cb_stub_fake.call_count, 1);
}

ZTEST_USER(pdc_power_mgmt_api, test_get_rdo_errors)
{
	/* The normal code path for pdc_power_mgmt_get_rdo() is tested in
	 * test_request_source_voltage. This test covers its error paths.
	 */

	uint32_t rdo;

	zassert_equal(-EINVAL, pdc_power_mgmt_get_rdo(TEST_PORT, NULL));
	zassert_equal(-ENODATA, pdc_power_mgmt_get_rdo(TEST_PORT, &rdo));
}
#endif

/*
 * Suspended PDC - These tests take place with the PDC Power Mgmt subsystem
 * in the suspended state, when communication with the PDC is not allowed.
 */

static void *pdc_power_mgmt_suspend_setup(void)
{
	zassert_ok(pdc_power_mgmt_set_comms_state(false));

	return NULL;
}

static void pdc_power_mgmt_suspend_before(void *fixture)
{
	reset_fakes();
}

static void pdc_power_mgmt_suspend_after(void *fixture)
{
	reset_fakes();
}

static void pdc_power_mgmt_suspend_teardown(void *fixture)
{
	zassert_ok(pdc_power_mgmt_set_comms_state(true));

	zassert_ok(emul_pdc_idle_wait(emul));
}

ZTEST_SUITE(pdc_power_mgmt_api_suspended, NULL, pdc_power_mgmt_suspend_setup,
	    pdc_power_mgmt_suspend_before, pdc_power_mgmt_suspend_after,
	    pdc_power_mgmt_suspend_teardown);

ZTEST_USER(pdc_power_mgmt_api_suspended, test_get_info)
{
	struct pdc_info_t info;
	int rv;

	rv = pdc_power_mgmt_get_info(TEST_PORT, &info, true);
	zassert_equal(-ENOTCONN, rv, "Expected %d (-ENOTCONN) but got %d",
		      -ENOTCONN, rv);
}
