/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "common.h"
#include "console.h"
#include "elan_sensor.h"
#include "elan_sensor_pal.h"
#include "elan_setting.h"
#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_console.h"
#include "gpio.h"
#include "link_defs.h"
#include "math_util.h"
#include "shared_mem.h"
#include "spi.h"
#include "system.h"
#include "timer.h"
#include "trng.h"
#include "util.h"

#include <errno.h>
#include <stddef.h>

static uint16_t errors;

/* Sensor description */
static struct ec_response_fp_info ec_fp_sensor_info = {
	/* Sensor identification */
	.vendor_id = FOURCC('E', 'L', 'A', 'N'),
	.product_id = PID,
	.model_id = MID,
	.version = VERSION,
	/* Image frame characteristics */
	.frame_size = FP_SENSOR_RES_X_ELAN * FP_SENSOR_RES_Y_ELAN * 2,
	.pixel_format = V4L2_PIX_FMT_GREY,
	.width = FP_SENSOR_RES_X_ELAN,
	.height = FP_SENSOR_RES_Y_ELAN,
	.bpp = FP_SENSOR_RES_BPP_ELAN,
};

static enum elan_capture_type
convert_fp_capture_type_to_elan_capture_type(enum fp_capture_type mode)
{
	switch (mode) {
	case FP_CAPTURE_VENDOR_FORMAT:
		return ELAN_CAPTURE_VENDOR_FORMAT;
	case FP_CAPTURE_SIMPLE_IMAGE:
		return ELAN_CAPTURE_SIMPLE_IMAGE;
	case FP_CAPTURE_PATTERN0:
		return ELAN_CAPTURE_PATTERN0;
	case FP_CAPTURE_PATTERN1:
		return ELAN_CAPTURE_PATTERN1;
	case FP_CAPTURE_QUALITY_TEST:
		return ELAN_CAPTURE_QUALITY_TEST;
	case FP_CAPTURE_RESET_TEST:
		return ELAN_CAPTURE_RESET_TEST;
	default:
		return ELAN_CAPTURE_TYPE_INVALID;
	}
}

int elan_get_hwid(uint16_t *id)
{
	int rc;
	uint8_t id_hi = 0, id_lo = 0;
	if (id == NULL)
		return EC_ERROR_INVAL;
	rc = elan_read_register(0x02, &id_hi);
	rc |= elan_read_register(0x04, &id_lo);
	if (rc) {
		CPRINTS("ELAN HW ID read failed %d", rc);
		return EC_ERROR_HW_INTERNAL;
	}
	*id = (id_hi << 8) | id_lo;
	return EC_SUCCESS;
}

int elan_check_hwid(void)
{
	uint16_t id = 0;
	int status;

	status = elan_get_hwid(&id);
	if (status != EC_SUCCESS) {
		assert(status == EC_ERROR_HW_INTERNAL);
		errors |= FP_ERROR_SPI_COMM;
	}

	if (id != FP_SENSOR_HWID_ELAN) {
		CPRINTS("ELAN unknown silicon 0x%04x", id);
		errors |= FP_ERROR_BAD_HWID;
		return EC_ERROR_HW_INTERNAL;
	}

	CPRINTS("ELAN HWID 0x%04x", id);
	return EC_SUCCESS;
}

/**
 * set fingerprint sensor into power saving mode
 */
void fp_sensor_low_power(void)
{
	elan_woe_mode();
}

/**
 * Reset and initialize the sensor IC
 */
int fp_sensor_init(void)
{
	CPRINTF("========%s=======\n", __func__);

	errors = FP_ERROR_DEAD_PIXELS_UNKNOWN;
	elan_execute_reset();
	elan_alg_param_setting();
	if (IC_SELECTION == EFSA80SG)
		elan_set_hv_chip(1);

	int rc = elan_check_hwid();
	if (rc != EC_SUCCESS) {
		errors |= FP_ERROR_INIT_FAIL;
		return EC_SUCCESS;
	}

	if (elan_execute_calibration() < 0)
		errors |= FP_ERROR_INIT_FAIL;
	if (elan_woe_mode() != 0)
		errors |= FP_ERROR_SPI_COMM;

	return EC_SUCCESS;
}

/**
 * Deinitialize the sensor IC
 */
int fp_sensor_deinit(void)
{
	CPRINTF("========%s=======\n", __func__);
	return elan_fp_deinit();
}

/**
 * Fill the 'ec_response_fp_info' buffer with the sensor information
 *
 * @param[out] resp      retrieve the version, sensor and template information
 *
 * @return EC_SUCCESS on success.
 * @return EC_RES_ERROR on error.
 */
int fp_sensor_get_info(struct ec_response_fp_info *resp)
{
	uint16_t id = 0;

	CPRINTF("========%s=======\n", __func__);
	memcpy(resp, &ec_fp_sensor_info, sizeof(struct ec_response_fp_info));

	if (elan_get_hwid(&id)) {
		return EC_RES_ERROR;
	}

	resp->model_id = id;
	resp->errors = errors;

	return EC_SUCCESS;
}

