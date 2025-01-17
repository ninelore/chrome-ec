/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppm_common.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>

#include <drivers/ucsi_v3.h>

/**
 * @brief Helper function for getting a device pointer to the PPM device
 *
 * @return NULL if device is not ready
 * @return device pointer on success
 */
static const struct device *get_ppm_dev(void)
{
	const struct device *ppm_dev = DEVICE_DT_GET(DT_INST(0, ucsi_ppm));

	if (!device_is_ready(ppm_dev)) {
		return NULL;
	}

	return ppm_dev;
}

/**
 * @brief Helper function to parse and validate a type-C port number
 *
 * @param sh Pointer to Zephyr shell object
 * @param ppm_dev Pointer to a PPM device ("usci-ppm" compat string). Used to
 *        get the number of active ports
 * @param arg_val Pointer to string to parse
 * @param port[out] Output pointer to write port number to, if successful
 *
 * @return 0 on success
 * @return -EINVAL if port number string cannot be parsed or is out of range
 */
static int cmd_get_pd_port(const struct shell *sh, const struct device *ppm_dev,
			   char *arg_val, uint8_t *port)
{
	const struct ucsi_pd_driver *ppm_api = ppm_dev->api;
	char *e;

	*port = strtoul(arg_val, &e, 0);
	if (*e || *port >= ppm_api->get_active_port_count(ppm_dev)) {
		shell_error(sh, "Invalid port");
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Run the GET_ALTERNATE_MODES UCSI command
 */
static int cmd_get_alt_modes(const struct shell *sh, int argc, char **argv)
{
	const struct device *dev = get_ppm_dev();
	const struct ucsi_pd_driver *ppm_api;
	uint8_t port;
	uint8_t recipient = 1; /* Default to SOP as recipient */
	struct ucsi_get_alternate_modes_t ucsi_response[4] = { 0 };
	int num_alt_modes = 0;
	int rv;

	__ASSERT(dev, "PPM device is not ready");

	ppm_api = dev->api;

	if (cmd_get_pd_port(sh, dev, argv[1], &port)) {
		shell_error(sh, "Invalid port");
		return -ERANGE;
	}

	if (argc > 2) {
		if (!strncmp(argv[2], "conn", strlen("conn"))) {
			recipient = 0;
		} else if (!strncmp(argv[2], "sopprimeprime",
				    strlen("sopprimeprime"))) {
			recipient = 3;
		} else if (!strncmp(argv[2], "sopprime", strlen("sopprime"))) {
			recipient = 2;
		} else if (!strncmp(argv[2], "sop", strlen("sop"))) {
			recipient = 1;
		} else {
			shell_error(sh, "Invalid recipient");
			return 1;
		}
	}

	struct ucsi_control_t get_am_cmd = {
                .command = UCSI_GET_ALTERNATE_MODES,
                .data_length = 0,
                .command_specific = {
                        recipient & 0x07,
                        /* Convert to 1-indexed port number */
                        (port + 1) & 0x7F,
                        0, /* Starting offset of 0 */
                        1, /* Read 2 (1+1) alternate modes fields */
                },
        };

	for (int i = 0; i < ARRAY_SIZE(ucsi_response); i++) {
		/* We receive two fields per command call. Set the offset */
		get_am_cmd.command_specific[2] = i * 2;

		rv = ppm_api->execute_cmd(dev, &get_am_cmd,
					  (uint8_t *)&ucsi_response[i]);
		if (rv < 0) {
			shell_error(sh,
				    "Failed to execute UCSI command: %d (i=%d)",
				    rv, i);
			return 1;
		}

		if (ucsi_response[i].altmode_fields[0].svid == 0) {
			num_alt_modes = 2 * i;
			break;
		}
		if (ucsi_response[i].altmode_fields[1].svid == 0) {
			num_alt_modes = 2 * i + 1;
			break;
		}
	}

	/* Print alternate mode info in a table */

	shell_info(sh, "Port: C%u (UCSI port %u), Recipient: %u\n", port,
		   port + 1, recipient);

	if (num_alt_modes == 0) {
		shell_info(sh, "No alternate modes reported");
		return 0;
	}

	shell_info(sh, "Offset | Alternate mode");
	shell_info(sh, "-------+--------------------------------------");

	for (int i = 0; i < num_alt_modes; i++) {
		shell_info(sh, "%03d    | SVID=0x%04x MID=0x%08x", i,
			   ucsi_response[i / 2].altmode_fields[i % 2].svid,
			   ucsi_response[i / 2].altmode_fields[i % 2].mid);
	}

	return 0;
}

/**
 * @brief Run the GET_CAM_SUPPORTED UCSI command
 */
static int cmd_get_cam_supported(const struct shell *sh, int argc, char **argv)
{
	const struct device *dev = get_ppm_dev();
	const struct ucsi_pd_driver *ppm_api;
	uint8_t port;
	uint8_t resp[8] = { 0 };
	int rv;

	__ASSERT(dev, "PPM device is not ready");

	ppm_api = dev->api;

	if (cmd_get_pd_port(sh, dev, argv[1], &port)) {
		shell_error(sh, "Invalid port");
		return -ERANGE;
	}

	struct ucsi_control_t get_cam_supported = {
                .command = UCSI_GET_CAM_SUPPORTED,
                .data_length = 0,
                .command_specific = {
                        /* Convert to 1-indexed port number */
                        (port + 1) & 0x7F,
                },
        };

	rv = ppm_api->execute_cmd(dev, &get_cam_supported, (uint8_t *)&resp);
	if (rv < 0) {
		shell_error(sh, "Failed to execute UCSI command: %d", rv);
		return 1;
	}

	shell_info(sh, "Port: C%u (UCSI port %u), Supported:", port, port + 1);
	shell_hexdump(sh, resp, rv);

	shell_fprintf(sh, SHELL_INFO, "\nSupported indexes: ");
	for (int i = 0; i < MIN(rv, ARRAY_SIZE(resp)); i++) {
		for (int j = 0; j < 8; j++) {
			if (resp[i] & BIT(j)) {
				shell_fprintf(sh, SHELL_INFO, "%02d ",
					      8 * i + j);
			}
		}
	}
	shell_fprintf(sh, SHELL_INFO, "\n");

	return 0;
}

/* LCOV_EXCL_START */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ppm_cmds,
	SHELL_CMD_ARG(
		get_alt_modes, NULL,
		"Run GET_ALTERNATE_MODES UCSI command. Gets up to 8 responses. "
		"Default recipient is SOP.\n"
		"Usage: ppm get_alt_modes <port> "
		"[conn|sop|sopprime|sopprimeprime]",
		cmd_get_alt_modes, 2, 1),
	SHELL_CMD_ARG(get_cam_supported, NULL,
		      "Run GET_CAM_SUPPORTED UCSI command.\n"
		      "Usage: ppm get_cam_supported <port>",
		      cmd_get_cam_supported, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ppm, &sub_ppm_cmds, "PPM console commands", NULL);

/* LCOV_EXCL_STOP */
