#!/usr/bin/env python3
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=line-too-long

"""Runs unit tests on device and displays the results.

This script assumes you have set up servod according to
https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/main/docs/servod_outside_chroot.md.

In addition to running this script locally, you can also run it from a remote
machine against a board connected to a local machine. For example:

Start servod and JLink locally:

(local outside) $ start-servod --channel=release --board=dragonclaw -p 9999 -f
(local chroot) $ sudo JLinkRemoteServerCLExe -select USB

Forward the FPMCU console on a TCP port:

(local outside) $ socat $(dut-control -- raw_fpmcu_console_uart_pty | cut -d: -f2) \
                  tcp4-listen:10000,fork

Forward all the ports to the remote machine:

(local outside) $ ssh -R 9999:localhost:9999 <remote> -N
(local outside) $ ssh -R 10000:localhost:10000 <remote> -N
(local outside) $ ssh -R 19020:localhost:19020 <remote> -N

Run the script on the remote machine:

(remote chroot) ./test/run_device_tests.py --remote 127.0.0.1 \
                --jlink_port 19020 --console_port 10000
"""

# pylint: enable=line-too-long

# TODO(b/267803007): refactor into multiple modules
# pylint: disable=too-many-lines

from __future__ import annotations

from abc import ABC, abstractmethod
import argparse
from collections import namedtuple
import concurrent
from concurrent.futures.thread import ThreadPoolExecutor
from contextlib import ExitStack
import copy
from dataclasses import dataclass
from dataclasses import field
from enum import Enum
import io
import logging
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time
from typing import BinaryIO, Callable, Optional

# pylint: disable=import-error
import colorama  # type: ignore[import]
import fmap
import yaml


# pylint: enable=import-error

EC_DIR = Path(os.path.dirname(os.path.realpath(__file__))).parent
JTRACE_FLASH_SCRIPT = os.path.join(EC_DIR, "util/flash_jlink.py")
SERVO_MICRO_FLASH_SCRIPT = os.path.join(EC_DIR, "util/flash_ec")
ZEPHYR_FPMCU_DIR = os.path.join(EC_DIR, "zephyr/program/fpmcu")
ZEPHYR_TWISTER = os.path.join(EC_DIR, "twister")
ZEPHYR_TWISTER_BUILD_DIR = os.path.join(EC_DIR, "build/zephyr/fpmcu-test")

# .* is added to regexes because Zephyr uses VT100 commands at the beginning of
# a new line.
ALL_TESTS_PASSED_REGEX = re.compile(r"(Pass!\r\n)|(.*TESTSUITE.*succeeded)")
ALL_TESTS_FAILED_REGEX = re.compile(
    r"(Fail! \(\d+ tests\)\r\n)|(.*TESTSUITE.*failed)"
)
# Finish regex for Zephyr upstream tests
ALL_TESTS_PASSED_REGEX_ZEPHYR = re.compile(r"PROJECT EXECUTION SUCCESSFUL")
ALL_TESTS_FAILED_REGEX_ZEPHYR = re.compile(r"PROJECT EXECUTION FAILED")

SINGLE_CHECK_PASSED_REGEX = re.compile(r"(Pass: .*)|(.* PASS - )")
SINGLE_CHECK_FAILED_REGEX = re.compile(r"(.*failed:.*)|(.* FAIL - )")

RW_IMAGE_BOOTED_REGEX = re.compile(r".*\[Image: RW.*")

ASSERTION_FAILURE_REGEX = re.compile(
    r"(ASSERTION FAILURE.*)|(.*Assertion failed at)"
)

DATA_ACCESS_VIOLATION_8020000_REGEX = re.compile(
    r"(Data access violation, mfar = 8020000\r\n)|(.*MMFAR Address: 0x8020000\r\n)"
)
DATA_ACCESS_VIOLATION_8040000_REGEX = re.compile(
    r"(Data access violation, mfar = 8040000\r\n)|(.*MMFAR Address: 0x8040000\r\n)"
)
DATA_ACCESS_VIOLATION_80C0000_REGEX = re.compile(
    r"Data access violation, mfar = 80c0000\r\n"
)
DATA_ACCESS_VIOLATION_80E0000_REGEX = re.compile(
    r"Data access violation, mfar = 80e0000\r\n"
)
DATA_ACCESS_VIOLATION_20000000_REGEX = re.compile(
    r"Data access violation, mfar = 20000000\r\n"
)
DATA_ACCESS_VIOLATION_24000000_REGEX = re.compile(
    r"Data access violation, mfar = 24000000\r\n"
)
DATA_ACCESS_VIOLATION_64020000_REGEX = re.compile(
    r"Data access violation, mfar = 64020000\r\n"
)
DATA_ACCESS_VIOLATION_64030000_REGEX = re.compile(
    r"Data access violation, mfar = 64030000\r\n"
)
DATA_ACCESS_VIOLATION_200B0000_REGEX = re.compile(
    r"Data access violation, mfar = 200b0000\r\n"
)
"""Helipilot's data RAM starting address."""
DATA_ACCESS_VIOLATION_200A8000_REGEX = re.compile(
    r"Data access violation, mfar = 200a8000\r\n"
)
"""Buccaneer's data RAM starting address.

This is 32K less than Helipilot's start address (0x200B0000). This corresponds
to HELIPILOT_DATA_RAM_SIZE_BYTES being increased from 156KiB to 188KiB.
"""
DATA_ACCESS_VIOLATION_20098000_REGEX = re.compile(
    r"Data access violation, mfar = 20098000\r\n"
)
"""Gwendolin's data RAM starting address.

This is 96K less than Helipilot's start address (0x200B0000). This corresponds
to HELIPILOT_DATA_RAM_SIZE_BYTES being increased from 156KiB to 252KiB.
"""

# \r is added twice by Zephyr code.
PRINTF_CALLED_REGEX = re.compile(r"printf called(\r){1,2}\n")

BLOONCHIPPER = "bloonchipper"
BUCCANEER = "buccaneer"
DARTMONKEY = "dartmonkey"
GWENDOLIN = "gwendolin"
HELIPILOT = "helipilot"

JTRACE = "jtrace"
SERVO_MICRO = "servo_micro"

GCC = "gcc"
CLANG = "clang"

PRIVATE_YES = "yes"
PRIVATE_NO = "no"
PRIVATE_ONLY = "only"

TEST_ASSETS_BUCKET = "gs://chromiumos-test-assets-public/fpmcu/RO"
DARTMONKEY_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "dartmonkey_v2.0.2887-311310808.bin"
)
NOCTURNE_FP_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "nocturne_fp_v2.2.64-58cf5974e.bin"
)
NAMI_FP_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "nami_fp_v2.2.144-7a08e07eb.bin"
)
BLOONCHIPPER_V4277_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "bloonchipper_v2.0.4277-9f652bb3.bin"
)
BLOONCHIPPER_V5938_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "bloonchipper_v2.0.5938-197506c1.bin"
)
BUCCANEER_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "buccaneer_v2.0.26328-821504380b.bin"
)
HELIPILOT_IMAGE_PATH = os.path.join(
    TEST_ASSETS_BUCKET, "helipilot_v2.0.24337-2726e9f149.bin"
)

RangedValue = namedtuple("RangedValue", "nominal range")
PowerUtilization = namedtuple("PowerUtilization", "idle sleep")


class ImageType(Enum):
    """EC Image type to use for the test."""

    RO = 1
    RW = 2


class ApplicationType(Enum):
    """
    Select the application type to use (test or production)
    """

    TEST = 1
    PRODUCTION = 2


class FPSensorType(Enum):
    """Fingerprint sensor types."""

    ELAN = 0
    FPC = 1
    # TODO(b/385142008): On Quincy Rev3, Egis is represented by value 2, which
    # utilizes two binary select lines. The current servod config only
    # understands the first control line, so we can't use value 2.
    # To fix this, we would need to add the additional control line to servod
    # config.
    EGIS = 0
    UNKNOWN = -1


