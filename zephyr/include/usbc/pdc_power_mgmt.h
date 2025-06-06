/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @file
 * @brief PDC API for USB-C Power Management.
 */
#ifndef __CROS_EC_PDC_POWER_MGMT_H
#define __CROS_EC_PDC_POWER_MGMT_H

#include "usb_mux.h"
#include "usb_pd.h"

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/sys/atomic.h>

#include <drivers/pdc.h>

#ifdef CONFIG_ZTEST
extern const char *const pdc_cmd_names[];
extern const int pdc_cmd_types;
#endif

/**
 * @brief State Machine States
 */
enum pdc_state_t {
	/** PDC_INIT */
	PDC_INIT,
	/** PDC_UNATTACHED */
	PDC_UNATTACHED,
	/** PDC_SNK_ATTACHED */
	PDC_SNK_ATTACHED,
	/** PDC_SRC_ATTACHED */
	PDC_SRC_ATTACHED,
	/** PDC_SEND_CMD_START */
	PDC_SEND_CMD_START,
	/** PDC_SEND_CMD_WAIT */
	PDC_SEND_CMD_WAIT,
	/** PDC_SRC_TYPEC_ONLY */
	PDC_SRC_TYPEC_ONLY,
	/** PDC_SNK_TYPEC_ONLY */
	PDC_SNK_TYPEC_ONLY,
	/** Pause operation temporarily (e.g. during FW update) */
	PDC_SUSPENDED,
	/** Permanently disabled (e.g. PDC driver not present) */
	PDC_DISABLED,

	/** Initial value, a placeholder state before entering init state */
	PDC_INVALID,
	/** State count. Always leave as last item. */
	PDC_STATE_COUNT,
};

/**
 * @brief PDC power mgmt board callback types
 */
enum pdc_power_mgmt_board_cb_t {
	/** Unattach */
	PDC_BOARD_CB_UNATTACH,
	/** DP Attention */
	PDC_BOARD_CB_DP_ATTENTION,

	/** State count. Always leave as last item. */
	PDC_BOARD_CB_COUNT,
};

/**
 * @brief Start the PDC subsystem and the PDC driver threads
 */
void pdc_subsys_start(void);

/**
 * @brief Get the state of the port partner connection
 *
 * @param port USB-C port number
 *
 * @retval True if port is in connected state
 */
bool pdc_power_mgmt_is_connected(int port);

/**
 * @brief Get the number of USB-C ports in the system
 *
 * @retval CONFIG_USB_PD_PORT_MAX_COUNT
 */
uint8_t pdc_power_mgmt_get_usb_pd_port_count(void);

/**
 * @brief Set the passed charge port as active.
 *
 * @param charge_port   Charge port to be enabled.
 *
 * @retval EC_SUCCESS if the charge port is accepted
 */
int pdc_power_mgmt_set_active_charge_port(int charge_port);

/**
 * @brief Get CC polarity of the port
 *
 * @param port USB-C port number
 *
 * @retval (POLARITY_CC1 or POLARITY_CC1_DTS) for non-flipped connection or
 * (POLARITY_CC2 or POLARITY_CC2_DTS)
 */
enum tcpc_cc_polarity pdc_power_mgmt_pd_get_polarity(int port);

/**
 * @brief Get current data role
 *
 * @param port USB-C port number
 *
 * @retval PD_ROLE_UFP for UFP, or PD_ROLE_DFP
 */
enum pd_data_role pdc_power_mgmt_pd_get_data_role(int port);

/**
 * @brief Request power swap to Source
 *
 * @param port USB-C port number
 *
 * @retval void
 */
void pdc_power_mgmt_request_swap_to_src(int port);

/**
 * @brief Request power swap to Sink
 *
 * @param port USB-C port number
 *
 * @retval void
 */
void pdc_power_mgmt_request_swap_to_snk(int port);

/**
 * @brief Request data swap to UFP
 *
 * @param port USB-C port number
 *
 * @retval void
 */
void pdc_power_mgmt_request_swap_to_ufp(int port);

