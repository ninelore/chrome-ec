# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_USBC_INTEL_ALTMODE intel_altmode.c)
zephyr_library_sources_ifdef(CONFIG_USBC_PDC_RTS54XX pdc_rts54xx.c ucsi_v3.c)
zephyr_library_sources_ifdef(CONFIG_USB_PDC_POWER_MGMT ucsi_v3.c)
zephyr_library_sources_ifdef(CONFIG_USBC_PDC_RTS54XX_CONSOLE_FW_UPDATER pdc_rts54xx_fwup.c)

if (CONFIG_USBC_PDC_TPS6699X)
zephyr_library_compile_definitions("TPS6699X_FW_ROOT=${PLATFORM_EC}/zephyr/drivers/usbc")
zephyr_library_sources(pdc_tps6699x.c tps6699x_cmd.c ucsi_v3.c)
zephyr_library_sources_ifdef(CONFIG_USBC_PDC_TPS6699X_FW_UPDATER tps6699x_fwup.c)
endif ()
