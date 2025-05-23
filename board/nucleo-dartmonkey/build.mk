# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

BASEBOARD:=nucleo-h743zi

board-y=board.o
board-y+=fpsensor_detect.o

# Enable on device tests
test-list-y=\
       abort \
       aes \
       boringssl_crypto \
       compile_time_macros \
       crc \
       debug \
       exception \
       flash_physical \
       flash_write_protect \
       fp_transport \
       fpsensor_auth_crypto_stateful \
       fpsensor_auth_crypto_stateless \
       fpsensor_crypto \
       fpsensor_hw \
       fpsensor_utils \
       libc_printf \
       mpu \
       mutex \
       pingpong \
       printf \
       queue \
       rng_benchmark \
       rollback \
       rollback_entropy \
       rsa3 \
       rtc \
       sbrk \
       scratchpad \
       sha256 \
       sha256_unrolled \
       static_if \
       stdlib \
       timer_dos \
       utils \
       utils_str \
       watchdog \
