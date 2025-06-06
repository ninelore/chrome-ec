/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "charge_state.h"
#include "common.h"
#include "driver/charger/rt9490.h"
#include "extpower.h"
#include "hooks.h"
#include "ite_hooks.h"
#include "temp_sensor/temp_sensor.h"

#define TEMP_BUFF_SIZE 60
#define KEEP_TIME 5

BUILD_ASSERT(IS_ENABLED(CONFIG_BOARD_VELUZA) || IS_ENABLED(CONFIG_TEST));

/* calculate current average temperature */
static int average_tempature(void)
{
	static int temp_history_buffer[TEMP_BUFF_SIZE];
	static int buff_ptr;
	static int temp_sum;
	static int past_temp;
	static int avg_temp;
	int cur_temp, t;

	temp_sensor_read(TEMP_SENSOR_ID(DT_NODELABEL(temp_charger)), &t);
	cur_temp = K_TO_C(t);
	past_temp = temp_history_buffer[buff_ptr];
	temp_history_buffer[buff_ptr] = cur_temp;
	temp_sum = temp_sum + temp_history_buffer[buff_ptr] - past_temp;
	buff_ptr++;
	if (buff_ptr >= TEMP_BUFF_SIZE) {
		buff_ptr = 0;
	}
	/* Calculate per minute temperature.
	 * It's expected low temperature when the first 60 seconds.
	 */
	avg_temp = temp_sum / TEMP_BUFF_SIZE;
	return avg_temp;
}
static int current_level;

struct current_table_struct {
	int temperature;
	int current;
};

/* Limit charging current table : 2554/1400/365
 * note this should be in descending order.
 */
static const struct current_table_struct current_table[] = {
	{ 0, 2554 },
	{ 55, 1400 },
	{ 57, 365 },
};

#define CURRENT_LEVELS ARRAY_SIZE(current_table)

/*
 * When the current average temperature is over the current_table setting
 * and the temperature is kept at 5 seconds, it will follow the thermal
 * table drop charging current to avoid skin over temperature.
 */
static void current_update(void)
{
	int temp;
	static uint8_t uptime;
	static uint8_t dntime;

	temp = average_tempature();
#ifndef CONFIG_TEST
	if (led_pwr_get_state() == LED_PWRS_DISCHARGE) {
		current_level = 0;
		uptime = 0;
		dntime = 0;
		return;
	}
#endif

	if ((current_level + 1) < CURRENT_LEVELS &&
	    temp >= current_table[current_level + 1].temperature) {
		/* Increase level */
		dntime = 0;
		if (uptime < KEEP_TIME) {
			uptime++;
		} else {
			uptime = 0;
			current_level++;
		}
	} else if (current_level != 0 &&
		   temp < current_table[current_level].temperature) {
		/* Decrease level */
		uptime = 0;
		if (dntime < KEEP_TIME) {
			dntime++;
		} else {
			dntime = 0;
			current_level--;
		}
	} else {
		uptime = 0;
		dntime = 0;
	}
}
DECLARE_HOOK(HOOK_SECOND, current_update, HOOK_PRIO_DEFAULT);

int charger_profile_override(struct charge_state_data *curr)
{
	/*
	 * Precharge must be executed when communication is failed on
	 * dead battery.
	 */

	if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return 0;
	if (curr->requested_current > current_table[current_level].current)
		curr->requested_current = current_table[current_level].current;
	return 0;
}

enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}

void board_rt9490_adc_enalbe(void)
{
	rt9490_enable_adc(CHARGER_SOLO, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_rt9490_adc_enalbe, HOOK_PRIO_DEFAULT);

void board_rt9490_adc_disable(void)
{
	rt9490_enable_adc(CHARGER_SOLO, extpower_is_present());
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_rt9490_adc_disable,
	     HOOK_PRIO_DEFAULT);

__override void board_rt9490_adc_control(void)
{
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		rt9490_enable_adc(CHARGER_SOLO, extpower_is_present());
}
