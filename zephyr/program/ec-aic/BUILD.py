# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""EC-AIC projects."""


def register_ite_project(
    project_name,
):
    """Register an ITE variant of ec-aic."""

    return register_binman_project(
        project_name=project_name,
        zephyr_board="it8xxx2/it82002aw",
        dts_overlays=[
            here / project_name / "project.overlay",
        ],
        kconfig_files=[
            # Common to all projects.
            here / "program.conf",
            # Project-specific KConfig customization.
            here / project_name / "project.conf",
        ],
        inherited_from=["ec-aic"],
    )


aic_ite = register_ite_project(
    project_name="ite-aic",
)

assert_rw_fwid_DO_NOT_EDIT(project_name="ite-aic", addr=0x60098)
