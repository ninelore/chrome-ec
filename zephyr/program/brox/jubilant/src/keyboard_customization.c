/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_cbi.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_config.h"
#include "keyboard_protocol.h"
#include "keyboard_raw.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(brox_keyboard, LOG_LEVEL_INF);

/* jubileum keyboard with numpad
 * G (Win key) : 30 (row:3, col: 0)
 * Fn          : 127(row:4, col:10)
 */
static uint16_t jubileum_scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ 0x0000, 0x0000, 0x0000, 0xe01f, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0078, 0x0076, 0x000d, 0x000e, 0x001c, 0x0016, 0x001a, 0x003c },
	{ 0x0005, 0x000c, 0x0004, 0x0006, 0x0023, 0x0041, 0x0026, 0x0043 },
	{ 0x0032, 0x0034, 0x002c, 0x002e, 0x002b, 0x0049, 0x0025, 0x0044 },
	{ 0x0009, 0x0083, 0x000b, 0x001b, 0x0003, 0x004a, 0x001e, 0x004d },
	{ 0x0031, 0x0007, 0x005b, 0x000f, 0x0042, 0x0021, 0x003e, 0x0015 },
	{ 0x0051, 0x0033, 0x0035, 0x004e, 0x003b, 0x0029, 0x0045, 0x001d },
	{ 0x0000, 0x0000, 0x0061, 0x0000, 0x0000, 0x0012, 0x0000, 0x0059 },
	{ 0x0055, 0x0052, 0x0054, 0x0036, 0x004c, 0x0022, 0x003d, 0x0024 },
	{ 0xe06c, 0x0001, 0xe071, 0x002f, 0x004b, 0x002a, 0x0046, 0x002d },
	{ 0xe011, 0x0000, 0x006a, 0x0000, 0x0037, 0x0000, 0x005d, 0x0000 },
	{ 0x0017, 0x0066, 0x000a, 0x005d, 0x005a, 0x003a, 0xe072, 0xe075 },
	{ 0x001f, 0x0064, 0xe07d, 0x0067, 0xe069, 0xe07a, 0xe074, 0xe06b },
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0011, 0x0000 },
	{ 0x0000, 0x0014, 0x0000, 0xe014, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0027, 0x0000 },
	{ 0xe04a, 0x007c, 0x007b, 0x0074, 0x0071, 0x0073, 0x006b, 0x0070 },
	{ 0x006c, 0x0075, 0x007d, 0x0079, 0x007a, 0x0072, 0x0069, 0xe05a },
};

/* jubilant keyboard without numpad
 * G (Win key) : 30 (row:3, col: 0)
 * Fn          : 127(row:0, col:16)
 */
static uint16_t jubilant_scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ 0x0000, 0x0000, 0x0000, 0xe01f, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0078, 0x0076, 0x000d, 0x000e, 0x001c, 0x0016, 0x001a, 0x003c },
	{ 0x0005, 0x000c, 0x0004, 0x0006, 0x0023, 0x0041, 0x0026, 0x0043 },
	{ 0x0032, 0x0034, 0x002c, 0x002e, 0x002b, 0x0049, 0x0025, 0x0044 },
	{ 0x0009, 0x0083, 0x000b, 0x001b, 0x0003, 0x004a, 0x001e, 0x004d },
	{ 0x0031, 0x0007, 0x005b, 0x000f, 0x0042, 0x0021, 0x003e, 0x0015 },
	{ 0x0051, 0x0033, 0x0035, 0x004e, 0x003b, 0x0029, 0x0045, 0x001d },
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0012, 0x0000, 0x0059 },
	{ 0x0055, 0x0052, 0x0054, 0x0036, 0x004c, 0x0022, 0x003d, 0x0024 },
	{ 0xe06c, 0x0001, 0xe071, 0x002f, 0x004b, 0x002a, 0x0046, 0x002d },
	{ 0xe011, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0017, 0x0066, 0x000a, 0x005d, 0x005a, 0x003a, 0xe072, 0xe075 },
	{ 0x001f, 0x0064, 0xe07d, 0x0067, 0xe069, 0xe07a, 0xe074, 0xe06b },
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0011, 0x0000 },
	{ 0x0000, 0x0014, 0x0000, 0xe014, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0027, 0x0000 },
	{ 0x0037, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x006a, 0x0000, 0x0000, 0x0000, 0x005d, 0x0061 },
};

