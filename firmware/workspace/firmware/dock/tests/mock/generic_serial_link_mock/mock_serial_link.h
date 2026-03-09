/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef MOCK_SERIAL_DRIVER_H_
#define MOCK_SERIAL_DRIVER_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "comms_driver_interface.h"
#include "queue.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MAX_SERIAL_RX_DATA_LENGTH (100u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct
{
   uint32_t data_length;
   uint8_t data[MAX_SERIAL_RX_DATA_LENGTH];
} element_data_t;

// Error codes specific to the module.
typedef enum
{
   MOCK_LL_ERROR_NONE = 0,         // No error
   MOCK_LL_ERROR_PTR_NULL,         // Null pointer error
   MOCK_LL_ERROR_INIT_FAILURE,     // Failure to initialize driver
   MOCK_LL_ERROR_NRF_ERR_CHECK,    // Generic Nordic SDK error
   MOCK_LL_ERROR_DEVICE_NOT_FOUND, // UART IR driver not detected
   MOCK_LL_ERROR_UNINITIALIZED,    // Driver not initialized
   MOCK_LL_ERROR_COMM_RX,          // Communication error on RX
   MOCK_LL_ERROR_COMM_TX,          // Communication error on TX
   MOCK_LL_ERROR_HW_FAULT,         // Hardware fault
   MOCK_LL_ERROR_MAX
} MOCK_LL_ERROR;

typedef struct comms_driver
{
   comms_driver_interface_t interface;

   bool _initialized;
   uint16_t _max_packet_len;
} serial_link_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t serial_link_driver_init(serial_link_driver_t *const self);

#endif // MOCK_SERIAL_DRIVER_H_