/**
 * @brief Request data swap to DFP
 *
 * @param port USB-C port number
 *
 * @retval void
 */
void pdc_power_mgmt_request_swap_to_dfp(int port);

/**
 * @brief Signal power request to indicate a charger update that affects the
 * port.
 *
 * @param port USB-C port number
 *
 * @retval EC_SUCCESS if successful, or -ENOTCONN if the port is not attached in
 * SNK role
 */
int pdc_power_mgmt_set_new_power_request(int port);

/**
 * @brief Get current power role
 *
 * @param port USB-C port number
 *
 * @retval PD_ROLE_SINK for SINK, or PD_ROLE_SOURCE
 */
enum pd_power_role pdc_power_mgmt_get_power_role(int port);

/**
 * @brief Get the current PD state of USB-C port
 *
 * @param port USB-C port number
 *
 * @retval PD state
 */
uint8_t pdc_power_mgmt_get_task_state(int port);

/**
 * @brief Check if PD communication is enabled
 *
 * @param port USB-C port number
 *
 * @retval true if it's enabled or false otherwise
 */
int pdc_power_mgmt_comm_is_enabled(int port);

/**
 * @brief Get current VCONN state of USB-C port
 *
 * @param port USB-C port number
 *
 * @retval true if the PDC is sourcing VCONN, else false
 */
bool pdc_power_mgmt_get_vconn_state(int port);

/**
 * @brief Check if port partner is dual role power
 *
 * @param port USB-C port number
 *
 * @retval true if partner is dual role power else false
 */
bool pdc_power_mgmt_get_partner_dual_role_power(int port);

/**
 * @brief Get port partner data swap capable status
 *
 * @param port USB-C port number
 *
 * @retval true if data swap capable, else false
 */
bool pdc_power_mgmt_get_partner_data_swap_capable(int port);

/**
 * @brief Check if port partner is USB comms capable
 *
 * @param port USB-C port number
 *
 * @retval true if partner port is capable of communication over USB data lines,
 * else false
 */
bool pdc_power_mgmt_get_partner_usb_comm_capable(int port);

/**
 * @brief Check if port partner is unconstrained power
 *
 * @param port USB-C port number
 *
 * @retval true if partner is unconstrained power, else false
 */
bool pdc_power_mgmt_get_partner_unconstr_power(int port);

/**
 * @brief Get the current CC line states from PD task
 *
 * @param port USB-C port number
 *
 * @retval CC state
 */
enum pd_cc_states pdc_power_mgmt_get_task_cc_state(int port);

/**
 * @brief Check if port partner is PD capable
 *
 * @param port USB-C port number
 *
 * @retval true if port partner is known to be PD capable
 */
bool pdc_power_mgmt_pd_capable(int port);

/**
 * @brief Measures and returns VBUS
 *
 * @param port USB-C port number
 *
 * @retval VBUS voltage in millivolts
 */
int pdc_power_mgmt_get_vbus_voltage(int port);

/**
 * @brief Resets the PDC
 *
 * @param port USB-C port number
 *
 * @retval 0 if successful, error code otherwise
 */
int pdc_power_mgmt_reset(int port);

/**
 * @brief Get the source caps list sent by the port partner
 *
 * @param port USB-C port number
 * @retval pointer to source caps list
 */
const uint32_t *const pdc_power_mgmt_get_src_caps(int port);

/**
 * @brief Get the number of source caps sent by the port partner
 *
 * @param port USB-C port number
 * @retval number of source caps
 */
uint8_t pdc_power_mgmt_get_src_cap_cnt(int port);

/**
 * @brief Get the sent RDO while in sink mode
 *
 * @param port USB-C port number
 * @param rdo Output parameter for RDO object
 * @retval 0 on success, -EINVAL if \p rdo is NULL, -ENODATA if not sinking.
 */
int pdc_power_mgmt_get_rdo(int port, uint32_t *rdo);

/**
 * @brief Set dual role state, from among enum pd_dual_role_states
 *
 * @param port USB-C port number
 * @param state New state of dual-role port, selected from enum
 * pd_dual_role_states
 */