/* Default set2 scan table with numpad designed */
static uint16_t (*scancode_set2)[KEYBOARD_ROWS] = jubileum_scancode_set2;

uint16_t get_scancode_set2(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		return scancode_set2[col][row];
	return 0;
}

void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		scancode_set2[col][row] = val;
}

static bool key_numpad = FW_KB_NUMERIC_PAD_ABSENT;
/*
 * Keyboard function decided by FW config.
 */
test_export_static void keyboard_matrix_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_NUMERIC_PAD, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d, "
			"assuming FW_KB_NUMERIC_PAD_PRESENT",
			FW_KB_NUMERIC_PAD);
		val = FW_KB_NUMERIC_PAD_PRESENT;
	}

	switch (val) {
	case FW_KB_NUMERIC_PAD_PRESENT:
		scancode_set2 = jubileum_scancode_set2;
		key_numpad = FW_KB_NUMERIC_PAD_PRESENT;
		LOG_INF("jubileum keyboard matrix");
		break;
	case FW_KB_NUMERIC_PAD_ABSENT:
		scancode_set2 = jubilant_scancode_set2;
		key_numpad = FW_KB_NUMERIC_PAD_ABSENT;
		LOG_INF("jubilant keyboard matrix");
		break;
	default:
		LOG_WRN("invalid cbi value: %x", val);
		return;
	}

	ret = cros_cbi_get_fw_config(FW_KB_TYPE, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_KB_TYPE);
		val = FW_KB_TYPE_DEFAULT;
	}

	if (val == FW_KB_TYPE_CA_FR) {
		if (key_numpad == FW_KB_NUMERIC_PAD_ABSENT) {
			/*
			 * Canadian French keyboard (US layout),
			 *   \| (key 45):     0x0061->0x61->0x56
			 *   r-ctrl (key 64): 0xe014->0x14->0x1d
			 * move key45 (row:7,col:17) to key64 (row:3,col:14)
			 */
			set_scancode_set2(3, 14, get_scancode_set2(7, 17));
		} else {
			/*
			 * Canadian French keyboard (US layout),
			 *   \| (key 45):     0x0061->0x61->0x56
			 *   r-ctrl (key 64): 0xe014->0x14->0x1d
			 * move key45 (row:2,col:7) to key64 (row:3,col:14)
			 */
			set_scancode_set2(3, 14, get_scancode_set2(2, 7));
		}
	}
}
DECLARE_HOOK(HOOK_INIT, keyboard_matrix_init, HOOK_PRIO_POST_FIRST);

#ifdef CONFIG_KEYBOARD_DEBUG
static uint8_t keycap_label[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_SEARC, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_F11, KLLI_ESC, KLLI_TAB, '~', 'a', '1', 'z', 'u' },
	{ KLLI_F1, KLLI_F4, KLLI_F3, KLLI_F2, 'd', ',', '3', 'i' },
	{ 'b', 'g', 't', '5', 'f', '.', '4', 'o' },
	{ KLLI_F10, KLLI_F7, KLLI_F6, 's', KLLI_F5, '/', '2', 'p' },
	{ 'n', KLLI_F12, ']', KLLI_F13, 'k', 'c', '8', 'q' },
	{ KLLI_UNKNO, 'h', 'y', '-', 'j', KLLI_SPACE, '0', 'w' },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_L_SHT, KLLI_UNKNO, KLLI_R_SHT },
	{ '=', '\'', '[', '6', ';', 'x', '7', 'e' },
	{ KLLI_UNKNO, KLLI_F9, KLLI_UNKNO, KLLI_UNKNO, 'l', 'v', '9',
	  'r' }, /*delete at 2*/
	{ KLLI_R_ALT, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_F14, KLLI_B_SPC, KLLI_F8, KLLI_UNKNO, KLLI_ENTER, 'm', KLLI_DOWN,
	  KLLI_UP },
	{ KLLI_F15, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_RIGHT, KLLI_LEFT },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_L_ALT, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
};

uint8_t get_keycap_label(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		return keycap_label[col][row];
	return KLLI_UNKNO;
}

void set_keycap_label(uint8_t row, uint8_t col, uint8_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		keycap_label[col][row] = val;
}
#endif
