/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dock_data_manager_interface.h
 * @ingroup dock_data_manager_module
 * @brief Interface definition for the dock data manager module.
 */

#ifndef DOCK_DATA_MANAGER_INTERFACE_H_
#define DOCK_DATA_MANAGER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef enum
{
   DOCK_DATA_MANAGER_ERROR_NONE = 0,      /**<No Error */
   DOCK_DATA_MANAGER_QUEUE_FULL,          /**< The internal queue is full and cannot accept new data. */
   DOCK_DATA_MANAGER_ERROR_NULL,          /**< Generic Null Pointer Error */
   DOCK_DATA_MANAGER_ERROR_INVALID_PARAM, /**< Invalid parameter passed to function */
   DOCK_DATA_MANAGER_ERROR_FDS,           /**< Flash data storage error */
   DOCK_DATA_MANAGER_ERROR_MAX,
} DOCK_DATA_MANAGER_ERROR;

typedef enum
{
   DATA_ID_DOSE_EVENT = 0,     /**< Expects dose_event_t type. Dose event Data */
   DATA_ID_DOSE_DATAPOINT,     /**< Expects dose_data_t type. Dose IMU Datapoint */
   DATA_ID_RING_DOCKED_STATUS, /**< Expects ring_docked_status_t type. Docking status of the ring */
   DATA_ID_WEIGHT_MEASUREMENT, /**< Expects dock_weight_measurement_t type. Weight measurement data */
   DATA_ID_TEMPERATURE_LOG,    /**< Expects temperature_log_t type. Temperature log from the dock */
   DATA_ID_RING_BATTERY_LEVEL, /**< Expects battery_level_t type. Battery level of the ring */
   DATA_ID_DOCK_BATTERY_LEVEL, /**< Expects battery_level_t type. Battery level of the dock */
   DATA_ID_DOCK_CHARGE_STATUS, /**< Expects dock_charge_status_t type. Dock charge status */
   DATA_ID_RING_STATUS,        /**< Expects ring_status_t type. Ring status */
   DATA_ID_DOCK_STATUS,        /**< Expects dock_status_t type. Dock status */
   DATA_ID_DOCK_DEBUG_LOG,     /**< Expects raw_debug_log_t type. Debug log from the dock */
   DATA_ID_RING_DEBUG_LOG,     /**< Expects raw_debug_log_t type. Debug log from the ring */
   DATA_ID_BLUETOOTH_STATUS,   /**< Expects bluetooth_status_t type. Current Bluetooth status of the dock */
   DATA_ID_MAX,
} DATA_ID;

struct dock_data_manager;                                                 // Forward declaration
typedef struct dock_data_manager_interface dock_data_manager_interface_t; // Forward declaration

/**
 * @struct dock_data_manager_interface
 * @brief Structure defining the dock data manager interface for data and event management operations.
 *
 * This structure contains function pointers for data management operations and a reference to the parent dock data
 * manager instance.
 *
 * @note
 * - For all "enqueue* " functions, if the internal queue is full, the function will return an appropriate error code
 *   indicating that the log or event was not queued with the data manager, resulting in potential data loss.
 */

struct dock_data_manager_interface
{
   struct dock_data_manager *parent; // Reference to the parent dock data manager instance

   /**
    * @brief Enqueue data into the specified data queue.
    *
    * @param data_id The ID of the data queue to enqueue data into.
    * @param data Pointer to the data to be enqueued.
    * @param element_size Size of each data element in bytes.
    * @param element_count Number of elements to enqueue.
    *
    * @return result_t indicating the success or failure of the operation.
    */
   result_t (*enqueue)(const dock_data_manager_interface_t *const interface,
                       DATA_ID data_id,
                       void *data,
                       size_t element_size,
                       size_t element_count);

   /**
    * @brief Dequeue data from the specified data queue.
    *
    * @param data_id The ID of the data queue to dequeue data from.
    * @param data Pointer to the buffer where dequeued data will be stored.
    * @param element_size Size of each data element in bytes.
    * @param element_count Number of elements to dequeue.
    *
    * @return result_t indicating the success or failure of the operation.
    */
   result_t (*dequeue)(const dock_data_manager_interface_t *const interface,
                       DATA_ID data_id,
                       void *data,
                       size_t element_size,
                       size_t element_count);

