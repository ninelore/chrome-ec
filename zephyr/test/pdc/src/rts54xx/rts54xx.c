/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "common.h"
#include "console.h"
#include "drivers/pdc.h"
#include "drivers/ucsi_v3.h"
#include "emul/emul_pdc.h"
#include "emul/emul_realtek_rts54xx_public.h"
#include "i2c.h"
#include "pdc_trace_msg.h"
#include "test/util.h"
#include "usbc/ppm.h"
#include "zephyr/sys/util.h"
#include "zephyr/sys/util_macro.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(test_rts54xx, LOG_LEVEL_INF);

#define RTS5453P_NODE DT_NODELABEL(pdc_emul1)
#define RTS5453P_NODE2 DT_NODELABEL(pdc_emul2)

#define EMUL_PORT 0
#define EMUL2_PORT 1

#define NUM_PORTS 2

static const uint32_t spr_pdos[] = {
	PDO_AUG(1000, 5000, 3000), PDO_FIXED(5000, 3000, 0),
	PDO_AUG(1000, 5000, 3000), PDO_FIXED(9000, 3000, 0),
	PDO_AUG(1000, 5000, 3000), PDO_FIXED(15000, 3000, 0),
	PDO_AUG(1000, 5000, 3000), PDO_FIXED(20000, 3000, 0),
};

static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);
static const struct emul *emul2 = EMUL_DT_GET(RTS5453P_NODE2);
static const struct device *dev = DEVICE_DT_GET(RTS5453P_NODE);
static const struct device *dev2 = DEVICE_DT_GET(RTS5453P_NODE2);

static void rts54xx_before_test(void *data)
{
	emul_pdc_reset(emul);
	emul_pdc_reset(emul2);
	emul_pdc_set_response_delay(emul, 0);
	if (IS_ENABLED(CONFIG_TEST_PDC_MESSAGE_TRACING)) {
		set_pdc_trace_msg_mocks();
	}

	zassert_ok(emul_pdc_idle_wait(emul));
}

static int emul_get_src_pdos(enum pdo_offset_t pdo_offset, uint8_t pdo_count,
			     uint32_t *pdos)
{
	return emul_pdc_get_pdos(emul, SOURCE_PDO, pdo_offset, pdo_count,
				 LPM_PDO, pdos);
}

static int emul_get_snk_pdos(enum pdo_offset_t pdo_offset, uint8_t pdo_count,
			     uint32_t *pdos)
{
	return emul_pdc_get_pdos(emul, SINK_PDO, pdo_offset, pdo_count, LPM_PDO,
				 pdos);
}

static int emul_set_src_pdos(enum pdo_offset_t pdo_offset, uint8_t pdo_count,
			     const uint32_t *pdos)
{
	return emul_pdc_set_pdos(emul, SOURCE_PDO, pdo_offset, pdo_count,
				 LPM_PDO, pdos);
}

static int emul_set_snk_pdos(enum pdo_offset_t pdo_offset, uint8_t pdo_count,
			     const uint32_t *pdos)
{
	return emul_pdc_set_pdos(emul, SINK_PDO, pdo_offset, pdo_count, LPM_PDO,
				 pdos);
}

ZTEST_SUITE(rts54xx, NULL, NULL, rts54xx_before_test, NULL, NULL);

ZTEST_USER(rts54xx, test_emul_reset)
{
	uint32_t pdos[PDO_OFFSET_MAX];

	/* Test source PDO reset values. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_get_src_pdos(PDO_OFFSET_0, 8, pdos));
	zassert_equal(pdos[0], RTS5453P_FIXED1_SRC);
	zassert_equal(pdos[1], RTS5453P_FIXED2_SRC);

	for (int i = 1; i < 7; i++) {
		zassert_equal(pdos[i + 1], 0);
	}

	/* Test sink PDO reset values. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_get_snk_pdos(PDO_OFFSET_0, 8, pdos));
	zassert_equal(pdos[0], RTS5453P_FIXED_SNK);
	zassert_equal(pdos[1], RTS5453P_BATT_SNK);
	zassert_equal(pdos[2], RTS5453P_VAR_SNK);

	for (int i = 3; i < 7; i++) {
		zassert_equal(pdos[i + 1], 0);
	}
}

ZTEST_USER(rts54xx, test_emul_pdos)
{
	uint32_t pdos[PDO_OFFSET_MAX];

	/* Port partner PDOs aren't currently supported. */
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1,
				     PARTNER_PDO, pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_0, 1,
				     PARTNER_PDO, pdos));

	/* Test PDO overflow. */
	zassert_not_ok(emul_set_src_pdos(PDO_OFFSET_1, 8, spr_pdos));
	zassert_not_ok(emul_set_snk_pdos(PDO_OFFSET_1, 8, spr_pdos));

	zassert_not_ok(emul_get_src_pdos(PDO_OFFSET_5, 8, pdos));
	zassert_not_ok(emul_get_snk_pdos(PDO_OFFSET_5, 8, pdos));

	/* Test that SPR PDOs can be placed in any offset. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_set_src_pdos(PDO_OFFSET_1, 7, spr_pdos));
	zassert_ok(emul_get_src_pdos(PDO_OFFSET_1, 7, pdos));
	zassert_ok(memcmp(pdos, spr_pdos, sizeof(uint32_t) * 7));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_set_snk_pdos(PDO_OFFSET_1, 7, spr_pdos));
	zassert_ok(emul_get_snk_pdos(PDO_OFFSET_1, 7, pdos));
	zassert_ok(memcmp(pdos, spr_pdos, sizeof(uint32_t) * 7));
}

ZTEST_USER(rts54xx, test_pdos)
{
	uint32_t pdos[PDO_OFFSET_MAX];
	int num_pdos = GET_PDOS_MAX_NUM;

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_set_src_pdos(PDO_OFFSET_1, 6, spr_pdos));

	/*
	 * This is implemented using the same underlying code as
	 * emul_pdc_get_pdos so we only need to do a basic test.
	 */
	memset(pdos, 0, sizeof(pdos));

	for (int i = PDO_OFFSET_1; i <= PDO_OFFSET_6; i += num_pdos) {
		if (i + num_pdos > PDO_OFFSET_6) {
			num_pdos = PDO_OFFSET_6 - i + 1;
		}
		/* UCSI GET_PDOS supports a maximum of 4 PDOs per
		 * request. */
		zassert_ok(pdc_get_pdos(dev, SOURCE_PDO, i, num_pdos, LPM_PDO,
					&pdos[i - 1]));
		k_sleep(K_MSEC(1000));
	}
	zassert_ok(memcmp(pdos, spr_pdos, 6));
}

