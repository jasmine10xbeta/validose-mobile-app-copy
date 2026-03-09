/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file i2c_driver_interface.h
 * @ingroup i2c_driver
 * @brief
 */

#ifndef I2C_DRIVER_INTERFACE_H_
#define I2C_DRIVER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "../../common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   TWI_DRV_ERROR_NONE = 0,
   TWI_DRV_ERROR_NULL,
   TWI_DRV_TIMEOUT,
   TWI_DRV_ERROR_INIT_GENERAL,
   TWI_DRV_ERROR_INIT,
   TWI_DRV_ERROR_BUSY,
   TWI_DRV_ERROR_RX,
   TWI_DRV_ERROR_DNAK,
   TWI_DRV_ERROR_ANAK,
   TWI_DRV_ERROR_TX,
   TWI_DRV_ERROR_IS_XFER_DONE,
   TWI_DRV_ERROR_NOT_INITIALIZED,
   TWI_DRV_ERROR_MAX,
} TWI_DRV_ERROR;

struct i2c_driver;                                          // Forward declaration
typedef struct i2c_driver_interface i2c_driver_interface_t; // Forward declaration

/**
 * @brief The unit interface is defined in this structure.
 *
 */
typedef struct i2c_driver_interface
{
   struct i2c_driver *parent; // Reference to the containing instance.

   /**
    * @brief Perform blocking read.
    *
    * Timeout guideline: Absolute minimum timeout = 10/twi_frequency * transaction bytes.
    * For ample margin, a safety factor of up to x10 can be used without significantly
    * affecting performance.
    *
    * Example for a transaction consisting of a single address and data byte, at 400kHz:
    *
    * recommended timeout = 10/400k * 2 * 10 = 500us
    *
    * @param address Address of slave device
    * @param p_rx_buffer Buffer to accept received data
    * @param rx_buffer_length Length of data in rx_buffer_length
    * @param timeout_us If > 0, wait at most this number of microseconds before timing out. If zero, don't block.
    */
   result_t (*receive)(const i2c_driver_interface_t *const interface,
                       uint8_t address,
                       uint8_t *p_rx_buffer,
                       uint8_t rx_buffer_length,
                       uint32_t timeout_us);

   result_t (*receive_after_tx_no_stop)(const i2c_driver_interface_t *const interface,
                                        uint8_t address,
                                        uint8_t *p_rx_buffer,
                                        uint8_t rx_buffer_length,
                                        uint32_t timeout_us);

   result_t (*transmit_receive)(const i2c_driver_interface_t *const interface,
                                uint8_t address,
                                uint8_t *p_tx_buffer,
                                uint8_t tx_buffer_length,
                                uint8_t *p_rx_buffer,
                                uint8_t rx_buffer_length,
                                uint32_t timeout_us);

   /**
    * @brief Perform blocking write
    *
    * Timeout guideline: Absolute minimum timeout = 10/twi_frequency * transaction bytes.
    * For ample margin, a safety factor of up to x10 can be used without significantly
    * affecting performance.
    *
    * Example for a transaction consisting of a single address and data byte, at 400kHz:
    *
    * recommended timeout = 10/400k * 2 * 10 = 500us
    *
    * @param address Address of slave device
    * @param p_tx_buffer Buffer storing data to be transmitted
    * @param tx_buffer_length Length of data in rx_buffer_length
    * @param timeout_us If > 0, wait at most this number of microseconds before timing out. If zero, don't block.
    */
   result_t (*transmit)(const i2c_driver_interface_t *const interface,
                        uint8_t address,
                        const uint8_t *p_tx_buffer,
                        uint8_t tx_buffer_length,
                        uint32_t timeout_us);

   /**
    * @brief Perform blocking write, with no stop condition. The stop condition is not generated on the bus after the
    * transfer has completed successfully (allowing for a repeated start in the next transfer).
    *
    * Timeout guideline: Absolute minimum timeout = 10/twi_frequency * transaction bytes.
    * For ample margin, a safety factor of up to x10 can be used without significantly
    * affecting performance.
    *
    * Example for a transaction consisting of a single address and data byte, at 400kHz:
    *
    * recommended timeout = 10/400k * 2 * 10 = 500us
    *
    * @param address Address of slave device
    * @param p_tx_buffer Buffer storing data to be transmitted
    * @param tx_buffer_length Length of data in rx_buffer_length
    */
   result_t (*transmit_no_stop)(const i2c_driver_interface_t *const interface,
                                uint8_t address,
                                const uint8_t *p_tx_buffer,
                                uint8_t tx_buffer_length);

   /**
    * @brief Disable TWI
    *
    * Disable TWI to save power
    */
   result_t (*disable_twi)(const i2c_driver_interface_t *const interface);

   /**
    * @brief Check whether the last transfer operation is complete
    *
    * @param is_done Set to true if the last operation is complete; false otherwise
    */
   result_t (*is_transfer_done)(const i2c_driver_interface_t *const interface, bool *is_done);

   /**
    * @brief Scans the I2C bus for devices. Every address at which a device is found is reported via debug output.
    *
    * @note This function is intended to assist during debugging only.
    */
   result_t (*i2c_bus_scan)(const i2c_driver_interface_t *const interface);
} i2c_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
extern i2c_driver_interface_t
   *gp_i2c_driver_interface; // global pointer to the i2c_driver interface. Note: this unit is treated as a singleton.
/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // I2C_DRIVER_INTERFACE_H_