@dataclass
# pylint: disable-next=too-many-instance-attributes
class BoardConfig:
    """Board-specific configuration."""

    name: str
    sensor_type: FPSensorType
    servo_uart_name: str
    servo_power_enable: str
    rollback_region0_regex: re.Pattern[str]
    rollback_region1_regex: re.Pattern[str]
    mpu_regex: re.Pattern[str]
    reboot_timeout: float
    fp_power_supply: str
    mcu_power_supply: str
    expected_fp_power: PowerUtilization
    expected_mcu_power: PowerUtilization
    variants: dict[str, dict[str, str]]
    expected_fp_power_zephyr: Optional[PowerUtilization] = None
    expected_mcu_power_zephyr: Optional[PowerUtilization] = None
    zephyr_board_name: Optional[str] = None


class Platform(ABC):
    """Platform-specific methods."""

    @abstractmethod
    def get_console(self, board_config: BoardConfig) -> Optional[str]:
        """Get the name of the console for a given board."""

    @abstractmethod
    def hw_write_protect(self, enable: bool) -> None:
        """Enable/disable hardware write protect."""

    @abstractmethod
    def power(self, board_config: BoardConfig, power_on: bool) -> None:
        """Turn power to board on/off."""

    @abstractmethod
    def flash(
        self,
        board_config: BoardConfig,
        image_path: str,
        flasher: str,
        remote_ip: str,
        remote_port: int,
        test_name: str,
        enable_hw_write_protect: bool,
        zephyr: bool,
    ) -> bool:
        """Flash specified test to specified board."""

    @abstractmethod
    def cleanup(self) -> None:
        """Clean up after a test run."""

    @abstractmethod
    def skip_test(
        self, test_name: str, board_config: BoardConfig, zephyr: bool
    ) -> bool:
        """Returns true if the given test should be skipped."""


class Hardware(Platform):
    """Platform implementation for running on development boards."""

    def get_console(self, board_config: BoardConfig) -> Optional[str]:
        cmd = [
            "dut-control",
            board_config.servo_uart_name,
        ]
        logging.debug('Running command: "%s"', " ".join(cmd))

        with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
            for line in io.TextIOWrapper(proc.stdout):  # type: ignore[arg-type]
                logging.debug(line)
                pty = line.split(":")
                if len(pty) == 2 and pty[0] == board_config.servo_uart_name:
                    return pty[1].strip()

        return None

    def hw_write_protect(self, enable: bool) -> None:
        if enable:
            state = "force_on"
        else:
            state = "force_off"

        cmd = [
            "dut-control",
            "fw_wp_state:" + state,
        ]
        logging.debug('Running command: "%s"', " ".join(cmd))
        subprocess.run(cmd, check=False).check_returncode()

    def power(self, board_config: BoardConfig, power_on: bool) -> None:
        if power_on:
            state = "pp3300"
        else:
            state = "off"

        cmd = [
            "dut-control",
            board_config.servo_power_enable + ":" + state,
        ]
        logging.debug('Running command: "%s"', " ".join(cmd))
        subprocess.run(cmd, check=False).check_returncode()

    def flash(
        self,
        board_config: BoardConfig,
        image_path: str,
        flasher: str,
        remote_ip: str,
        remote_port: int,
        test_name: str,
        enable_hw_write_protect: bool,
        zephyr: bool,
    ) -> bool:
        logging.info("Flashing test")

        cmd = []
        if flasher == JTRACE:
            cmd.append(JTRACE_FLASH_SCRIPT)
            if remote_ip:
                cmd.extend(["--remote", remote_ip + ":" + str(remote_port)])
        elif flasher == SERVO_MICRO:
            cmd.append(SERVO_MICRO_FLASH_SCRIPT)
        else:
            logging.error('Unknown flasher: "%s"', flasher)
            return False
        cmd.extend(
            [
                "--board",
                board_config.name,
                "--image",
                image_path,
            ]
        )
        logging.debug('Running command: "%s"', " ".join(cmd))
        completed_process = subprocess.run(cmd, check=False)
        return completed_process.returncode == 0

    def cleanup(self) -> None:
        pass

    def skip_test(
        self, test_name: str, board_config: BoardConfig, zephyr: bool
    ) -> bool:
        return False


class Renode(Platform):
    """Platform implementation for running on Renode emulator."""

    def __init__(self):
        self.process = None

    def get_console(self, board_config: BoardConfig) -> Optional[str]:
        return "/tmp/renode-uart"

    def hw_write_protect(self, enable: bool) -> None:
        pass

    def power(self, board_config: BoardConfig, power_on: bool) -> None:
        pass

    def flash(
        self,
        board_config: BoardConfig,
        image_path: str,
        flasher: str,
        remote_ip: str,
        remote_port: int,
        test_name: str,
        enable_hw_write_protect: bool,
        zephyr: bool,
    ) -> bool:
        cmd = [
            "./util/renode-ec-launch",
            "--board",
            board_config.name,
        ]
        if zephyr:
            # We've adopted the convention that we prefix upstream Zephyr test
            # names with "zephyr_".
            if test_name.startswith("zephyr_"):
                cmd.extend(["--zephyr-bin", image_path])
            else:
                cmd.append("--zephyr")
        else:
            cmd.extend(["--ec", test_name])

        if enable_hw_write_protect:
            cmd.append("--enable-write-protect")

        # pylint: disable-next=consider-using-with
        self.process = subprocess.Popen(
            cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE
        )
        time.sleep(10)
        return True

    def cleanup(self) -> None:
        self.process.kill()

    def skip_test(
        self, test_name: str, board_config: BoardConfig, zephyr: bool
    ) -> bool:
        # Tests failures that are independent of the board.
        if test_name in [
            "fpsensor_hw",  # TODO(b/384743080)
            "power_utilization",  # Can't measure power on Renode.
            "production_app_test",  # TODO(b/384740370)
            "watchdog",  # TODO(b/390021699)
        ]:
            return True

        if board_config.name in [BLOONCHIPPER, DARTMONKEY]:
            if board_config.name == BLOONCHIPPER:
                # bloonchipper Zephyr tests to skip on Renode.
                if zephyr and test_name in [
                    "abort",  # TODO(b/384094781)
                    "benchmark",  # TODO(b/390253975)
                    "exception",  # TODO(b/388327673)
                    # TODO(b/382705460): We have seen this flake in the CQ.
                    # Re-enable when missing character bug is fixed.
                    "flash_physical",
                    "fp_transport",  # TODO(b/384094788)
                    "fpsensor_debug",  # TODO(b/384110894)
                    "ftrapv",  # TODO(b/384095271)
                    "panic",  # TODO(b/384095226)
                    "panic_data",  # TODO(b/384095623)
                    "zephyr_flash_stm32f4",  # TODO(b/384974228)
                    # TODO(b/384975384)
                    "zephyr_counter_basic_api_stm32_subsec",
                    # TODO(b/390255521)
                    "timer",
                    # TODO(b/405230727)
                    "stdlib",
                ]:
                    return True

                # bloonchipper EC tests to skip on Renode.
                if test_name in [
                    "rtc_stm32f4",  # TODO(b/384991107)
                ]:
                    return True
        elif board_config.name in [HELIPILOT, BUCCANEER, GWENDOLIN]:
            if test_name in [
                "exception",  # TODO(b/384730599)
                "otp_key",  # TODO(b/385216796)
                "ram_lock",  # TODO(b/385216805)
                "rtc_npcx9",  # TODO(b/385217282)
            ]:
                return True

        return False


