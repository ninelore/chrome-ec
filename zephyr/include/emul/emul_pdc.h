/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_PDC_H_
#define ZEPHYR_INCLUDE_EMUL_PDC_H_

#include "drivers/ucsi_v3.h"
#include "usb_pd.h"

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>

#include <drivers/pdc.h>

/**
 * Flags that may be passed to PDC emulators to alter their behavior in some
 * way. Can be used to toggle support for a feature specific to certain PDC FW
 * versions or variations, or similar. May be common to multiple PDC emulators
 * or for a specific one.
 */
enum emul_pdc_feature_flag {
	/** Enable support for SBU mux override commands, used on PDC-driven CCD
	 *  DUTs */
	EMUL_PDC_FEATURE_SBU_MUX_OVERRIDE,
	EMUL_PDC_FEATURE_COUNT,
};

/* Mirror PDC APIs to fetch or set emulated values */
typedef int (*emul_pdc_set_ucsi_version_t)(const struct emul *target,
					   uint16_t version);
typedef int (*emul_pdc_reset_t)(const struct emul *target);
typedef int (*emul_pdc_get_connector_reset_t)(const struct emul *dev,
					      union connector_reset_t *reset);
typedef int (*emul_pdc_set_capability_t)(const struct emul *target,
					 const struct capability_t *caps);
typedef int (*emul_pdc_set_connector_capability_t)(
	const struct emul *target, const union connector_capability_t *caps);
typedef int (*emul_pdc_get_ccom_t)(const struct emul *target,
				   enum ccom_t *ccom);
typedef int (*emul_pdc_get_drp_mode_t)(const struct emul *target,
				       enum drp_mode_t *dm);
typedef int (*emul_pdc_get_supported_drp_modes_t)(const struct emul *target,
						  enum drp_mode_t *modes,
						  uint8_t size, uint8_t *num);

typedef int (*emul_pdc_get_uor_t)(const struct emul *target, union uor_t *uor);
typedef int (*emul_pdc_get_pdr_t)(const struct emul *target, union pdr_t *pdr);
typedef int (*emul_pdc_get_rdo_t)(const struct emul *target, uint32_t *rdo);
typedef int (*emul_pdc_set_partner_rdo_t)(const struct emul *target,
					  uint32_t rdo);
typedef int (*emul_pdc_get_sink_path_t)(const struct emul *target, bool *en);
typedef int (*emul_pdc_set_connector_status_t)(
	const struct emul *target,
	const union connector_status_t *connector_status);
typedef int (*emul_pdc_set_error_status_t)(const struct emul *target,
					   const union error_status_t *es);

typedef int (*emul_pdc_set_vbus_t)(const struct emul *target,
				   const uint16_t *vbus);
typedef int (*emul_pdc_get_pdos_t)(const struct emul *target,
				   enum pdo_type_t pdo_type,
				   enum pdo_offset_t pdo_offset,
				   uint8_t num_pdos, enum pdo_source_t source,
				   uint32_t *pdos);
typedef int (*emul_pdc_set_pdos_t)(const struct emul *target,
				   enum pdo_type_t pdo_type,
				   enum pdo_offset_t pdo_offset,
				   uint8_t num_pdos, enum pdo_source_t source,
				   const uint32_t *pdos);
typedef int (*emul_pdc_set_info_t)(const struct emul *target,
				   const struct pdc_info_t *info);
typedef int (*emul_pdc_set_lpm_ppm_info_t)(const struct emul *target,
					   const struct lpm_ppm_info_t *info);
typedef int (*emul_pdc_set_current_pdo_t)(const struct emul *target,
					  uint32_t pdo);
typedef int (*emul_pdc_set_current_flash_bank_t)(const struct emul *target,
						 uint8_t bank);
typedef int (*emul_pdc_get_retimer_fw_t)(const struct emul *target,
					 bool *enable);

typedef int (*emul_pdc_set_response_delay_t)(const struct emul *target,
					     uint32_t delay_ms);
typedef int (*emul_pdc_get_requested_power_level_t)(
	const struct emul *target, enum usb_typec_current_t *level);

typedef int (*emul_pdc_get_reconnect_req_t)(const struct emul *target,
					    uint8_t *expecting, uint8_t *val);

typedef int (*emul_pdc_pulse_irq_t)(const struct emul *target);

typedef int (*emul_pdc_get_cable_property_t)(const struct emul *target,
					     union cable_property_t *property);
typedef int (*emul_pdc_set_cable_property_t)(
	const struct emul *target, const union cable_property_t property);

