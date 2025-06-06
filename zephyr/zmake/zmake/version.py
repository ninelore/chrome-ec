# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code to generate the ec_version.h file."""

import datetime
import getpass
import io
import pathlib
import platform
import re
import subprocess

from zmake import util


EC_BASE = pathlib.Path(__file__).parent.parent


def _get_num_commits(repo):
    """Get the number of commits that have been made.

    If a Git repository is available, return the number of commits that have
    been made. Otherwise return a fixed count.

    Args:
        repo: The path to the git repo.

    Returns:
        An integer, the number of commits that have been made.
    """
    try:
        result = subprocess.run(
            ["git", "-C", repo, "rev-list", "HEAD", "--count"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError:
        commits = "9999"
    else:
        commits = result.stdout

    return int(commits)


def _get_revision(repo):
    """Get the current revision hash.

    If a Git repository is available, return the hash of the current index.
    Otherwise return the hash of the VCSID environment variable provided by
    the packaging system.

    Args:
        repo: The path to the git repo.

    Returns:
        A string, of the current revision.
    """
    try:
        result = subprocess.run(
            ["git", "-C", repo, "log", "-n1", "--format=%H"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError:
        revision = "unknown"
    else:
        revision = result.stdout[:7]

    return revision


def _is_tree_dirty(repo):
    """Check if the tree is dirty.

    Args:
        repo: The path to the git repo.

    Returns:
        A bool to indicate if the tree is dirty.
    """
    try:
        result = subprocess.run(
            [
                "git",
                "-C",
                repo,
                "ls-files",
                "-z",
                "--modified",
                "-o",
                "--exclude-standard",
            ],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
        )
    except subprocess.CalledProcessError:
        dirty = False
    else:
        dirty = len(result.stdout) > 0

    return dirty


def get_version_string(
    project="unknown",
    version=None,
    static=False,
    git_path=None,
):
    """Get the version string associated with a build.

    Args:
        project: a string project name
        version: a build version string
        git_path: the path to get the git revision.
        static: if set, create a version string not dependent on git
            commits, thus allowing binaries to be compared between two
            commits. The official builds have this flag set.

    Returns:
        A version string which can be placed in FRID, FWID, or used in
        the build for the OS.
    """
    if not version:
        version = "0.0.0"

    dir_path_1 = pathlib.Path(
        EC_BASE.resolve().parent.parent
        / "build"
        / "zephyr"
        / f"{project}"
        / "build-singleimage"
        / "zephyr"
        / "include"
        / "generated"
        / "zephyr"
    )

    dir_path_2 = pathlib.Path(
        EC_BASE.resolve().parent.parent.parent.parent
        / "build"
        / f"{project}"
        / "build-singleimage"
        / "zephyr"
        / "include"
        / "generated"
        / "zephyr"
    )

    if dir_path_1.exists():
        dir_path = dir_path_1
    elif dir_path_2.exists():
        dir_path = dir_path_2
    else:
        dir_path = None

    if dir_path:
        ish_prj = util.read_kconfig_autoconf_value(
            dir_path,
            "CONFIG_SOC_FAMILY_INTEL_ISH",
        )
        if ish_prj:
            ish_version = util.read_kconfig_autoconf_value(
                dir_path,
                "CONFIG_BOARD",
            )
            # remove "intel_ish_" from config value
            ish_version = re.sub(r'"intel_ish_', "", ish_version)
            ish_version = re.sub(r'"', "", ish_version)
            result = f"{project}-{ish_version}-{version}"
        else:
            result = f"{project}-{version}"
    else:
        result = f"{project}-{version}"

    if static:
        return result

    # dev build
    if not git_path:
        git_path = "."
    revision = _get_revision(git_path)
    commit = f"-{revision}"
    result += commit

    if _is_tree_dirty(git_path):
        result += "+"

    return result


def write_version_header(version_str, output_path, tool, static=False):
    """Generate a version header and write it to the specified path.

    Generate a version header in the format expected by the EC build
    system, and write it out only if the version header does not exist
    or changes.  We don't write in the case that the version header
    does exist and was unchanged, which allows "zmake build" commands
    on an unchanged tree to be an effective no-op.

    Args:
        version_str: The version string to be used in the header, such
            as one generated by get_version_string.
        output_path: The file path to write at (a pathlib.Path
            object).
        tool: Name of the tool that is invoking this function ("zmake",
            "generate_ec_version.py", etc). Included in a comment in the
            header.
        static: If true, generate a header which does not include
            information like the username, hostname, or date, allowing
            the build to be reproducible.
    """
    output = io.StringIO()
    output.write(f"/* This file is automatically generated by {tool} */\n")

    def add_def(name, value):
        output.write(f"#define {name} {util.c_str(value)}\n")

    add_def("VERSION", version_str)
    add_def("CROS_EC_VERSION32", version_str[:31])

    if static:
        add_def("BUILDER", "reproducible@build")
        add_def("DATE", "STATIC_VERSION_DATE")
    else:
        add_def("BUILDER", f"{getpass.getuser()}@{platform.node()}")
        add_def("DATE", datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

    contents = output.getvalue()
    if not output_path.exists() or output_path.read_text() != contents:
        output_path.write_text(contents)
