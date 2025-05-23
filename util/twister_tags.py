#!/usr/bin/env vpython3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Twister tag management.

Script that contains definitions for Twister test tags while also checking
testcase.yaml files to see if they only used predefined tags.
"""

# [VPYTHON:BEGIN]
# python_version: "3.11"
# wheel: <
#   name: "infra/python/wheels/pyyaml-py3"
#   version: "version:5.3.1"
# >
# [VPYTHON:END]

import logging
import os
import sys

import preupload.lib
import yaml  # pylint: disable=import-error


TAG_TO_DESCRIPTION = {
    "common": "Directly test shared code in the ec/common dir",
    "mkbp": "Testing the MKBP (Matrix Keyboard Protocol) stack",
    "system": "Directly test functions in common/system.c or shim/src/system.c",
    "spi": "SPI related tests",
    "uart": "UART related tests",
}

SCRIPT_PATH = os.path.realpath(__file__)


def main(args):
    """List and/or validate testcase.yaml tags."""
    parser = preupload.lib.argument_parser()
    parser.add_argument("-l", "--list-tags", action="store_true")
    args = parser.parse_args()
    preupload.lib.populate_default_filenames(args)

    list_tags = args.list_tags
    validate_files = args.filename

    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger(__file__)

    if list_tags:
        logger.info("Listing defined Twister testcase.yaml test tags:")
        for tag, description in TAG_TO_DESCRIPTION.items():
            logger.info("%s=%s", tag, description)

    if validate_files:
        testcase_yamls = []

        bad_tag = False

        for validated_file in validate_files:
            if os.path.basename(validated_file) == "testcase.yaml":
                if validated_file.exists():
                    testcase_yamls.append(validated_file)

        def test_tags_generator(root):
            """Returns space separated tags denoted by a tags key"""
            if "tags" in root:
                yield from root["tags"]

            for val in root.values():
                if isinstance(val, dict):
                    yield from test_tags_generator(val)

        for testcase_yaml in testcase_yamls:
            logger.info("Validating test tags in %s", testcase_yaml)
            with open(testcase_yaml, encoding="utf-8") as yaml_fd:
                yaml_obj = yaml.safe_load(yaml_fd)
                for tag in test_tags_generator(yaml_obj):
                    if tag not in TAG_TO_DESCRIPTION:
                        bad_tag = True
                        # TODO(b/249280155) Suggest tag
                        logger.error(
                            f'Unrecognized tag: "{tag}" in {testcase_yaml}'
                            + f' define "{tag}" in {SCRIPT_PATH}',
                        )

        if bad_tag:
            sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
