# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources(ppm_common.c)
zephyr_library_sources_ifdef(CONFIG_UCSI_PPM_CONSOLE ppm_console.c)
zephyr_library_sources(ppm_driver.c)
zephyr_library_sources(ucsi_host_cmd.c)
