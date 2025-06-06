# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Define zmake projects for nissa."""

# Nivviks and Craask, Pujjo, Xivu, Xivur, Uldren has NPCX993F, Nereid
# and Joxer, Yaviks, Yavilla, Yavista, Quandiso, Quandiso2, Domika has
# ITE81302


def register_nissa_project(
    project_name,
    chip="it8xxx2/it81302bx",
    kconfig_files=None,
    modules=None,
):
    """Register a variant of nissa."""
    kwargs = {}
    if modules:
        kwargs["modules"] = modules

    register_func = register_binman_project
    if chip.startswith("npcx"):
        register_func = register_npcx_project

    chip_kconfig = {"it8xxx2/it81302bx": "it8xxx2", "npcx9/npcx9m3f": "npcx"}[
        chip
    ]

    if kconfig_files is None:
        kconfig_files = [
            here / "program.conf",
            here / f"{chip_kconfig}_program.conf",
            here / project_name / "project.conf",
        ]

    return register_func(
        project_name=project_name,
        zephyr_board=chip,
        dts_overlays=[here / project_name / "project.overlay"],
        kconfig_files=kconfig_files,
        inherited_from=["nissa"],
        supported_toolchains=["coreboot-sdk", "zephyr"],
        **kwargs,
    )


def register_nivviks_project(
    project_name,
):
    """Wrapper function for registering a variant of nivviks."""
    return register_nissa_project(
        project_name=project_name,
        chip="npcx9/npcx9m3f",
    )


def register_nereid_project(
    project_name,
):
    """Wrapper function for registering a variant of nereid."""
    return register_nissa_project(
        project_name=project_name,
        chip="it8xxx2/it81302bx",
    )


nivviks = register_nissa_project(
    project_name="nivviks",
    chip="npcx9/npcx9m3f",
)

nereid = register_nissa_project(
    project_name="nereid",
    chip="it8xxx2/it81302bx",
)

nereid_cx = register_binman_project(
    project_name="nereid_cx",
    zephyr_board="it8xxx2/it81302cx",
    dts_overlays=[here / "nereid" / "project.overlay"],
    kconfig_files=[
        here / "program.conf",
        here / "it8xxx2_program.conf",
        here / "it8xxx2cx_program.conf",
        here / "nereid" / "project.conf",
    ],
)

nokris = register_nissa_project(
    project_name="nokris",
    chip="npcx9/npcx9m3f",
)

naktal = register_nissa_project(
    project_name="naktal",
    chip="it8xxx2/it81302bx",
)

craask = register_nissa_project(
    project_name="craask",
    chip="npcx9/npcx9m3f",
)

pujjo = register_nissa_project(
    project_name="pujjo",
    chip="npcx9/npcx9m3f",
)

pujjoga = register_nissa_project(
    project_name="pujjoga",
    chip="npcx9/npcx9m3f",
)

pujjogatwin = register_nissa_project(
    project_name="pujjogatwin",
    chip="npcx9/npcx9m3f",
)

xivu = register_nissa_project(
    project_name="xivu",
    chip="npcx9/npcx9m3f",
)

xivur = register_nissa_project(
    project_name="xivur",
    chip="npcx9/npcx9m3f",
)

joxer = register_nissa_project(
    project_name="joxer",
    chip="it8xxx2/it81302bx",
)

yaviks = register_nissa_project(
    project_name="yaviks",
    chip="it8xxx2/it81302bx",
)

yavilla = register_nissa_project(
    project_name="yavilla",
    chip="it8xxx2/it81302bx",
)

yavista = register_nissa_project(
    project_name="yavista",
    chip="it8xxx2/it81302bx",
)

