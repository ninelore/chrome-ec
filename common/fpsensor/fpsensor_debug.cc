/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "common.h"
#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_console.h"
#include "fpsensor/fpsensor_detect.h"
#include "fpsensor/fpsensor_modes.h"
#include "fpsensor/fpsensor_state.h"
#include "fpsensor/fpsensor_utils.h"
#include "system.h"
#include "util.h"
#include "watchdog.h"

#ifdef CONFIG_ZEPHYR
#include <zephyr/shell/shell.h>
#endif

#ifdef CONFIG_CMD_FPSENSOR_DEBUG
/* --- Debug console commands --- */

/*
 * Send the current Fingerprint buffer to the host
 * it is formatted as an 8-bpp PGM ASCII file.
 *
 * In addition, it prepends a short Z-Modem download signature,
 * which triggers automatically your preferred viewer if you configure it
 * properly in "File transfer protocols" in the Minicom options menu.
 * (as triggered by Ctrl-A O)
 * +--------------------------------------------------------------------------+
 * |     Name             Program             Name U/D FullScr IO-Red. Multi  |
 * | A  zmodem     /usr/bin/sz -vv -b          Y    U    N       Y       Y    |
 *  [...]
 * | L  pgm        /usr/bin/display_pgm        N    D    N       Y       N    |
 * | M  Zmodem download string activates... L                                 |
 *
 * My /usr/bin/display_pgm looks like this:
 * #!/bin/sh
 * TMPF=$(mktemp)
 * ascii-xfr -rdv ${TMPF}
 * display ${TMPF}
 *
 * Alternative (if you're using screen as your terminal):
 *
 * From *outside* the chroot:
 *
 * Install ascii-xfr: sudo apt-get install minicom
 * Install imagemagick: sudo apt-get install imagemagick
 *
 * Add the following to your ${HOME}/.screenrc:
 *
 * zmodem catch
 * zmodem recvcmd '!!! bash -c "ascii-xfr -rdv /tmp/finger.pgm && display
 * /tmp/finger.pgm"'
 *
 * From *outside the chroot*, use screen to connect to UART console:
 *
 * sudo screen -c ${HOME}/.screenrc /dev/pts/NN 115200
 *
 */
test_export_static enum ec_error_list upload_pgm_image(uint8_t *frame,
						       uint8_t bpp)
{
	uint8_t *ptr = frame;
	uint8_t bytes_per_pixel = DIV_ROUND_UP(bpp, 8);

	if (bytes_per_pixel != 1 && bytes_per_pixel != 2) {
		return EC_ERROR_UNKNOWN;
	}

	/* fake Z-modem ZRQINIT signature */
	CPRINTF("#IGNORE for ZModem\r**\030B00");
	crec_msleep(2000); /* let the download program start */

	/* Print 8-bpp or 16-bpp PGM ASCII header */
	CPRINTF("P2\n%d %d\n%d\n", FP_SENSOR_RES_X, FP_SENSOR_RES_Y,
		(bytes_per_pixel == 2) ? 65535 : 255);

	for (int y = 0; y < FP_SENSOR_RES_Y; y++) {
		watchdog_reload();
		for (int x = 0; x < FP_SENSOR_RES_X;
		     x++, ptr += bytes_per_pixel) {
			CPRINTF("%d ", (bytes_per_pixel == 2) ?
					       *(uint16_t *)ptr :
					       *ptr);
		}
		CPRINTF("\n");
		cflush();
	}

	CPRINTF("\x04"); /* End Of Transmission */
	return EC_SUCCESS;
}

static enum ec_error_list fp_console_action(uint32_t mode)
{
	if (!(global_context.sensor_mode & FP_MODE_RESET_SENSOR))
		CPRINTS("Waiting for finger ...");

	uint32_t mode_output = 0;
	const int rc = fp_set_sensor_mode(mode, &mode_output);

	if (rc != EC_RES_SUCCESS) {
		/*
		 * EC host command errors do not directly map to console command
		 * errors.
		 */
		return EC_ERROR_UNKNOWN;
	}

	int tries = 200;
	while (tries--) {
		if (!(global_context.sensor_mode & FP_MODE_ANY_CAPTURE)) {
			CPRINTS("done (events:%x)",
				static_cast<int>(global_context.fp_events));
			return EC_SUCCESS;
		}
		crec_usleep(100 * MSEC);
	}
	return EC_ERROR_TIMEOUT;
}

