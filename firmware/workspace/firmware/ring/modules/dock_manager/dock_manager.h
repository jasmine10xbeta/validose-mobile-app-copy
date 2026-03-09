/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup dock_manager Dock Manager
 * @ingroup modules
 * @brief This module handles communication to and from the dock
 * @details
 *
 * @file dock_manager.h
 * @ingroup dock_manager
 * @brief
 */

#ifndef DOCK_MANAGER_H_
#define DOCK_MANAGER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
#include "dock_manager_interface.h"
#include "message_protocol.h"
#include "ring_data_manager_interface.h"
#include "ring_dock_ppi.h"
#include "ring_sources_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct transfer_info
{
   bool active;          /**< Indicates if a transfer is currently active. Used to enforce flow control */
   PPI_RD ppi;           /**< The PPI associated with the transfer. */
   PPI_TYPE type;        /**< The type of the transfer (request, response, push). */
   SOURCE_ID source_id;  /**< The source ID associated with the transfer, if applicable. */
   uint16_t total_bytes; /**< The total number of bytes to be transferred. */
} transfer_info_t;

typedef struct dock_manager
{
   dock_manager_interface_t interface;                     /**< Interface to the dock manager */
   const ring_sources_interface_t *_source_manager_ifc;    /**< Source manager interface */
   const message_protocol_interface_t *_msg_prot_ifc;      /**< Message protocol interface */
   const ring_data_manager_interface_t *_data_manager_ifc; /**< Ring data manager interface */
   transfer_info_t _busy;                                  /**< Status of any ongoing TX operations */
   transfer_info_t _requested;                             /**< Status indicating requested TX operations */
} dock_manager_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t dock_manager_init(dock_manager_t *const self,
                           const message_protocol_interface_t *msg_prot_ifc,
                           const ring_data_manager_interface_t *data_manager_ifc,
                           const ring_sources_interface_t *source_manager_ifc,
                           system_time_interface_t *systime);

#endif // DOCK_MANAGER_H_
