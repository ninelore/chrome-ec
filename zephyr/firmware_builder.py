#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build and test all of the Zephyr boards.

This is the entry point for the custom firmware builder workflow recipe.
"""

import collections
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys

from google.protobuf import json_format  # pylint: disable=import-error

from chromite.api.gen_sdk.chromite.api import firmware_pb2
from chromite.lib.chromeos_version import VersionInfo
import scripts.firmware_builder_lib


# Add the zmake dir early in the python search path
ZEPHYR_DIR = pathlib.Path(__file__).parent.resolve()
sys.path.insert(1, str(ZEPHYR_DIR / "zmake"))

# Add the util directory to the search path
sys.path.append(str(ZEPHYR_DIR / "../util"))

# pylint: disable=wrong-import-position, import-error
from coreboot_sdk import init_toolchain
import zmake.modules  # pylint: disable=wrong-import-position
import zmake.project  # pylint: disable=wrong-import-position


DEFAULT_BUNDLE_DIRECTORY = "/tmp/artifact_bundles"
DEFAULT_BUNDLE_METADATA_FILE = "/tmp/artifact_bundle_metadata"

# Boards that we want to track the coverage of our own files specifically.
SPECIAL_BOARDS = [
    "krabby",
    "skyrim",
    "kingler",
    "rex",
    "geralt",
    "roach",
    "ovis",
    # Brox variants
    "brox",
    "brox-sku4",
    "rauru",
    # Fatcat/Felino variants
    "fatcat_npcx9m7f",
    "fatcat_it82002aw",
    "felino",
    "francka",
    # Nissa variants
    "nereid",
    "nivviks",
    "orisa",
    "orisa-ish",
    # Trulo variants
    "trulo",
    "trulo-ti",
    "trulo-ish",
    # Skyrim variants
    "winterhold",
    "frostflow",
    "crystaldrift",
    "markarth",
]

BINARY_SIZE_REGIONS = [
    "RO_FLASH",
    "RO_RAM",
    "RO_ROM",
    "RW_FLASH",
    "RW_RAM",
    "RW_ROM",
]


def log_cmd(cmd, env=None, cwd=None):
    """Log subprocess command."""
    if cwd:
        print(f"cd {cwd};", end=" ")
    if env is not None:
        print("env", end=" ")
        [  # pylint:disable=expression-not-assigned
            print(key + "=" + shlex.quote(str(value)), end=" ")
            for key, value in env.items()
        ]
    print(" ".join(shlex.quote(str(x)) for x in cmd))
    sys.stdout.flush()


def find_checkout():
    """Find the path to the base of the checkout (e.g., ~/chromiumos)."""
    for path in pathlib.Path(__file__).resolve().parents:
        if (path / ".repo").is_dir():
            return path
    raise FileNotFoundError("Unable to locate the root of the checkout")


def get_version():
    """Determine the current chroot version."""
    ver = VersionInfo.from_repo(source_repo=find_checkout())
    if ver:
        return ver.VersionString()
    return None


def build(opts):
    """Builds all Zephyr firmware targets"""
    metric_list = firmware_pb2.FwBuildMetricList()  # pylint: disable=no-member
    env = os.environ.copy()
    env.update(init_toolchain())
    env.update(
        {
            "PYTHONPATH": str(ZEPHYR_DIR / "zmake"),
        }
    )

    platform_ec = ZEPHYR_DIR.parent
    modules = zmake.modules.locate_from_checkout(find_checkout())
    projects_path = zmake.modules.default_projects_dirs(modules)

    # Start with a clean build environment
    cmd = ["make", "clobber"]
    log_cmd(cmd)
    subprocess.run(
        cmd,
        cwd=platform_ec,
        check=True,
        stdin=subprocess.DEVNULL,
        env=env,
    )

    cmd = ["zmake", "-D", "build", "-a", "--static"]
    if opts.code_coverage:
        cmd.append("--coverage")
    if opts.bcs_version:
        cmd.extend(["-v", opts.bcs_version])
    else:
        version = get_version()
        if version:
            cmd.extend(["-v", version])

    log_cmd(cmd)
    subprocess.run(
        cmd,
        cwd=ZEPHYR_DIR,
        check=True,
        stdin=subprocess.DEVNULL,
        env=env,
    )
    if not opts.code_coverage:
        for project in zmake.project.find_projects(projects_path).values():
            build_dir = (
                platform_ec / "build" / "zephyr" / project.config.project_name
            )
            metric = metric_list.value.add()
            full_name = project.config.full_name.split(".")
            metric.target_name = full_name[-1]
            metric.platform_name = ".".join(full_name[:-1])
            for variant, _ in project.iter_builds():
                build_log = build_dir / f"build-{variant}" / "build.log"
                parse_buildlog(
                    build_log,
                    metric,
                    variant.upper(),
                    metric.target_name in SPECIAL_BOARDS,
                )
    if opts.metrics:
        with open(opts.metrics, "w", encoding="utf-8") as file:
            file.write(json_format.MessageToJson(metric_list))
    return 0


UNITS = {
    "B": 1,
    "KB": 1024,
    "MB": 1024 * 1024,
    "GB": 1024 * 1024 * 1024,
}


def _memory_region_is_valid(parts):
    """Verify a region (FLASH, RAM, ETC) from the in build.log memory report."""
    # Expected format
    #    <region>: <used-bytes> <unit> <total-bytes> <unit>
    if len(parts) < 5:
        return False

    # Region name must have a trailing colon
    if not parts[0].endswith(":"):
        return False

    # Verify "used" size and units
    if not parts[1].isdecimal() or not parts[2] in UNITS:
        return False

    # Verify "region" size and units
    if not parts[3].isdecimal() or not parts[4] in UNITS:
        return False

    return True


def parse_buildlog(filename, metric, variant, track_on_gerrit):
    """Parse the build.log generated by zmake to find the size of the image."""
    with open(filename, "r", encoding="utf-8") as infile:
        # Skip over all lines until the memory report is found
        while True:
            line = infile.readline()
            if not line:
                return
            if line.startswith("Memory region"):
                break

        for line in infile.readlines():
            # Skip any lines that are not part of the report
            if not line.startswith(" "):
                continue
            parts = line.split()

            if not _memory_region_is_valid(parts):
                print(
                    f"Warning: {metric.target_name} unsupported memory region: '{line[:-1]}'"
                )
                continue

            fw_section = metric.fw_section.add()
            fw_section.region = variant + "_" + parts[0][:-1]
            fw_section.used = int(parts[1]) * UNITS[parts[2]]
            fw_section.total = int(parts[3]) * UNITS[parts[4]]
            if track_on_gerrit and fw_section.region in BINARY_SIZE_REGIONS:
                fw_section.track_on_gerrit = True
            else:
                fw_section.track_on_gerrit = False


def bundle(opts):
    """Bundle the artifacts for either firmware or code coverage."""
    if opts.code_coverage:
        return bundle_coverage(opts)
    return bundle_firmware(opts)


def get_bundle_dir(opts):
    """Get the directory for the bundle from opts or use the default.

    Also create the directory if it doesn't exist.
    """
    if opts.output_dir:
        bundle_dir = opts.output_dir
    else:
        bundle_dir = DEFAULT_BUNDLE_DIRECTORY
    bundle_dir = pathlib.Path(bundle_dir)
    if not bundle_dir.is_dir():
        bundle_dir.mkdir()
    return bundle_dir


def write_metadata(opts, info):
    """Write the metadata about the bundle."""
    bundle_metadata_file = (
        opts.metadata if opts.metadata else DEFAULT_BUNDLE_METADATA_FILE
    )
    with open(bundle_metadata_file, "w", encoding="utf-8") as file:
        file.write(json_format.MessageToJson(info))


def bundle_coverage(opts):
    """Bundles the artifacts from code coverage into its own tarball."""
    info = firmware_pb2.FirmwareArtifactInfo()  # pylint: disable=no-member
    info.bcs_version_info.version_string = opts.bcs_version
    bundle_dir = get_bundle_dir(opts)
    platform_ec = ZEPHYR_DIR.parent
    build_dir = platform_ec / "build" / "zephyr"
    tarball_name = "coverage.tbz2"
    tarball_path = bundle_dir / tarball_name
    cmd = ["tar", "cvfj", tarball_path, "lcov.info"]
    log_cmd(cmd)
    subprocess.run(cmd, cwd=build_dir, check=True, stdin=subprocess.DEVNULL)
    meta = info.objects.add()
    meta.file_name = tarball_name
    meta.lcov_info.type = (
        firmware_pb2.FirmwareArtifactInfo.LcovTarballInfo.LcovType.LCOV  # pylint: disable=no-member
    )
    (bundle_dir / "html").mkdir(exist_ok=True)
    cmd = ["mv", "lcov_rpt"]
    for board in SPECIAL_BOARDS:
        cmd.append(board + "_rpt")
    cmd.append(bundle_dir / "html/")
    log_cmd(cmd)
    subprocess.run(cmd, cwd=build_dir, check=True, stdin=subprocess.DEVNULL)
    meta = info.objects.add()
    meta.file_name = "html"
    meta.coverage_html.SetInParent()

    write_metadata(opts, info)
    return 0


def bundle_firmware(opts):
    """Bundles the artifacts from each target into its own tarball."""
    info = firmware_pb2.FirmwareArtifactInfo()  # pylint: disable=no-member
    info.bcs_version_info.version_string = opts.bcs_version
    version = opts.bcs_version or get_version()

    bundle_dir = get_bundle_dir(opts)
    platform_ec = ZEPHYR_DIR.parent
    modules = zmake.modules.locate_from_checkout(find_checkout())
    projects_path = zmake.modules.default_projects_dirs(modules)
    subprocesses = []
    per_board_targets = collections.defaultdict(list)
    for project in zmake.project.find_projects(projects_path).values():
        build_dir = (
            platform_ec / "build" / "zephyr" / project.config.project_name
        )
        artifacts_dir = build_dir / "output"
        # karis.EC.15709.192.0.tar.bz2
        if version:
            tarball_name = f"{project.config.project_name}.EC.{version}.tar.bz2"
        else:
            tarball_name = f"{project.config.project_name}.EC.tar.bz2"
        tarball_path = bundle_dir.joinpath(tarball_name)
        for board in set(project.config.inherited_from):
            per_board_targets[board].append(
                f"{project.config.project_name}/output"
            )
        cmd = [
            "tar",
            "--exclude=*.elf",
            "--exclude=*.lst",
            "-cjf",
            tarball_path,
        ]
        cmd.extend(
            [x.relative_to(artifacts_dir) for x in artifacts_dir.glob("*")]
        )
        log_cmd(cmd, cwd=artifacts_dir)
        subprocesses.append(
            subprocess.Popen(  # pylint: disable=consider-using-with
                cmd, cwd=artifacts_dir, stdin=subprocess.DEVNULL
            )
        )
        meta = info.objects.add()
        meta.tarball_info.board.extend(set(project.config.inherited_from))
        meta.file_name = tarball_name
        meta.tarball_info.type = (
            firmware_pb2.FirmwareArtifactInfo.TarballInfo.FirmwareType.EC  # pylint: disable=no-member
        )
        # TODO(kmshelton): Populate the rest of metadata contents as it
        # gets defined in infra/proto/src/chromite/api/firmware.proto.
    # For each board, create a big tar file that contains all the models.
    for board, dirs in per_board_targets.items():
        tarball_name = f"{board}/firmware_from_source.tar.bz2"
        (bundle_dir / board).mkdir(exist_ok=True)
        cmd = [
            "tar",
            "-cjf",
            str(bundle_dir / tarball_name),
            "-C",
            str(platform_ec / "build" / "zephyr"),
            "--transform",
            "s,/output,,",
        ] + dirs
        log_cmd(cmd)
        subprocesses.append(
            subprocess.Popen(  # pylint: disable=consider-using-with
                cmd, stdin=subprocess.DEVNULL
            )
        )
        meta = info.objects.add()
        meta.tarball_info.board.append(board)
        meta.file_name = tarball_name
        meta.tarball_info.type = (
            firmware_pb2.FirmwareArtifactInfo.TarballInfo.FirmwareType.EC  # pylint: disable=no-member
        )
    for proc in subprocesses:
        proc.wait()
        if proc.returncode != 0:
            raise subprocess.CalledProcessError(proc.returncode, proc.args)

    tokens_file = "tokens.bin"
    tokens_path = platform_ec / "build" / tokens_file
    print(f"{tokens_path} exists={pathlib.Path(tokens_path).is_file()}")
    if pathlib.Path(tokens_path).is_file():
        cmd = ["shutil.copyfile", tokens_path, bundle_dir / tokens_file]
        log_cmd(cmd)
        shutil.copyfile(tokens_path, bundle_dir / tokens_file)
        meta = info.objects.add()
        meta.file_name = tokens_file
        meta.token_info.type = (
            firmware_pb2.FirmwareArtifactInfo.TokenDatabaseInfo.TokenDatabaseType.EC  # pylint: disable=no-member
        )

    write_metadata(opts, info)
    return 0


def test(opts):
    """Runs all of the unit tests for Zephyr firmware"""
    metrics = firmware_pb2.FwTestMetricList()  # pylint: disable=no-member

    # Run tests from Makefile.cq because make knows how to run things
    # in parallel.
    cmd = ["make", "-f", "Makefile.cq", f"-j{opts.cpus}", "test"]
    env = os.environ.copy()
    env.update(init_toolchain())
    if opts.code_coverage:
        cmd.append("COVERAGE=1")
    if SPECIAL_BOARDS:
        cmd.append(f"SPECIAL_BOARDS={' '.join(SPECIAL_BOARDS)}")
    log_cmd(cmd)
    subprocess.run(
        cmd,
        check=True,
        cwd=ZEPHYR_DIR,
        stdin=subprocess.DEVNULL,
        env=env,
    )

    # Twister-based tests
    platform_ec = ZEPHYR_DIR.parent
    twister_out_dir = platform_ec / "twister-out-llvm"
    twister_out_dir_gcc = platform_ec / "twister-out-host"
    modules = zmake.modules.locate_from_checkout(find_checkout())
    projects_path = zmake.modules.default_projects_dirs(modules)

    if opts.code_coverage:
        build_dir = platform_ec / "build" / "zephyr"
        if twister_out_dir.exists():
            _extract_lcov_summary(
                "EC_ZEPHYR_TESTS", metrics, twister_out_dir / "coverage.info"
            )
        _extract_lcov_summary(
            "EC_ZEPHYR_TESTS_GCC",
            metrics,
            twister_out_dir_gcc / "coverage.info",
        )
        _extract_lcov_summary(
            "EC_LEGACY_TESTS", metrics, platform_ec / "build/coverage/lcov.info"
        )
        _extract_lcov_summary(
            "ALL_TESTS", metrics, build_dir / "all_tests.info"
        )
        _extract_lcov_summary(
            "EC_ZEPHYR_MERGED", metrics, build_dir / "zephyr_merged.info"
        )
        _extract_lcov_summary("ALL_MERGED", metrics, build_dir / "lcov.info")
        _extract_lcov_summary(
            "ALL_FILTERED", metrics, build_dir / "lcov_no_tests.info"
        )

        for project in zmake.project.find_projects(projects_path).values():
            if project.config.project_name in SPECIAL_BOARDS:
                _extract_lcov_summary(
                    f"BOARD_{project.config.full_name}".upper(),
                    metrics,
                    build_dir / (project.config.project_name + "_final.info"),
                )

    if opts.metrics:
        with open(opts.metrics, "w", encoding="utf-8") as file:
            file.write(json_format.MessageToJson(metrics))  # type: ignore
    return 0


def check_inherits(_opts):
    """Reads the src/project/*/*/generated/joined.jsonproto files and compares
    the boards and zephyr_ec targets with the zephyr inherited_from values.
    """

    modules = zmake.modules.locate_from_checkout(find_checkout())
    projects_path = zmake.modules.default_projects_dirs(modules)
    # Ec target name -> board name -> boolean if seen in Boxster
    ec_to_board = collections.defaultdict(dict)
    for project in zmake.project.find_projects(projects_path).values():
        board_dict = {}
        for board in project.config.inherited_from:
            board_dict[board] = False
        ec_to_board[project.config.project_name] = board_dict

    retcode = 0
    configs = (find_checkout() / "src" / "project").glob(
        "*/*/generated/joined.jsonproto"
    )
    for cfg_path in configs:
        with open(cfg_path, "r", encoding="utf-8") as file:
            cfg = json.load(file)
            board_name = None
            for design_list in cfg.get("designList", []):
                this_board = design_list.get("programId", {}).get("value", None)
                if this_board is not None:
                    this_board = this_board.lower()
                    if board_name is not None and board_name != this_board:
                        print(
                            f"ERROR: Found multiple boards in {cfg_path}: "
                            f"{this_board} != {board_name}"
                        )
                        retcode = 1
                    board_name = this_board
            if board_name is None:
                print(f"WARNING: Found no board in {cfg_path}")

            for software_config in cfg.get("softwareConfigs", []):
                zephyr_ec = (
                    software_config.get("firmwareBuildConfig", {})
                    .get("buildTargets", {})
                    .get("zephyrEc", None)
                )
                if zephyr_ec:
                    if zephyr_ec not in ec_to_board:
                        print(
                            f"ERROR: Unknown Zephyr target {zephyr_ec} in {cfg_path}"
                        )
                        retcode = 1
                    elif board_name not in ec_to_board[zephyr_ec]:
                        print(
                            f"ERROR: Zephyr target {zephyr_ec} does not have "
                            f"inherited_from {board_name}"
                        )
                        retcode = 1
                    ec_to_board[zephyr_ec][board_name] = True
    for zephyr_ec, boards in ec_to_board.items():
        for board, found in boards.items():
            if not found:
                print(
                    f"ERROR: Zephyr target {zephyr_ec} has unexpected "
                    f"inherited_from of {board}"
                )
                retcode = 1

    return retcode


COVERAGE_RE = re.compile(
    r"lines\.*: *([0-9\.]+)% \(([0-9]+) of ([0-9]+) lines\)"
)


def _extract_lcov_summary(name, metrics, filename):
    cmd = [
        "/usr/bin/lcov",
        "--ignore-errors",
        "inconsistent",
        "--summary",
        filename,
    ]
    log_cmd(cmd)
    output = subprocess.run(
        cmd,
        cwd=ZEPHYR_DIR,
        check=True,
        stdout=subprocess.PIPE,
        universal_newlines=True,
        stdin=subprocess.DEVNULL,
    ).stdout
    re_match = COVERAGE_RE.search(output)
    if re_match:
        metric = metrics.value.add()
        metric.name = name
        metric.coverage_percent = float(re_match.group(1))
        metric.covered_lines = int(re_match.group(2))
        metric.total_lines = int(re_match.group(3))


def main(args):
    """Builds and tests all of the Zephyr targets and reports build metrics"""
    parser, sub_cmds = scripts.firmware_builder_lib.create_arg_parser(
        build, bundle, test
    )

    check_inherits_cmd = sub_cmds.add_parser(
        "check_inherits",
        help="Checks the inherited_from values against Boxster",
    )
    check_inherits_cmd.set_defaults(func=check_inherits)

    opts = parser.parse_args(args)

    # Convert the full version strings (R130-16032.8.0-1) to the short form (16032.8.0).
    if opts.bcs_version:
        match = re.compile(r"R\d+-(\d+\.\d+\.\d+)(-\d+)?").fullmatch(
            opts.bcs_version
        )
        if match:
            opts.bcs_version = match[1]

    if not hasattr(opts, "func"):
        print("Must select a valid sub command!")
        return -1

    # Run selected sub command function
    return opts.func(opts)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
