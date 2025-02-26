#!/usr/bin/env -S python3 -u
# -*- coding: utf-8 -*-
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide coreboot-sdk-portage-deps to callers

Parse the portage deps from the coreboot-sdk and return a dict containing
the results.
"""

import argparse
import pathlib
import subprocess
from typing import List, Optional


workspace = pathlib.Path(__file__).resolve().parent.parent.parent.parent

DEFAULT_ECLASS = workspace / (
    "third_party/chromiumos-overlay/"
    "eclass/coreboot-sdk-ec-dependencies.eclass"
)


def get_portage_deps(eclass_path=DEFAULT_ECLASS):
    """Obtain the list of dependencies from the eclass.

    Args:
        eclass_path: Path to the eclass to parse
        output: Path of file to output

    Returns:
        Dict of architectures and the desired version/hash
    """
    run_result = subprocess.run(
        [
            "bash",
            "-c",
            f"EAPI=7; source {eclass_path};"
            ' for key in "${!COREBOOT_SDK_VERSIONS[@]}"; do'
            ' echo "${key}=${COREBOOT_SDK_VERSIONS["${key}"]}";'
            " done",
        ],
        stdout=subprocess.PIPE,
        check=True,
    )
    try:
        return {
            key: tuple(value.split("/"))
            for key, value in [
                line.split("=", 1)
                for line in run_result.stdout.strip().decode().splitlines()
            ]
        }
    except UnicodeDecodeError:
        print(f"Unable to decode the portage deps: {run_result}")
        return {}


def main(argv: Optional[List[str]] = None):
    """Main calling function for the script"""
    parser = argparse.ArgumentParser(description="coreboot_sdk")
    parser.add_argument(
        "--eclass",
        help="Eclass to parse and generate toolchains from",
        default=DEFAULT_ECLASS,
    )
    opts = parser.parse_args(argv)
    toolchains = get_portage_deps(opts.eclass)

    for arch, (version, name) in toolchains.items():
        print(f"{arch} {version} {name}")


if __name__ == "__main__":
    main()
