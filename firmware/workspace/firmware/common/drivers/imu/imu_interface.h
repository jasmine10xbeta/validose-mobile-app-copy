/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file imu_interface.h
 * @ingroup imu_driver
 * @brief
 */

#ifndef IMU_INTERFACE_H_
#define IMU_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"

#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define IMU_FIFO_SIZE       (2048u)                          /**< Size of the IMU fifo in bytes */
#define IMU_FRAME_SIZE      (14u)                            /**< Size of an IMU frame in bytes */
#define IMU_MAX_FIFO_FRAMES (IMU_FIFO_SIZE / IMU_FRAME_SIZE) /**< Maximum number of frames in the IMU fifo */

/**
 * @brief Sampling rate of the IMU when active
 *
 * @note The allowed sampling rates are defined by the IMU hardware capabilities. Refer to the driver implementation for
 * details.
 */
#define IMU_ACTIVE_SAMPLING_RATE_HZ (100u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   IMU_ERROR_NONE = 0,
   IMU_ERROR_PTR_NULL,
   IMU_ERROR_INIT_FAILURE,
   IMU_ERROR_ALREADY_INITIALIZED,
   IMU_ERROR_I2C_ERROR,
   IMU_ERROR_INVALID_ARG,
   IMU_ERROR_INVALID_STATE,
   IMU_ERROR_NO_ENTRIES,
   IMU_ERROR_MAX,
} IMU_ERROR;

struct imu; // Forward declaration

/**
 * @brief Enum representing IMU states
 */
typedef enum
{
   IMU_STATE_UNINITIALIZED = 0,
   IMU_STATE_DORMANT,
   IMU_STATE_ACTIVE,
   IMU_STATE_MAX
} IMU_STATE;

/**
 * @brief A struct to contain a map of the IMU interrupts
 */
typedef struct
{
   bool single_tap;
   bool double_tap;
   bool triple_tap;
   bool sig_motion;
   bool limited_data_storage_remaining;
} imu_interrupt_map_t;

/**
 * @brief A struct to hold a frame of IMU data
 *
 * The IMU stores movement data as a frame that consists of all the enabled sensor data
 * concatenated.
 *
 * The IMU handles handles reads from and writes to the fifo as full frames - only complete
 * frame reads will result in the data being removed from the fifo
 * For this reason movement data will be handled in full frames
 */
typedef struct __attribute__((packed, aligned(1)))
{
   int16_t acc_x_axis;
   int16_t acc_y_axis;
   int16_t acc_z_axis;

   int16_t angular_rate_x_axis;
   int16_t angular_rate_y_axis;
   int16_t angular_rate_z_axis;

   uint16_t sensor_time;

} imu_data_entry_t;

typedef struct imu_interface imu_interface_t;

struct imu_interface
{
   struct imu *parent; // Reference to the containing instance.

   /**
    * @brief Gets the current temperature from the IMU
    *
    * @param interface A pointer to the interface instance.
    * @param temperature_celsius: A pointer to a signed 16 bit integer indicating the temperature detected by the IMU.
    * @return result_t A status code indicating success or failure.
    */
   result_t (*get_temperature_celsius)(const imu_interface_t *const interface, int16_t *temperature_celsius);

   /**
    * @brief Set the state of the IMU
    *
    * @param interface A pointer to the interface instance.
    * @param state: Enum indicating the state to which the IMU must be set
    * @return status code indicating success or failure.
    *
    * @note: Setting the IMU to ACTIVE state or UNINITIALIZED state will block internally until the startup time of the
    * IMU has elapsed. See implementation for details.
    */
   result_t (*set_state)(const imu_interface_t *const interface, IMU_STATE state);

   /**
    * @brief Get the state of the IMU
    *
    * @param interface A pointer to the interface instance.
    * @param state: Pointer to an IMU_STATE enum to be set with the IMU state
    * @return status code indicating success or failure.
    */
   result_t (*get_state)(const imu_interface_t *const interface, IMU_STATE *state);

   /**
    * @brief Get a map of interrupts that have been triggered from the IMU
    *
    * @param interface A pointer to the interface instance.
    * @param map: A pointer to an interrupt map struct that will be populated with triggered interrupts
    * @return status code indicating success or failure.
    */
   result_t (*get_interrupts)(const imu_interface_t *const interface, imu_interrupt_map_t *map);

   /**
    * @brief Get the number of data frames currently stored in the IMU fifo
    *
    * @param interface: A pointer to the interface instance.
    * @param count: A pointer to a uint16_t that will be set to the number of data frames in the buffer
    * @return status code indicating success or failure.
    */
   result_t (*get_imu_data_entry_count)(const imu_interface_t *const interface, uint16_t *count);

   /**
    * @brief Flushes all the fifo buffer of all the frames currently contained therein
    *
    * @param interface: A pointer to the interface instance.
    * @return status code indicating success or failure.
    */
   result_t (*clear_imu_data)(const imu_interface_t *const interface);

   /**
    * @brief Retrieves the given number of frames from the fifo
    *
    * @param interface: A pointer to the interface instance.
    * @param count: Number of entries to copy out of the IMU
    * @param entries: Pointer to the memory allocated to receive the entries
    * @return status code indicating success or failure.
    */
   result_t (*get_imu_data_entries)(const imu_interface_t *const interface, uint16_t count, imu_data_entry_t *entries);

   /**
    * @brief Retrieves the latest frame from the fifo and discards the rest
    *
    * @param interface: A pointer to the interface instance.
    * @param entry: Pointer to the memory allocated to receive the entry
    * @return status code indicating success or failure.
    */
   result_t (*get_imu_data_entry)(const imu_interface_t *const interface, imu_data_entry_t *entry);
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // IMU_INTERFACE_H_