ZTEST_USER(rts54xx, test_get_hw_config)
{
	struct pdc_hw_config_t config;
	struct i2c_dt_spec i2c_spec = I2C_DT_SPEC_GET(RTS5453P_NODE);

	zassert_not_ok(pdc_get_hw_config(dev, NULL));

	zassert_ok(pdc_get_hw_config(dev, &config));
	zassert_equal(config.bus_type, PDC_BUS_TYPE_I2C);
	zassert_equal(config.i2c.bus, i2c_spec.bus);
	zassert_equal(config.i2c.addr, i2c_spec.addr);
}

static volatile struct {
	const struct device *port_devs[NUM_PORTS];
	bool port_interrupt[NUM_PORTS];
} shared_cb_data;

static void ci_handler_cb(const struct device *cidev,
			  const struct pdc_callback *callback,
			  union cci_event_t cci_event)
{
	if (cci_event.vendor_defined_indicator) {
		for (int i = 0; i < NUM_PORTS; ++i) {
			if (shared_cb_data.port_devs[i] == cidev) {
				LOG_INF("Interrupt on port %d", i);
				shared_cb_data.port_interrupt[i] = true;
				break;
			}
		}
	}
}

bool port_interrupt(int port)
{
	return shared_cb_data.port_interrupt[port];
}

/* Validate IRQ handling for both happy and edge cases. */
ZTEST_USER(rts54xx, test_irq)
{
#define IRQ_TEST_TIMEOUT_MS (TEST_WAIT_FOR_INTERVAL_MS * 5)

	/* Set connector statuses for both ports to be disconnected. This test
	 * only cares about triggering an interrupt / callback, so don't
	 * inadvertently trigger other actions.
	 */
	union connector_status_t status1 = { .connect_status = 0 };
	union connector_status_t status2 = { .connect_status = 0 };
	struct capability_t unused_caps;
	struct pdc_callback ci_cb;

	shared_cb_data.port_devs[EMUL_PORT] = dev;
	shared_cb_data.port_devs[EMUL2_PORT] = dev2;
	for (int i = 0; i < NUM_PORTS; ++i) {
		shared_cb_data.port_interrupt[i] = false;
	}

	ci_cb.handler = ci_handler_cb;
	zassert_ok(pdc_add_ci_callback(dev, &ci_cb));
	zassert_ok(pdc_add_ci_callback(dev2, &ci_cb));

	/* Put driver in non-idle state and then queue interrupts. */
	emul_pdc_set_response_delay(emul, IRQ_TEST_TIMEOUT_MS);
	zassert_ok(pdc_get_capability(dev, &unused_caps));

	/* Trigger an interrupt but expect that we don't see interrupts until
	 * the command is completed.
	 */
	zassert_ok(emul_pdc_connect_partner(emul, &status1));
	zassert_ok(emul_pdc_connect_partner(emul2, &status2));
	zassert_false(TEST_WAIT_FOR((port_interrupt(EMUL_PORT) ||
				     port_interrupt(EMUL2_PORT)),
				    TEST_WAIT_FOR_INTERVAL_MS * 4));

	/* Let command complete. */
	k_sleep(K_MSEC(IRQ_TEST_TIMEOUT_MS * 2));

	/* Now interrupts should work. */
	zassert_true(TEST_WAIT_FOR((port_interrupt(EMUL_PORT) &&
				    port_interrupt(EMUL2_PORT)),
				   IRQ_TEST_TIMEOUT_MS));
}

/* UCSI command callback handler. */
void ucsi_cc_callback(const struct device *port, struct pdc_callback *cb,
		      union cci_event_t cci_event)
{
}
