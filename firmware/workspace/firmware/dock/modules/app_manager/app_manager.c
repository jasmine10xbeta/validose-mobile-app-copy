
/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file app_manager.c
 * @ingroup app_manager_module
 * @brief Implementation of the app manager module.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <string.h>

// Custom includes
#include "app_manager.h"
#include "common.h"
#include "debug.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_APP_MANAGER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t process(const app_manager_interface_t *const interface);
static result_t set_comms_link_status(const app_manager_interface_t *const interface, bool is_active);

// Non-interface functions
static bool check_tx_completed(MSG_PROT_TX_PACKET_STATUS status);
static result_t handle_rx(const app_manager_interface_t *const interface);
static result_t handle_tx(const app_manager_interface_t *const interface);
static PPI_AD get_ppi_for_data_id(DATA_ID data_id);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static bool check_tx_completed(MSG_PROT_TX_PACKET_STATUS status)
{
   return (MSG_PROT_TX_PACKET_STATUS_COMPLETED == status) || //
          (MSG_PROT_TX_PACKET_STATUS_ABANDONED == status);
}

static PPI_AD get_ppi_for_data_id(DATA_ID data_id)
{
   PPI_AD ppi = PPI_AD_MAX;

   switch(data_id)
   {
      case DATA_ID_DOSE_EVENT:
         ppi = PPI_AD_DOSE_EVENT_REPORT;
         break;
      case DATA_ID_DOSE_DATAPOINT:
         ppi = PPI_AD_DOSE_DATA_POINT;
         break;
      case DATA_ID_DOCK_CHARGE_STATUS:
         ppi = PPI_AD_DOCK_CHARGE_STATUS;
         break;
      case DATA_ID_RING_DOCKED_STATUS:
         ppi = PPI_AD_RING_DOCKED_STATUS;
         break;
      case DATA_ID_BLUETOOTH_STATUS:
         ppi = PPI_AD_BL_STATUS;
         break;
      case DATA_ID_DOCK_BATTERY_LEVEL:
         ppi = PPI_AD_DOCK_BATT_LEVEL_LOG;
         break;
      case DATA_ID_RING_BATTERY_LEVEL:
         ppi = PPI_AD_RING_BATT_LEVEL_LOG;
         break;
      case DATA_ID_DOCK_DEBUG_LOG:
         ppi = PPI_AD_DOCK_DEBUG_LOG;
         break;
      case DATA_ID_RING_DEBUG_LOG:
         ppi = PPI_AD_RING_DEBUG_LOG;
         break;
      case DATA_ID_TEMPERATURE_LOG:
         ppi = PPI_AD_DOCK_TEMP_LOG;
         break;
      case DATA_ID_WEIGHT_MEASUREMENT:
         ppi = PPI_AD_DOCK_WEIGHT_LOG;
         break;
      case DATA_ID_RING_STATUS:
         ppi = PPI_AD_RING_STATUS;
         break;
      case DATA_ID_DOCK_STATUS:
         ppi = PPI_AD_DOCK_STATUS;
         break;
      case DATA_ID_MAX:
         break;
      default:
         // No valid mapping for this data ID
         break;
   }

   return ppi;
}

