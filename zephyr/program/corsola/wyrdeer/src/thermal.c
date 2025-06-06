/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "power.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

static enum {
	TEMP_ZONE_0, /* not limit */
	TEMP_ZONE_1, /* 2000mA */
	TEMP_ZONE_2, /* 500mA */
} temp_zone = TEMP_ZONE_0;

#define TEMP_ZONE_1_CURRENT_LIMIT 2000 /* mA */
#define TEMP_ZONE_2_CURRENT_LIMIT 500 /* mA */

#define COL_NUM 3
#define ROW_NUM 2

#define TEMP_MAX_COUNT 3

/*
 * ROW is 0 ,which is the matrix of temperature increase.
 * ROW is 1 ,which is the matrix of temperature decrease.
 */
static int thermals[5], time[ROW_NUM][COL_NUM];
static int thermal_cyc;
static int current = -1;
static int charger_temp_ave_bef, charger_temp_ave;

/*
 * Except time[exceptrow][exceptcol], everything else is cleared to 0
 * Used to record the number of times the temperature reaches a certain
 * level three times in a row.
 */
static void clear_remaining_array(int arr[][COL_NUM], int row, int exceptrow,
				  int exceptcol)
{
	int i, j;

	time[exceptrow][exceptcol]++;

	for (i = 0; i < row; i++) {
		if (i != exceptrow) {
			for (j = 0; j < COL_NUM; j++) {
				if (j != exceptcol) {
					arr[i][j] = 0;
				}
			}
		}
	}
}

/* Called by hook task every hook second (1 sec) */
static void average_tempature(void)
{
	int charger_temp, charger_temp_c;
	int charger_temp_sum = 0;
	static int temperature_increase;

	/*
	 * Keep track of battery temperature range:
	 *
	 *     ZONE_0  ZONE_1   ZONE_2
	 * --->------>-------->--- Temperature (C)
	 *    0      53       65
	 *     ZONE_0  ZONE_1   ZONE_2
	 * ---<------<--------<--- Temperature (C)
	 *    0      48        60
	 */
	temp_sensor_read(
		TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(charger_bc12_port1)),
		&charger_temp);

	charger_temp_c = K_TO_C(charger_temp);

	thermals[thermal_cyc] = charger_temp_c;
	thermal_cyc = (thermal_cyc + 1) % 5;
	for (int i = 0; i < 5; i++)
		charger_temp_sum += thermals[i];

	charger_temp_ave_bef = charger_temp_ave;
	charger_temp_ave = (charger_temp_sum + 2.5) / 5;

	if ((charger_temp_ave - charger_temp_ave_bef) > 0) {
		temperature_increase = 1;
	} else if ((charger_temp_ave - charger_temp_ave_bef) < 0) {
		temperature_increase = 0;
	}

	if (thermals[4]) {
		if (temperature_increase) {
			if (charger_temp_ave >= 65 && temp_zone <= TEMP_ZONE_2)
				clear_remaining_array(time, ROW_NUM, 0, 2);
			else if (charger_temp_ave >= 53 &&
				 temp_zone <= TEMP_ZONE_1)
				clear_remaining_array(time, ROW_NUM, 0, 1);
		} else {
			if (charger_temp_ave < 48 && temp_zone >= TEMP_ZONE_1)
				clear_remaining_array(time, ROW_NUM, 1, 0);
			else if (charger_temp_ave < 60 &&
				 temp_zone >= TEMP_ZONE_2)
				clear_remaining_array(time, ROW_NUM, 1, 1);
		}
	}

	for (int i = 0; i < COL_NUM; i++) {
		if (time[0][i] == TEMP_MAX_COUNT) {
			temp_zone = i;
			time[0][i] = 0;
		} else if (time[1][i] == TEMP_MAX_COUNT) {
			temp_zone = i;
			time[1][i] = 0;
		}
	}

	switch (temp_zone) {
	case TEMP_ZONE_0:
		/* No current limit */
		current = -1;
		break;
	case TEMP_ZONE_1:
		current = TEMP_ZONE_1_CURRENT_LIMIT;
		break;
	case TEMP_ZONE_2:
		current = TEMP_ZONE_2_CURRENT_LIMIT;
		break;
	}
}
DECLARE_HOOK(HOOK_SECOND, average_tempature, HOOK_PRIO_DEFAULT);

int charger_profile_override(struct charge_state_data *curr)
{
	enum power_state chipset_state;
	/*
	 * Precharge must be executed when communication is failed on
	 * dead battery.
	 */
	if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return -1;

	/* Don't charge if outside of allowable temperature range */
	if (current == 0) {
		curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
		if (curr->state != ST_DISCHARGE)
			curr->state = ST_IDLE;
	}

	/* This policy only execute in state s0 */
	chipset_state = power_get_state();

	if (chipset_state != POWER_S0)
		return 0;

	if (current >= 0)
		curr->requested_current = MIN(curr->requested_current, current);

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
