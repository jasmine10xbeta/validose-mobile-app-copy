/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file rfal_nrf5_port.h
 * @brief RFAL platform glue for nRF5 platform.
 *
 * This file, together with rfal_nrf5_port.c, provides the hardware abstraction layer (HAL) and platform glue code
 * required to connect the STMicroelectronics RFAL (RF Abstraction Layer) library to Nordic nRF52 series MCUs.
 *
 * It implements initialization, I2C and timing interface, and error handling for the ST25R3916B NFC frontend.
 *
 * - rfal_nrf5_port.h: Declarations and error codes for the platform glue.
 * - rfal_nrf5_port.c: Implements the actual hardware access, IRQ handling, and platform-specific logic.
 */

#ifndef RFAL_NRF5_PORT_H_
#define RFAL_NRF5_PORT_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
#include "i2c_driver.h"
#include "system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define TX_DRIVER_D_RES_MASK (0x0Fu)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for the RFAL nRF5 platform glue layer.
 */
typedef enum
{
   NFC_RFAL_GLUE_ERROR_NONE = 0,
   NFC_RFAL_GLUE_ERROR_PTR_NULL,
   NFC_RFAL_GLUE_ERROR_MAX,
} NFC_RFAL_GLUE_ERROR;
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initialize the RFAL platform glue for nRF5.
 *
 * This function must be called before using any RFAL features. It sets up the I2C and system timer interfaces
 * required by the ST25R3916B NFC frontend and the RFAL library. The function stores the provided driver interfaces
 * for later use by the platform glue code.
 *
 * @param i2c_interface Pointer to the I2C driver interface to use for communication with the NFC frontend.
 * @param systick_interface Pointer to the system time interface for delays and timeouts.
 * @return RESULT_OK on success, or an error code on failure.
 */
result_t rfal_nrf_platform_init(const i2c_driver_interface_t *i2c_interface,
                                const system_time_interface_t *systick_interface);

#endif // RFAL_NRF5_PORT_H_