static result_t handle_tx(const app_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, APP_MANAGER_ERROR_NULL);

   app_manager_t *self = interface->parent;

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_MAX;
   result_t result = self->_message_protocol_ifc->get_tx_packet_status(self->_message_protocol_ifc, &tx_status);

   if(IS_OK(result) && self->_current_tx_transaction.active)
   {
      // Check if current transmission has been completed
      if(check_tx_completed(tx_status))
      {
         if(MSG_PROT_TX_PACKET_STATUS_COMPLETED == tx_status)
         {
            // Packet was sent successfully, we now need to delete the data from it's appropriate source.
            // We identify packets from the app_manager_tx_queue by their data_id being DATA_ID_MAX
            if(DATA_ID_MAX == self->_current_tx_transaction.data_id)
            {
               result = self->_app_manager_tx_queue_ifc->pop(self->_app_manager_tx_queue_ifc);
               ON_ERR_DEBUG_ERROR(
                  result, "[App Manager] Failed to Pop Data from App Manager TX Queue after successful transmission");
            }
            else if(DATA_ID_MAX > self->_current_tx_transaction.data_id)
            {
               result = self->_dock_data_manager_ifc->pop(
                  self->_dock_data_manager_ifc, self->_current_tx_transaction.data_id, 1u);
               ON_ERR_DEBUG_ERROR(result,
                                  "[App Manager] Failed to Pop Data from Dock Data Manager Queue after successful "
                                  "transmission, data ID: %u",
                                  self->_current_tx_transaction.data_id);
            }
         }

         // Current transaction is done.
         self->_current_tx_transaction.active = false;
      }
   }

   // Now we check for messages in the TX queue populated from GC
   if(IS_OK(result) && (false == self->_current_tx_transaction.active))
   {
      size_t tx_queue_count = 0u;
      result = self->_app_manager_tx_queue_ifc->get_count(self->_app_manager_tx_queue_ifc, &tx_queue_count);

      if(IS_OK(result) && (tx_queue_count > 0u) && check_tx_completed(tx_status))
      {
         mp_packet_payload_t tx_packet = {0};
         result = self->_app_manager_tx_queue_ifc->peek(self->_app_manager_tx_queue_ifc, &tx_packet);

         IF_OK_RUN_AND_UPDATE(result, self->_message_protocol_ifc->send(self->_message_protocol_ifc, &tx_packet));
         if(IS_OK(result))
         {
            self->_current_tx_transaction.active = true;
            self->_current_tx_transaction.ppi = tx_packet.ppi;
            self->_current_tx_transaction.type = tx_packet.type;
            self->_current_tx_transaction.data_id
               = DATA_ID_MAX; // Use DATA_ID_MAX to indicate this transaction is not associated with a specific data
                              // ID from Dock Data Manager
         }
      }
   }

   // Now we check for any pending data in the Dock Data Manager and push it
   if(IS_OK(result) && (false == self->_current_tx_transaction.active))
   {
      // Find the next ID with available data in the Dock Data Manager.
      // Lower ID's have higher priority. Dose event report being the highest priority at the moment.
      DATA_ID data_id = DATA_ID_MAX;
      for(uint8_t idx = 0u; idx < DATA_ID_MAX; idx++)
      {
         size_t count = 0u;
         result = self->_dock_data_manager_ifc->get_element_count(self->_dock_data_manager_ifc, idx, &count);
         if(IS_OK(result) && (count > 0u))
         {
            data_id = (DATA_ID)idx;
            break;
         }
      }

      if(IS_OK(result) && (DATA_ID_MAX != data_id) && check_tx_completed(tx_status))
      {
         mp_packet_payload_t tx_packet = {0};
         tx_packet.ppi = get_ppi_for_data_id(data_id);

         // This should not be possible, but if there is no valid PPI mapping for this data ID, we log an error and skip
         // pushing this data to avoid lockups from repeatedly trying to push data with an invalid PPI.
         if(PPI_AD_MAX == tx_packet.ppi)
         {
            DEBUG_ERROR("[App Manager] No valid PPI mapping for data ID: %u, cannot push data", data_id);
            result = self->_dock_data_manager_ifc->pop(self->_dock_data_manager_ifc, data_id, 1u);
         }
         else
         {
            // We still push the packet to avoid lockups
            tx_packet.type = PPI_TYPE_PUSH;

            // This element size switch is purely because the element size needs a size_t
            size_t element_size = 0u;
            result
               = self->_dock_data_manager_ifc->get_element_size(self->_dock_data_manager_ifc, data_id, &element_size);

            uint16_t max_payload_length = 0u;
            IF_OK_RUN_AND_UPDATE(
               result,
               self->_message_protocol_ifc->get_max_payload_length(self->_message_protocol_ifc, &max_payload_length));

            if(IS_OK(result) && (element_size > max_payload_length))
            {
               DEBUG_ERROR(
                  "[App Manager] Requested data from Dock Data Manager exceeds max payload size, data ID: %u, element "
                  "size: %u, max payload size: %u",
                  data_id,
                  (uint32_t)element_size,
                  (uint32_t)max_payload_length);

               result = self->_dock_data_manager_ifc->pop(self->_dock_data_manager_ifc, data_id, 1u);
               // Ignoring result of pop since if that fails there is not a lot we can do, we report that this started
               // with a payload too big.
               SET_ERR(result, APP_MANAGER_ERROR_PAYLOAD_TOO_BIG);
            }
            else if(IS_OK(result))
            {
               tx_packet.pkt_payload_len = (uint16_t)element_size;
            }

            IF_OK_RUN_AND_UPDATE(
               result,
               self->_dock_data_manager_ifc->peek(
                  self->_dock_data_manager_ifc, data_id, &tx_packet.payload, tx_packet.pkt_payload_len, 1u));

            ON_ERR_DEBUG_ERROR(
               result,
               "[App Manager] Failed to fetch data from Dock Data Manager for data push transmission, data ID: %u",
               data_id);

            IF_OK_RUN_AND_UPDATE(result, self->_message_protocol_ifc->send(self->_message_protocol_ifc, &tx_packet));

            if(IS_OK(result))
            {
               self->_current_tx_transaction.active = true;
               self->_current_tx_transaction.ppi = tx_packet.ppi;
               self->_current_tx_transaction.type = tx_packet.type;
               self->_current_tx_transaction.data_id = data_id;
            }
         }
      }
   }
   return result;
}