void pdc_power_mgmt_set_dual_role(int port, enum pd_dual_role_states state);

/**
 * @brief Get the previously set dual role state
 *
 * @param port USB-C port number
 * @return most recently-set dual role state, or -1 if never set.
 */
enum pd_dual_role_states pdc_power_mgmt_get_dual_role(int port);

/**
 * @brief Get the current PD state name of USB-C port
 *
 * @param port USB-C port number
 * @retval name of task state
 */
const char *pdc_power_mgmt_get_task_state_name(int port);

/**
 * @brief Request a power role swap
 *
 * @param port USB-C port number
 */
void pdc_power_mgmt_request_power_swap(int port);

/**
 * @brief Request a data role swap
 *
 * @param port USB-C port number
 */
void pdc_power_mgmt_request_data_swap(int port);

/*
 * @brief Query info from the PD chip (USB PID/VID, FW ver, etc)
 *
 * @param port USB-C port number
 * @param pdc_info Output struct for chip info
 * @param live True forces a read from the chip. False returns a cached value
 *        from driver initialization or latest live read.
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_info(int port, struct pdc_info_t *pdc_info, bool live);

/**
 * @brief Query hw config from PDC used to access the chip
 *
 * @param port USB-C port number
 * @param pdc_hw_config Output struct for hw config
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_hw_config(int port,
				 struct pdc_hw_config_t *pdc_hw_config);

/**
 * @brief Get current PD Revision
 *
 * @param port USB-C port number
 * @param type SOP* type
 *
 * @retval PD_REV10 for PD Revision 1.0
 *         PD_REV20 for PD Revision 2.0
 *         PD_REV30 for PD Revision 3.0
 */
int pdc_power_mgmt_get_rev(int port, enum tcpci_msg_type type);

/**
 * @brief Returns the sink caps list
 *
 * @param port USB-C port number
 *
 * @retval List of sink capabilities
 */
const uint32_t *const pdc_power_mgmt_get_snk_caps(int port);

/**
 * @brief Returns the number of sink caps
 *
 * @param port USB-C port number
 *
 * @retval Number of sink capabilities
 */
uint8_t pdc_power_mgmt_get_snk_cap_cnt(int port);

/**
 * @brief Gets the port partner's Revision Message Data Object (RMDO)
 *
 * @param port USB-C port number
 *
 * @retval port partner's RMDO
 */
struct rmdo pdc_power_mgmt_get_partner_rmdo(int port);

/**
 * @brief Get identity discovery state for this type and port
 *
 * @param port USB-C port number
 *
 * @retval Current discovery state
 */
enum pd_discovery_state
pdc_power_mgmt_get_identity_discovery(int port, enum tcpci_msg_type type);

/**
 * @brief Get port partner VID
 *
 * @param port USB-C port number
 *
 * @retval VID if available, 0 otherwise
 */
uint16_t pdc_power_mgmt_get_identity_vid(int port);

/**
 * @brief Get port partner PID
 *
 * @param port USB-C port number
 *
 * @retval PID if available, 0 otherwise
 */
uint16_t pdc_power_mgmt_get_identity_pid(int port);

/**
 * @brief Get port partner prodcut type
 *
 * @param port USB-C port number
 *
 * @retval product type if available, 0 otherwise
 */
uint8_t pdc_power_mgmt_get_product_type(int port);

/**
 * @brief Triggers hard or data reset
 *
 * @param port USB-C port number
 * @param reset_type Hard or Data
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_connector_reset(int port, enum connector_reset reset_type);

/**
 * @brief Get the current events for the port.
 *
 * @param port USB-C port number
 *
 * @retval PD_STATUS_EVENT_* bitmask
 */
atomic_val_t pdc_power_mgmt_get_events(int port);

/**
 * @brief Clear specified events for the port.
 *
 * @param port USB-C port number
 * @param event_mask PD_STATUS_EVENT_* bitmask to clear
 */
void pdc_power_mgmt_clear_event(int port, atomic_t event_mask);

/**
 * Notify the host of an event on the port.
 *
 * @param port USB-C port number
 * @param event_mask PD_STATUS_EVENT_* bitmask to set
 */
