# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

target_sources_ifdef(CONFIG_TEST_BOARD_USB_MUX_CONFIG app PRIVATE
        ${PLATFORM_EC_PROGRAM_DIR}/skyrim/${CONFIG_TEST_BOARD_NAME}/src/usb_mux_config.c
)
target_sources_ifdef(CONFIG_TEST_BOARD_KEYBOARD app PRIVATE
	src/keyboard.c
        ${PLATFORM_EC_PROGRAM_DIR}/skyrim/${CONFIG_TEST_BOARD_NAME}/src/keyboard.c
)
