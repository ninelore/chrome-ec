# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

cros_ec_library_include_directories(include)

# Include selected EC source from the baseboard
zephyr_library_sources(
	"src/board.c"
	"src/hibernate.c"
)

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "src/usbc.c")
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_USBC "src/usb_pd_policy.c")
zephyr_library_sources_ifdef(CONFIG_VARIANT_CORSOLA_DB_DETECTION
			     "src/variant_db_detection.c")
zephyr_library_sources_ifndef(CONFIG_CORSOLA_BOARD_CHIPSET_CUSTOM "src/board_chipset.c")

if(DEFINED CONFIG_BOARD_KRABBY)
	project(krabby)
	add_subdirectory(krabby)
elseif(DEFINED CONFIG_BOARD_KINGLER)
	project(kingler)
	add_subdirectory(kingler)
elseif(DEFINED CONFIG_BOARD_KYOGRE)
	project(kyogre)
	add_subdirectory(kyogre)
elseif(DEFINED CONFIG_BOARD_STARMIE)
	project(starmie)
	add_subdirectory(starmie)
elseif(DEFINED CONFIG_BOARD_STEELIX)
	project(steelix)
	add_subdirectory(steelix)
elseif(DEFINED CONFIG_BOARD_TENTACRUEL)
	project(tentacruel)
	add_subdirectory(tentacruel)
elseif(DEFINED CONFIG_BOARD_MAGIKARP)
	project(magikarp)
	add_subdirectory(magikarp)
elseif(DEFINED CONFIG_BOARD_VOLTORB)
	project(voltorb)
	add_subdirectory(voltorb)
elseif(DEFINED CONFIG_BOARD_PONYTA)
	project(ponyta)
	add_subdirectory(ponyta)
elseif(DEFINED CONFIG_BOARD_CHINCHOU)
	project(chinchou)
	add_subdirectory(chinchou)
elseif(DEFINED CONFIG_BOARD_WOOBAT)
	project(woobat)
	add_subdirectory(woobat)
elseif(DEFINED CONFIG_BOARD_WUGTRIO)
	project(wugtrio)
	add_subdirectory(wugtrio)
elseif(DEFINED CONFIG_BOARD_SKITTY)
	project(skitty)
	add_subdirectory(skitty)
elseif(DEFINED CONFIG_BOARD_VELUZA)
	project(veluza)
	add_subdirectory(veluza)
elseif(DEFINED CONFIG_BOARD_SQUIRTLE)
	project(squirtle)
	add_subdirectory(squirtle)
elseif(DEFINED CONFIG_BOARD_WYRDEER)
	project(wyrdeer)
	add_subdirectory(wyrdeer)
endif()
