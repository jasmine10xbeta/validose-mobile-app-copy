/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/*
 * Mock I2C Driver Header File
 */

#ifndef I2C_DRIVER_MOCK_H_

#define I2C_DRIVER_MOCK_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard Includes
// Custom Includes
#include "common.h"
#include "i2c_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
typedef void (*on_mock_i2c_tx_data_ptr)(const uint8_t *tx_data, uint8_t tx_data_len);
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct i2c_driver
{
   i2c_driver_interface_t interface; // Interface for I2C operations
} i2c_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Function Declarations
 **********************************************************************************************************************/
result_t mock_i2c_driver_init(i2c_driver_t *const self, on_mock_i2c_tx_data_ptr on_tx_data, uint16_t device_address);
void mock_i2c_set_rx_data(uint8_t *rx_data, uint8_t length);

#endif // I2C_DRIVER_MOCK_H_