# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

file(SHA256 ${INPUT_FILE} npcx_monitor_hash)

set(expected_hash "331629c64ef65caef1b23f0f29474f13d9b41447408a70b6e4260310801d324d")

if("${npcx_monitor_hash}" STREQUAL "${expected_hash}")
  message(STATUS "NPCX Monitor hash match")
else()
  message(STATUS "NPCX monitor expected hash ${expected_hash}")
  message(STATUS "NPCX monitor actual hash ${npcx_monitor_hash}")
  message(FATAL_ERROR "NPCX monitor change detected. See README.md for instructions.")
endif()
