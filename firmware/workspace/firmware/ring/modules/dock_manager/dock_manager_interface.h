/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dock_manager_interface.h
 * @ingroup dock_manager
 * @brief
 */

#ifndef DOCK_MANAGER_INTERFACE_H_
#define DOCK_MANAGER_INTERFACE_H_
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

// Error codes specific to the module.
typedef enum
{
   DOCK_MANAGER_ERROR_NONE = 0,
   DOCK_MANAGER_ERROR_NULL_PTR,
   DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR,
   DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX,
   DOCK_MANAGER_ERROR_UNKNOWN_PPI_FOR_RX,
   DOCK_MANAGER_ERROR_PROCESSING,
   DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN,
   DOCK_MANAGER_ERROR_REQUESTED_BYTES_EXCEED_MAX_PAYLOAD,
   DOCK_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE,
   DOCK_MANAGER_ERROR_INVALID_RX_PACKET_TYPE,
   DOCK_MANAGER_ERROR_MAX,
} DOCK_MANAGER_ERROR;

struct dock_manager;

typedef struct dock_manager_interface dock_manager_interface_t;

typedef struct dock_manager_interface
{
   struct dock_manager *parent; // Reference to the containing instance.

   /**
    * @brief Run the Dock Manager
    *
    * @details This function is called periodically to process the communications between the ring and the dock.
    *
    * Received packets will be processed and any packets that need to be sent will be transmitted.
    *
    * @param is_docked Tells the dock manager if the dock is present. If it is not present then the function will not
    * attempt transmission or reception.
    *
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*process)(const dock_manager_interface_t *const interface, bool is_docked);

} dock_manager_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // DOCK_MANAGER_INTERFACE_H_