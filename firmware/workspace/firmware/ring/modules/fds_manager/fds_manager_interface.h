/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file
 * @ingroup
 * @brief
 */

#ifndef FDS_MANAGER_INTERFACE_H_
#define FDS_MANAGER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
typedef enum
{
   FDS_MANAGER_ERROR_NULL_PTR,
   FDS_MANAGER_ERROR_NOT_INITIALIZED,
   FDS_MANAGER_ERROR_INVALID_RECORD_ID,
   FDS_MANAGER_ERROR_INVALID_VALUE_PARAMETERS,
   FDS_MANAGER_ERROR_STORAGE_FAILURE,
} FDS_MANAGER_ERROR;

typedef enum
{
   RECORD_ID_SHIP_MODE_EXIT_TIME,
   RECORD_ID_BATTERY_SAMPLE_FREQUENCY,
   RECORD_ID_CAP_DETECTION_THRESHOLD,
   RECORD_ID_CAP_DETECTION_HYSTERESIS,
   RECORD_ID_MAX,
} RECORD_ID;

typedef enum
{
   RECORD_KEY_SHIP_MODE_EXIT_TIME = 0x0001u,
   RECORD_KEY_BATTERY_SAMPLE_FREQUENCY = 0x0002u,
   RECORD_KEY_CAP_DETECTION_THRESHOLD = 0x0003u,
   RECORD_KEY_CAP_DETECTION_HYSTERESIS = 0x0004u,
} RECORD_KEY;

typedef struct __attribute__((packed, aligned(4)))
{
   uint8_t data[8]; // Max size of any record is 8 bytes
   RECORD_KEY key;
} record_info_t;

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

// Forward declaration of the fds manager struct
typedef struct fds_manager fds_manager_t;
typedef struct fds_manager_interface fds_manager_interface_t;

typedef struct fds_manager_interface
{
   struct fds_manager *parent; // Reference to the containing instance.

   /**
    * @brief Store a uint64_t in NVM
    *
    * @param ifc A pointer to the interface instance.
    * @param record_id The ID of the record to store.
    * @param data Value to be stored
    * @return result_t A status code indicating success or failure.
    */
   result_t (*store_uint64_t)(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t data);

   /**
    * @brief Store a uint32_t in NVM
    *
    * @param ifc A pointer to the interface instance.
    * @param record_id The ID of the record to store.
    * @param data Value to be stored
    * @return result_t A status code indicating success or failure.
    */
   result_t (*store_uint32_t)(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint32_t data);

   /**
    * @brief Store a uint16_t in NVM
    *
    * @param ifc A pointer to the interface instance.
    * @param record_id The ID of the record to store.
    * @param data Value to be stored
    * @return result_t A status code indicating success or failure.
    */
   result_t (*store_uint16_t)(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint16_t data);

   /**
    * @brief Retrieve a uint64_t from NVM
    *
    * @param ifc A pointer to the interface instance.
    * @param record_id The ID of the record to retrieve.
    * @param data Pointer to the memory allocated to receive the record data.
    *
    * @return result_t A status code indicating success or failure.
    */
   result_t (*retrieve_uint64_t)(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t *data);

   /**
    * @brief Retrieve a uint32_t from NVM
    *
    * @param ifc A pointer to the interface instance.
    * @param record_id The ID of the record to retrieve.
    * @param data Pointer to the memory allocated to receive the record data.
    *
    * @return result_t A status code indicating success or failure.
    */
   result_t (*retrieve_uint32_t)(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint32_t *data);

   /**
    * @brief Retrieve a uint16_t from NVM
    *
    * @param ifc A pointer to the interface instance.
    * @param record_id The ID of the record to retrieve.
    * @param data Pointer to the memory allocated to receive the record data.
    *
    * @return result_t A status code indicating success or failure.
    */
   result_t (*retrieve_uint16_t)(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint16_t *data);

} fds_manager_interface_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static const record_info_t RECORD_INFOS[] = {
   {{0}, RECORD_KEY_SHIP_MODE_EXIT_TIME},
   {{0}, RECORD_KEY_BATTERY_SAMPLE_FREQUENCY},
   {{0}, RECORD_KEY_CAP_DETECTION_THRESHOLD},
   {{0}, RECORD_KEY_CAP_DETECTION_HYSTERESIS},
};
/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // FDS_MANAGER_INTERFACE_H_