typedef int (*emul_pdc_set_vdo_t)(const struct emul *target, uint8_t num_vdos,
				  const uint32_t *vdos);

typedef int (*emul_pdc_get_frs_t)(const struct emul *target, bool *enabled);

typedef int (*emul_pdc_idle_wait_t)(const struct emul *target);

typedef int (*emul_pdc_set_vconn_sourcing_t)(const struct emul *target,
					     bool enabled);

typedef int (*emul_pdc_set_cmd_error_t)(const struct emul *target,
					bool enabled);
typedef int (*emul_pdc_set_attention_vdo_t)(const struct emul *target,
					    union get_attention_vdo_t);
typedef int (*emul_pdc_get_data_role_preference_t)(const struct emul *target,
						   int *swap_to_dfp,
						   int *swap_to_ufp);
typedef int (*emul_pdc_set_feature_flag_t)(const struct emul *target,
					   enum emul_pdc_feature_flag feature);
typedef int (*emul_pdc_clear_feature_flag_t)(
	const struct emul *target, enum emul_pdc_feature_flag feature);
typedef void (*emul_pdc_reset_feature_flags_t)(const struct emul *target);

__subsystem struct emul_pdc_driver_api {
	emul_pdc_set_response_delay_t set_response_delay;
	emul_pdc_set_ucsi_version_t set_ucsi_version;
	emul_pdc_reset_t reset;
	emul_pdc_get_connector_reset_t get_connector_reset;
	emul_pdc_set_capability_t set_capability;
	emul_pdc_set_connector_capability_t set_connector_capability;
	emul_pdc_get_ccom_t get_ccom;
	emul_pdc_get_drp_mode_t get_drp_mode;
	emul_pdc_get_supported_drp_modes_t get_supported_drp_modes;
	emul_pdc_get_uor_t get_uor;
	emul_pdc_get_pdr_t get_pdr;
	emul_pdc_get_rdo_t get_rdo;
	emul_pdc_set_partner_rdo_t set_partner_rdo;
	emul_pdc_get_sink_path_t get_sink_path;
	emul_pdc_set_connector_status_t set_connector_status;
	emul_pdc_set_error_status_t set_error_status;
	emul_pdc_set_vbus_t set_vbus_voltage;
	emul_pdc_get_pdos_t get_pdos;
	emul_pdc_set_current_pdo_t set_current_pdo;
	emul_pdc_set_pdos_t set_pdos;
	emul_pdc_set_info_t set_info;
	emul_pdc_set_lpm_ppm_info_t set_lpm_ppm_info;
	emul_pdc_set_current_flash_bank_t set_current_flash_bank;
	emul_pdc_get_retimer_fw_t get_retimer;
	emul_pdc_get_requested_power_level_t get_requested_power_level;
	emul_pdc_get_reconnect_req_t get_reconnect_req;
	emul_pdc_pulse_irq_t pulse_irq;
	emul_pdc_set_cable_property_t set_cable_property;
	emul_pdc_get_cable_property_t get_cable_property;
	emul_pdc_set_vdo_t set_vdo;
	emul_pdc_get_frs_t get_frs;
	emul_pdc_idle_wait_t idle_wait;
	emul_pdc_set_vconn_sourcing_t set_vconn_sourcing;
	emul_pdc_set_cmd_error_t set_cmd_error;
	emul_pdc_set_attention_vdo_t set_attention_vdo;
	emul_pdc_get_data_role_preference_t get_data_role_preference;
	emul_pdc_set_feature_flag_t set_feature_flag;
	emul_pdc_clear_feature_flag_t clear_feature_flag;
	emul_pdc_reset_feature_flags_t reset_feature_flags;
};

static inline int emul_pdc_set_ucsi_version(const struct emul *target,
					    uint16_t version)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_ucsi_version) {
		return api->set_ucsi_version(target, version);
	}
	return -ENOSYS;
}

static inline int emul_pdc_reset(const struct emul *target)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->reset) {
		return api->reset(target);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_connector_reset(const struct emul *target,
					       union connector_reset_t *reset)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_connector_reset) {
		return api->get_connector_reset(target, reset);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_capability(const struct emul *target,
					  const struct capability_t *caps)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_capability) {
		return api->set_capability(target, caps);
	}
	return -ENOSYS;
}

