/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_vivaldi_kbd

#include <zephyr/device.h>
#include <zephyr/init.h>

#include <drivers/vivaldi_kbd.h>
#include <ec_commands.h>
#include <keyboard_8042_sharedlib.h>
#include <keyboard_protocol.h>
#include <keyboard_scan.h>
#if CONFIG_INPUT_KBD_MATRIX
#include <zephyr/input/input_kbd_matrix.h>
#endif
#include <zephyr/logging/log.h>

#include <dt-bindings/vivaldi_kbd.h>
#include <hooks.h>
#include <host_command.h>
#include <keyboard_config.h>

LOG_MODULE_REGISTER(vivaldi_kbd, LOG_LEVEL_ERR);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
	     "Exactly one instance of cros-ec,vivaldi-kbd should be defined.");

static const uint16_t action_scancodes[] = {
	[TK_BACK] = SCANCODE_BACK,
	[TK_FORWARD] = SCANCODE_FORWARD,
	[TK_REFRESH] = SCANCODE_REFRESH,
	[TK_FULLSCREEN] = SCANCODE_FULLSCREEN,
	[TK_OVERVIEW] = SCANCODE_OVERVIEW,
	[TK_VOL_MUTE] = SCANCODE_VOLUME_MUTE,
	[TK_VOL_DOWN] = SCANCODE_VOLUME_DOWN,
	[TK_VOL_UP] = SCANCODE_VOLUME_UP,
	[TK_PLAY_PAUSE] = SCANCODE_PLAY_PAUSE,
	[TK_NEXT_TRACK] = SCANCODE_NEXT_TRACK,
	[TK_PREV_TRACK] = SCANCODE_PREV_TRACK,
	[TK_SNAPSHOT] = SCANCODE_SNAPSHOT,
	[TK_BRIGHTNESS_DOWN] = SCANCODE_BRIGHTNESS_DOWN,
	[TK_BRIGHTNESS_UP] = SCANCODE_BRIGHTNESS_UP,
	[TK_KBD_BKLIGHT_DOWN] = SCANCODE_KBD_BKLIGHT_DOWN,
	[TK_KBD_BKLIGHT_UP] = SCANCODE_KBD_BKLIGHT_UP,
	[TK_PRIVACY_SCRN_TOGGLE] = SCANCODE_PRIVACY_SCRN_TOGGLE,
	[TK_MICMUTE] = SCANCODE_MICMUTE,
	[TK_KBD_BKLIGHT_TOGGLE] = SCANCODE_KBD_BKLIGHT_TOGGLE,
	[TK_MENU] = SCANCODE_MENU,
	[TK_DICTATE] = SCANCODE_DICTATE,
	[TK_ACCESSIBILITY] = SCANCODE_ACCESSIBILITY,
	[TK_DONOTDISTURB] = SCANCODE_DONOTDISTURB,
};
BUILD_ASSERT(ARRAY_SIZE(action_scancodes) == TK_COUNT);

#define VIVALDI_KEY_INIT(node_id, prop, idx)                             \
	{                                                                \
		.row = (DT_PROP_BY_IDX(node_id, prop, idx) >> 8) & 0xff, \
		.col = DT_PROP_BY_IDX(node_id, prop, idx) & 0xff,        \
	},

static const struct {
	uint8_t row;
	uint8_t col;
} vivaldi_keys[] = { DT_INST_FOREACH_PROP_ELEM(0, vivaldi_keys,
					       VIVALDI_KEY_INIT) };

/* If cros_ec_boot_keys exists and is okay, validate that the vivaldi key
 * TK_REFRESH is the same keycode as boot-keys.refresh-rc.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(cros_ec_boot_keys)
#define BOOT_KEY_VALIDATE(node_id, prop, idx)                               \
	BUILD_ASSERT(TK_REFRESH != DT_PROP_BY_IDX(node_id, prop, idx) ||    \
			     DT_INST_PROP_BY_IDX(0, vivaldi_keys, idx) ==   \
				     DT_PROP(DT_INST(0, cros_ec_boot_keys), \
					     refresh_rc),                   \
		     "Vivaldi TK_REFRESH key must match boot-key.refresh-rc");
#else
#define BOOT_KEY_VALIDATE(node_id, prop, idx)
#endif

/* If KEYBOARD_ROW_REFRESH exists, verify that the vivaldi key TK_REFRESH is
 * on the correct row.
 */
#ifdef KEYBOARD_ROW_REFRESH
#define REFRESH_ROW_VALIDATE(node_id, prop, idx)                              \
	BUILD_ASSERT(TK_REFRESH != DT_PROP_BY_IDX(node_id, prop, idx) ||      \
			     (KBD_RC_ROW(DT_INST_PROP_BY_IDX(0, vivaldi_keys, \
							     idx)) ==         \
				      KEYBOARD_ROW_REFRESH &&                 \
			      KBD_RC_COL(DT_INST_PROP_BY_IDX(0, vivaldi_keys, \
							     idx)) ==         \
				      KEYBOARD_COL_REFRESH),                  \
		     "Vivaldi TK_REFRESH key must match \
KEYBOARD_ROW_REFRESH/KEYBOARD_COL_REFRESH");
#else
#define REFRESH_ROW_VALIDATE(node_id, prop, idx)
#endif