@dataclass
class TestConfig:
    """Configuration for a given test."""

    # pylint: disable=too-many-instance-attributes
    test_name: str
    imagetype_to_use: ImageType = ImageType.RW
    apptype_to_use: ApplicationType = ApplicationType.TEST
    finish_regexes: Optional[list[re.Pattern[str]]] = None
    fail_regexes: Optional[list[re.Pattern[str]]] = None
    toggle_power: bool = False
    test_args: list[str] = field(default_factory=list)
    timeout_secs: int = 60
    enable_hw_write_protect: bool = False
    ro_image: Optional[str] = None
    build_board: Optional[str] = None
    config_name: Optional[str] = None
    exclude_boards: list = field(default_factory=list)
    logs: list = field(init=False, default_factory=list)
    passed: bool = field(init=False, default=False)
    num_passes: int = field(init=False, default=0)
    num_fails: int = field(init=False, default=0)
    skip_for_zephyr: bool = False
    zephyr_name: Optional[str] = None

    # The callbacks below are called before and after a test is executed and
    # may be used for additional test setup, post test activities, or other tasks
    # that do not otherwise fit into the test workflow. The default behavior is
    # to simply return True and if either callback returns False then the test
    # is reported a failure.
    pre_test_callback: Callable = field(init=True, default=lambda board: True)
    post_test_callback: Callable = field(init=True, default=lambda board: True)

    def __post_init__(self):
        if self.finish_regexes is None:
            self.finish_regexes = [
                ALL_TESTS_PASSED_REGEX,
                ALL_TESTS_FAILED_REGEX,
            ]
        if self.fail_regexes is None:
            self.fail_regexes = [
                SINGLE_CHECK_FAILED_REGEX,
                ALL_TESTS_FAILED_REGEX,
                ASSERTION_FAILURE_REGEX,
            ]
        if self.config_name is None:
            self.config_name = self.test_name