   /**
    * @brief Peek at data in the specified data queue without removing it.
    *
    * @param data_id The ID of the data queue to peek at.
    * @param data Pointer to the buffer where peeked data will be stored.
    * @param element_size Size of each data element in bytes.
    * @param element_count Number of elements to peek at.
    *
    * @return result_t indicating the success or failure of the operation.
    */

   result_t (*peek)(const dock_data_manager_interface_t *const interface,
                    DATA_ID data_id,
                    void *data,
                    size_t element_size,
                    size_t element_count);

   /**
    * @brief Pop data from the specified data queue, removing it from the queue.
    * @param data_id The ID of the data queue to pop data from.
    * @param element_count Number of elements to pop from the queue.
    * @return result_t indicating the success or failure of the operation.
    */

   result_t (*pop)(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t element_count);

   /**
    * @brief Get the number of free elements available in the specified data queue.
    *
    * @param data_id The ID of the data queue to query.
    * @param free_element_count Pointer to store the number of free elements available in the queue.
    *
    * @return result_t indicating the success or failure of the operation.
    */
   result_t (*get_free_element_count)(const dock_data_manager_interface_t *const interface,
                                      DATA_ID data_id,
                                      size_t *free_element_count);

   /**
    * @brief Get the element size in bytes for the specified data queue.
    *
    * @param data_id The ID of the data queue to query.
    * @param element_size Pointer to store the size of each element in bytes.
    *
    * @return result_t indicating the success or failure of the operation.
    */
   result_t (*get_element_size)(const dock_data_manager_interface_t *const interface,
                                DATA_ID data_id,
                                size_t *element_size);

   /**
    * @brief Get the number of elements currently stored in the specified data queue.
    *
    * @param data_id The ID of the data queue to query.
    * @param element_count Pointer to store the number of elements currently in the queue.
    * @return result_t indicating the success or failure of the operation.
    */
   result_t (*get_element_count)(const dock_data_manager_interface_t *const interface,
                                 DATA_ID data_id,
                                 size_t *element_count);

   /**
    * @brief Store the weight stack calibration record into NVM.
    */
   result_t (*set_weight_calibration_record)(const dock_data_manager_interface_t *const interface,
                                             weight_stack_calibration_record_t *calibration_record);
   /**
    * @brief Retrieve the weight stack calibration record from NVM.
    */
   result_t (*get_weight_calibration_record)(const dock_data_manager_interface_t *const interface,
                                             weight_stack_calibration_record_t *calibration_record);

   /**
    * @brief Retrieve the total weight, in milligrams, dispensed since new medication was added.
    */
   result_t (*get_total_weight_dispensed_mg)(const dock_data_manager_interface_t *const interface,
                                             uint32_t *total_weight_dispensed_mg);

   /**
    * @brief Store the total weight, in milligrams, dispensed since new medication was added.
    */
   result_t (*set_total_weight_dispensed_mg)(const dock_data_manager_interface_t *const interface,
                                             uint32_t total_weight_dispensed_mg);

   /**
    * @brief Retrieve the current dose schedule from NVM.
    */
   result_t (*get_dose_schedule_record)(const dock_data_manager_interface_t *const interface,
                                        dose_schedule_record_t *dose_schedule_record_out);
   /**
    * @brief Store the provided dose schedule into NVM.
    */
   result_t (*set_dose_schedule_record)(const dock_data_manager_interface_t *const interface,
                                        dose_schedule_record_t *dose_schedule_record_in);

   /**
    * @brief Retrieve the current dose schedule from NVM.
    */
   result_t (*get_ship_mode_data)(const dock_data_manager_interface_t *const interface, ship_mode_t *ship_mode_data);
   /**
    * @brief Store the provided dose schedule into NVM.
    */
   result_t (*set_ship_mode_data)(const dock_data_manager_interface_t *const interface, ship_mode_t *ship_mode_data);

   result_t (*get_med_nfc_uid)(const dock_data_manager_interface_t *const interface,
                               uint8_t *med_nfc_uid,
                               uint8_t uid_buffer_size);
   result_t (*set_med_nfc_uid)(const dock_data_manager_interface_t *const interface,
                               uint8_t *med_nfc_uid,
                               uint8_t uid_buffer_size);
};

#endif // DOCK_DATA_MANAGER_INTERFACE_H_