#define KEYBD_CONFIG_VALIDATE(node_id)                                  \
	BUILD_ASSERT(IN_RANGE(DT_PROP_LEN(node_id, vivaldi_codes),      \
			      MIN_TOP_ROW_KEYS, MAX_TOP_ROW_KEYS),      \
		     "invalid number of codes specified");              \
	DT_FOREACH_PROP_ELEM(node_id, vivaldi_codes, BOOT_KEY_VALIDATE) \
	DT_FOREACH_PROP_ELEM(node_id, vivaldi_codes, REFRESH_ROW_VALIDATE)
DT_INST_FOREACH_CHILD(0, KEYBD_CONFIG_VALIDATE)

#define NODE_SUM_ONE(fn) 1 +
#define VIVALDI_CONFIG_COUNT (DT_INST_FOREACH_CHILD(0, NODE_SUM_ONE) 0)

#define KEYBD_CONFIG_INIT(node_id)                                       \
	[DT_NODE_CHILD_IDX(node_id)] = {                                 \
		.action_keys = DT_PROP(node_id, vivaldi_codes),          \
		.num_top_row_keys = DT_PROP_LEN(node_id, vivaldi_codes), \
		.capabilities = DT_PROP(node_id, capabilities),          \
	},

static const struct ec_response_keybd_config keybd_configs[] = {
	DT_INST_FOREACH_CHILD(0, KEYBD_CONFIG_INIT)
};

#if VIVALDI_CONFIG_COUNT > 1
static int8_t vivaldi_kbd_active_config_idx = -1;
#else
static const int8_t vivaldi_kbd_active_config_idx;
#endif

static enum ec_status
get_vivaldi_keybd_config(struct host_cmd_handler_args *args)
{
	struct ec_response_keybd_config *resp = args->response;

	if (vivaldi_kbd_active_config_idx < 0) {
		LOG_ERR("no active keybd config");
		return EC_RES_ERROR;
	}

	memcpy(resp, &keybd_configs[vivaldi_kbd_active_config_idx],
	       sizeof(*resp));
	args->response_size = sizeof(*resp);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_KEYBD_CONFIG, get_vivaldi_keybd_config,
		     EC_VER_MASK(0));

#define CROS_EC_KEYBOARD_NODE DT_CHOSEN(cros_ec_keyboard)

static struct {
	uint8_t row;
	uint8_t col;
} vivaldi_kbd_vol_up = { UINT8_MAX, UINT8_MAX };

bool vivaldi_kbd_is_vol_up(uint8_t row, uint8_t col)
{
	return row == vivaldi_kbd_vol_up.row && col == vivaldi_kbd_vol_up.col;
}

static void vivaldi_kbd_init(void)
{
	const struct ec_response_keybd_config *keybd_config;

#if VIVALDI_CONFIG_COUNT > 1
	int8_t config_idx;

	config_idx = board_vivaldi_keybd_idx();
	if (config_idx < 0) {
		LOG_ERR("top row not enabled");
		return;
	} else if (config_idx >= ARRAY_SIZE(keybd_configs)) {
		LOG_ERR("invalid keybd config index: %d", config_idx);
		return;
	}

	vivaldi_kbd_active_config_idx = config_idx;
#endif

	keybd_config = &keybd_configs[vivaldi_kbd_active_config_idx];

	LOG_INF("config: %d top keys: %d", vivaldi_kbd_active_config_idx,
		keybd_config->num_top_row_keys);

	for (uint8_t i = 0; i < ARRAY_SIZE(vivaldi_keys); i++) {
		uint8_t row;
		uint8_t col;
		enum action_key key;

		if (i >= keybd_config->num_top_row_keys) {
			break;
		}

		key = keybd_config->action_keys[i];
		if (key == TK_ABSENT) {
			continue;
		}

		col = vivaldi_keys[i].col;
		row = vivaldi_keys[i].row;

#if defined(CONFIG_PLATFORM_EC_KEYBOARD_CROS_EC_RAW_KB) || defined(CONFIG_ZTEST)
		keyscan_config.actual_key_mask[col] |= BIT(row);
#endif

#if CONFIG_INPUT_KBD_MATRIX && DT_NODE_EXISTS(CROS_EC_KEYBOARD_NODE)
		input_kbd_matrix_actual_key_mask_set(
			DEVICE_DT_GET(CROS_EC_KEYBOARD_NODE), row, col, true);
#endif

		set_scancode_set2(row, col, action_scancodes[key]);

		if (key == TK_VOL_UP) {
			vivaldi_kbd_vol_up.row = row;
			vivaldi_kbd_vol_up.col = col;
#if defined(CONFIG_PLATFORM_EC_KEYBOARD_CROS_EC_RAW_KB) || defined(CONFIG_ZTEST)
			set_vol_up_key(row, col);
#endif
		}
	}
}
DECLARE_HOOK(HOOK_INIT, vivaldi_kbd_init, HOOK_PRIO_DEFAULT);