test_export_static uint8_t get_sensor_bpp(void)
{
#if defined(HAVE_FP_PRIVATE_DRIVER) || defined(BOARD_HOST)
	ec_response_fp_info info;
	if (fp_sensor_get_info(&info) < 0) {
		return EC_ERROR_UNKNOWN;
	}
	return info.bpp;
#else
	return EC_ERROR_UNKNOWN;
#endif
}

static int command_fpcapture(int argc, const char **argv)
{
	if (system_is_locked())
		return EC_ERROR_ACCESS_DENIED;

	int capture_type = FP_CAPTURE_SIMPLE_IMAGE;

	if (argc >= 2) {
		char *e;

		capture_type = strtoi(argv[1], &e, 0);
		if (*e || capture_type < 0 ||
		    capture_type >= FP_CAPTURE_TYPE_MAX)
			return EC_ERROR_PARAM1;
	}
	const uint32_t mode = FP_MODE_CAPTURE |
			      ((capture_type << FP_MODE_CAPTURE_TYPE_SHIFT) &
			       FP_MODE_CAPTURE_TYPE_MASK);

	const enum ec_error_list rc = fp_console_action(mode);
	if (rc == EC_SUCCESS)
		return upload_pgm_image(fp_buffer + FP_SENSOR_IMAGE_OFFSET,
					get_sensor_bpp());

	return rc;
}
DECLARE_CONSOLE_COMMAND(fpcapture, command_fpcapture, nullptr,
			"Capture fingerprint in PGM format");

/* Transfer a chunk of the image from the host to the FPMCU
 *
 * Command format:
 *  fpupload <offset> <hex encoded pixel string>
 *
 * To limit the size of the commands, only a chunk of the image is sent for
 * each command invocation.
 */