static inline int
emul_pdc_set_connector_capability(const struct emul *target,
				  const union connector_capability_t *caps)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_connector_capability) {
		return api->set_connector_capability(target, caps);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_ccom(const struct emul *target,
				    enum ccom_t *ccom)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_ccom) {
		return api->get_ccom(target, ccom);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_drp_mode(const struct emul *target,
					enum drp_mode_t *dm)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_drp_mode) {
		return api->get_drp_mode(target, dm);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_supported_drp_modes(const struct emul *target,
						   enum drp_mode_t *modes,
						   uint8_t size, uint8_t *num)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_supported_drp_modes) {
		return api->get_supported_drp_modes(target, modes, size, num);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_uor(const struct emul *target, union uor_t *uor)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_uor) {
		return api->get_uor(target, uor);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_pdr(const struct emul *target, union pdr_t *pdr)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_pdr) {
		return api->get_pdr(target, pdr);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_rdo(const struct emul *target, uint32_t *rdo)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_rdo) {
		return api->get_rdo(target, rdo);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_partner_rdo(const struct emul *target,
					   uint32_t rdo)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_partner_rdo) {
		return api->set_partner_rdo(target, rdo);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_sink_path(const struct emul *target, bool *en)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_sink_path) {
		return api->get_sink_path(target, en);
	}
	return -ENOSYS;
}

static inline int
emul_pdc_set_connector_status(const struct emul *target,
			      const union connector_status_t *connector_status)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_connector_status) {
		return api->set_connector_status(target, connector_status);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_error_status(const struct emul *target,
					    const union error_status_t *es)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_error_status) {
		return api->set_error_status(target, es);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_vbus(const struct emul *target,
				    const uint16_t *vbus)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_vbus_voltage) {
		return api->set_vbus_voltage(target, vbus);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_pdos(const struct emul *target,
				    enum pdo_type_t pdo_type,
				    enum pdo_offset_t pdo_offset,
				    uint8_t num_pdos, enum pdo_source_t source,
				    uint32_t *pdos)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_pdos) {
		return api->get_pdos(target, pdo_type, pdo_offset, num_pdos,
				     source, pdos);
	}
	return -ENOSYS;
}

/*
 * Set the PDOs of the LPM or the partner.
 *
 * When setting the partner sink PDO, this function automatically sets
 * the partner RDO to match the fixed PDO provided.  To change the partner
 * RDO, call emul_pdc_set_partner_rdo() after this function.
 */
static inline int emul_pdc_set_pdos(const struct emul *target,
				    enum pdo_type_t pdo_type,
				    enum pdo_offset_t pdo_offset,
				    uint8_t num_pdos, enum pdo_source_t source,
				    const uint32_t *pdos)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_pdos) {
		return api->set_pdos(target, pdo_type, pdo_offset, num_pdos,
				     source, pdos);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_info(const struct emul *target,
				    const struct pdc_info_t *info)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_info) {
		return api->set_info(target, info);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_lpm_ppm_info(const struct emul *target,
					    const struct lpm_ppm_info_t *info)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_lpm_ppm_info) {
		return api->set_lpm_ppm_info(target, info);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_current_pdo(const struct emul *target,
					   uint32_t pdo)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_current_pdo) {
		return api->set_current_pdo(target, pdo);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_current_flash_bank(const struct emul *target,
						  uint8_t bank)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_current_flash_bank) {
		return api->set_current_flash_bank(target, bank);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_retimer_fw(const struct emul *target,
					  bool *enable)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_retimer) {
		return api->get_retimer(target, enable);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_response_delay(const struct emul *target,
					      uint32_t delay_ms)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_response_delay) {
		return api->set_response_delay(target, delay_ms);
	}
	return -ENOSYS;
}

static inline int
emul_pdc_get_requested_power_level(const struct emul *target,
				   enum usb_typec_current_t *level)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_requested_power_level) {
		return api->get_requested_power_level(target, level);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_reconnect_req(const struct emul *target,
					     uint8_t *expecting, uint8_t *val)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_reconnect_req) {
		return api->get_reconnect_req(target, expecting, val);
	}
	return -ENOSYS;
}

static inline int emul_pdc_pulse_irq(const struct emul *target)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->pulse_irq) {
		return api->pulse_irq(target);
	}
	return -ENOSYS;
}

static inline int emul_pdc_get_cable_property(const struct emul *target,
					      union cable_property_t *property)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_cable_property) {
		return api->get_cable_property(target, property);
	}
	return -ENOSYS;
}

static inline int
emul_pdc_set_cable_property(const struct emul *target,
			    const union cable_property_t property)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_cable_property) {
		return api->set_cable_property(target, property);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_vdo(const struct emul *target, uint8_t num_vdos,
				   const uint32_t *vdos)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_vdo) {
		return api->set_vdo(target, num_vdos, vdos);
	}
	return -ENOSYS;
}