# All possible tests.
class AllTests:
    """All possible tests."""

    @staticmethod
    def get(
        platform: Platform,
        board_config: BoardConfig,
        with_private: str,
        zephyr: bool,
    ) -> list[TestConfig]:
        """Return public and private test configs for the specified board."""
        public_tests = (
            []
            if with_private == PRIVATE_ONLY
            else AllTests.get_public_tests(platform, board_config)
        )
        private_tests = (
            [] if with_private == PRIVATE_NO else AllTests.get_private_tests()
        )

        zephyr_upstream_tests = (
            []
            if with_private == PRIVATE_ONLY or not zephyr
            else AllTests.get_zephyr_tests()
        )

        all_tests = public_tests + private_tests + zephyr_upstream_tests
        board_tests = list(
            filter(
                lambda e: (board_config.name not in e.exclude_boards), all_tests
            )
        )
        return board_tests

    @staticmethod
    def get_public_tests(
        platform: Platform, board_config: BoardConfig
    ) -> list[TestConfig]:
        """Return public test configs for the specified board."""
        tests = [
            TestConfig(
                test_name="production_app_test",
                finish_regexes=[RW_IMAGE_BOOTED_REGEX],
                imagetype_to_use=ImageType.RW,
                apptype_to_use=ApplicationType.PRODUCTION,
            ),
            TestConfig(test_name="abort"),
            TestConfig(test_name="aes"),
            # Cryptoc is not supported with Zephyr.
            # TODO(b/333039464) A new test for OPENSSL_cleanse has to be implemented.
            TestConfig(test_name="always_memset", skip_for_zephyr=True),
            TestConfig(
                test_name="assert_builtin",
                fail_regexes=[
                    SINGLE_CHECK_FAILED_REGEX,
                    ALL_TESTS_FAILED_REGEX,
                ],
                # TODO(b/365628799): Need to port to Zephyr.
                skip_for_zephyr=True,
            ),
            TestConfig(
                test_name="assert_stdlib",
                fail_regexes=[
                    ALL_TESTS_FAILED_REGEX,
                    ASSERTION_FAILURE_REGEX,
                ],
                # TODO(b/365628799): Need to port to Zephyr.
                skip_for_zephyr=True,
            ),
            TestConfig(test_name="benchmark", timeout_secs=120),
            TestConfig(test_name="boringssl_crypto"),
            TestConfig(test_name="cortexm_fpu"),
            TestConfig(test_name="crc"),
            TestConfig(test_name="exception"),
            TestConfig(test_name="exit"),
            TestConfig(
                test_name="flash_physical",
                imagetype_to_use=ImageType.RO,
                toggle_power=True,
            ),
            TestConfig(
                test_name="flash_write_protect",
                imagetype_to_use=ImageType.RO,
                toggle_power=True,
                enable_hw_write_protect=True,
            ),
            TestConfig(
                config_name="fp_transport_spi_ro",
                test_name="fp_transport",
                imagetype_to_use=ImageType.RO,
                test_args=["spi"],
            ),
            TestConfig(
                config_name="fp_transport_spi_rw",
                test_name="fp_transport",
                test_args=["spi"],
            ),
            TestConfig(
                config_name="fp_transport_uart_ro",
                test_name="fp_transport",
                imagetype_to_use=ImageType.RO,
                test_args=["uart"],
            ),
            TestConfig(
                config_name="fp_transport_uart_rw",
                test_name="fp_transport",
                test_args=["uart"],
            ),
            TestConfig(test_name="fpsensor_auth_crypto_stateful"),
            TestConfig(
                test_name="fpsensor_auth_crypto_stateful_otp",
                exclude_boards=[BLOONCHIPPER, DARTMONKEY],
            ),
            TestConfig(test_name="fpsensor_auth_crypto_stateless"),
            TestConfig(test_name="fpsensor_crypto"),
            TestConfig(test_name="fpsensor_debug"),
            TestConfig(
                test_name="fpsensor_hw",
                pre_test_callback=lambda config: fp_sensor_sel(
                    platform=platform, board_config=config
                ),
            ),
            TestConfig(test_name="fpsensor_utils"),
            TestConfig(test_name="ftrapv"),
            TestConfig(
                test_name="libc_printf",
                finish_regexes=[PRINTF_CALLED_REGEX],
            ),
            # Handled by Zephyr - cpp.main.* tests
            TestConfig(test_name="global_initialization", skip_for_zephyr=True),
            TestConfig(test_name="libcxx"),
            TestConfig(test_name="malloc", imagetype_to_use=ImageType.RO),
            # TODO(b/363277530): Add Zephyr MPU tests.
            TestConfig(
                config_name="mpu_ro",
                test_name="mpu",
                imagetype_to_use=ImageType.RO,
                finish_regexes=[board_config.mpu_regex],
                skip_for_zephyr=True,
            ),
            TestConfig(
                config_name="mpu_rw",
                test_name="mpu",
                finish_regexes=[board_config.mpu_regex],
                skip_for_zephyr=True,
            ),
            # Handled by Zephyr - kernel.mutex test
            TestConfig(test_name="mutex", skip_for_zephyr=True),
            TestConfig(test_name="mutex_trylock", skip_for_zephyr=True),
            TestConfig(test_name="mutex_recursive", skip_for_zephyr=True),
            TestConfig(
                test_name="otp_key",
                exclude_boards=[BLOONCHIPPER, DARTMONKEY],
            ),
            TestConfig(test_name="panic"),
            TestConfig(
                config_name="panic_data",
                test_name="panic_data",
                fail_regexes=[
                    SINGLE_CHECK_FAILED_REGEX,
                    ALL_TESTS_FAILED_REGEX,
                ],
            ),
            # Task synchronization covered by Zephyr tests and shim layer by unit tests.
            # task_wait_event is implemented based on k_poll_event and it is verified by
            # the kernel.poll test.
            TestConfig(test_name="pingpong", skip_for_zephyr=True),
            TestConfig(test_name="printf"),
            TestConfig(test_name="queue"),
            TestConfig(
                test_name="ram_lock",
                exclude_boards=[BLOONCHIPPER, DARTMONKEY],
            ),
            TestConfig(test_name="restricted_console"),
            TestConfig(test_name="rng_benchmark"),
            TestConfig(
                config_name="rollback_region0",
                test_name="rollback",
                finish_regexes=[board_config.rollback_region0_regex],
                test_args=["region0"],
            ),
            TestConfig(
                config_name="rollback_region1",
                test_name="rollback",
                finish_regexes=[board_config.rollback_region1_regex],
                test_args=["region1"],
            ),
            TestConfig(
                test_name="rollback_entropy", imagetype_to_use=ImageType.RO
            ),
            # RTC is handled by Zephyr drivers, covered by Zephyr tests. Time
            # translation is covered by the utilities.time test.
            TestConfig(test_name="rtc", skip_for_zephyr=True),
            TestConfig(
                test_name="rtc_npcx9",
                exclude_boards=[BLOONCHIPPER, DARTMONKEY],
            ),
            # Covered by Zephyr drivers.counter.basic_api.stm32_subsec test
            TestConfig(
                test_name="rtc_stm32f4",
                exclude_boards=[DARTMONKEY, HELIPILOT, BUCCANEER, GWENDOLIN],
                skip_for_zephyr=True,
            ),
            TestConfig(test_name="sbrk", imagetype_to_use=ImageType.RO),
            TestConfig(test_name="sha256"),
            TestConfig(test_name="sha256_unrolled"),
            TestConfig(test_name="static_if"),
            TestConfig(test_name="stdlib"),
            TestConfig(test_name="std_vector"),
            TestConfig(
                config_name="system_is_locked_wp_on",
                test_name="system_is_locked",
                test_args=["wp_on"],
                toggle_power=True,
                enable_hw_write_protect=True,
            ),
            TestConfig(
                config_name="system_is_locked_wp_off",
                test_name="system_is_locked",
                test_args=["wp_off"],
                toggle_power=True,
                enable_hw_write_protect=False,
            ),
            TestConfig(test_name="timer"),
            # task_wait_event works only with the shimmed task list, which is
            # hardcoded. The task synchronization functions are covered by
            # Zephyr tests. task_wait_event is implemented based on k_poll_event
            # and it is verified by the kernel.poll test.
            TestConfig(test_name="timer_dos", skip_for_zephyr=True),
            TestConfig(test_name="tpm_seed_clear"),
            # UART buffering is not used with Zephyr.
            TestConfig(test_name="uart", skip_for_zephyr=True),
            TestConfig(test_name="unaligned_access"),
            TestConfig(test_name="unaligned_access_benchmark"),
            TestConfig(test_name="utils"),
            TestConfig(test_name="utils_str"),
            TestConfig(
                test_name="watchdog",
                # Increase timeout since this executes more slowly in Renode.
                timeout_secs=120,
            ),
            TestConfig(
                config_name="power_utilization_idle",
                test_name="power_utilization",
                apptype_to_use=ApplicationType.PRODUCTION,
                toggle_power=True,
                pre_test_callback=lambda config: power_pre_test(
                    platform=platform, board_config=config, enter_sleep=False
                ),
                post_test_callback=verify_idle_power_utilization,
                finish_regexes=[RW_IMAGE_BOOTED_REGEX],
            ),
            TestConfig(
                config_name="power_utilization_sleep",
                test_name="power_utilization",
                apptype_to_use=ApplicationType.PRODUCTION,
                toggle_power=True,
                pre_test_callback=lambda config: power_pre_test(
                    platform=platform, board_config=config, enter_sleep=True
                ),
                post_test_callback=verify_sleep_power_utilization,
                finish_regexes=[RW_IMAGE_BOOTED_REGEX],
            ),
        ]

        # Run unaligned access tests for all boards and RO versions.
        for variant_name, variant_info in board_config.variants.items():
            tests.append(
                TestConfig(
                    config_name="unaligned_access_" + variant_name,
                    test_name="unaligned_access",
                    fail_regexes=[
                        SINGLE_CHECK_FAILED_REGEX,
                        ALL_TESTS_FAILED_REGEX,
                    ],
                    ro_image=variant_info.get("ro_image_path"),
                    build_board=variant_info.get("build_board"),
                )
            )

        # Run panic data test for all boards and RO versions.
        for variant_name, variant_info in board_config.variants.items():
            tests.append(
                TestConfig(
                    config_name="panic_data_" + variant_name,
                    test_name="panic_data",
                    fail_regexes=[
                        SINGLE_CHECK_FAILED_REGEX,
                        ALL_TESTS_FAILED_REGEX,
                    ],
                    ro_image=variant_info.get("ro_image_path"),
                    build_board=variant_info.get("build_board"),
                )
            )

        return tests

    @staticmethod
    def get_private_tests() -> list[TestConfig]:
        """Return private test configs for the specified board, if available."""
        tests = []
        try:
            current_dir = os.path.dirname(__file__)
            private_dir = os.path.join(
                current_dir, os.pardir, os.pardir, "ec-private/test"
            )
            have_private = os.path.isdir(private_dir)
            if not have_private:
                return []
            sys.path.append(private_dir)
            import private_tests  # pylint: disable=import-error,import-outside-toplevel

            for test_args in private_tests.tests:
                tests.append(TestConfig(**test_args))
        # Catch all exceptions to avoid disruptions in public repo
        except BaseException as exception:  # pylint: disable=broad-except
            logging.debug(
                "Failed to get list of private tests: %s", str(exception)
            )
            logging.debug("Ignore error and continue.")
            return []
        return tests

    @staticmethod
    def get_zephyr_tests() -> list[TestConfig]:
        """Return Zephyr upstream test configs."""
        # Make sure proper paths are added in the twister script, see ZEPHYR_TEST_PATHS
        tests = [
            # TODO(b/380492754): Fix compilation.
            TestConfig(
                zephyr_name="cpp.main.newlib",
                test_name="zephyr_cpp_newlib",
            ),
            # TODO(b/380491850): Test hangs.
            # TestConfig(
            #    zephyr_name="cpp.main.cpp20",
            #    test_name="zephyr_cpp_std20",
            # ),
            TestConfig(
                zephyr_name="drivers.entropy",
                test_name="zephyr_drivers_entropy",
            ),
            TestConfig(
                zephyr_name="drivers.flash.stm32.f4",
                test_name="zephyr_flash_stm32f4",
                exclude_boards=[DARTMONKEY, HELIPILOT],
            ),
            TestConfig(
                zephyr_name="drivers.flash.stm32.f4.block_registers",
                test_name="zephyr_flash_stm32f4_block_registers",
                exclude_boards=[DARTMONKEY, HELIPILOT],
            ),
            TestConfig(
                zephyr_name="drivers.counter.basic_api.stm32_subsec",
                test_name="zephyr_counter_basic_api_stm32_subsec",
                exclude_boards=[DARTMONKEY, HELIPILOT],
                timeout_secs=60,
            ),
            TestConfig(
                zephyr_name="kernel.poll",
                test_name="zephyr_kernel_poll",
            ),
        ]

        for test in tests:
            test.finish_regexes = [
                ALL_TESTS_PASSED_REGEX_ZEPHYR,
                ALL_TESTS_FAILED_REGEX_ZEPHYR,
            ]

        return tests


