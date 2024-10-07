# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _ec_deps_impl(module_ctx):
    def _coreboot_sdk_subtool(arch, version, sha256, bucket = "chromiumos-sdk"):
        name = "ec-coreboot-sdk-%s" % arch
        toolchain_name = "coreboot-sdk-%s" % arch
        http_archive(
            name = name,
            build_file = "//platform/rules_cros_firmware/cros_firmware:BUILD.gcs_subtool",
            sha256 = sha256,
            url = "https://storage.googleapis.com/%s/toolchains/%s/%s.tar.zst" % (bucket, toolchain_name, version),
        )

    _coreboot_sdk_subtool(
        "nds32le-elf",
        "14.2.0-r3/dcf960a7d85793abe1b27aef46a1e5e79e8b995e",
        "1c8c5b206bd13a40761fff9cd104b2dae32790eea1688f3ec98d3975aa26ee07",
    )
    _coreboot_sdk_subtool(
        "i386-elf",
        "14.2.0-r3/b55705844741bee0577171f53edd14dac5df0e43",
        "8967be6a0022e41569a367c4ed31d31284fec36070d5bafb3e0ec472805154b2",
    )
    _coreboot_sdk_subtool(
        "picolibc-i386-elf",
        "14.2.0-r1/d0c4b73b623a8df5c00cecfa0b6ad11a67765075",
        "1477392d1bdaa8de5ae7bfb469803f335333c6fa46fccf4a34d7e7a5beca3c43",
    )
    _coreboot_sdk_subtool(
        "libstdcxx-i386-elf",
        "14.2.0-r1/995394d19979448598c258a1c9ee5e4a8e9595c2",
        "d60b3b4eb92d0015ea937cc56adfdf4f1a1deb862b2432ae967408cf96fe2b40",
    )
    _coreboot_sdk_subtool(
        "arm-eabi",
        "14.2.0-r3/d1512baac52606aa45d0bdd38040e67df2e17d7c",
        "d2e4f86a37f8674bb172ccb52a0fe8d1364564f9e37e1464fc7303fb50adb0f3",
    )
    _coreboot_sdk_subtool(
        "picolibc-arm-eabi",
        "14.2.0-r1/c32e56a1f505e22a4233680329e8a13ac999a1f3",
        "82d99348a576a5f81a383cdc239ace7a2f82e61b0e56903e271c4bbdcc9c1e04",
    )
    _coreboot_sdk_subtool(
        "libstdcxx-arm-eabi",
        "14.2.0-r1/cc19d2614d096f58cd5c2483a2b1d313e0d23625",
        "1c0a1a050be72363ccc07d41e9a96e4e46121874c859869f968076671e302926",
    )
    _coreboot_sdk_subtool(
        "riscv-elf",
        "14.2.0-r3/f86d8c0ebc8e5d03f4193a7c6b9732a52a2778c1",
        "4fcde5976454537569dd07e62c47f35fc2f4db745a4ad269cfb153d27da8d0b1",
    )

    return module_ctx.extension_metadata(
        root_module_direct_deps = [
            "ec-coreboot-sdk-arm-eabi",
            "ec-coreboot-sdk-picolibc-arm-eabi",
            "ec-coreboot-sdk-libstdcxx-arm-eabi",
            "ec-coreboot-sdk-i386-elf",
            "ec-coreboot-sdk-picolibc-i386-elf",
            "ec-coreboot-sdk-libstdcxx-i386-elf",
            "ec-coreboot-sdk-nds32le-elf",
            "ec-coreboot-sdk-riscv-elf",
        ],
        root_module_direct_dev_deps = [],
        reproducible = True,
    )

ec_deps = module_extension(
    implementation = _ec_deps_impl,
)