void pdc_power_mgmt_notify_event(int port, atomic_t event_mask);

/**
 * @brief Control if the PDC power mgmt and underlying driver threads are
 *        active.
 *
 * @param run True to allow comms, false to suspend
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_set_comms_state(bool run);

/**
 * @brief Return the current UCSI connector status on a port
 *
 * @param port USB-C port number
 * @param connector_status Output variable to store the connector status
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_connector_status(
	int port, union connector_status_t *connector_status);

/**
 * @brief Return the last non-zero connector status change on a port
 *
 * @param port USB-C port number
 * @param status_change Output variable to store the connector status change
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_last_status_change(
	int port, union conn_status_change_bits_t *status_change);

/**
 * @brief Return the current DP pin assignment configured by the PDC as
 * as the DP source.
 *
 * @param port USB-C port number
 *
 * @retval DP pin assignment mask (MODE_DP_PIN_x defines from ec_commands.h)
 */
uint8_t pdc_power_mgmt_get_dp_pin_mode(int port);

/**
 * @brief Put a cap on the max voltage requested as a sink.
 *
 * @param mv maximum voltage in millivolts.
 */
void pdc_power_mgmt_set_max_voltage(unsigned int mv);

/**
 * @brief Get the max voltage that can be requested as set by
 * pd_set_max_voltage().
 *
 * @return max voltage
 */
unsigned int pdc_power_mgmt_get_max_voltage(void);

/**
 * @brief Requests the specified voltage from the PD source and triggers
 *	  a new negotiation sequence with the source.
 *
 * @param port USB-C port number
 * @param mv request voltage in millivolts.
 */
void pdc_power_mgmt_request_source_voltage(int port, int mv);

/**
 * @brief Return the current UCSI cable property on a port
 *
 * @param port USB-C port number
 * @param cable_prop Output variable to store the cable property
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_cable_prop(int port, union cable_property_t *cable_prop);

/**
 * @brief Sets the SRC CAPs sent by the PDC to its port partner when attached in
 * a source power role
 *
 * @param port USB-C port number
 * @param src_pdo Pointer to array of PDOs
 * @param pdc_count Number of PDOs to write
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_set_src_pdo(int port, const uint32_t *src_pdo,
			       uint8_t pdo_count);

/**
 * @brief Set current limit for USB-C port acting in a source power role
 *
 * @param port USB-C port number
 * @param tcc Desired source current limit (1.5 or 3.0A)
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_set_current_limit(int port, enum usb_typec_current_t tcc);

/**
 * @brief Get the default source current limit for a USB-C port
 *
 * @param port USB-C port number
 *
 * @retval USB-C current limit enum value
 */
enum usb_typec_current_t pdc_power_mgmt_get_default_current_limit(int port);

/**
 * @brief Enable/Disable FRS for a given port
 *
 * @param port USB-C port number
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_frs_enable(int port_num, bool enable);

/**
 * @brief Enable/Disable PDC TrySRC on a port
 *
 * @param port USB-C port number
 * @param enable enable or disable TrySRC on port
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_set_trysrc(int port, bool enable);
int pdc_power_mgmt_get_drp_mode(int port, enum drp_mode_t *drp_mode);

/*
 * @brief Return PCH DATA STATUS register for PMC Debug
 *
 * @param port USB-C port number
 * @param status PCH data status Output variable to store register value
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_pch_data_status(int port, uint8_t *status);

/**
 * @brief Run the USCI_GET_LPM_PPM_INFO command on the PDC
 *
 * @param port USB-C port number
 * @param info Pointer to user-supplied output location for response
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_lpm_ppm_info(int port, struct lpm_ppm_info_t *info);

/**
 * @brief Recheck connector status on given port and wait for state to settle.
 *
 * Role swaps do not trigger a connector status change event when initiated by
 * the host so manually query connector status again and wait for the state to
 * settle.
 *
 * @param port USB-C port number
 * @param timeout_ms Timeout in milliseconds. A value of -1 uses configured
 *                   timeout
 * CONFIG_PDC_POWER_MGMT_STATE_MACHINE_SETTLED_TIMEOUT_MS
 * @retval 0 if successful or -ETIMEDOUT or error code
 */
