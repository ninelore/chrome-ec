/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FPC1145_PRIVATE_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FPC1145_PRIVATE_H_

#include <stdint.h>

/* Opaque FPC context */
#define FP_SENSOR_CONTEXT_SIZE 4944

/* External capture types from FPC's sensor library */
enum fpc_capture_type {
	FPC_CAPTURE_VENDOR_FORMAT = 0,
	FPC_CAPTURE_SIMPLE_IMAGE = 1,
	FPC_CAPTURE_PATTERN0 = 2,
	FPC_CAPTURE_PATTERN1 = 3,
	FPC_CAPTURE_QUALITY_TEST = 4,
	FPC_CAPTURE_RESET_TEST = 5,
};

/* External error codes from FPC's sensor library */
enum fpc_error_code_external {
	FPC_ERROR_NONE = 0,
	FPC_ERROR_NOT_FOUND = 1,
	FPC_ERROR_CAN_BE_USED_2 = 2,
	FPC_ERROR_CAN_BE_USED_3 = 3,
	FPC_ERROR_CAN_BE_USED_4 = 4,
	FPC_ERROR_PAL = 5,
	FPC_ERROR_IO = 6,
	FPC_ERROR_CANCELLED = 7,
	FPC_ERROR_UNKNOWN = 8,
	FPC_ERROR_MEMORY = 9,
	FPC_ERROR_PARAMETER = 10,
	FPC_ERROR_TEST_FAILED = 11,
	FPC_ERROR_TIMEDOUT = 12,
	FPC_ERROR_SENSOR = 13,
	FPC_ERROR_SPI = 14,
	FPC_ERROR_NOT_SUPPORTED = 15,
	FPC_ERROR_OTP = 16,
	FPC_ERROR_STATE = 17,
	FPC_ERROR_PN = 18,
	FPC_ERROR_DEAD_PIXELS = 19,
	FPC_ERROR_TEMPLATE_CORRUPTED = 20,
	FPC_ERROR_CRC = 21,
	FPC_ERROR_STORAGE = 22, /**< Errors related to storage **/
	FPC_ERROR_MAXIMUM_REACHED = 23, /**< The allowed maximum has been
					   reached       **/
	FPC_ERROR_MINIMUM_NOT_REACHED = 24, /**< The required minimum was not
					       reached       **/
	FPC_ERROR_SENSOR_LOW_COVERAGE = 25, /**< Minimum sensor coverage was not
					       reached    **/
	FPC_ERROR_SENSOR_LOW_QUALITY = 26, /**< Sensor image is considered low
					      quality     **/
	FPC_ERROR_SENSOR_FINGER_NOT_STABLE = 27, /**< Finger was not stable
						    during image capture **/
};