BLOONCHIPPER_CONFIG = BoardConfig(
    name=BLOONCHIPPER,
    sensor_type=FPSensorType.FPC,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    # TODO(b/410057326): add polling logic to reboot to RW, then decrease this timeout to 2s.
    reboot_timeout=4.0,
    rollback_region0_regex=DATA_ACCESS_VIOLATION_8020000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_8040000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_20000000_REGEX,
    fp_power_supply="ppvar_fp_mw",
    mcu_power_supply="ppvar_mcu_mw",
    expected_fp_power=PowerUtilization(
        idle=RangedValue(0.71, 0.53), sleep=RangedValue(0.69, 0.51)
    ),
    expected_mcu_power=PowerUtilization(
        idle=RangedValue(16.05, 0.14 * 2), sleep=RangedValue(0.53, 0.35 * 2)
    ),
    expected_fp_power_zephyr=PowerUtilization(
        idle=RangedValue(0.17, 0.04), sleep=RangedValue(0.17, 0.04)
    ),
    expected_mcu_power_zephyr=PowerUtilization(
        idle=RangedValue(14.10, 0.14 * 2), sleep=RangedValue(0.28, 0.04)
    ),
    variants={
        "bloonchipper_v2.0.4277": {
            "ro_image_path": BLOONCHIPPER_V4277_IMAGE_PATH
        },
        "bloonchipper_v2.0.5938": {
            "ro_image_path": BLOONCHIPPER_V5938_IMAGE_PATH
        },
    },
    zephyr_board_name="google_dragonclaw",
)

DARTMONKEY_CONFIG = BoardConfig(
    name=DARTMONKEY,
    sensor_type=FPSensorType.FPC,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    reboot_timeout=1.0,
    rollback_region0_regex=DATA_ACCESS_VIOLATION_80C0000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_80E0000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_24000000_REGEX,
    fp_power_supply="ppvar_fp_mw",
    mcu_power_supply="ppvar_mcu_mw",
    expected_fp_power=PowerUtilization(
        idle=RangedValue(0.03, 0.05), sleep=RangedValue(0.03, 0.05)
    ),
    expected_mcu_power=PowerUtilization(
        idle=RangedValue(42.67, 0.35 * 2), sleep=RangedValue(4.46, 0.23 * 2)
    ),
    # For dartmonkey board, run panic data test also on nocturne_fp and
    # nami_fp boards with appropriate RO image.
    variants={
        "dartmonkey_v2.0.2887": {"ro_image_path": DARTMONKEY_IMAGE_PATH},
        "nocturne_fp_v2.2.64": {
            "ro_image_path": NOCTURNE_FP_IMAGE_PATH,
            "build_board": "nocturne_fp",
        },
        "nami_fp_v2.2.144": {
            "ro_image_path": NAMI_FP_IMAGE_PATH,
            "build_board": "nami_fp",
        },
    },
    zephyr_board_name="google_icetower",
)

HELIPILOT_CONFIG = BoardConfig(
    name=HELIPILOT,
    sensor_type=FPSensorType.FPC,
    servo_uart_name="raw_fpmcu_console_uart_pty",
    servo_power_enable="fpmcu_pp3300",
    reboot_timeout=3,
    rollback_region0_regex=DATA_ACCESS_VIOLATION_64020000_REGEX,
    rollback_region1_regex=DATA_ACCESS_VIOLATION_64030000_REGEX,
    mpu_regex=DATA_ACCESS_VIOLATION_200B0000_REGEX,
    fp_power_supply="ppvar_fp_mw",
    mcu_power_supply="pp3300_mcu_mw",
    # The original power utilization numbers were experimentally derived via
    # onboard ADCs and verified with a DMM on one dev board. However, we have
    # not formally measured the power variance across multiple dev boards.
    # Without knowing the true mean, we have just expanded the tolerance
    # parameter of the RangedValue.
    expected_fp_power=PowerUtilization(
        idle=RangedValue(0.0, 0.1), sleep=RangedValue(0.0, 0.1)
    ),
    expected_mcu_power=PowerUtilization(
        idle=RangedValue(34.8, 7.0), sleep=RangedValue(2.7, 2.5)
    ),
    expected_fp_power_zephyr=PowerUtilization(
        idle=RangedValue(0.0, 0.1), sleep=RangedValue(0.0, 0.1)
    ),
    expected_mcu_power_zephyr=PowerUtilization(
        idle=RangedValue(34.8, 7.0), sleep=RangedValue(2.7, 2.5)
    ),
    variants={
        "helipilot_v2.0.24337": {"ro_image_path": HELIPILOT_IMAGE_PATH},
        "buccaneer_v2.0.26328": {
            "ro_image_path": BUCCANEER_IMAGE_PATH,
            "build_board": "buccaneer",
        },
    },
    zephyr_board_name="google_quincy",
)

BUCCANEER_CONFIG = copy.deepcopy(HELIPILOT_CONFIG)
BUCCANEER_CONFIG.name = BUCCANEER
BUCCANEER_CONFIG.sensor_type = FPSensorType.ELAN
BUCCANEER_CONFIG.mpu_regex = DATA_ACCESS_VIOLATION_200A8000_REGEX
# The Elan 80SG is said to have the following power profile:
# - power down mode current draw is less than 12 uA (0.0396 mW)
# - finger detection with 40ms scan rate current draw less than 20 uA (0.066 mW)
# - fingerprint sensing current draw less than 8.11 mA (26.763 mW)
BUCCANEER_CONFIG.fp_power_supply = "pp3300_fp_mw"
# 0.25 mW is roughly 76 uA @ 3.3V.
BUCCANEER_CONFIG.expected_fp_power = PowerUtilization(
    idle=RangedValue(0.25, 0.3), sleep=RangedValue(0.25, 0.3)
)

GWENDOLIN_CONFIG = copy.deepcopy(HELIPILOT_CONFIG)
GWENDOLIN_CONFIG.name = GWENDOLIN
GWENDOLIN_CONFIG.sensor_type = FPSensorType.EGIS
GWENDOLIN_CONFIG.mpu_regex = DATA_ACCESS_VIOLATION_20098000_REGEX

BOARD_CONFIGS = {
    "bloonchipper": BLOONCHIPPER_CONFIG,
    "buccaneer": BUCCANEER_CONFIG,
    "dartmonkey": DARTMONKEY_CONFIG,
    "gwendolin": GWENDOLIN_CONFIG,
    "helipilot": HELIPILOT_CONFIG,
}


def read_file_gsutil(path: str) -> bytes:
    """Get data from bucket, using gsutil tool"""
    cmd = ["gsutil", "cat", path]

    logging.debug('Running command: "%s"', " ".join(cmd))
    gsutil = subprocess.run(cmd, stdout=subprocess.PIPE, check=False)
    gsutil.check_returncode()

    return gsutil.stdout


def find_section_offset_size(section: str, image: bytes) -> tuple[int, int]:
    """Get offset and size of the section in image"""
    areas = fmap.fmap_decode(image)["areas"]
    area = next(area for area in areas if area["name"] == section)
    return area["offset"], area["size"]


def read_section(src: bytes, section: str) -> bytes:
    """Read FMAP section content into byte array"""
    (src_start, src_size) = find_section_offset_size(section, src)
    src_end = src_start + src_size
    return src[src_start:src_end]


def write_section(data: bytes, image: bytearray, section: str):
    """Replace the specified section in image with the contents of data"""
    (section_start, section_size) = find_section_offset_size(section, image)

    if section_size < len(data):
        raise ValueError(section + " section size is not enough to store data")

    section_end = section_start + section_size
    filling = bytes([0xFF for _ in range(section_size - len(data))])

    image[section_start:section_end] = data + filling


def copy_section(src: bytes, dst: bytearray, section: str):
    """Copy section from src image to dst image"""
    (src_start, src_size) = find_section_offset_size(section, src)
    (dst_start, dst_size) = find_section_offset_size(section, dst)

    if dst_size < src_size:
        raise ValueError(
            "Section " + section + " from source image has "
            "greater size than the section in destination image"
        )

    src_end = src_start + src_size
    dst_end = dst_start + dst_size
    filling = bytes([0xFF for _ in range(dst_size - src_size)])

    dst[dst_start:dst_end] = src[src_start:src_end] + filling