static result_t handle_rx(const app_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, APP_MANAGER_ERROR_NULL);

   app_manager_t *self = interface->parent;

   mp_packet_payload_t rx_payload = {0};
   result_t result = self->_message_protocol_ifc->get_rx_packet(self->_message_protocol_ifc, &rx_payload);

   IF_OK_RUN_AND_UPDATE(result, self->_app_manager_rx_queue_ifc->enqueue(self->_app_manager_rx_queue_ifc, &rx_payload));

   ON_ERR_DEBUG_ERROR(result, "[App Manager] Failed to enqueue received RQ packet to RX queue");

   return result;
}
/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t process(const app_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, APP_MANAGER_ERROR_NULL);

   app_manager_t *self = interface->parent;
   result_t result = RESULT_OK;

   // We check for and process RX packets even when _comms_link_active is false, because we want to make sure to
   // process any pending packets (e.g., ACKs for previously sent messages) even if the link is currently down.
   // However, we only check for new messages to send and handle received messages when _comms_link_active is
   // true, since if the link is down then sending new messages will fail.
   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   result_t rx_result = self->_message_protocol_ifc->get_rx_packet_status(self->_message_protocol_ifc, &rx_status);

   if(IS_OK(rx_result) && (MSG_PROT_RX_PACKET_STATUS_NEW == rx_status))
   {
      rx_result = handle_rx(interface);
   }
   ON_ERR_DEBUG_ERROR(rx_result, "[App Manager] Error Handling RX packet");

   result_t tx_result = RESULT_OK;

   if(self->_comms_link_active)
   {
      tx_result = handle_tx(interface);
   }
   ON_ERR_DEBUG_ERROR(tx_result, "[App Manager] Error Handling TX packet");

   // Return error if either RX or TX had an error
   UPDATE_ERR_IF_TRUE(result, (IS_ERR(rx_result) || IS_ERR(tx_result)), APP_MANAGER_ERROR_PROCESSING);

   return result;
}

static result_t set_comms_link_status(const app_manager_interface_t *const interface, bool is_active)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, APP_MANAGER_ERROR_NULL);

   app_manager_t *self = interface->parent;

   self->_comms_link_active = is_active;

   return RESULT_OK;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t app_manager_init(app_manager_t *const self,
                          const message_protocol_interface_t *msg_prot_ifc,
                          const queue_interface_t *app_manager_rx_queue_ifc,
                          const queue_interface_t *app_manager_tx_queue_ifc,
                          const comms_driver_interface_t *comms_ifc,
                          const system_time_interface_t *systick_ifc,
                          const dock_data_manager_interface_t *dock_data_manager_ifc)
{
   RETURN_ERR_IF_NULL(self, APP_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(msg_prot_ifc, APP_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(app_manager_rx_queue_ifc, APP_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(app_manager_tx_queue_ifc, APP_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(comms_ifc, APP_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(systick_ifc, APP_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(dock_data_manager_ifc, APP_MANAGER_ERROR_NULL);

   self->interface.parent = self;

   self->_message_protocol_ifc = msg_prot_ifc;
   self->_app_manager_rx_queue_ifc = app_manager_rx_queue_ifc;
   self->_app_manager_tx_queue_ifc = app_manager_tx_queue_ifc;
   self->_comms_ifc = comms_ifc;
   self->_systick_ifc = systick_ifc;
   self->_dock_data_manager_ifc = dock_data_manager_ifc;
   self->_comms_link_active = false;

   self->_current_tx_transaction.active = false;
   self->_current_tx_transaction.ppi = PPI_AD_MAX;
   self->_current_tx_transaction.type = PPI_TYPE_MAX;
   self->_current_tx_transaction.data_id = DATA_ID_MAX;

   self->interface.process = process;
   self->interface.set_comms_link_status = set_comms_link_status;

   return RESULT_OK;
}
