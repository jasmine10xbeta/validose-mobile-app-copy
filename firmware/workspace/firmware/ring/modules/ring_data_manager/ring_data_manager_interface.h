/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_data_manager_interface.h
 * @ingroup ring data manager
 * @brief Interface for the ring data manager module
 */

#ifndef RING_DATA_MANAGER_INTERFACE_H_
#define RING_DATA_MANAGER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
#include "debug.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
// Error codes specific to the module.
typedef enum
{
   RING_DATA_MANAGER_ERROR_NONE = 0,
   RING_DATA_MANAGER_ERROR_NOT_INITIALIZED,
   RING_DATA_MANAGER_ERROR_NULL_PTR,
   RING_DATA_MANAGER_ERROR_NULL_INTERFACE_PTR,
   RING_DATA_MANAGER_ERROR_INVALID_ARG,
   RING_DATA_MANAGER_ERROR_MAX,
} RING_DATA_MANAGER_ERROR;

// Forward declaration of the ring data manager struct and its interface
struct ring_data_manager;
typedef struct ring_data_manager_interface ring_data_manager_interface_t;

typedef struct ring_data_manager_interface
{
   struct ring_data_manager *parent; // Reference to the containing instance.

   /**
    * @brief Run the Ring Data Manager
    *
    * @details This function should be called periodically to update data points managed by the data manager.
    *
    * The data manager will sample data at the sampling rate that has been set for each data point
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*process)(const ring_data_manager_interface_t *const interface);

   /**
    * @brief Update the battery sample rate
    *
    * @details This function updates the sample rate for the battery data point.
    *
    * @param sample_rate_millihz The new sample rate in millihertz.
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*update_battery_sample_rate)(const ring_data_manager_interface_t *const interface,
                                          uint16_t sample_rate_millihz);

   /**
    * @brief Get the current ring status
    *
    * @details This function retrieves the current status of the ring including hardware and firmware versions,
    *          MAC address, uptime, temperature, flash usage, and sampling frequencies.
    *
    * @param status Pointer to a ring_status_t structure where the status will be stored.
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*get_status)(const ring_data_manager_interface_t *const interface, ring_status_t *const status);

   /**
    * @brief Update the current time
    *
    * @details This function updates the current systick time
    *
    * @param time_ms The new current time in milliseconds.
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*update_time)(const ring_data_manager_interface_t *const interface, uint64_t time_ms);

   /**
    * @brief Enqueue error data
    *
    * @details This function enqueues error data into the error queue
    *
    * @param debug_log Pointer to the error data to be enqueued.
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*enqueue_error)(const ring_data_manager_interface_t *const ifc, raw_debug_log_t *debug_log);

   /**
    * @brief Update proximity data
    *
    * @details This function updates the proximity data (cap on and cap off values)
    *
    * @param prox_data The new proximity data.
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*update_prox_data)(const ring_data_manager_interface_t *const ifc, prox_data_t prox_data);

} ring_data_manager_interface_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // RING_DATA_MANAGER_INTERFACE_H_