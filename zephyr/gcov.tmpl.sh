#!/bin/bash
#
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# shellcheck disable=SC2154
exec ${CMAKE_GCOV} "$@"