def replace_ro(image: bytearray, ro_section: bytes):
    """Replace RO in image with provided one"""
    # Backup RO public key since its private part was used to sign RW.
    ro_pubkey = read_section(image, "KEY_RO")

    # Copy RO part of the firmware to the image. Please note that RO public key
    # is copied too since EC_RO area includes KEY_RO area.
    copy_section(ro_section, image, "EC_RO")

    # Restore RO public key.
    write_section(ro_pubkey, image, "KEY_RO")


def set_sleep_mode(enter_sleep: bool) -> bool:
    """Enters or exists sleep mode based on enter_sleep parameter"""
    sleep_mode = "on" if enter_sleep else "off"
    cmd = [
        "dut-control",
        f"fpmcu_slp:{sleep_mode}",
    ]

    logging.debug('Running command: "%s"', cmd)
    proc = subprocess.run(cmd, check=False)
    return proc.returncode == 0


def power_cycle(platform: Platform, board_config: BoardConfig) -> None:
    """power_cycle the boards."""
    logging.debug("power_cycling board")
    platform.power(board_config, power_on=False)
    time.sleep(board_config.reboot_timeout)
    platform.power(board_config, power_on=True)
    time.sleep(board_config.reboot_timeout)


def fp_sensor_sel(
    platform: Platform,
    board_config: BoardConfig,
    sensor_type: Optional[FPSensorType] = None,
) -> bool:
    """
    Explicitly select the appropriate fingerprint sensor.
    This function assumes that the fp_sensor_sel servo control is connected to
    the proper gpio on the development board. This is not the case on some
    older development boards. This should not result in any failures but also
    may have not actually changed the selected sensor.
    """

    if sensor_type is None:
        sensor_type = board_config.sensor_type

    cmd = [
        "dut-control",
        "fp_sensor_sel" + ":" + str(sensor_type.value),
    ]

    logging.debug('Running command: "%s"', " ".join(cmd))
    proc = subprocess.run(cmd, check=False)

    if proc.returncode == 0:
        # power cycle after setting sensor type to ensure detection
        power_cycle(platform, board_config)
        return True

    return False


def power_pre_test(
    platform: Platform, board_config: BoardConfig, enter_sleep: bool
) -> bool:
    """
    Prepare a board for a power_utilization test
    """

    if not set_sleep_mode(enter_sleep):
        return False

    return fp_sensor_sel(platform, board_config)


def build_ec(
    test_name: str,
    board_name: str,
    compiler: str,
    app_type: ApplicationType,
) -> list[str]:
    """Prepare a command to build test using CrosEC"""
    cmd = ["make"]
    if compiler == CLANG:
        cmd = cmd + ["CC=arm-none-eabi-clang"]
    cmd = cmd + [
        "BOARD=" + board_name,
        "-j",
    ]
    # If the image type is a test image, then apply test- prefix to the target name
    if app_type == ApplicationType.TEST:
        cmd = cmd + [
            "test-" + test_name,
        ]

    return cmd


def build_zephyr_upstream(test_name: str, board_name: str) -> list[str]:
    """Prepare a command to build Zephyr test"""
    # Build only with Zephyr and clobber a previous build
    cmd = [ZEPHYR_TWISTER] + ["-b"] + ["-c"]
    cmd = cmd + ["-p"] + [board_name]
    cmd = cmd + ["-O"] + [ZEPHYR_TWISTER_BUILD_DIR]
    cmd = cmd + ["-s"] + [test_name]
    cmd = cmd + ["--no-upload-cros-rdb"]

    return cmd


def build_zephyr(test: TestConfig, board_name: str) -> list[str]:
    """Prepare a command to build test using Zephyr"""
    if test.zephyr_name is not None:
        return build_zephyr_upstream(test.zephyr_name, board_name)

    test_name = test.test_name
    app_type = test.apptype_to_use
    img_type = test.imagetype_to_use

    cmd = ["zmake"] + ["build"]
    cmd = cmd + [board_name] + ["--clobber"]
    if app_type != ApplicationType.TEST:
        return cmd

    # Create tmp file to pass needed configs
    test_conf = os.path.join(tempfile.gettempdir(), "test.conf")
    with open(test_conf, "w", encoding="utf-8") as f_test_config:
        cmd = cmd + ["-DOVERLAY_CONFIG=" + test_conf]
        # Get list of supported tests
        testcase = os.path.join(ZEPHYR_FPMCU_DIR, "testcase.yaml")
        with open(testcase, encoding="utf-8") as testcase:
            lines = testcase.read()
        testcase_data = yaml.load(lines, Loader=yaml.SafeLoader)
        if "tests" not in testcase_data:
            raise ValueError(
                'testcase.yaml file doesn\'t contain "tests" section'
            )
        # Make sure the current test is supported
        if test_name not in testcase_data["tests"]:
            raise ValueError(test_name + " not present in testcase.yaml")
        # Add configs needed for a specific test from "extra_conf_files"
        # and "extra_configs" sections
        if "extra_conf_files" in testcase_data["tests"][test_name]:
            for config_file in testcase_data["tests"][test_name][
                "extra_conf_files"
            ]:
                config_file_path = os.path.join(ZEPHYR_FPMCU_DIR, config_file)
                with open(
                    config_file_path, "r", encoding="utf-8"
                ) as f_config_file:
                    f_test_config.write(f_config_file.read())
        if "extra_configs" in testcase_data["tests"][test_name]:
            for config in testcase_data["tests"][test_name]["extra_configs"]:
                f_test_config.write(config + "\n")
        # Include tests also in RO part. It is not done by default
        # because of lack of space
        if img_type == ImageType.RO:
            f_test_config.write("CONFIG_HW_TEST_RW_ONLY=n\n")

    return cmd


def build(
    test: TestConfig,
    board_name: str,
    compiler: str,
    zephyr: bool,
) -> None:
    """Build specified test for specified board."""
    if zephyr:
        cmd = build_zephyr(test, board_name)
    else:
        cmd = build_ec(
            test.test_name, board_name, compiler, test.apptype_to_use
        )

    logging.debug('Running command: "%s"', " ".join(cmd))
    subprocess.run(cmd, check=False).check_returncode()


def patch_image(test: TestConfig, image_path: str):
    """Replace RO part of the firmware with provided one."""
    with open(image_path, "rb+") as image_file:
        image = bytearray(image_file.read())
        ro_section = read_file_gsutil(test.ro_image)
        replace_ro(image, ro_section)
        image_file.seek(0)
        image_file.write(image)
        image_file.truncate()


def readline(
    executor: ThreadPoolExecutor, file: BinaryIO, timeout_secs: int
) -> Optional[bytes]:
    """Read a line with timeout."""
    future = executor.submit(file.readline)
    try:
        return future.result(timeout_secs)
    except concurrent.futures.TimeoutError:
        return None


def readlines_until_timeout(
    executor, file: BinaryIO, timeout_secs: int
) -> list[bytes]:
    """Continuously read lines for timeout_secs."""
    lines: list[bytes] = []
    while True:
        line = readline(executor, file, timeout_secs)
        if not line:
            return lines
        lines.append(line)


def process_console_output_line(line: bytes, test: TestConfig):
    """Parse console output line and update test pass/fail counters."""
    try:
        line_str = line.decode()

        if SINGLE_CHECK_PASSED_REGEX.match(line_str):
            test.num_passes += 1

        for regex in test.fail_regexes:
            if regex.match(line_str):
                test.num_fails += 1
                break

        return line_str
    except UnicodeDecodeError:
        # Sometimes we get non-unicode from the console (e.g., when the
        # board reboots.) Not much we can do in this case, so we'll just
        # ignore it.
        return None


def run_test_ec(test: TestConfig) -> str:
    """Prepare a command to run test on CrosEC"""
    test_cmd = "runtest " + " ".join(test.test_args) + "\n"

    return test_cmd