/**
 * Compares given finger image against enrolled templates.
 *
 * @param[in]  templ            a pointer to the array of template buffers.
 * @param[in]  templ_count      the number of buffers in the array of templates.
 * @param[in]  image            the buffer containing the finger image
 * @param[out] match_index      index of the matched finger in the template
 *                              array if any.
 * @param[out] update_bitmap    contains one bit per template, the bit is set if
 *                              the match has updated the given template.
 *
 * @return negative value on error, else one of the following code :
 * - EC_MKBP_FP_ERR_MATCH_NO on non-match
 * - EC_MKBP_FP_ERR_MATCH_YES for match when template was not updated with
 *   new data
 * - EC_MKBP_FP_ERR_MATCH_YES_UPDATED for match when template was updated
 * - EC_MKBP_FP_ERR_MATCH_YES_UPDATE_FAILED match, but update failed (not saved)
 * - EC_MKBP_FP_ERR_MATCH_LOW_QUALITY when matching could not be performed due
 *   to low image quality
 * - EC_MKBP_FP_ERR_MATCH_LOW_COVERAGE when matching could not be performed
 *   due to finger covering too little area of the sensor
 */
int fp_finger_match(void *templ, uint32_t templ_count, uint8_t *image,
		    int32_t *match_index, uint32_t *update_bitmap)
{
	int res;
	CPRINTF("========%s=======\n", __func__);
	res = elan_match(templ, templ_count, image, match_index, update_bitmap);
	if (res == EC_MKBP_FP_ERR_MATCH_YES)
		res = elan_template_update(templ, *match_index);

	return res;
}

/**
 * start a finger enrollment session and initialize enrollment data
 *
 * @return 0 on success.
 *
 */
int fp_enrollment_begin(void)
{
	CPRINTF("========%s=======\n", __func__);
	return elan_enrollment_begin();
}

/**
 * Generate a template from the finger whose enrollment has just being
 * completed.
 *
 * @param templ the buffer which will receive the template.
 *              templ can be set to NULL to abort the current enrollment
 *              process.
 *
 * @return 0 on success or a negative error code.
 */
int fp_enrollment_finish(void *templ)
{
	CPRINTF("========%s=======\n", __func__);
	return elan_enrollment_finish(templ);
}

/**
 * Adds fingerprint image to the current enrollment session.
 *
 * @param[in]  image             fingerprint image data
 * @param[out] completion        retrieve percentage of current enrollment
 *
 * @return a negative value on error or one of the following codes:
 * - EC_MKBP_FP_ERR_ENROLL_OK when image was successfully enrolled
 * - EC_MKBP_FP_ERR_ENROLL_IMMOBILE when image added, but user should be
 *   advised to move finger
 * - EC_MKBP_FP_ERR_ENROLL_LOW_QUALITY when image could not be used due to
 *   low image quality
 * - EC_MKBP_FP_ERR_ENROLL_LOW_COVERAGE when image could not be used due to
 *   finger covering too little area of the sensor
 */
int fp_finger_enroll(uint8_t *image, int *completion)
{
	CPRINTF("========%s=======\n", __func__);
	return elan_enroll(image, completion);
}

/**
 * Put the sensor in its lowest power state.
 *
 * fp_sensor_configure_detect needs to be called to restore finger detection
 * functionality.
 */
void fp_configure_detect(void)
{
	CPRINTF("========%s=======\n", __func__);
	elan_woe_mode();
}

/**
 * Acquires a fingerprint image with specific capture mode.
 *
 * @param[out] image_data    Memory buffer to retrieve fingerprint image data
 *                          Image_data is allocated by the caller and the size
 *                          is FP_SENSOR_IMAGE_SIZE.
 * @param[in]  mode         one of the FP_CAPTURE_ constants to get a specific
 *                          image type
 * - FP_CAPTURE_VENDOR_FORMAT: Full blown vendor-defined capture
 * - FP_CAPTURE_SIMPLE_IMAGE: Simple raw image capture
 * - FP_CAPTURE_PATTERN0: Self test pattern
 * - FP_CAPTURE_PATTERN1: Self test pattern
 * - FP_CAPTURE_QUALITY_TEST
 * - FP_CAPTURE_RESET_TEST
 * - FP_CAPTURE_TYPE_MAX
 *
 * @return
 * - 0 on success
 * - negative value on error
 * - FP_SENSOR_LOW_IMAGE_QUALITY on image captured but quality is too low
 * - FP_SENSOR_TOO_FAST on finger removed before image was captured
 * - FP_SENSOR_LOW_SENSOR_COVERAGE on sensor not fully covered by finger
 */
int fp_acquire_image(uint8_t *image_data, enum fp_capture_type capture_type)
{
	enum elan_capture_type rc =
		convert_fp_capture_type_to_elan_capture_type(capture_type);

	if (rc == ELAN_CAPTURE_TYPE_INVALID) {
		CPRINTF("Unsupported capture_type %d provided", capture_type);
		return -EINVAL;
	}

	CPRINTF("========%s=======\n", __func__);
	return elan_sensor_acquire_image_with_mode(image_data, rc);
}

/**
 * Returns the status of the finger on the sensor.
 *
 * @return one of the following codes:
 * - FINGER_NONE
 * - FINGER_PARTIAL
 * - FINGER_PRESENT
 */
enum finger_state fp_finger_status(void)
{
	CPRINTF("========%s=======\n", __func__);
	return elan_sensor_finger_status();
}

/**
 * Runs a test for defective pixels.
 *
 * Should be triggered periodically by the client. The maintenance command can
 * take several hundred milliseconds to run.
 *
 * @return EC_ERROR_HW_INTERNAL on error (such as finger on sensor)
 * @return EC_SUCCESS on success
 */
int fp_maintenance(void)
{
	CPRINTF("========%s=======\n", __func__);
	return elan_fp_maintenance(&errors);
}
