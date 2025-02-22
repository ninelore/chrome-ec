/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief PDC PDO helper functions
 */

#ifndef __EMUL_PDC_PDO_H
#define __EMUL_PDC_PDO_H

#include "drivers/pdc.h"
#include "drivers/ucsi_v3.h"

struct emul_pdc_pdo_t {
	/* PDOs and RDO of the LPM */
	uint32_t snk_pdos[PDO_OFFSET_MAX];
	uint32_t src_pdos[PDO_OFFSET_MAX];
	uint32_t rdo;
	/* PDOs and RDO of the partner */
	uint32_t partner_snk_pdos[PDO_OFFSET_MAX];
	uint32_t partner_src_pdos[PDO_OFFSET_MAX];
	uint32_t partner_rdo;
};

/**
 * @brief Reset PDOs structure to defaults
 *
 * @param pdos
 * @return int
 */
int emul_pdc_pdo_reset(struct emul_pdc_pdo_t *pdos);

/**
 * @brief Gets PDO data based on pdo_type and source
 *
 * @param data
 * @param pdo_type
 * @param pdo_offset
 * @param num_pdos
 * @param source
 * @param pdos
 * @return int
 */
int emul_pdc_pdo_get_direct(struct emul_pdc_pdo_t *data,
			    enum pdo_type_t pdo_type,
			    enum pdo_offset_t pdo_offset, uint8_t num_pdos,
			    enum pdo_source_t source, uint32_t *pdos);

/**
 * @brief Sets PDO data based on pdo_type and source
 *
 * @param data Pointer to the emulator's collection of LPM and partner PDOs
 * @param pdo_type PDO type to update
 * @param pdo_offset Index of the first PDO to write
 * @param num_pdos Number of PDOs in this call
 * @param source Indicates LPM or partner PDOs
 * @param pdos Array of new PDOs, must contain @num_pdos entries
 * @return int
 */
int emul_pdc_pdo_set_direct(struct emul_pdc_pdo_t *data,
			    enum pdo_type_t pdo_type,
			    enum pdo_offset_t pdo_offset, uint8_t num_pdos,
			    enum pdo_source_t source, const uint32_t *pdos);

#endif /* __EMUL_PDC_PDO_H */
