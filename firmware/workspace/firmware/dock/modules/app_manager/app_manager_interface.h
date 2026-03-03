/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file app_manager_interface.h
 * @ingroup app_manager_module
 * @brief Interface definition for the app manager module.
 */

#ifndef APP_MANAGER_INTERFACE_H_
#define APP_MANAGER_INTERFACE_H_
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
   APP_MANAGER_ERROR_NONE = 0,                 /**<No Error */
   APP_MANAGER_ERROR_NULL,                     /**< Generic Null Pointer Error */
   APP_MANAGER_ERROR_PROCESSING,               /**< Error occurred during processing */
   APP_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE, /**< Received packet is of incorrect type */
   APP_MANAGER_ERROR_INVALID_RX_PACKET_TYPE,   /**< Received packet has invalid type */
   APP_MANAGER_ERROR_PAYLOAD_TOO_BIG,          /**< Payload size exceeds maximum allowed size for message protocol */
   APP_MANAGER_ERROR_MAX,
} APP_MANAGER_ERROR;

struct app_manager;                                           // Forward declaration
typedef struct app_manager_interface app_manager_interface_t; // Forward declaration

/**
 * @struct app_manager_interface
 * @brief Structure defining the app manager interface for data and event management operations.
 *
 * This structure contains function pointers for data management operations and a reference to the parent app
 * manager instance.
 *
 */

struct app_manager_interface
{
   struct app_manager *parent; // Reference to the parent app manager instance

   /**
    * @brief Processes function for the app manager module.
    *
    * This function must be called periodically.
    *
    * From a data transmission perspective, this function checks the comms link status and then sends out pending
    * messages in the TX queue, as well as checking whether any data is available to send in the Dock Data Manager.
    *
    * From a data reception perspective, this function receives messages and then responds appropriately depending on
    * the actual message. Currently this is either fetching the requested data from the Dock Data Manager and sending
    * it, or passing along the message via the RX queue.
    *
    * @param interface Pointer to the app_manager_interface_t instance.
    *
    * @return Result of the operation - See APP_MANAGER_ERROR enum for possible error codes.
    */
   result_t (*process)(const app_manager_interface_t *const interface);

   /**
    * @brief Sets the communication link status.
    *
    * This function is used to update the communication link status in the app manager. The app manager can use this
    * information to determine whether to attempt sending messages or not, as well as for other logic that may depend
    * on whether the comms link is active or not.
    *
    * @param interface Pointer to the app_manager_interface_t instance.
    * @param is_active Boolean indicating whether the communication link is active (true) or not (false).
    *
    * @return Result of the operation - See APP_MANAGER_ERROR enum for possible error codes.
    */
   result_t (*set_comms_link_status)(const app_manager_interface_t *const interface, bool is_active);
};

#endif // APP_MANAGER_INTERFACE_H_