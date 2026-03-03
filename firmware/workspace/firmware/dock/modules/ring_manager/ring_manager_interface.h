/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_manager_interface.h
 * @ingroup ring_manager
 * @brief
 */

#ifndef RING_MANAGER_INTERFACE_H_
#define RING_MANAGER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct status_update
{
   ring_status_t ring_status;   /**< Current status of the ring as reported in the latest status update. */
   uint32_t update_time_unix_s; /**< Unix timestamp in seconds when the status update was received. */
} status_update_t;

struct ring_manager;
typedef struct ring_manager_interface ring_manager_interface_t;

typedef struct ring_manager_interface
{
   struct ring_manager *parent; // Reference to the containing instance.

   /**
    * @brief Update the battery sample frequency.
    *
    * @param ifc Pointer to the ring manager interface.
    * @param battery_sample_freq The new battery sample frequency in milliHz.
    * @return result_t Result of the operation.
    */
   result_t (*update_battery_sample_freq)(const ring_manager_interface_t *const ifc, uint16_t battery_sample_freq);

   /**
    * @brief Retrieve the current status of the ring.
    *
    * @param ifc Pointer to the ring manager interface.
    * @param ring_status Pointer to store the retrieved ring status.
    * @return result_t Result of the operation.
    */

   result_t (*get_ring_status)(const ring_manager_interface_t *const ifc, status_update_t *ring_status);

   /**
    * @brief Process ring manager tasks
    *
    * @param ifc Pointer to the ring manager interface.
    * @param is_docked Boolean indicating whether the ring is currently docked or not. Communication with the ring is
    * only attempted when docked. This value is updated externally to allow for unit testing of the module as a whole
    * without hardware dependencies.
    *
    * Process will automatically trigger a status update request at a defined interval and will check for pending data
    * requests to ensure that they are only sent when there is capacity in the corresponding data store queue. The
    * process function should be called frequently (e.g., every 100ms) to ensure timely processing of commands and
    * reception of data from the ring.
    *
    * The process function also checks for time drift between the ring and dock. If the drift exceeds a defined
    * threshold, a time update command is marked as pending to trigger a time update on the ring.
    * This drift is checked each time a status update is received from the ring
    *
    * @return result_t Result of the operation.
    */
   result_t (*process)(const ring_manager_interface_t *const ifc, bool is_docked);

   result_t (*update_proximity_thresholds)(const ring_manager_interface_t *const ifc, uint16_t on, uint16_t off);

} ring_manager_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // RING_MANAGER_INTERFACE_H_