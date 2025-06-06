# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Definitions of toolchain variables."""

import os
import pathlib
import shutil

from zmake import build_config


class GenericToolchain:
    """Default toolchain if not known to zmake.

    Simply pass ZEPHYR_TOOLCHAIN_VARIANT=name to the build, with
    nothing extra.
    """

    def __init__(self, name, modules=None):
        self.name = name
        self.modules = modules or {}

    def probe(self):
        """Probe if the toolchain is available on the system."""
        # Since the toolchain is not known to zmake, we have no way to
        # know if it's installed.  Simply return False to indicate not
        # installed.  An unknown toolchain would only be used if -t
        # was manually passed to zmake, and is not valid to put in a
        # BUILD.py file.
        return False

    def get_build_config(self):
        """Get the build configuration for the toolchain.

        Returns:
            A build_config.BuildConfig to be applied to the build.
        """
        return build_config.BuildConfig(
            cmake_defs={
                "ZEPHYR_TOOLCHAIN_VARIANT": self.name,
            },
        )


class CorebootSdkToolchain(GenericToolchain):
    """Coreboot SDK toolchain installed in default location."""

    def probe(self):
        """coreboot-sdk is always available, as it downloads on demand."""
        return True

    def get_build_config(self):
        cmake_defs = {
            "TOOLCHAIN_ROOT": str(self.modules["ec"] / "zephyr"),
        }

        # Pass-along COREBOOT_SDK_ROOT* variables from the environment.
        for var, val in os.environ.items():
            if var.startswith("COREBOOT_SDK_ROOT"):
                cmake_defs[var] = val

        return (
            build_config.BuildConfig(cmake_defs=cmake_defs)
            | super().get_build_config()
        )


class ZephyrToolchain(GenericToolchain):
    """Zephyr SDK toolchain.

    Either set the environment var ZEPHYR_SDK_INSTALL_DIR, or install
    the SDK in one of the common known locations.
    """

    def __init__(self, *args, **kwargs):
        self.zephyr_sdk_install_dir = self._find_zephyr_sdk()
        super().__init__(*args, **kwargs)

    @staticmethod
    def _find_zephyr_sdk():
        """Find the Zephyr SDK, if it's installed.

        Returns:
            The path to the Zephyr SDK, using the search rules defined by
            https://docs.zephyrproject.org/latest/getting_started/installation_linux.html,
            or None, if one cannot be found on the system.
        """
        from_env = os.getenv("ZEPHYR_SDK_INSTALL_DIR")
        if from_env:
            return pathlib.Path(from_env)

        def _gen_sdk_paths():
            for prefix in (
                "~",
                "~/.local",
                "~/.local/opt",
                "~/bin",
                "/opt",
                "/usr",
                "/usr/local",
            ):
                prefix = pathlib.Path(os.path.expanduser(prefix))
                yield prefix / "zephyr-sdk"
                yield from prefix.glob("zephyr-sdk-*")

        for path in _gen_sdk_paths():
            if (path / "sdk_version").is_file():
                return path

        return None

    def probe(self):
        return bool(self.zephyr_sdk_install_dir)

    def get_build_config(self):
        if not self.zephyr_sdk_install_dir:
            raise RuntimeError(
                "No installed Zephyr SDK was found"
                " (see docs/zephyr/zephyr_build.md for documentation)"
            )
        tc_vars = {
            "ZEPHYR_SDK_INSTALL_DIR": str(self.zephyr_sdk_install_dir),
        }
        return (
            build_config.BuildConfig(cmake_defs=tc_vars)
            | super().get_build_config()
        )


class LlvmToolchain(GenericToolchain):
    """LLVM toolchain as used in the chroot."""

    def probe(self):
        # TODO: differentiate chroot llvm path vs. something more
        # generic?
        return bool(shutil.which("x86_64-pc-linux-gnu-clang"))

    def get_build_config(self):
        # TODO: this contains custom settings for the chroot.  Plumb a
        # toolchain for "generic-llvm" for external uses?
        return (
            build_config.BuildConfig(
                cmake_defs={
                    "TOOLCHAIN_ROOT": str(self.modules["ec"] / "zephyr"),
                },
            )
            | super().get_build_config()
        )


class HostToolchain(GenericToolchain):
    """GCC toolchain found in the PATH."""

    def probe(self):
        # "host" toolchain for Zephyr means GCC.
        return bool(shutil.which("gcc"))


# Mapping of toolchain names -> support class
support_classes = {
    "coreboot-sdk": CorebootSdkToolchain,
    "host": HostToolchain,
    "llvm": LlvmToolchain,
    "zephyr": ZephyrToolchain,
}