def run_test_zephyr(test: TestConfig) -> str:
    """Prepare a command to run test on Zephyr"""
    # Zephyr upstream tests run automatically
    if test.zephyr_name:
        return []

    # TODO(b/382705460): This extra command is to work around an issue where
    # sometimes there is a missing character in the test command: "zest"
    # instead of "ztest".
    test_cmd = "\n\n\n\n\n\n"

    if len(test.test_args) == 0:
        # If there are no args just run-all not to be limited by suite name
        test_cmd += "ztest run-all\n"
    else:
        # ZTEST console doesn't support passing test arguments
        # Assume a testsuite for every test + arg combination
        test_cmd += "ztest run-testcase " + test.test_name
        for test_arg in test.test_args:
            test_cmd = test_cmd + "_" + test_arg
        test_cmd = test_cmd + "\n"

    return test_cmd


def run_test(
    test: TestConfig,
    board_config: BoardConfig,
    console: io.FileIO,
    executor: ThreadPoolExecutor,
    zephyr: bool,
) -> bool:
    """Run specified test."""
    start = time.time()

    reboot_timeout = board_config.reboot_timeout
    logging.debug("Calling pre-test callback")
    if not test.pre_test_callback(board_config):
        logging.error("pre-test callback failed, aborting")
        return False

    # Wait for boot to finish
    time.sleep(reboot_timeout)

    if test.apptype_to_use != ApplicationType.PRODUCTION:
        console.write("\n".encode())

    if test.imagetype_to_use == ImageType.RO:
        console.write("reboot ro\n".encode())
        time.sleep(reboot_timeout)

    # Skip runtest if using standard app type
    if test.apptype_to_use != ApplicationType.PRODUCTION:
        if zephyr:
            test_cmd = run_test_zephyr(test)
        else:
            test_cmd = run_test_ec(test)

        if len(test_cmd) > 0:
            console.write(test_cmd.encode())

    while True:
        console.flush()

        elapsed_secs = time.time() - start
        remaining_secs = int(test.timeout_secs - elapsed_secs)
        if remaining_secs <= 0:
            logging.debug("Test timed out")
            return False

        line = readline(executor, console, remaining_secs)
        if not line:
            continue

        test.logs.append(line)
        # Look for test_print_result() output (success or failure)
        line_str = process_console_output_line(line, test)
        if line_str is None:
            # Sometimes we get non-unicode from the console (e.g., when the
            # board reboots.) Not much we can do in this case, so we'll just
            # ignore it and print log the line as is.
            logging.debug(line)
            continue

        logging.debug(line_str.rstrip())

        for finish_re in test.finish_regexes:
            if finish_re.match(line_str):
                # flush read the remaining
                lines = readlines_until_timeout(executor, console, 1)
                logging.debug(lines)
                test.logs.append(lines)

                for line in lines:
                    process_console_output_line(line, test)

                logging.debug("Calling post-test callback")
                post_cb_passed = test.post_test_callback(board_config)
                return test.num_fails == 0 and post_cb_passed


def get_test_list(
    platform: Platform,
    config: BoardConfig,
    test_args,
    with_private: str,
    zephyr: bool,
) -> list[TestConfig]:
    """Get a list of tests to run."""
    if test_args == "all":
        return AllTests.get(platform, config, with_private, zephyr)

    test_list = []
    for test in test_args:
        logging.debug("test: %s", test)
        test_regex = re.compile(test)
        tests = [
            test
            for test in AllTests.get(platform, config, with_private, zephyr)
            if test_regex.fullmatch(test.config_name)
        ]
        if not tests:
            logging.error(
                'Test "%s" is either not configured or not supported on board "%s"',
                test,
                config.name,
            )
            sys.exit(1)
        test_list += tests

    return test_list


def get_zephyr_image_path(test: TestConfig, build_board: str):
    """Get a path to a Zephyr built image"""
    if test.zephyr_name is not None:
        # The path to binary differs depending on a test name, path and platform,
        # so just find the zephyr.bin in the build dir.
        twister_out = os.walk(ZEPHYR_TWISTER_BUILD_DIR)
        image_path = None
        for dirpath, _, filenames in twister_out:
            for file in filenames:
                if file == "zephyr.bin":
                    image_path = os.path.join(dirpath, "zephyr.bin")
                    break
            if image_path is not None:
                break
    else:
        image_path = os.path.join(
            EC_DIR, "build", "zephyr", build_board, "output", "ec.bin"
        )

    return image_path


def get_image_path(test: TestConfig, build_board: str, zephyr: bool):
    """Get a path to a built image"""
    if zephyr:
        return get_zephyr_image_path(test, build_board)

    if test.apptype_to_use == ApplicationType.PRODUCTION:
        image_path = os.path.join(EC_DIR, "build", build_board, "ec.bin")
    else:
        image_path = os.path.join(
            EC_DIR,
            "build",
            build_board,
            test.test_name,
            test.test_name + ".bin",
        )

    return image_path


def flash_and_run_test(
    test: TestConfig,
    platform: Platform,
    board_config: BoardConfig,
    args: argparse.Namespace,
    executor,
) -> bool:
    """Run a single test using the test and board configuration specified"""
    build_board = board_config.name
    # If test provides this information, build image for board specified
    # by test. Also if a test is in Zephyr upstream use the dev board name.
    if test.build_board is not None:
        build_board = test.build_board
    elif test.zephyr_name is not None:
        build_board = board_config.zephyr_board_name

    # attempt to build test binary, reporting a test failure on error
    try:
        build(
            test,
            build_board,
            args.compiler,
            args.zephyr,
        )
    except Exception as exception:  # pylint: disable=broad-except
        logging.error("failed to build %s: %s", test.test_name, exception)
        return False

    image_path = get_image_path(test, build_board, args.zephyr)

    logging.debug("image_path: %s", image_path)

    if test.ro_image is not None:
        try:
            patch_image(test, image_path)
        except Exception as exception:  # pylint: disable=broad-except
            logging.warning(
                "An exception occurred while patching image: %s", exception
            )
            return False

    # Get the console file before flashing to listen ASAP after flashing.
    console_pty = platform.get_console(board_config)

    # flash test binary
    if not platform.flash(
        board_config,
        image_path,
        args.flasher,
        args.remote,
        args.jlink_port,
        test.test_name,
        test.enable_hw_write_protect,
        args.zephyr,
    ):
        logging.debug("Flashing failed")
        return False

    with ExitStack() as stack:
        if args.remote and args.console_port:
            console_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            console_socket.connect((args.remote, args.console_port))
            console = stack.enter_context(
                console_socket.makefile(mode="rwb", buffering=0)
            )
        else:
            # pylint: disable-next=consider-using-with
            console_file = open(console_pty, "wb+", buffering=0)
            console = stack.enter_context(console_file)

        platform.hw_write_protect(test.enable_hw_write_protect)

        if test.toggle_power:
            power_cycle(platform, board_config)
        else:
            # In some cases flash_ec leaves the board off, so just ensure it is on
            platform.power(board_config, power_on=True)

        # run the test
        logging.info('Running test: "%s"', test.config_name)

        ret = run_test(
            test,
            board_config,
            console,
            executor=executor,
            zephyr=args.zephyr,
        )

        platform.cleanup()

        return ret


def parse_remote_arg(remote: str) -> str:
    """Convert the 'remote' input argument to IP address, if available."""
    if not remote:
        return ""

    try:
        ip_addr = socket.gethostbyname(remote)
        return ip_addr
    except socket.gaierror:
        logging.error('Failed to resolve host "%s".', remote)
        sys.exit(1)