/* Internal error codes from FPC's sensor library */
enum fpc_error_code_internal {
	FPC_ERROR_INTERNAL_0 = 0, /* Indicates that no internal code was set. */
	FPC_ERROR_INTERNAL_1 = 1, /* Not supported by sensor. */
	FPC_ERROR_INTERNAL_2 = 2, /* Sensor got a NULL response (from other
				     module).                  */
	FPC_ERROR_INTERNAL_3 = 3, /* Runtime config not supported by firmware.
				   */
	FPC_ERROR_INTERNAL_4 = 4, /* CAC has not been created. */
	FPC_ERROR_INTERNAL_5 = 5, /* CAC returned an error to the sensor. */
	FPC_ERROR_INTERNAL_6 = 6, /* CAC fasttap image capture failed. */
	FPC_ERROR_INTERNAL_7 = 7, /* CAC fasttap image capture failed. */
	FPC_ERROR_INTERNAL_8 = 8, /* CAC Simple image capture failed. */
	FPC_ERROR_INTERNAL_9 = 9, /* CAC custom image capture failed. */
	FPC_ERROR_INTERNAL_10 = 10, /* CAC MQT image capture failed. */
	FPC_ERROR_INTERNAL_11 = 11, /* CAC PN image capture failed. */
	FPC_ERROR_INTERNAL_12 = 12, /* Reading CAC context size. */
	FPC_ERROR_INTERNAL_13 = 13, /* Reading CAC context size. */
	FPC_ERROR_INTERNAL_14 = 14, /* Sensor context invalid. */
	FPC_ERROR_INTERNAL_15 = 15, /* Buffer reference is invalid. */
	FPC_ERROR_INTERNAL_16 = 16, /* Buffer size reference is invalid. */
	FPC_ERROR_INTERNAL_17 = 17, /* Image data reference is invalid. */
	FPC_ERROR_INTERNAL_18 = 18, /* Capture type specified is invalid. */
	FPC_ERROR_INTERNAL_19 = 19, /* Capture config specified is invalid. */
	FPC_ERROR_INTERNAL_20 = 20, /* Sensor type in hw desc could not be
				       extracted.                   */
	FPC_ERROR_INTERNAL_21 = 21, /* Failed to create BNC component. */
	FPC_ERROR_INTERNAL_22 = 22, /* BN calibration failed. */
	FPC_ERROR_INTERNAL_23 = 23, /* BN memory allocation failed. */
	FPC_ERROR_INTERNAL_24 = 24, /* Companion type in hw desc could not be
				       extracted.                */
	FPC_ERROR_INTERNAL_25 = 25, /* Coating type in hw desc could not be
				       extracted.                  */
	FPC_ERROR_INTERNAL_26 = 26, /* Sensor mode type is invalid. */
	FPC_ERROR_INTERNAL_27 = 27, /* Wrong Sensor state in OTP read. */
	FPC_ERROR_INTERNAL_28 = 28, /* Mismatch of register size in overlay vs
				       rrs.                     */
	FPC_ERROR_INTERNAL_29 = 29, /* Checkerboard capture failed. */
	FPC_ERROR_INTERNAL_30 = 30, /* Error converting to fpc_image in dp
				       calibration.                 */
	FPC_ERROR_INTERNAL_31 = 31, /* Failed to capture reset pixel image. */
	FPC_ERROR_INTERNAL_32 = 32, /* API level not support in dp calibration.
				     */
	FPC_ERROR_INTERNAL_33 = 33, /* The image data in parameter is invalid.
				     */
	FPC_ERROR_INTERNAL_34 = 34, /* PAL delay function has failed. */
	FPC_ERROR_INTERNAL_35 = 35, /* AFD sensor command did not complete. */
	FPC_ERROR_INTERNAL_36 = 36, /* AFD wrong runlevel detected after
				       calibration.                   */
	FPC_ERROR_INTERNAL_37 = 37, /* Wrong rrs size. */
	FPC_ERROR_INTERNAL_38 = 38, /* There was a finger on the sensor when
				       calibrating finger detect. */
	FPC_ERROR_INTERNAL_39 = 39, /* The calculated calibration value is
				       larger than max.             */
	FPC_ERROR_INTERNAL_40 = 40, /* The sensor fifo always underflows */
	FPC_ERROR_INTERNAL_41 = 41, /* The oscillator calibration resulted in a
				       too high or low value   */
	FPC_ERROR_INTERNAL_42 = 42, /* Sensor driver was opened with NULL
				       configuration                 */
	FPC_ERROR_INTERNAL_43 = 43, /* Sensor driver as opened with NULL hw
				       descriptor                  */
	FPC_ERROR_INTERNAL_44 = 44, /* Error occurred during image drive test */
};

/* FPC specific initialization function to fill their context */
int fp_sensor_open(void *ctx, uint32_t ctx_size);

/*
 * Get library version code.
 * version code contains three digits. x.y.z
 *   x - major version
 *   y - minor version
 *   z - build index
 */
const char *fp_sensor_get_version(void);

struct fp_sensor_info {
	uint32_t num_defective_pixels;
};

// TODO move to common fpc file

/**
 * fp_sensor_maintenance runs a test for defective pixels and should
 * be triggered periodically by the client. Internally, a defective
 * pixel list is maintained and the algorithm will compensate for
 * any defect pixels when matching towards a template.
 *
 * The defective pixel update will abort and return an error if any of
 * the finger detect zones are covered. A client can call
 * fp_sensor_finger_status to determine the current status.
 *
 * @param[in]  image_data      pointer to a buffer containing at least
 * FP_SENSOR_IMAGE_SIZE_FPC bytes of memory
 * @param[out] fp_sensor_info  Structure containing output data.
 *
 * @return
 * - 0 on success
 * - negative value on error
 */
int fp_sensor_maintenance(uint8_t *image_data,
			  struct fp_sensor_info *fp_sensor_info);

/** Image captured. */
#define FPC_SENSOR_GOOD_IMAGE_QUALITY 0
/** Image captured but quality is too low. */
#define FPC_SENSOR_LOW_IMAGE_QUALITY 1
/** Finger removed before image was captured. */
#define FPC_SENSOR_TOO_FAST 2
/** Sensor not fully covered by finger. */
#define FPC_SENSOR_LOW_COVERAGE 3

/**
 * Acquires a fingerprint image with specific capture mode.
 *
 * Same as the fp_sensor_acquire_image function(),
 * except @p mode can be set to one of the fp_capture_type constants
 * to get a specific image type (e.g. a pattern) rather than the default one.
 *
 * @param[out] image_data Image from sensor. Buffer must be allocated by
 * caller with size FP_SENSOR_IMAGE_SIZE.
 * @param mode  enum fp_capture_type
 *
 * @return 0 on success
 * @return negative value on error
 */
int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode);

/**
 * Configure finger detection.
 *
 * Send the settings to the sensor, so it is properly configured to detect
 * the presence of a finger.
 */
void fp_sensor_configure_detect(void);

#define FPC_FINGER_NONE 0
#define FPC_FINGER_PARTIAL 1
#define FPC_FINGER_PRESENT 2

/**
 * Returns the status of the finger on the sensor.
 * (assumes fp_sensor_configure_detect was called before)
 *
 * @return finger_state
 */
int fp_sensor_finger_status(void);

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_FPC1145_PRIVATE_H_ */
