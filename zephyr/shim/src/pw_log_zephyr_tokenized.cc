/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log_core.h>
#include <zephyr/spinlock.h>

#include <pw_log_tokenized/base64.h>
#include <pw_log_tokenized/config.h>
#include <pw_log_tokenized/handler.h>
#include <pw_log_tokenized/metadata.h>

extern "C" {
#include "console.h"
#include "panic_log.h"
#include "zephyr_console_shim.h"
}

namespace pw::log_zephyr
{
namespace
{
	// The Zephyr console may output raw text along with Base64 tokenized
	// messages, which could interfere with detokenization. Output a
	// character to mark the end of a Base64 message.
	// If delimiter changes, make sure to update the following files to
	// match
	//  -- src/third_party/hdctools/servo/ec3po/console.py
	constexpr char kEndDelimiter = '~';

	struct k_spinlock lock;
	k_spinlock_key_t key;
} // namespace

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
					   const uint8_t log_buffer[],
					   size_t size_bytes)
{
	pw::log_tokenized::Metadata meta(metadata);

	/* flags == 0 --> regular Zephyr Logging
	 * flags != 0 --> EC Console Channel Logging offset by 1
	 *                Check if channel is enabled to send log to console
	 */
	if (meta.flags() > 0) {
		if (console_channel_is_disabled(
			    PW_FLAG_TO_EC_CHANNEL(meta.flags()))) {
			return;
		}
	}

	// Encode the tokenized message as Base64.
	InlineBasicString base64_string =
		log_tokenized::PrefixedBase64Encode(log_buffer, size_bytes);

	if (base64_string.empty()) {
		return;
	}

	if (IS_ENABLED(CONFIG_PLATFORM_EC_PANIC_LOG)) {
		panic_log_write_str(base64_string.c_str(),
				    base64_string.size());
	}

	// On DUT, timberslide doesn't receive console raw text, okay to send
	// base64 message without end delimiter
	if (IS_ENABLED(CONFIG_PLATFORM_EC_HOSTCMD_CONSOLE)) {
		console_buf_notify_chars(base64_string.c_str(),
					 base64_string.size());
	}
	base64_string += kEndDelimiter;

	// TODO(asemjonovs):
	// https://github.com/zephyrproject-rtos/zephyr/issues/59454 Zephyr
	// frontend should protect messages from getting corrupted from multiple
	// threads.
	key = k_spin_lock(&lock);
	LOG_PRINTK("%s", base64_string.c_str());
	k_spin_unlock(&lock, key);
}

} // namespace pw::log_zephyr
