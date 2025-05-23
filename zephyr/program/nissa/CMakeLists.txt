# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_include_directories(include)
if(NOT DEFINED CONFIG_BOARD_INTEL_ISH_5_4_1)
	zephyr_library_sources("src/common.c")
endif()
zephyr_library_sources_ifdef(CONFIG_NISSA_SUB_BOARD "src/sub_board.c")
zephyr_library_sources_ifdef(CONFIG_AP_PWRSEQ "src/board_power.c")

if(DEFINED CONFIG_BOARD_NIVVIKS)
	project(nivviks)
	zephyr_library_sources(
			"nivviks/src/led.c"
			"nivviks/src/form_factor.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "nivviks/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "nivviks/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "nivviks/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_NEREID OR DEFINED CONFIG_BOARD_NEREID_CX)
	project(nereid)
	zephyr_library_sources(
			"src/led.c"
			"nereid/src/hdmi.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "nereid/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "nereid/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_NOKRIS)
	project(nokris)
	zephyr_library_sources(
			"nokris/src/led.c"
			"nokris/src/form_factor.c"
			"nokris/src/hdmi.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "nokris/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "nokris/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "nokris/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_NAKTAL)
        project(naktal)
        zephyr_library_sources(
                        "naktal/src/led.c"
                        "naktal/src/hdmi.c"
        )
        zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "naktal/src/usbc.c")
        zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "naktal/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_CRAASK)
	zephyr_library_sources(
			"craask/src/fan.c"
			"craask/src/form_factor.c"
			"craask/src/keyboard.c"
			"craask/src/kb_backlight.c"
			"craask/src/led.c"
			"craask/src/thermal.c"
			"craask/src/touch_panel.c"
	)
	project(craask)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "craask/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "craask/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_PUJJO)
	project(pujjo)
	zephyr_library_sources(
			"pujjo/src/led.c"
			"pujjo/src/hdmi.c"
			"pujjo/src/form_factor.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "pujjo/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "pujjo/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "pujjo/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_XIVU)
	project(xivu)
	zephyr_library_sources(
			"xivu/src/form_factor.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "xivu/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "xivu/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_JOXER)
	project(joxer)
	zephyr_library_sources(
			"joxer/src/board.c"
			"joxer/src/led.c"
			"joxer/src/keyboard.c"
			"joxer/src/form_factor.c"
			"joxer/src/sub_board.c"
			"joxer/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "joxer/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "joxer/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "joxer/src/fan.c")
endif()
if(DEFINED CONFIG_BOARD_YAVIKS)
	project(yaviks)
	zephyr_library_sources(
			"yaviks/src/led.c"
			"yaviks/src/keyboard.c"
			"yaviks/src/board.c"
			"yaviks/src/sub_board.c"
			"yaviks/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "yaviks/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "yaviks/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "yaviks/src/fan.c")
endif()
if(DEFINED CONFIG_BOARD_YAVILLA)
	project(yavilla)
	zephyr_library_sources(
			"yavilla/src/led.c"
			"yavilla/src/keyboard.c"
			"yavilla/src/board.c"
			"yavilla/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "yavilla/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "yavilla/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "yavilla/src/fan.c")
endif()
if(DEFINED CONFIG_BOARD_YAVISTA)
	project(yavista)
	zephyr_library_sources(
			"yavista/src/led.c"
			"yavista/src/keyboard.c"
			"yavista/src/board.c"
			"yavista/src/sub_board.c"
			"yavista/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "yavista/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "yavista/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "yavista/src/fan.c")
endif()
if(DEFINED CONFIG_BOARD_XIVUR)
	project(xivur)
	zephyr_library_sources(
			"xivur/src/led.c"
			"xivur/src/board.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "xivur/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "xivur/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "xivur/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_ULDREN)
	project(uldren)
	zephyr_library_sources(
			"uldren/src/form_factor.c"
			"uldren/src/kb_backlight.c"
			"uldren/src/sub_board.c"
			"uldren/src/mp2964_update.c"
			"uldren/src/sensor.c"
			"uldren/src/usb_mux_config.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "uldren/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "uldren/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_GOTHRAX)
	project(gothrax)
	zephyr_library_sources(
			"gothrax/src/hdmi.c"
			"gothrax/src/usb_mux_config.c"
			"gothrax/src/form_factor.c"
			"gothrax/src/sub_board.c"
			"gothrax/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "gothrax/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "gothrax/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "gothrax/src/fan.c")
endif()
if(DEFINED CONFIG_BOARD_CRAASKOV)
	project(craaskov)
	zephyr_include_directories("craaskov/include")
	zephyr_library_sources(
			"craaskov/src/board.c"
			"craaskov/src/keyboard.c"
			"craaskov/src/hdmi.c"
			"craaskov/src/touch_panel.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "craaskov/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "craaskov/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"craaskov/src/keyboard.c"
	"craaskov/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_ORISA)
	project(orisa)
	zephyr_include_directories("orisa/include")
	zephyr_library_sources(
			"orisa/src/usbc.c"
			"orisa/src/sub_board.c"
			"orisa/src/touch_panel.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"orisa/src/keyboard.c"
	"orisa/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_QUANDISO)
	project(quandiso)
	zephyr_library_sources(
			"quandiso/src/board.c"
			"quandiso/src/sub_board.c"
			"quandiso/src/keyboard.c"
			"quandiso/src/led.c"
			"quandiso/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "quandiso/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "quandiso/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "quandiso/src/usbc.c")
endif()
if(DEFINED CONFIG_BOARD_QUANDISO2)
	project(quandiso2)
	zephyr_library_sources(
			"quandiso2/src/board.c"
			"quandiso2/src/sub_board.c"
			"quandiso2/src/keyboard.c"
			"quandiso2/src/led.c"
			"quandiso2/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "quandiso2/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "quandiso2/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "quandiso2/src/usbc.c")
endif()
if(DEFINED CONFIG_BOARD_PIRRHA)
	project(pirrha)
	zephyr_include_directories("pirrha/include")
	zephyr_library_sources(
			"pirrha/src/led.c"
			"pirrha/src/board.c"
			"pirrha/src/battery.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "pirrha/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "pirrha/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_ANRAGGAR)
	project(anraggar)
	zephyr_include_directories("anraggar/include")
	zephyr_library_sources(
			"anraggar/src/board.c"
			"anraggar/src/sub_board.c"
			"anraggar/src/prochot.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "anraggar/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "anraggar/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
			"anraggar/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_GLASSWAY)
	project(glassway)
	zephyr_library_sources(
			"glassway/src/sub_board.c"
			"glassway/src/kb_backlight.c"
			"glassway/src/board.c"
			"glassway/src/usb_mux_config.c"
			"glassway/src/keyboard.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "glassway/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "glassway/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "glassway/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_SUNDANCE)
	project(sundance)
	zephyr_library_sources(
			"sundance/src/led.c"
			"sundance/src/form_factor.c"
			"sundance/src/board.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "sundance/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "sundance/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_PUJJOGA)
	project(pujjoga)
	zephyr_library_sources(
			"pujjoga/src/board.c"
			"pujjoga/src/led.c"
			"pujjoga/src/hdmi.c"
			"pujjoga/src/sub_board.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "pujjoga/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "pujjoga/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_PUJJOGATWIN)
	project(pujjogatwin)
	zephyr_library_sources(
			"pujjogatwin/src/board.c"
			"pujjogatwin/src/led.c"
			"pujjogatwin/src/hdmi.c"
			"pujjogatwin/src/sub_board.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "pujjogatwin/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "pujjogatwin/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_RIVEN)
	zephyr_include_directories("riven/include")
	zephyr_library_sources(
			"riven/src/form_factor.c"
			"riven/src/led.c"
			"riven/src/touch_panel.c"
			"riven/src/usb_mux_config.c"
	)
	project(riven)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "riven/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "riven/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
	"riven/src/keyboard.c"
	"riven/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_DOMIKA)
	project(domika)
	zephyr_library_sources(
			"domika/src/led.c"
			"domika/src/keyboard.c"
			"domika/src/board.c"
			"domika/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "domika/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "domika/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "domika/src/fan.c")
endif()
if(DEFINED CONFIG_BOARD_TELIKS)
	project(teliks)
	zephyr_include_directories("teliks/include")
	zephyr_library_sources(
			"teliks/src/board.c"
			"teliks/src/sub_board.c"
			"teliks/src/prochot.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "teliks/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "teliks/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
			"teliks/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_RULL)
	project(rull)
	zephyr_include_directories("rull/include")
	zephyr_library_sources(
			"rull/src/board.c"
			"rull/src/sub_board.c"
			"rull/src/usb_mux_config.c"
			"rull/src/board_touchpanal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "rull/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "rull/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
			"rull/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_TELITH)
	project(telith)
	zephyr_include_directories("telith/include")
	zephyr_library_sources(
			"telith/src/board_als.c"
			"telith/src/board.c"
			"telith/src/prochot.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "telith/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "telith/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
			"telith/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_PUJJONIRU)
	project(pujjoniru)
	zephyr_include_directories("pujjoniru/include")
	zephyr_library_sources(
			"pujjoniru/src/board.c"
			"pujjoniru/src/board_touchpanel.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "pujjoniru/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "pujjoniru/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_KEYBOARD_CUSTOMIZATION
			"pujjoniru/src/keyboard_customization.c"
	)
endif()
if(DEFINED CONFIG_BOARD_GUREN)
	project(guren)
	zephyr_library_sources(
			"guren/src/sub_board.c"
			"guren/src/kb_backlight.c"
			"guren/src/board.c"
			"guren/src/usb_mux_config.c"
			"guren/src/keyboard.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "guren/src/fan.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "guren/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "guren/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_INTEL_ISH_5_4_1)
	project(orisa-ish)
	add_subdirectory(orisa-ish)
endif()
if(DEFINED CONFIG_BOARD_DIRKS)
	project(dirks)
	zephyr_include_directories("dirks/include")
	zephyr_library_sources(
			"dirks/src/barrel_jack.c"
			"dirks/src/led.c"
			"dirks/src/hdmi.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "dirks/src/usbc.c")
endif()
if(DEFINED CONFIG_BOARD_MELIKS)
	project(meliks)
	zephyr_include_directories("meliks/include")
	zephyr_library_sources(
			"meliks/src/led.c"
			"meliks/src/board.c"
			"meliks/src/battery.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "meliks/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "meliks/src/charger.c")
endif()
if(DEFINED CONFIG_BOARD_EPIC)
	project(epic)
	zephyr_library_sources(
			"epic/src/hdmi.c"
			"epic/src/usb_mux_config.c"
			"epic/src/form_factor.c"
			"epic/src/sub_board.c"
			"epic/src/thermal.c"
	)
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "epic/src/usbc.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_CHARGER "epic/src/charger.c")
	zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_FAN "epic/src/fan.c")
endif()
