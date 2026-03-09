/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup crc32 CRC32
 * @ingroup common
 * @brief Implements the CRC-32 (IEEE 802.3 / ISO-HDLC) algorithm.
 * @details
 * Reference: https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat.crc-32-iso-hdlc
 *
 * @file crc32.h
 * @ingroup crc32
 * @brief
 */

#ifndef CRC32_H_
#define CRC32_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>
#include <stdlib.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Calculate CRC32 (polynomial: 0x04C11DB7, reflected 0xEDB88320)
 *
 * @param p_data [in] Pointer to the data over which to calculate the CRC.
 * @param data_len [in] Number of bytes in `p_data`.
 * @param p_crc [in] Pointer to the running CRC value; pass NULL to start with 0xFFFFFFFF.
 *
 * @return The updated CRC value. For a finalized CRC-32/IEEE 802.3 value, XOR the result with 0xFFFFFFFF.
 */
uint32_t crc32_update(uint8_t const *p_data, size_t data_len, uint32_t const *p_crc);
#endif /* CRC32_H_ */
