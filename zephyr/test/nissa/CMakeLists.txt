# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")
project(nissa)

add_subdirectory(${PLATFORM_EC}/zephyr/test/test_utils test_utils)

zephyr_include_directories(
        "${PLATFORM_EC_PROGRAM_DIR}/nissa/include"
        include)

target_sources(app PRIVATE src/stubs.c)

if(CONFIG_TEST_NISSA_BOARD_POWER)
    # Test source
    if(CONFIG_AP_PWRSEQ_DRIVER)
        target_sources(app PRIVATE src/board_ap_pwrseq_drv.c)
    else()
        target_sources(app PRIVATE src/board_power.c)
    endif()
    # Module under test
    target_sources(app PRIVATE ${PLATFORM_EC_PROGRAM_DIR}/nissa/src/board_power.c)
endif()

target_sources_ifdef(
        CONFIG_TEST_NISSA_SUB_BOARD
        app PRIVATE
        src/sub_board.c
        src/log_module.c
        ${PLATFORM_EC_PROGRAM_DIR}/nissa/src/sub_board.c)

target_sources_ifdef(
        CONFIG_TEST_NISSA_COMMON
        app PRIVATE
        src/common.c
        ${PLATFORM_EC_PROGRAM_DIR}/nissa/src/common.c)

target_sources_ifdef(
        CONFIG_TEST_NISSA_NIVVIKS
        app PRIVATE
        src/nivviks.c
        ${PLATFORM_EC_PROGRAM_DIR}/nissa/nivviks/src/fan.c
        ${PLATFORM_EC_PROGRAM_DIR}/nissa/nivviks/src/form_factor.c
        ${PLATFORM_EC_PROGRAM_DIR}/nissa/nivviks/src/led.c
)

if(CONFIG_TEST_NISSA_NIVVIKS_CHARGER)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
            app PRIVATE
            src/nivviks_charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nivviks/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nivviks/src/charger.c
    )
endif()

if(CONFIG_TEST_NISSA_NEREID)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
            app PRIVATE
            src/nereid.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nereid/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nereid/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nereid/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_NOKRIS)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0)
    target_sources(
            app PRIVATE
            src/nokris.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nokris/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nokris/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/nokris/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_SUNDANCE)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0)
    target_sources(
            app PRIVATE
            src/sundance.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/sundance/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/sundance/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/sundance/src/led.c
    )
endif()

if(CONFIG_TEST_NISSA_GOTHRAX)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
            app PRIVATE
            src/gothrax.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/usb_mux_config.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/form_factor.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/thermal.c
    )
endif()

target_sources_ifdef(
        CONFIG_TEST_GOTHRAX_SUB_BOARD
        app PRIVATE
        src/gothrax_sub_board.c
        src/log_module.c
        ${PLATFORM_EC_PROGRAM_DIR}/nissa/gothrax/src/sub_board.c)

if(CONFIG_TEST_NISSA_PUJJO)
    target_sources(
            app PRIVATE
            src/pujjo.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjo/src/form_factor.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjo/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjo/src/led.c
    )
endif()

if(CONFIG_TEST_NISSA_PUJJOGA)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0)
    target_sources(
            app PRIVATE
            src/pujjoga.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjoga/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjoga/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjoga/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjoga/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjoga/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjoga/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_PUJJOGATWIN)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0)
    target_sources(
            app PRIVATE
            src/pujjogatwin.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjogatwin/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjogatwin/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjogatwin/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjogatwin/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjogatwin/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pujjogatwin/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_JOXER)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
            app PRIVATE
            src/joxer.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/joxer/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/joxer/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/joxer/src/form_factor.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/joxer/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/joxer/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/joxer/src/sub_board.c
    )
endif()

if(CONFIG_TEST_NISSA_CRAASK)
    target_sources(
	    app PRIVATE
	    src/craask.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/charger.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/fan.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/form_factor.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/kb_backlight.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/keyboard.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/led.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/thermal.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/touch_panel.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craask/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_CRAASKOV)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/nissa/craaskov/include")
    target_sources(
	    app PRIVATE
	    src/craaskov.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craaskov/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/craaskov/src/hdmi.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craaskov/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/craaskov/src/touch_panel.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/craaskov/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_PIRRHA)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/nissa/pirrha/include")
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
	    app PRIVATE
	    src/pirrha.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/pirrha/src/charger.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/pirrha/src/led.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/pirrha/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pirrha/src/battery.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/pirrha/src/board.c
    )
endif()

if(CONFIG_TEST_NISSA_ANRAGGAR)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/nissa/anraggar/include")
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0)
    target_sources(
            app PRIVATE
            src/anraggar.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/anraggar/src/board.c
    )
endif()

if(CONFIG_TEST_NISSA_ULDREN)
    target_sources(
	    app PRIVATE
	    src/uldren.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/form_factor.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/kb_backlight.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/mp2964_update.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/sensor.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/usb_mux_config.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/uldren/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_YAVIKS)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    zephyr_include_directories("${PLATFORM_EC}/zephyr/shim/chip/it8xxx2/include")
    target_sources(
            app PRIVATE
            src/yaviks.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/thermal.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yaviks/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_QUANDISO)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    zephyr_include_directories("${PLATFORM_EC}/zephyr/shim/chip/it8xxx2/include")
    target_sources(
            app PRIVATE
            src/quandiso.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/thermal.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_QUANDISO2)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    zephyr_include_directories("${PLATFORM_EC}/zephyr/shim/chip/it8xxx2/include")
    target_sources(
            app PRIVATE
            src/quandiso2.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/thermal.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/quandiso2/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_YAVILLA)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    zephyr_include_directories("${PLATFORM_EC}/zephyr/shim/chip/it8xxx2/include")
    target_sources(
            app PRIVATE
            src/yavilla.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yavilla/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yavilla/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yavilla/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yavilla/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yavilla/src/thermal.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/yavilla/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_GLASSWAY)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
            app PRIVATE
            src/glassway.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/usb_mux_config.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/glassway/src/keyboard.c
    )
endif()

if(CONFIG_TEST_NISSA_RIVEN)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/include")
    target_sources(
	    app PRIVATE
	    src/riven.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/charger.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/form_factor.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/keyboard.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/keyboard_customization.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/touch_panel.c
	    ${PLATFORM_EC_PROGRAM_DIR}/nissa/riven/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_DOMIKA)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    zephyr_include_directories("${PLATFORM_EC}/zephyr/shim/chip/it8xxx2/include")
    target_sources(
            app PRIVATE
            src/domika.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/domika/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/domika/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/domika/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/domika/src/keyboard.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/domika/src/thermal.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/domika/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_TELIKS)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/nissa/teliks/include")
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0)
    target_sources(
            app PRIVATE
            src/teliks.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/teliks/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/teliks/src/keyboard_customization.c
    )
endif()

if(CONFIG_TEST_NISSA_DIRKS)
    zephyr_include_directories("${PLATFORM_EC_PROGRAM_DIR}/nissa/dirks/include")
    target_sources(
            app PRIVATE
            src/dirks.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/dirks/src/barrel_jack.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/dirks/src/led.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/dirks/src/hdmi.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/dirks/src/usbc.c
    )
endif()

if(CONFIG_TEST_NISSA_GUREN)
    target_compile_definitions(app PRIVATE CHARGER_PRIMARY=0 CHARGER_SECONDARY=1)
    target_sources(
            app PRIVATE
            src/guren.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/fan.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/sub_board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/usbc.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/charger.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/board.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/usb_mux_config.c
            ${PLATFORM_EC_PROGRAM_DIR}/nissa/guren/src/keyboard.c
    )
endif()
