/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file imu.h
 * @ingroup mock/drivers/imu
 * @brief Header file for the mock IMU driver used for unit testing
 */

#ifndef MOCK_IMU_H
#define MOCK_IMU_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"
#include "imu_interface.h"

#include <stdbool.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Maximum number of data entries the mock IMU can simulate. Corresponds to maximum frames in the IMU FIFO. */
#define IMU_MAX_DATA_ENTRIES (IMU_FIFO_SIZE / IMU_FRAME_SIZE)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Additional error definitions for testing
 */
typedef enum
{
   MOCK_IMU_ERROR_NONE = IMU_ERROR_MAX, /**< No error. Starts after the real errors */

   MOCK_IMU_ERROR_NULL,        /**< Unexpected NULL reference */
   MOCK_IMU_ERROR_INVALID_ARG, /**< Invalid argument */

   MOCK_IMU_ERROR_MAX /**< Sentinel value */
} MOCK_IMU_ERROR;

/**
 * @brief Mock IMU instance
 */
typedef struct imu
{
   // Interface
   imu_interface_t interface;

   // Private data
   IMU_STATE _state;

   imu_data_entry_t _mock_raw_imu_data_entries[IMU_MAX_DATA_ENTRIES]; /**< Mock raw IMU data entries */
   uint16_t _mock_imu_data_entry_count;      /**< Number of data entries to simulate in the IMU fifo */
   uint16_t _mock_imu_data_entry_read_index; /**< Current read index for the mock IMU data entries */
   int16_t _mock_temperature_degc;           /**< Mock temperature in degrees Celsius */
   imu_interrupt_map_t _mock_interrupt_map;  /**< Mock interrupt map */

   bool _fail_imu_reg_read;  /**< Whether to simulate a failure in the imu_reg_read function */
   bool _fail_imu_reg_write; /**< Whether to simulate a failure in the imu_reg_write function */

   bool _is_initialized; /**< Whether the instance has been initialized */
} imu_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes a mock IMU instance
 *
 * @param[in,out] p_self Pointer to the IMU instance to initialize
 *
 * @return Status code indicating result of the operation
 */
result_t mock_imu_init(imu_t *const p_self);

/**
 * @brief Sets the mock IMU data entries to be returned by the get_imu_data_entries function
 *
 * @param[in,out] p_self Pointer to the IMU instance
 * @param[in] p_entries Pointer to the array of imu_data_entry_t to set
 * @param[in] entry_count Number of entries in the p_entries array
 *
 * @return Status code indicating result of the operation
 *
 * @note The IMU driver (both mock and real) converts the IMU raw data to units of mm/s² for acceleration. The data
 * passed to this function should be in raw units as read from the IMU sensor (i.e., LSBs). The mock driver will perform
 * the conversion internally.
 */
result_t
   mock_imu_set_data_entries(imu_t *const p_self, const imu_data_entry_t *const p_entries, const uint16_t entry_count);

/**
 * @brief Sets the mock IMU temperature to be returned by the get_temperature_celsius function
 *
 * @param[in,out] p_self Pointer to the IMU instance
 * @param[in] temperature_degc Temperature in degrees Celsius to set
 *
 * @return Status code indicating result of the operation
 */
result_t mock_imu_set_temperature(imu_t *const p_self, const int16_t temperature_degc);

/**
 * @brief Sets the mock IMU interrupt map to be returned by the get_interrupts function
 */
result_t mock_imu_set_interrupt_map(imu_t *const p_self, const imu_interrupt_map_t *const p_map);

#endif /* MOCK_IMU_H */