def validate_args_combination(args: argparse.Namespace):
    """Check that the current combination of arguments is supported.

    Not all combinations of command line arguments are valid or currently
    supported. If tests can't be executed, print and error message and exit.
    """
    if args.jlink_port and not args.flasher == JTRACE:
        logging.error("jlink_port specified, but flasher is not set to J-Link.")
        sys.exit(1)

    if args.remote and not (args.jlink_port or args.console_port):
        logging.error(
            "jlink_port or console_port must be specified when using "
            "the remote option."
        )
        sys.exit(1)

    if (args.jlink_port or args.console_port) and not args.remote:
        logging.error(
            "The remote option must be specified when using the "
            "jlink_port or console_port options."
        )
        sys.exit(1)

    if args.remote and args.flasher == SERVO_MICRO:
        logging.error(
            "The remote option is not supported when flashing with servo "
            "micro. Use J-Link instead or flash with a local servo micro."
        )
        sys.exit(1)

    if args.board not in BOARD_CONFIGS:
        logging.error('Unable to find a config for board: "%s"', args.board)
        sys.exit(1)


def main():
    """Run unit tests on device and displays the results."""
    parser = argparse.ArgumentParser()

    default_board = "bloonchipper"
    parser.add_argument(
        "--board",
        "-b",
        help="Board (default: " + default_board + ")",
        default=default_board,
    )

    default_tests = "all"
    parser.add_argument(
        "--tests",
        "-t",
        nargs="+",
        help="Tests (default: " + default_tests + ")",
        default=default_tests,
    )

    log_level_choices = ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
    parser.add_argument(
        "--log_level", "-l", choices=log_level_choices, default="DEBUG"
    )

    flasher_choices = [SERVO_MICRO, JTRACE]
    parser.add_argument(
        "--flasher", "-f", choices=flasher_choices, default=JTRACE
    )

    compiler_options = [GCC, CLANG]
    parser.add_argument(
        "--compiler", "-c", choices=compiler_options, default=GCC
    )

    # This might be expanded to serve as a "remote" for flash_ec also, so
    # we will leave it generic.
    parser.add_argument(
        "--remote",
        "-n",
        help="The remote host connected to one or both of: J-Link and Servo.",
        type=parse_remote_arg,
    )

    parser.add_argument(
        "--jlink_port",
        "-j",
        type=int,
        help="The port to use when connecting to JLink.",
    )
    parser.add_argument(
        "--console_port",
        "-p",
        type=int,
        help="The port connected to the FPMCU console.",
    )

    with_private_choices = [PRIVATE_YES, PRIVATE_NO, PRIVATE_ONLY]
    parser.add_argument(
        "--with_private", choices=with_private_choices, default=PRIVATE_YES
    )

    parser.add_argument(
        "--zephyr", help="Use Zephyr build", action="store_true"
    )

    parser.add_argument(
        "--renode", help="Run tests with Renode emulator", action="store_true"
    )

    args = parser.parse_args()
    logging.basicConfig(
        format="%(levelname)s:%(message)s", level=args.log_level
    )
    validate_args_combination(args)

    board_config = BOARD_CONFIGS[args.board]
    # Use expected values for Zephyr
    if args.zephyr:
        board_config.expected_fp_power = board_config.expected_fp_power_zephyr
        board_config.expected_mcu_power = board_config.expected_mcu_power_zephyr

    if args.renode:
        platform = Renode()
    else:
        platform = Hardware()

    test_list = get_test_list(
        platform, board_config, args.tests, args.with_private, args.zephyr
    )
    logging.debug("Running tests: %s", [test.config_name for test in test_list])

    with ThreadPoolExecutor(max_workers=1) as executor:
        for test in test_list:
            if (test.skip_for_zephyr and args.zephyr) or platform.skip_test(
                test.test_name, board_config, args.zephyr
            ):
                continue
            test.passed = flash_and_run_test(
                test, platform, board_config, args, executor
            )

        colorama.init()
        exit_code = 0
        for test in test_list:
            # print results
            print('Test "' + test.config_name + '": ', end="")
            if (test.skip_for_zephyr and args.zephyr) or platform.skip_test(
                test.test_name, board_config, args.zephyr
            ):
                print(colorama.Fore.YELLOW + "SKIPPED")
            else:
                if test.passed:
                    print(colorama.Fore.GREEN + "PASSED")
                else:
                    print(colorama.Fore.RED + "FAILED")
                    exit_code = 1

            print(colorama.Style.RESET_ALL)

        if exit_code != 0:
            print(
                f"Tests failed for {args.board}"
                f'{" Zephyr" if args.zephyr else ""}'
                f'{" Renode" if args.renode else ""}'
            )
        # TODO(b/368684364): Fix the underlying issue that prevents sys.exit()
        # from working correctly.
        os._exit(exit_code)  # pylint: disable=protected-access


def get_power_utilization(
    board_config: BoardConfig,
) -> tuple[Optional[float], Optional[float]]:
    """Retrieve board power utilization data"""
    fp_power_signal = board_config.fp_power_supply
    mcu_power_signal = board_config.mcu_power_supply
    cmd = [
        "dut-control",
        "--value_only",  # only the summary will print the field names
        "-t",
        "10",  # sample time in seconds
        fp_power_signal,
        mcu_power_signal,
    ]

    logging.debug('Running command: "%s"', " ".join(cmd))

    fp_power_mw = None
    mcu_power_mw = None
    with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
        for line in io.TextIOWrapper(proc.stdout):  # type: ignore[arg-type]
            # Only the summary is required and those lines start with @@ and have 6 other fields
            # (NAME, COUNT, AVERAGE, STDDEV, MAX, MIN)
            response = line.split()
            if len(response) != 7 or response[0] != "@@":
                continue

            resp_name, _, resp_avg, _, _, _ = response[1:]

            if resp_name == fp_power_signal:
                fp_power_mw = float(resp_avg.strip())
            elif resp_name == mcu_power_signal:
                mcu_power_mw = float(resp_avg.strip())

            logging.debug(
                "fp_power_mw:%s \t mcu_power_mw:%s", fp_power_mw, mcu_power_mw
            )

            if fp_power_mw is not None and mcu_power_mw is not None:
                return (fp_power_mw, mcu_power_mw)

    logging.error("Failed to receive required power data from FPMCU")
    return (None, None)


def verify_power_utilization(
    fp_power_mw: float,
    mcu_power_mw: float,
    fp_expected: RangedValue,
    mcu_expected: RangedValue,
) -> bool:
    """Get the name of the console for a given board."""

    if fp_power_mw is not None and mcu_power_mw is not None:
        fp_mw_delta = abs(fp_power_mw - fp_expected.nominal)
        mcu_mw_delta = abs(mcu_power_mw - mcu_expected.nominal)

        logging.info(
            "fp power:\tactual: %0.2f expected: %0.2f threshold: +/-%0.2f",
            fp_power_mw,
            fp_expected.nominal,
            fp_expected.range,
        )
        logging.info(
            "mcu power:\tactual: %0.2f expected: %0.2f threshold: +/-%0.2f",
            mcu_power_mw,
            mcu_expected.nominal,
            mcu_expected.range,
        )

        return (
            fp_mw_delta <= fp_expected.range
            and mcu_mw_delta <= mcu_expected.range
        )

    logging.error("Failed to receive required power data from FPMCU")
    return False


def verify_idle_power_utilization(board_config: BoardConfig) -> bool:
    """Verifies that idle power utilization is within range for the specified board"""

    fp_power_mw, mcu_power_mw = get_power_utilization(board_config)
    return verify_power_utilization(
        fp_power_mw,
        mcu_power_mw,
        board_config.expected_fp_power.idle,
        board_config.expected_mcu_power.idle,
    )


def verify_sleep_power_utilization(board_config: BoardConfig) -> bool:
    """Verifies that sleep power utilization is within range for the specified board"""

    fp_power_mw, mcu_power_mw = get_power_utilization(board_config)
    ret = verify_power_utilization(
        fp_power_mw,
        mcu_power_mw,
        board_config.expected_fp_power.sleep,
        board_config.expected_mcu_power.sleep,
    )

    # Make sure to exit sleep mode!
    set_sleep_mode(False)
    return ret


if __name__ == "__main__":
    sys.exit(main())
