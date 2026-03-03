/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef APP_MANAGER_H_
#define APP_MANAGER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "app_dock_ppi.h"
#include "app_manager_interface.h"
#include "common.h"
#include "comms_driver_interface.h"
#include "dock_data_manager_interface.h"
#include "message_protocol_interface.h"
#include "queue_interface.h"
#include "system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct transfer_info
{
   bool active;     /**< Indicates if a transfer is currently active. Used to enforce flow control */
   PPI_AD ppi;      /**< The PPI associated with the transfer. */
   PPI_TYPE type;   /**< The type of the transfer (request, response, push etc). */
   DATA_ID data_id; /**< The data ID associated with the transfer, if applicable. */
} ad_transfer_info_t;

typedef struct app_manager
{
   app_manager_interface_t interface;

   const message_protocol_interface_t *_message_protocol_ifc;
   const queue_interface_t *_app_manager_rx_queue_ifc;
   const queue_interface_t *_app_manager_tx_queue_ifc;
   const comms_driver_interface_t *_comms_ifc;
   const system_time_interface_t *_systick_ifc;
   const dock_data_manager_interface_t *_dock_data_manager_ifc;

   ad_transfer_info_t _current_tx_transaction; /**< Status of ongoing TX operation */

   bool _comms_link_active;
} app_manager_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t app_manager_init(app_manager_t *const self,
                          const message_protocol_interface_t *msg_prot_ifc,
                          const queue_interface_t *app_manager_rx_queue_ifc,
                          const queue_interface_t *app_manager_tx_queue_ifc,
                          const comms_driver_interface_t *comms_ifc,
                          const system_time_interface_t *systick_ifc,
                          const dock_data_manager_interface_t *dock_data_manager_ifc);
#endif // APP_MANAGER_H_