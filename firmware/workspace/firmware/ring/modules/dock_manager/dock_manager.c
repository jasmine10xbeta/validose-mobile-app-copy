/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dock_manager.c
 * @ingroup dock_manager
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "dock_manager.h"
#include "debug.h"
#include "result.h"
#include "ring_dock_ppi.h"
#include "string.h"
#include "system_time.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOCK_MANAGER;

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
static result_t process(const dock_manager_interface_t *interface, bool is_docked);

// Non-interface functions
static result_t dispatch_by_ppi(const dock_manager_interface_t *interface, mp_packet_payload_t *rx_packet);
static result_t handle_rx(const dock_manager_interface_t *interface);
static result_t handle_tx(const dock_manager_interface_t *interface);

static bool get_is_tx_not_in_progress(MSG_PROT_TX_PACKET_STATUS status);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static bool get_is_tx_not_in_progress(MSG_PROT_TX_PACKET_STATUS status)
{
   return (MSG_PROT_TX_PACKET_STATUS_COMPLETED == status) || //
          (MSG_PROT_TX_PACKET_STATUS_ERROR == status) ||     //
          (MSG_PROT_TX_PACKET_STATUS_ABANDONED == status);
}

static result_t dispatch_push_ppi(const dock_manager_interface_t *interface, mp_packet_payload_t *rx_packet)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(rx_packet, DOCK_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_TRUE(PPI_TYPE_PUSH != rx_packet->type, DOCK_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE);

   result_t result = RESULT_OK;

   dock_manager_t *self = interface->parent;

   uint64_t time = 0;
   uint16_t battery_sample_hz = 0;
   cap_detection_cfg_t cap_detection_cfg = {0};

   // Dispatch based on PPI
   switch(rx_packet->ppi)
   {
      case PPI_RD_TIME:

         if(rx_packet->pkt_payload_len != sizeof(uint32_t))
         {
            DEBUG_ERROR("Invalid TIME SET length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }

         memcpy(&time, rx_packet->payload, rx_packet->pkt_payload_len);
         result = self->_data_manager_ifc->update_time(self->_data_manager_ifc, (uint64_t)time * COMMON_1K_CST);
         break;

      case PPI_RD_BATTERY_SAMPLE_RATE:

         if(rx_packet->pkt_payload_len != sizeof(uint16_t))
         {
            DEBUG_ERROR("Invalid BATTERY SAMPLE RATE SET length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }

         memcpy(&battery_sample_hz, rx_packet->payload, rx_packet->pkt_payload_len);
         result = self->_data_manager_ifc->update_battery_sample_rate(self->_data_manager_ifc, battery_sample_hz);
         break;

      case PPI_RD_CAP_DETECTION_CONFIG:

         if(rx_packet->pkt_payload_len != sizeof(cap_detection_cfg_t))
         {
            DEBUG_ERROR("Invalid CAP DETECTION CONFIG SET length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }
         memcpy(&cap_detection_cfg, rx_packet->payload, rx_packet->pkt_payload_len);
         result = self->_data_manager_ifc->set_cap_detection_config(self->_data_manager_ifc, cap_detection_cfg);

         break;

      case PPI_RD_BLE_ADVERTISE:
         // Do nothing for now.
         break;

      case PPI_RD_STATUS:
      case PPI_RD_DOSE_DATA:
      case PPI_RD_ERROR_DATA:
      case PPI_RD_BATTERY_DATA:
         DEBUG_ERROR("Received innappropriate PPI for push packet type");
         SET_ERR(result, DOCK_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE);
         break;

      default:
         DEBUG_ERROR("Invalid PPI received in dock manager");
         SET_ERR(result, DOCK_MANAGER_ERROR_UNKNOWN_PPI_FOR_RX);
         break;
   }
   return result;
}

static result_t dispatch_req_ppi(const dock_manager_interface_t *interface, mp_packet_payload_t *rx_packet)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(rx_packet, DOCK_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_TRUE(PPI_TYPE_RQ != rx_packet->type, DOCK_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE);

   result_t result = RESULT_OK;

   dock_manager_t *self = interface->parent;

   // Dispatch based on PPI
   switch(rx_packet->ppi)
   {
      case PPI_RD_STATUS:
         self->_requested.ppi = PPI_RD_STATUS;
         self->_requested.type = PPI_TYPE_RE;
         self->_requested.source_id = SOURCE_ID_STATUS;
         self->_requested.total_bytes = sizeof(ring_status_t);
         self->_requested.active = true;

         break;

      case PPI_RD_DOSE_DATA:

         if(rx_packet->pkt_payload_len != sizeof(uint16_t))
         {
            DEBUG_ERROR("Invalid DOSE DATA REQ length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }
         self->_requested.ppi = PPI_RD_DOSE_DATA;
         self->_requested.type = PPI_TYPE_RE;
         self->_requested.source_id = SOURCE_ID_DOSE;
         self->_requested.total_bytes = sizeof(dose_data_t);
         self->_requested.active = true;

         break;

      case PPI_RD_ERROR_DATA:

         if(rx_packet->pkt_payload_len != sizeof(uint16_t))
         {
            DEBUG_ERROR("Invalid ERROR DATA REQ length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }
         self->_requested.ppi = PPI_RD_ERROR_DATA;
         self->_requested.type = PPI_TYPE_RE;
         self->_requested.source_id = SOURCE_ID_ERROR;
         self->_requested.total_bytes = sizeof(raw_debug_log_t);
         self->_requested.active = true;

         break;

      case PPI_RD_BATTERY_DATA:

         if(rx_packet->pkt_payload_len != sizeof(uint16_t))
         {
            DEBUG_ERROR("Invalid BATTERY DATA REQ length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }
         self->_requested.ppi = PPI_RD_BATTERY_DATA;
         self->_requested.type = PPI_TYPE_RE;
         self->_requested.source_id = SOURCE_ID_BATTERY;
         self->_requested.total_bytes = sizeof(ring_battery_data_t);
         self->_requested.active = true;

         break;

      case PPI_RD_CAP_DETECTION_CONFIG:

         if(rx_packet->pkt_payload_len != 0)
         {
            DEBUG_ERROR("Invalid CAP DETECTION CONFIG REQ length");
            SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_LEN);
            break;
         }

         self->_requested.ppi = PPI_RD_CAP_DETECTION_CONFIG;
         self->_requested.type = PPI_TYPE_RE;
         self->_requested.source_id = SOURCE_ID_CAP_DETECTION_CFG;
         self->_requested.total_bytes = sizeof(cap_detection_status_t);
         self->_requested.active = true;

         break;

      case PPI_RD_TIME:
      case PPI_RD_BATTERY_SAMPLE_RATE:
      case PPI_RD_BLE_ADVERTISE:
         DEBUG_ERROR("Received innappropriate PPI for req packet type");
         SET_ERR(result, DOCK_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE);
         break;

      default:
         DEBUG_ERROR("Invalid PPI received in dock manager");
         SET_ERR(result, DOCK_MANAGER_ERROR_UNKNOWN_PPI_FOR_RX);
         break;
   }

   // Check that requested bytes do not exceed max payload length
   if((self->_requested.total_bytes > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN) && IS_OK(result))
   {
      self->_requested.active = false;
      DEBUG_ERROR("Requested bytes exceed max payload length");
      SET_ERR(result, DOCK_MANAGER_ERROR_REQUESTED_BYTES_EXCEED_MAX_PAYLOAD);
   }

   return result;
}

static result_t dispatch_by_ppi(const dock_manager_interface_t *interface, mp_packet_payload_t *rx_packet)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(rx_packet, DOCK_MANAGER_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   switch(rx_packet->type)
   {
      case PPI_TYPE_PUSH:
         result = dispatch_push_ppi(interface, rx_packet);
         break;

      case PPI_TYPE_RQ:
         result = dispatch_req_ppi(interface, rx_packet);
         break;

      case PPI_TYPE_RE:
         DEBUG_ERROR("Received unexpected response packet");
         SET_ERR(result, DOCK_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE);
         break;

      default:
         DEBUG_ERROR("Invalid packet type received in dock manager");
         SET_ERR(result, DOCK_MANAGER_ERROR_INVALID_RX_PACKET_TYPE);
         break;
   }
   return result;
}

static result_t handle_rx(const dock_manager_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR);

   dock_manager_t *self = interface->parent;
   mp_packet_payload_t rx_packet = {0};

   result_t result = self->_msg_prot_ifc->get_rx_packet(self->_msg_prot_ifc, &rx_packet);

   IF_OK_RUN_AND_UPDATE(result, dispatch_by_ppi(interface, &rx_packet));

   return result;
}

static result_t handle_tx(const dock_manager_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR);

   result_t result = RESULT_OK;
   dock_manager_t *self = interface->parent;
   MSG_PROT_TX_PACKET_STATUS status = MSG_PROT_TX_PACKET_STATUS_MAX;

   // ******************************************
   // Handle any busy transmissions
   // ******************************************
   // Check status of busy transmission
   result = self->_msg_prot_ifc->get_tx_packet_status(self->_msg_prot_ifc, &status);

   if(self->_busy.active && IS_OK(result))
   {
      // Check for TX status indicating transmission is complete one way or another
      if(get_is_tx_not_in_progress(status))
      {
         // If transmission was successful, clear bytes from source
         // If not successful, do not clear it they can be retried later
         // @todo: Retry logic already implemented in message protocol - consider clearing regardless?
         if(MSG_PROT_TX_PACKET_STATUS_COMPLETED == status)
         {
            // Successful transmission, clear data
            result = self->_source_manager_ifc->clear_bytes(
               self->_source_manager_ifc, self->_busy.source_id, self->_busy.total_bytes);
         }

         // Mark busy as inactive regardless of success or failure
         self->_busy.active = false;
      }
   }

   // ******************************************
   // Handle any new transmissions if not busy
   // ******************************************
   if(!self->_busy.active && self->_requested.active && IS_OK(result))
   {
      // Check for TX status indicating transmission is complete one way or another
      if(get_is_tx_not_in_progress(status))
      {
         // Prepare to transmit from requested source
         mp_packet_payload_t tx_packet = {0};
         tx_packet.ppi = self->_requested.ppi;
         tx_packet.type = self->_requested.type;

         result = self->_source_manager_ifc->copy_bytes(self->_source_manager_ifc,
                                                        self->_requested.source_id,
                                                        self->_requested.total_bytes,
                                                        tx_packet.payload,
                                                        &tx_packet.pkt_payload_len);
         IF_OK_RUN_AND_UPDATE(result,
                              interface->parent->_msg_prot_ifc->send(interface->parent->_msg_prot_ifc, &tx_packet));
         if(IS_OK(result))
         {
            // Mark requested as busy
            self->_busy.active = true;
            self->_busy.ppi = self->_requested.ppi;
            self->_busy.type = self->_requested.type;
            self->_busy.source_id = self->_requested.source_id;
            self->_busy.total_bytes = tx_packet.pkt_payload_len; // Bytes actually sent

            // Clear requested data
            self->_requested.active = false;
         }
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t process(const dock_manager_interface_t *interface, bool is_docked)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOCK_MANAGER_ERROR_NULL_INTERFACE_PTR);

   result_t result = RESULT_OK;
   dock_manager_t *self = interface->parent;

   // ********************************
   // Handle RX
   // ********************************
   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   result_t rx_result = self->_msg_prot_ifc->get_rx_packet_status(self->_msg_prot_ifc, &rx_status);

   if(IS_OK(rx_result) && (MSG_PROT_RX_PACKET_STATUS_NEW == rx_status))
   {
      rx_result = handle_rx(interface);
   }
   ON_ERR_DEBUG_ERROR(rx_result, "[Dock Manager] Error handling RX");

   // ********************************
   // Handle TX
   // ********************************
   result_t tx_result = RESULT_OK;

   if(is_docked)
   {
      tx_result = handle_tx(interface);
   }
   ON_ERR_DEBUG_ERROR(tx_result, "[Dock Manager] Error handling TX");

   // Return error if either RX or TX had an error
   UPDATE_ERR_IF_TRUE(result, (IS_ERR(rx_result) || IS_ERR(tx_result)), DOCK_MANAGER_ERROR_PROCESSING);

   return result;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t dock_manager_init(dock_manager_t *const self,
                           const message_protocol_interface_t *msg_prot_ifc,
                           const ring_data_manager_interface_t *data_manager_ifc,
                           const ring_sources_interface_t *source_manager_ifc)
{
   RETURN_ERR_IF_NULL(self, DOCK_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(msg_prot_ifc, DOCK_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(data_manager_ifc, DOCK_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(source_manager_ifc, DOCK_MANAGER_ERROR_NULL_PTR);

   // Assign parents
   self->interface.parent = self;

   // Assign interfaces
   self->_msg_prot_ifc = msg_prot_ifc;
   self->_data_manager_ifc = data_manager_ifc;
   self->_source_manager_ifc = source_manager_ifc;

   // Map function pointers
   self->interface.process = process;

   // Set defaults for busy tx source tracking
   self->_busy.active = false;
   self->_busy.ppi = PPI_RD_BATTERY_DATA;
   self->_busy.source_id = SOURCE_ID_BATTERY;
   self->_busy.total_bytes = 0u;

   // Set defaults for requested tx status
   self->_requested.active = false;
   self->_requested.ppi = PPI_RD_BATTERY_DATA;
   self->_requested.source_id = SOURCE_ID_BATTERY;
   self->_requested.total_bytes = 0u;

   // No initialization flag necessary as the only failures that can occur are before the interface parent is
   // assigned. A NULL parent pointer indicates uninitialized and will be caught by by standard interface checks in
   // guard clauses.

   return RESULT_OK;
}