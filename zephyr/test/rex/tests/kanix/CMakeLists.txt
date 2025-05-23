# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/include")

target_sources_ifdef(CONFIG_TEST_PROJECT_USB_CONFIG app PRIVATE src/usb_config.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_USB_CONFIG app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/usb_mux_config.c
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/usbc_config.c)

target_sources_ifdef(CONFIG_TEST_PROJECT_KEYBOARD app PRIVATE src/keyboard.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_KEYBOARD app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/keyboard.c)

target_sources_ifdef(CONFIG_TEST_PROJECT_KEYBOARD_CUSTOMIZATION app PRIVATE
	src/keyboard_customization.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_KEYBOARD_CUSTOMIZATION app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/keyboard_customization.c)

target_sources_ifdef(CONFIG_TEST_PROJECT_CHARGER app PRIVATE src/charger.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_CHARGER app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/charger.c)

target_sources_ifdef(CONFIG_TEST_PROGRAM_I2C_POLICY app PRIVATE src/i2c_policy.c)
target_sources_ifdef(CONFIG_TEST_PROGRAM_I2C_POLICY app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/i2c_policy.c)

target_sources_ifdef(CONFIG_TEST_PROJECT_TOUCH_PANEL app PRIVATE src/touch_panel.c)
target_sources_ifdef(CONFIG_TEST_PROJECT_TOUCH_PANEL app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/touch_panel.c)

target_sources_ifdef(CONFIG_TEST_BOARD_FAN_CUSTOM app PRIVATE src/fan.c)
target_sources_ifdef(CONFIG_TEST_BOARD_FAN_CUSTOM app PRIVATE
	${PLATFORM_EC_PROGRAM_DIR}/rex/kanix/src/thermal.c)