static int command_fpupload(int argc, const char **argv)
{
	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;
	if (system_is_locked())
		return EC_ERROR_ACCESS_DENIED;
	int offset = atoi(argv[1]);
	if (offset < 0)
		return EC_ERROR_PARAM1;
	uint8_t *dest = fp_buffer + FP_SENSOR_IMAGE_OFFSET + offset;

	const char *pixels_str = argv[2];
	while (*pixels_str) {
		if (dest >= fp_buffer + FP_SENSOR_IMAGE_SIZE)
			return EC_ERROR_PARAM1;
		const char hex_str[] = { pixels_str[0], pixels_str[1], '\0' };
		*dest = static_cast<uint8_t>(strtol(hex_str, nullptr, 16));
		pixels_str += 2;
		++dest;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fpupload, command_fpupload, nullptr,
			"Copy fp image onto fpmcu fpsensor buffer");

/* Transfer an image from the FPMCU to the host
 *
 * Command format:
 *  fpdownload
 *
 * This is useful to verify the data was transferred correctly. Note that it
 * requires the terminal to be configured as explained in the comment above
 * upload_pgm_image().
 */
static int command_fpdownload(int argc, const char **argv)
{
	if (system_is_locked())
		return EC_ERROR_ACCESS_DENIED;

	return upload_pgm_image(fp_buffer + FP_SENSOR_IMAGE_OFFSET,
				get_sensor_bpp());
}
DECLARE_CONSOLE_COMMAND(fpdownload, command_fpdownload, nullptr,
			"Copy fp image from fpmcu fpsensor buffer");

static int command_fpenroll(int argc, const char **argv)
{
	enum ec_error_list rc;
	int percent = 0;
	static const char *const enroll_str[] = { "OK", "Low Quality",
						  "Immobile", "Low Coverage" };

	if (system_is_locked())
		return EC_ERROR_ACCESS_DENIED;

	do {
		int tries = 1000;

		rc = fp_console_action(FP_MODE_ENROLL_SESSION |
				       FP_MODE_ENROLL_IMAGE);
		if (rc != EC_SUCCESS)
			break;
		const uint32_t event = atomic_clear(&global_context.fp_events);
		percent = EC_MKBP_FP_ENROLL_PROGRESS(event);
		CPRINTS("Enroll capture: %s (%d%%)",
			enroll_str[EC_MKBP_FP_ERRCODE(event) & 3], percent);
		/* wait for finger release between captures */
		global_context.sensor_mode = FP_MODE_ENROLL_SESSION |
					     FP_MODE_FINGER_UP;
		task_set_event(TASK_ID_FPSENSOR, TASK_EVENT_UPDATE_CONFIG);
		while (tries-- &&
		       global_context.sensor_mode & FP_MODE_FINGER_UP)
			crec_usleep(20 * MSEC);
	} while (percent < 100);
	global_context.sensor_mode = 0; /* reset FP_MODE_ENROLL_SESSION */
	task_set_event(TASK_ID_FPSENSOR, TASK_EVENT_UPDATE_CONFIG);

	return rc;
}
DECLARE_CONSOLE_COMMAND(fpenroll, command_fpenroll, nullptr,
			"Enroll a new fingerprint");

static int command_fpinfo(int argc, const char **argv)
{
	ec_response_fp_info info;

#if defined(HAVE_FP_PRIVATE_DRIVER) || defined(BOARD_HOST)
	if (fp_sensor_get_info(&info) < 0)
		return EC_ERROR_UNKNOWN;
#else
	return EC_ERROR_UNKNOWN;
#endif

	constexpr int align = 15;

	ccprintf("%*s: 0x%X (%s)\n", align, "Vendor ID", info.vendor_id,
		 fourcc_to_string(info.vendor_id).c_str());
	ccprintf("%*s: 0x%X\n", align, "Product ID", info.product_id);
	ccprintf("%*s: 0x%X\n", align, "Model ID", info.model_id);
	ccprintf("%*s: 0x%X\n", align, "Version", info.version);

	ccprintf("%*s: %u x %u %ubpp\n", align, "Sensor (w x h)", info.width,
		 info.height, info.bpp);
	ccprintf("%*s: %u\n", align, "Frame Size", info.frame_size);
	ccprintf("%*s: 0x%X (%s)\n", align, "Pixel Format", info.pixel_format,
		 fourcc_to_string(info.pixel_format).c_str());

	ccprintf("%*s: 0x%X\n", align, "Error State", info.errors);

	ccprintf("%*s: %s\n", align, "Sensor Strap",
		 fp_sensor_type_to_str(fpsensor_detect_get_type()));

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(fpinfo, command_fpinfo, nullptr,
			     "Print fingerprint system info");

static int command_fpmatch(int argc, const char **argv)
{
	if (system_is_locked())
		return EC_ERROR_ACCESS_DENIED;

	const enum ec_error_list rc = fp_console_action(FP_MODE_MATCH);
	const uint32_t event = atomic_clear(&global_context.fp_events);

	if (rc == EC_SUCCESS && event & EC_MKBP_FP_MATCH) {
		const uint32_t match_errcode = EC_MKBP_FP_ERRCODE(event);

		CPRINTS("Match: %s (%d)",
			fp_match_success(match_errcode) ? "YES" : "NO",
			match_errcode);
	}

	return rc;
}
DECLARE_CONSOLE_COMMAND(fpmatch, command_fpmatch, nullptr,
			"Run match algorithm against finger");

static int command_fpclear(int argc, const char **argv)
{
	/*
	 * We intentionally run this on the fp_task so that we use the
	 * same code path as host commands.
	 */
	const enum ec_error_list rc = fp_console_action(FP_MODE_RESET_SENSOR);

	if (rc != EC_SUCCESS)
		CPRINTS("Failed to clear fingerprint context: %d", rc);

	atomic_clear(&global_context.fp_events);

	return rc;
}
DECLARE_CONSOLE_COMMAND(fpclear, command_fpclear, nullptr,
			"Clear fingerprint sensor context");

static int command_fpmaintenance(int argc, const char **argv)
{
#ifdef HAVE_FP_PRIVATE_DRIVER
	uint32_t mode_output = 0;
	const int rc =
		fp_set_sensor_mode(FP_MODE_SENSOR_MAINTENANCE, &mode_output);

	if (rc != EC_RES_SUCCESS) {
		/*
		 * EC host command errors do not directly map to console command
		 * errors.
		 */
		return EC_ERROR_UNKNOWN;
	}

	/* Block console until maintenance is finished. */
	while (global_context.sensor_mode & FP_MODE_SENSOR_MAINTENANCE) {
		crec_usleep(100 * MSEC);
	}
#endif /* #ifdef HAVE_FP_PRIVATE_DRIVER */

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fpmaintenance, command_fpmaintenance, nullptr,
			"Run fingerprint sensor maintenance");

#endif /* CONFIG_CMD_FPSENSOR_DEBUG */