static inline void
emul_pdc_configure_src(const struct emul *target,
		       union connector_status_t *connector_status)
{
	uint32_t partner_pdos[PDO_OFFSET_MAX] = {
		PDO_FIXED(5000, 3000, 0),
		PDO_FIXED(12000, 3000, 0),
		PDO_FIXED(20000, 5000, 0),
	};

	connector_status->power_operation_mode = PD_OPERATION;
	connector_status->power_direction = 1;

	emul_pdc_set_pdos(target, SOURCE_PDO, PDO_OFFSET_0,
			  ARRAY_SIZE(partner_pdos), PARTNER_PDO, partner_pdos);
}

static inline void
emul_pdc_configure_snk(const struct emul *target,
		       union connector_status_t *connector_status)
{
	uint32_t partner_pdos[] = {
		PDO_FIXED(5000, 3000, 0),
		PDO_FIXED(12000, 3000, 0),
		PDO_FIXED(20000, 5000, 0),
	};

	connector_status->power_operation_mode = PD_OPERATION;
	connector_status->power_direction = 0;

	emul_pdc_set_pdos(target, SOURCE_PDO, PDO_OFFSET_0,
			  ARRAY_SIZE(partner_pdos), PARTNER_PDO, partner_pdos);
}

static inline int
emul_pdc_connect_partner(const struct emul *target,
			 union connector_status_t *connector_status)
{
	connector_status->connect_status = 1;
	emul_pdc_set_connector_status(target, connector_status);
	emul_pdc_pulse_irq(target);

	return 0;
}

static inline int emul_pdc_disconnect(const struct emul *target)
{
	union connector_status_t connector_status;
	uint32_t partner_pdos[PDO_MAX_OBJECTS] = { 0 };

	emul_pdc_set_pdos(target, SOURCE_PDO, PDO_OFFSET_0, PDO_MAX_OBJECTS,
			  PARTNER_PDO, partner_pdos);
	emul_pdc_set_pdos(target, SINK_PDO, PDO_OFFSET_0, PDO_MAX_OBJECTS,
			  PARTNER_PDO, partner_pdos);

	connector_status.connect_status = 0;
	emul_pdc_set_connector_status(target, &connector_status);
	emul_pdc_pulse_irq(target);

	return 0;
}

static inline int emul_pdc_get_frs(const struct emul *target, bool *enabled)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_frs) {
		return api->get_frs(target, enabled);
	}

	return -ENOSYS;
}

static inline int emul_pdc_get_data_role_preference(const struct emul *target,
						    int *swap_to_dfp,
						    int *swap_to_ufp)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->get_data_role_preference) {
		return api->get_data_role_preference(target, swap_to_dfp,
						     swap_to_ufp);
	}

	return -ENOSYS;
}

static inline int emul_pdc_idle_wait(const struct emul *target)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->idle_wait) {
		return api->idle_wait(target);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_vconn_sourcing(const struct emul *target,
					      bool enabled)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_vconn_sourcing) {
		return api->set_vconn_sourcing(target, enabled);
	}
	return -ENOSYS;
}

static inline int emul_pdc_set_cmd_error(const struct emul *target,
					 bool enabled)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;

	if (api->set_cmd_error) {
		return api->set_cmd_error(target, enabled);
	}
	return -ENOSYS;
}

static inline int
emul_pdc_set_attention_vdo(const struct emul *target,
			   union get_attention_vdo_t attention_vdo)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;
	if (api->set_attention_vdo) {
		return api->set_attention_vdo(target, attention_vdo);
	}
	return -ENOSYS;
}

/* LCOV_EXCL_START - Internal emulator feature */
static inline int emul_pdc_set_feature_flag(const struct emul *target,
					    enum emul_pdc_feature_flag feature)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;
	if (api->set_feature_flag) {
		return api->set_feature_flag(target, feature);
	}
	return -ENOSYS;
}

static inline int
emul_pdc_clear_feature_flag(const struct emul *target,
			    enum emul_pdc_feature_flag feature)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;
	if (api->clear_feature_flag) {
		return api->clear_feature_flag(target, feature);
	}
	return -ENOSYS;
}

static inline int emul_pdc_reset_feature_flags(const struct emul *target)
{
	if (!target || !target->backend_api) {
		return -ENOTSUP;
	}

	const struct emul_pdc_driver_api *api = target->backend_api;
	if (api->reset_feature_flags) {
		api->reset_feature_flags(target);
		return 0;
	}
	return -ENOSYS;
}
/* LCOV_EXCL_STOP - Internal emulator feature */

#endif /* ZEPHYR_INCLUDE_EMUL_PDC_H_ */