uldren = register_nissa_project(
    project_name="uldren",
    chip="npcx9/npcx9m3f",
)
gothrax = register_nissa_project(
    project_name="gothrax",
    chip="it8xxx2/it81302bx",
)
craaskov = register_nissa_project(
    project_name="craaskov",
    chip="npcx9/npcx9m3f",
)
orisa = register_nissa_project(
    project_name="orisa",
    chip="npcx9/npcx9m3f",
    kconfig_files=[
        here / "program.conf",
        here / "npcx_program.conf",
        here / "orisa" / "project.conf",
        here / "orisa.conf",
    ],
    modules=["ec", "cmsis", "pigweed", "nanopb"],
)

orisa_ti = register_nissa_project(
    project_name="orisa_ti",
    chip="npcx9/npcx9m3f",
    kconfig_files=[
        here / "program.conf",
        here / "npcx_program.conf",
        here / "orisa" / "project.conf",
        here / "orisa_ti" / "project.conf",
    ],
)
pirrha = register_nissa_project(
    project_name="pirrha",
    chip="it8xxx2/it81302bx",
)
quandiso = register_nissa_project(
    project_name="quandiso",
)
quandiso2 = register_nissa_project(
    project_name="quandiso2",
)
anraggar = register_nissa_project(
    project_name="anraggar",
    chip="it8xxx2/it81302bx",
)
glassway = register_nissa_project(
    project_name="glassway",
    chip="npcx9/npcx9m3f",
)
sundance = register_nissa_project(
    project_name="sundance",
    chip="npcx9/npcx9m3f",
)

riven = register_nissa_project(
    project_name="riven",
    chip="npcx9/npcx9m3f",
)

domika = register_nissa_project(
    project_name="domika",
    chip="it8xxx2/it81302bx",
)

teliks = register_nissa_project(
    project_name="teliks",
    chip="it8xxx2/it81302bx",
)

telith = register_nissa_project(
    project_name="telith",
    chip="it8xxx2/it81302bx",
)

register_ish_project(
    project_name="orisa-ish",
    zephyr_board="intel_ish_5_4_1",
    dts_overlays=[
        here / "orisa-ish" / "project.overlay",
    ],
    kconfig_files=[
        here / "orisa-ish" / "prj.conf",
        # Uncomment the following line for UART support
        # here / "orisa-ish" / "debug.conf",
        here / "orisa.conf",
    ],
    modules=["ec", "cmsis", "hal_intel_public", "pigweed", "nanopb"],
)

rull = register_nissa_project(
    project_name="rull",
    chip="it8xxx2/it81302bx",
)

pujjoniru = register_nissa_project(
    project_name="pujjoniru",
    chip="it8xxx2/it81302bx",
)

dirks = register_nissa_project(
    project_name="dirks",
    chip="it8xxx2/it81302bx",
)

guren = register_nissa_project(
    project_name="guren",
    chip="npcx9/npcx9m3f",
)

meliks = register_nissa_project(
    project_name="meliks",
    chip="npcx9/npcx9m3f",
)

epic = register_nissa_project(
    project_name="epic",
    chip="it8xxx2/it81302bx",
)

# Note for reviews, do not let anyone edit these assertions, the addresses
# must not change after the first RO release.
assert_rw_fwid_DO_NOT_EDIT(project_name="anraggar", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="craask", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="craaskov", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="dirks", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="orisa", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="orisa_ti", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="gothrax", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="joxer", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="naktal", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="nereid", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="nereid_cx", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="nivviks", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="nokris", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="pirrha", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="pujjo", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="pujjoga", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="pujjogatwin", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="quandiso", addr=0xB7FE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="quandiso2", addr=0xB7FE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="uldren", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="xivu", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="xivur", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="yaviks", addr=0xAFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="yavilla", addr=0xB7FE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="glassway", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="yavista", addr=0xAFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="sundance", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="riven", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="domika", addr=0xB7FE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="teliks", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="rull", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="telith", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="pujjoniru", addr=0xBFFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="guren", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="meliks", addr=0x7FFE0)
assert_rw_fwid_DO_NOT_EDIT(project_name="epic", addr=0xBFFE0)