int pdc_power_mgmt_wait_for_sync(int port, int timeout_ms);

/**
 * @brief Register for notifications from PDM when connector interrupts occur.
 *
 * In order to keep the PPM and the PDC power mgmt api in sync, we let PDM first
 * handle interrupts and then forward them to the PPM.
 *
 * @param callback - Callback for when a new connector interrupt is seen.
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_register_ppm_callback(const struct pdc_callback *callback);

/**
 * @brief Register board callbacks for PDC events .
 *
 * In order to keep the PPM and the PDC power mgmt api in sync, we let PDM first
 * handle interrupts and then forward them to the PPM.
 *
 * @param callback - Callback for when a new connector interrupt is seen.
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_register_board_callback(enum pdc_power_mgmt_board_cb_t type,
					   const void *callback);
/**
 * @brief Acknowledge connector status change bits with PDM.
 *
 * @param port - USB-C port number
 * @param ci - Connector Status change bits to acknowledge
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_ppm_ack_status_change(int port,
					 union conn_status_change_bits_t ci);

/**
 * @brief Board hook for DP Attention event
 *
 * @param port USB-C port number
 * @param port vdo_dp_status Attention VDO
 */
typedef void (*pdc_power_mgmt_board_dp_attention_cb)(int port,
						     uint32_t vdo_dp_status);
/**
 * @brief Board hook for port unattached event
 *
 * @param port USB-C port number
 */
typedef void (*pdc_power_mgmt_board_unattached_cb)(int port);

/**
 * @brief Get the latest DP Attention/Status VDO for the port.
 *
 * @param port USB-C port number
 *
 * @retval Cached Attention VDO
 */
uint32_t pdc_power_mgmt_get_dp_status(int port);

/**
 * @brief Get DP mux mode by the pin assignment
 *
 * @param port USB-C port number
 *
 * @retval Mux state for the DP pin assignment
 */
mux_state_t pdc_power_mgmt_get_dp_mux_mode(int port);

/**
 * @brief Return the current UCSI connector status on a port for the PPM.
 *
 * This api may have different connector status change bits indicated where the
 * pdc power mgmt api may have triggered role swaps and the LPM did not generate
 * any change bits. This is to make sure the OPM stays in sync with the current
 * role.
 *
 * @param port USB-C port number
 * @param connector_status Output variable to store the connector status
 *
 * @retval 0 if successful or error code
 */
int pdc_power_mgmt_get_connector_status_for_ppm(
	int port, union connector_status_t *connector_status);

/**
 * @brief Find an active port with CCD enabled based on devicetree properties
 *
 * @return port number if found
 * @return -1 if no marked CCD port found
 */
int pdc_power_mgmt_get_ccd_port(void);

#ifdef CONFIG_USBC_PDC_DRIVEN_CCD
/**
 * @brief Reads the current SBU mux operating mode. This targets the port with
 *        the `ccd` property in the devicetree.
 *
 * @param mode Output pointer for current mode to be written to
 * @param port_num Optional output pointer to get the number of the CCD port
 * @return 0 on success, or negative error code
 */
int pdc_power_mgmt_get_sbu_mux_mode(enum pdc_sbu_mux_mode *mode, int *port_num);

/**
 * @brief Sets the SBU mux operating mode. This targets the port with the `ccd`
 *        property in the devicetree.
 *
 * @param mode Output pointer for current mode to be written to
 * @return 0 on success, or negative error code
 */
int pdc_power_mgmt_set_sbu_mux_mode(enum pdc_sbu_mux_mode mode);
#endif /* defined(CONFIG_USBC_PDC_DRIVEN_CCD) */

/**
 * @brief Get a handle to the PDC (LPM) driver serving a given port
 *
 * @return NULL if port is disabled or invalid
 * @return Device pointer on success
 */
const struct device *pdc_power_mgmt_get_port_pdc_driver(int port);

#endif /* __CROS_EC_PDC_POWER_MGMT_H */
