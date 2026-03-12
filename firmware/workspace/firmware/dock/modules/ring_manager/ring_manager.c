/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_manager.c
 * @ingroup ring_manager
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "ring_manager.h"
#include "debug.h"
#include <math.h>
#include <string.h>

#include <stdio.h>

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_RING_MANAGER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define STATUS_POLL_INTERVAL_MS            (1000u)
#define STATUS_STORAGE_RATE_LIMIT_S        (10u)
#define PERMISSIBLE_RING_DRIFT_S           (2u)
#define MINIMUM_PERMISSIBLE_SAMPLE_TIME_MS (500u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t update_battery_sample_freq(const ring_manager_interface_t *const ifc, uint16_t battery_sample_freq);
static result_t get_ring_status(const ring_manager_interface_t *const ifc, status_update_t *ring_status);
static result_t process(const ring_manager_interface_t *const ifc, bool is_docked);
static result_t get_cap_detection_status(const ring_manager_interface_t *const ifc, cap_detection_status_t *status);
static result_t send_cap_detection_config_update(const ring_manager_interface_t *const ifc, cap_detection_cfg_t config);
static result_t set_cap_detection_poll_period_ms(const ring_manager_interface_t *const ifc, uint16_t poll_period_ms);

// Non-interface functions
/**
 * @brief Checks all data store queues for available space
 *
 * If there is no space available for a given data store, the associated data request command is marked as not pending.
 *
 * If there is space available then the data field of the corresponding data request command is updated with the number
 * of requested bytes
 *
 * @param ifc Pointer to the ring manager interface.
 * @return result_t Result of the operation.
 */
static result_t check_capacity_and_update_data_request_commands(const ring_manager_interface_t *const ifc);

/**
 * @brief Convenience function to check whether a transmission has completed
 *
 * @return Boolean indicating whether the transmission has completed
 */
static bool check_tx_completed(MSG_PROT_TX_PACKET_STATUS status);

/**
 * @brief Dispatch received packet based on PPI
 *
 * @param ifc Pointer to the ring manager interface.
 * @param rx_packet Pointer to the received packet.
 *
 * @return result_t Result of the operation.
 */
static result_t dispatch_by_ppi(const ring_manager_interface_t *ifc, mp_packet_payload_t *rx_packet);

/**
 * @brief Handle transmission of outgoing packets
 *
 * @param ifc Pointer to the ring manager interface.
 *
 * @return result_t Result of the operation.
 */
static result_t handle_tx(const ring_manager_interface_t *const ifc);

/**
 * @brief Handle reception of incoming packets
 *
 * @param ifc Pointer to the ring manager interface.
 *
 * @return result_t Result of the operation.
 */
static result_t handle_rx(const ring_manager_interface_t *const ifc);

/**
 * @brief Check for changes in ring status and store a new status update if there are significant changes or if the
 * storage rate limit has been exceeded.
 *
 * @param ifc Pointer to the ring manager interface.
 * @param status Pointer to the new ring status to be evaluated for storage.
 * @param unix_time The unix time in seconds associated with the new status update.
 *
 * @return result_t Result of the operation.
 */
result_t update_pending_commands_based_on_status(const ring_manager_interface_t *ifc,
                                                 const ring_status_t *status,
                                                 uint32_t unix_time);

/**
 * @brief Check for significant changes in ring status or if the storage rate limit has been exceeded to determine
 * whether to store a new status update.
 *
 * @param ifc Pointer to the ring manager interface.
 * @param new_status Pointer to the new ring status to be evaluated for storage.
 * @param systick_ms The current systick time in milliseconds to evaluate the storage rate
 *
 * @return result_t Result of the operation.
 */
static result_t store_status_on_delta_and_timeout(const ring_manager_interface_t *ifc,
                                                  const ring_status_t *new_status,
                                                  uint64_t systick_ms);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static bool check_tx_completed(MSG_PROT_TX_PACKET_STATUS status)
{
   return (MSG_PROT_TX_PACKET_STATUS_COMPLETED == status) || //
          (MSG_PROT_TX_PACKET_STATUS_ERROR == status) ||     //
          (MSG_PROT_TX_PACKET_STATUS_ABANDONED == status);
}

static result_t check_capacity_and_update_data_request_commands(const ring_manager_interface_t *const ifc)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   result_t result = RESULT_OK;

   dock_data_manager_interface_t const *data_manager_ifc = ifc->parent->_dock_data_manager_ifc;

   // First update the data store free byte counts
   for(uint8_t idx = 0u; idx < RING_MANAGER_DATA_ID_MAX && IS_OK(result); idx++)
   {
      data_store_t *store = &ifc->parent->_data_store[idx];
      size_t free_element_count = 0u;

      result = data_manager_ifc->get_free_element_count(data_manager_ifc, store->id, &free_element_count);
      store->free_bytes = (uint16_t)free_element_count * store->element_size;
   }

   // Now nullify any pending data requests that cannot be fulfilled due to lack of space and
   // Update the payload with the number of requested bytes
   for(uint8_t idx = 0u; idx < RING_MANAGER_DATA_ID_MAX && IS_OK(result); idx++)
   {
      // Get the the data store associated with the DATA ID
      data_store_t *store = &ifc->parent->_data_store[idx];

      // Get the pending command associated with the data store
      pending_command_t *command = &ifc->parent->_pending_commands[store->associated_command_id];

      // If there is no space available then nullify the pending flag for the command
      command->pending = command->pending && (store->free_bytes > 0u);

      // If there is space available then update the command data with the number of bytes that can be sent
      // Trimming it to the maximum allowed per request
      if(command->pending)
      {
         store->free_bytes = store->free_bytes > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN ? MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN :
                                                                                    store->free_bytes;

         memcpy(&command->data, &store->free_bytes, sizeof(command->data_length));
      }
   }

   return result;
}

static result_t store_status_on_delta_and_timeout(const ring_manager_interface_t *ifc,
                                                  const ring_status_t *new_status,
                                                  uint64_t systick_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(new_status, RING_MANAGER_ERROR_NULL_PTR);
   RETURN_OK_IF_TRUE(ifc->parent->_last_status_store_systick_ms + (STATUS_STORAGE_RATE_LIMIT_S * 1000) > systick_ms);

   result_t result = RESULT_OK;

   // Update the last status update systick
   ifc->parent->_last_status_store_systick_ms = systick_ms;

   // Convenience pointer to keep the statements a reasonable length
   const ring_status_t *old_status = &ifc->parent->_stored_status;
   bool update_needed = false;

   // Prompt update if there is any change in firmware or hardware version or the MAC - This indicates that the ring has
   // been updated or swapped and we should prompt an update to get the new status information
   if((0 != memcmp(old_status->mac, new_status->mac, sizeof(old_status->mac))) ||                                //
      (0 != memcmp(&old_status->firmware_version, &new_status->firmware_version, sizeof(semantic_version_t))) || //
      (0 != memcmp(&old_status->hardware_version, &new_status->hardware_version, sizeof(semantic_version_t))))
   {
      update_needed = true;
   }

   // Prompt update if the ring ship mode exit time changes
   if(old_status->ship_mode_exit_timestamp_unix != new_status->ship_mode_exit_timestamp_unix)
   {
      update_needed = true;
   }

   // Prompt update if the sampling rate has changed
   if(old_status->battery_sample_frequency_millihz != new_status->battery_sample_frequency_millihz)
   {
      update_needed = true;
   }

   // Update if the watermark for any of the FIFOs has changed by more than a 10 percent step
   if((old_status->dose_fifo_used_percent_watermark / 10u != new_status->dose_fifo_used_percent_watermark / 10u) || //
      (old_status->battery_fifo_used_percent_watermark / 10u != new_status->battery_fifo_used_percent_watermark / 10u)
      ||                                                                                                          //
      (old_status->imu_fifo_used_percent_watermark / 10u != new_status->imu_fifo_used_percent_watermark / 10u) || //
      (old_status->error_fifo_used_percent_watermark / 10u != new_status->error_fifo_used_percent_watermark / 10u))
   {
      update_needed = true;
   }

   // Update if the fill level of any of the fifos has changed by more than a 20 percent step
   if((old_status->dose_fifo_used_percent / 20u) != (new_status->dose_fifo_used_percent / 20u) ||       //
      (old_status->battery_fifo_used_percent / 20u) != (new_status->battery_fifo_used_percent / 20u) || //
      (old_status->imu_fifo_used_percent / 20u) != (new_status->imu_fifo_used_percent / 20u) ||         //
      (old_status->error_fifo_used_percent / 20u) != (new_status->error_fifo_used_percent / 20u))
   {
      update_needed = true;
   }

   // Update if the temperature has changed by more than 5 degrees Celsius
   if(old_status->temperature_celsius != new_status->temperature_celsius)
   {
      update_needed = true;
   }

   if(update_needed)
   {
      // Store the new status
      ifc->parent->_stored_status = *new_status;
      ring_status_t status_to_enqueue = *new_status;
      result = ifc->parent->_dock_data_manager_ifc->enqueue(
         ifc->parent->_dock_data_manager_ifc, DATA_ID_RING_STATUS, &status_to_enqueue, sizeof(ring_status_t), 1u);
   }

   return result;
}

result_t update_pending_commands_based_on_status(const ring_manager_interface_t *ifc,
                                                 const ring_status_t *status,
                                                 uint32_t unix_time)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(status, RING_MANAGER_ERROR_NULL_PTR);

   pending_command_t *pending_commands = ifc->parent->_pending_commands;

   // Data request commands
   pending_commands[CMD_ID_REQ_DOSE_EVENT_DATA].pending = (status->dose_fifo_used_percent > 0u);
   pending_commands[CMD_ID_REQ_RING_DEBUG_LOG_DATA].pending = (status->error_fifo_used_percent > 0u);
   pending_commands[CMD_ID_REQ_RING_BATTERY_LEVEL_DATA].pending = (status->battery_fifo_used_percent > 0u);

   // Time sync command - If the ring is docked check for time drift and update time if necessary
   uint32_t ring_time = status->timestamp_unix_s;
   uint32_t drift_s = 0u;

   drift_s = unix_time > ring_time ? (unix_time - ring_time) : (ring_time - unix_time);
   pending_commands[CMD_ID_UPDATE_TIME].pending = (drift_s > PERMISSIBLE_RING_DRIFT_S);

   return RESULT_OK;
}

static result_t dispatch_by_ppi(const ring_manager_interface_t *ifc, mp_packet_payload_t *rx_packet)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(rx_packet, RING_MANAGER_ERROR_NULL_PTR);
   // Should only be receiving packets of type response to our requests
   RETURN_ERR_IF_TRUE(PPI_TYPE_RE != rx_packet->type, RING_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE);
   RETURN_ERR_IF_TRUE(PPI_TYPE_MAX <= rx_packet->type, RING_MANAGER_ERROR_INVALID_RX_PACKET_TYPE);

   result_t result = RESULT_OK;

   // Get reference to commonly used variables
   ring_manager_t *self = ifc->parent;
   data_store_t *data_store = ifc->parent->_data_store;
   dock_data_manager_interface_t const *data_manager_ifc = ifc->parent->_dock_data_manager_ifc;

   // Convenience variables
   uint16_t payload_len = rx_packet->pkt_payload_len;
   uint16_t element_size = 0u;
   uint16_t element_count = 0u;
   DATA_ID data_id = 0u;
   ring_status_t status = {0};

   // Get the current unix time
   uint32_t unix_time = 0u;
   IF_OK_RUN_AND_UPDATE(result, self->_rtc_ifc->get_time_unix(self->_rtc_ifc, &unix_time));

   // Get the current systick time in ms
   uint64_t systick_ms = 0u;
   IF_OK_RUN_AND_UPDATE(result, self->_systick_ifc->get_time_ms(self->_systick_ifc, &systick_ms));

   // Dispatch based on PPI
   if(IS_OK(result))
   {
      switch(rx_packet->ppi)
      {
         case PPI_RD_DOSE_DATA:
            element_size = data_store[RING_MANAGER_DATA_ID_DOSE_EVENT].element_size;
            element_count = payload_len / data_store[RING_MANAGER_DATA_ID_DOSE_EVENT].element_size;
            data_id = data_store[RING_MANAGER_DATA_ID_DOSE_EVENT].id;

            // Check that the payload length is valid - There should be no remainder bytes when dividing by element size
            UPDATE_ERR_IF_TRUE(result, (0 != (payload_len % element_size)), RING_MANAGER_ERROR_INVALID_PAYLOAD_LENGTH);

            // Enqueue the received dose data
            IF_OK_RUN_AND_UPDATE(
               result,
               data_manager_ifc->enqueue(data_manager_ifc, data_id, rx_packet->payload, element_size, element_count));
            break;

         case PPI_RD_ERROR_DATA:

            element_size = data_store[RING_MANAGER_DATA_ID_RING_DEBUG_LOG_DATA].element_size;
            element_count = payload_len / data_store[RING_MANAGER_DATA_ID_RING_DEBUG_LOG_DATA].element_size;
            data_id = data_store[RING_MANAGER_DATA_ID_RING_DEBUG_LOG_DATA].id;

            // Check that the payload length is valid - There should be no remainder bytes when dividing by element size
            UPDATE_ERR_IF_TRUE(result, (0 != (payload_len % element_size)), RING_MANAGER_ERROR_INVALID_PAYLOAD_LENGTH);

            // Enqueue the received error data
            IF_OK_RUN_AND_UPDATE(
               result,
               data_manager_ifc->enqueue(data_manager_ifc, data_id, rx_packet->payload, element_size, element_count));
            break;

         case PPI_RD_BATTERY_DATA:

            element_size = data_store[RING_MANAGER_DATA_ID_RING_BATTERY_LEVEL].element_size;
            element_count = payload_len / data_store[RING_MANAGER_DATA_ID_RING_BATTERY_LEVEL].element_size;
            data_id = data_store[RING_MANAGER_DATA_ID_RING_BATTERY_LEVEL].id;

            // Check that the payload length is valid - There should be no remainder bytes when dividing by element size
            UPDATE_ERR_IF_TRUE(result, (0 != (payload_len % element_size)), RING_MANAGER_ERROR_INVALID_PAYLOAD_LENGTH);

            // Enqueue the received battery data
            IF_OK_RUN_AND_UPDATE(
               result,
               data_manager_ifc->enqueue(data_manager_ifc, data_id, rx_packet->payload, element_size, element_count));
            break;

         // Process a status update
         // Parse the status command to determine which data needs to be requested from the ring.
         // If a fifo is empty then no request command is set pending for that data type.
         case PPI_RD_STATUS:

            // Check that the payload length is valid - It should be equal to the size of ring_status_t
            UPDATE_ERR_IF_TRUE(
               result, (sizeof(ring_status_t) != payload_len), RING_MANAGER_ERROR_INVALID_PAYLOAD_LENGTH);

            if(IS_OK(result))
            {
               // Copy the status data from the payload
               memcpy(&status, rx_packet->payload, sizeof(ring_status_t));

               // Update the last status update with the new status and the current time
               self->_last_status_update.ring_status = status;
               self->_last_status_update.update_time_unix_s = unix_time;
            }

            // Determine which pending commands should be set as pending based on the status of the ring.
            IF_OK_RUN_AND_UPDATE(result, update_pending_commands_based_on_status(ifc, &status, unix_time));

            // Check if the new status is sufficiently different from the last stored status to warrant storing it
            IF_OK_RUN_AND_UPDATE(result, store_status_on_delta_and_timeout(ifc, &status, systick_ms));
            break;

         case PPI_RD_CAP_DETECTION_CONFIG:

            // Check that the payload length is valid - It should be equal to the size of cap_detection_status_t
            UPDATE_ERR_IF_TRUE(
               result, (sizeof(cap_detection_status_t) != payload_len), RING_MANAGER_ERROR_INVALID_PAYLOAD_LENGTH);

            if(IS_OK(result))
            {
               // Copy the cap detection status data from the payload
               memcpy(&self->_last_cap_detection_status, rx_packet->payload, sizeof(cap_detection_status_t));
            }
            break;

         default:
            DEBUG_ERROR("[RING_MANAGER] RX: Invalid PPI received in ring manager");
            SET_ERR(result, RING_MANAGER_ERROR_UNKNOWN_PPI_FOR_RX);
      }
   }
   return result;
}

static result_t handle_rx(const ring_manager_interface_t *ifc)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   ring_manager_t *self = ifc->parent;
   mp_packet_payload_t rx_packet = {0};

   result_t result = self->_msg_prot_ifc->get_rx_packet(self->_msg_prot_ifc, &rx_packet);

   IF_OK_RUN_AND_UPDATE(result, dispatch_by_ppi(ifc, &rx_packet));

   return result;
}

static result_t handle_tx(const ring_manager_interface_t *ifc)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   result_t result = RESULT_OK;

   // Check if the status poll time has expired - If so, set the status update command as pending to trigger a new
   // status update from the ring
   uint64_t systick_ms = 0u;
   IF_OK_RUN_AND_UPDATE(result, ifc->parent->_systick_ifc->get_time_ms(ifc->parent->_systick_ifc, &systick_ms));

   if(ifc->parent->_last_status_request_systick_ms + (STATUS_POLL_INTERVAL_MS) < systick_ms && IS_OK(result))
   {
      ifc->parent->_last_status_request_systick_ms = systick_ms;
      ifc->parent->_pending_commands[CMD_ID_REQ_STATUS_UPDATE].pending = true;
   }

   // Check if the cap detection status poll time has expired - If so, set the cap detection status request command as
   // pending to trigger a new cap detection status update from the ring
   // Note. The statement checks that the poll period is greater than 0 to allow for disabling of automatic polling
   if(((ifc->parent->_last_cap_detection_status_systick_ms + ifc->parent->_cap_detection_poll_period_ms) < systick_ms)
      && (ifc->parent->_cap_detection_poll_period_ms > 0u) && (IS_OK(result)))
   {
      ifc->parent->_last_cap_detection_status_systick_ms = systick_ms;
      ifc->parent->_pending_commands[CMD_ID_REQ_CAP_DETECTION_STATUS_DATA].pending = true;
   }

   // Ensure that there is space to receive data for any pending data request commands
   IF_OK_RUN_AND_UPDATE(result, check_capacity_and_update_data_request_commands(ifc));

   // Update the timestamp for the set time command to ensure that the most recent time is sent in the command data
   if(ifc->parent->_pending_commands[CMD_ID_UPDATE_TIME].pending && IS_OK(result))
   {
      uint32_t unix_time = 0u;
      result = ifc->parent->_rtc_ifc->get_time_unix(ifc->parent->_rtc_ifc, &unix_time);
      memcpy(ifc->parent->_pending_commands[CMD_ID_UPDATE_TIME].data, &unix_time, sizeof(unix_time));
   }

   if(IS_OK(result))
   {
      // Now the command queue is updated we can cycle through the commands and process the highest priority one.
      for(uint8_t idx = 0u; idx < CMD_ID_MAX && IS_OK(result); idx++)
      {
         if(ifc->parent->_pending_commands[idx].pending)
         {
            mp_packet_payload_t packet = {0};
            packet.type = ifc->parent->_pending_commands[idx].type;
            packet.ppi = ifc->parent->_pending_commands[idx].ppi;
            packet.pkt_payload_len = ifc->parent->_pending_commands[idx].data_length;
            memcpy(packet.payload, ifc->parent->_pending_commands[idx].data, packet.pkt_payload_len);

            result = ifc->parent->_msg_prot_ifc->send(ifc->parent->_msg_prot_ifc, &packet);

            if(IS_OK(result))
            {
               ifc->parent->_pending_commands[idx].pending = false;
            }
            break; // Only process, or attempt to process one TX per call
         }
      }
   }
   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t update_battery_sample_freq(const ring_manager_interface_t *const ifc, uint16_t battery_sample_freq)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   ifc->parent->_pending_commands[CMD_ID_UPDATE_BATTERY_SAMPLE_FREQ].pending = true;
   memcpy(ifc->parent->_pending_commands[CMD_ID_UPDATE_BATTERY_SAMPLE_FREQ].data,
          &battery_sample_freq,
          sizeof(battery_sample_freq));

   return RESULT_OK;
}

static result_t get_ring_status(const ring_manager_interface_t *const ifc, status_update_t *ring_status)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(ring_status, RING_MANAGER_ERROR_NULL_PTR);

   *ring_status = ifc->parent->_last_status_update;

   return RESULT_OK;
}

static result_t process(const ring_manager_interface_t *const ifc, bool is_docked)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   result_t result = RESULT_OK;
   ring_manager_t *self = ifc->parent;

   // ********************************
   // Handle RX
   // ********************************
   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   result_t rx_result = self->_msg_prot_ifc->get_rx_packet_status(self->_msg_prot_ifc, &rx_status);

   if(IS_OK(rx_result) && (MSG_PROT_RX_PACKET_STATUS_NEW == rx_status))
   {
      rx_result = handle_rx(ifc);
   }
   ON_ERR_DEBUG_ERROR(rx_result, "[RING Manager] Error handling RX");

   // ********************************
   // Handle RX
   // ********************************
   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_MAX;
   result_t tx_result = ifc->parent->_msg_prot_ifc->get_tx_packet_status(ifc->parent->_msg_prot_ifc, &tx_status);

   // Only run TX if ring is docked and previous TX is completed
   if(IS_OK(tx_result) && is_docked && check_tx_completed(tx_status))
   {
      tx_result = handle_tx(ifc);
   }
   ON_ERR_DEBUG_ERROR(tx_result, "[RING Manager] Error handling TX");

   // Return error if either RX or TX had an error
   UPDATE_ERR_IF_TRUE(result, (IS_ERR(rx_result) || IS_ERR(tx_result)), RING_MANAGER_ERROR_PROCESSING);

   return result;
}

static result_t send_cap_detection_config_update(const ring_manager_interface_t *const ifc, cap_detection_cfg_t config)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   ifc->parent->_pending_commands[CMD_ID_UPDATE_CAP_DETECTION_CONFIG].pending = true;
   memcpy(ifc->parent->_pending_commands[CMD_ID_UPDATE_CAP_DETECTION_CONFIG].data, &config, sizeof(config));

   return RESULT_OK;
}

static result_t get_cap_detection_status(const ring_manager_interface_t *const ifc, cap_detection_status_t *status)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(status, RING_MANAGER_ERROR_NULL_PTR);

   *status = ifc->parent->_last_cap_detection_status;

   return RESULT_OK;
}

static result_t set_cap_detection_poll_period_ms(const ring_manager_interface_t *const ifc, uint16_t poll_period_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   // Clamp the poll period to a minimum of 500ms to prevent congestion on the nfc link
   if(poll_period_ms == 0u)
   {
      ifc->parent->_cap_detection_poll_period_ms = 0u;
   }
   else if(poll_period_ms < MINIMUM_PERMISSIBLE_SAMPLE_TIME_MS)
   {
      DEBUG_WARNING("[RING_MANAGER] Cap detection poll period too low - Clamping to 500ms");
      ifc->parent->_cap_detection_poll_period_ms = MINIMUM_PERMISSIBLE_SAMPLE_TIME_MS;
   }
   else
   {
      ifc->parent->_cap_detection_poll_period_ms = poll_period_ms;
   }
   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t ring_manager_init(ring_manager_t *const self,
                           const system_time_interface_t *systick_ifc,
                           const message_protocol_interface_t *msg_prot_ifc,
                           const dock_data_manager_interface_t *dock_data_manager_ifc,
                           const rtc_system_time_interface_t *rtc_ifc)
{
   RETURN_ERR_IF_NULL(self, RING_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(systick_ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(msg_prot_ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(dock_data_manager_ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(rtc_ifc, RING_MANAGER_ERROR_NULL_INTERFACE_PTR);

   result_t result = RESULT_OK;

   self->interface.parent = self;
   self->_systick_ifc = systick_ifc;
   self->_msg_prot_ifc = msg_prot_ifc;
   self->_dock_data_manager_ifc = dock_data_manager_ifc;
   self->_rtc_ifc = rtc_ifc;

   self->interface.get_cap_detection_status = get_cap_detection_status;
   self->interface.send_cap_detection_config_update = send_cap_detection_config_update;
   self->interface.update_battery_sample_freq = update_battery_sample_freq;
   self->interface.set_cap_detection_poll_period_ms = set_cap_detection_poll_period_ms;
   self->interface.get_ring_status = get_ring_status;
   self->interface.process = process;

   // *****************************
   // Initialize pending table
   // *****************************

   // Set all commands to not pending
   for(uint8_t i = 0u; i < CMD_ID_MAX; i++)
   {
      self->_pending_commands[i].pending = false;
   }

   // Update PPIs and data lengths for all command types
   self->_pending_commands[CMD_ID_UPDATE_TIME].ppi = PPI_RD_TIME;
   self->_pending_commands[CMD_ID_UPDATE_TIME].type = PPI_TYPE_PUSH;
   self->_pending_commands[CMD_ID_UPDATE_TIME].data_length = sizeof(uint32_t);

   self->_pending_commands[CMD_ID_UPDATE_BATTERY_SAMPLE_FREQ].ppi = PPI_RD_BATTERY_SAMPLE_RATE;
   self->_pending_commands[CMD_ID_UPDATE_BATTERY_SAMPLE_FREQ].type = PPI_TYPE_PUSH;
   self->_pending_commands[CMD_ID_UPDATE_BATTERY_SAMPLE_FREQ].data_length = sizeof(uint16_t);

   self->_pending_commands[CMD_ID_UPDATE_CAP_DETECTION_CONFIG].ppi = PPI_RD_CAP_DETECTION_CONFIG;
   self->_pending_commands[CMD_ID_UPDATE_CAP_DETECTION_CONFIG].type = PPI_TYPE_PUSH;
   self->_pending_commands[CMD_ID_UPDATE_CAP_DETECTION_CONFIG].data_length = sizeof(cap_detection_cfg_t);

   self->_pending_commands[CMD_ID_REQ_STATUS_UPDATE].ppi = PPI_RD_STATUS;
   self->_pending_commands[CMD_ID_REQ_STATUS_UPDATE].type = PPI_TYPE_RQ;
   self->_pending_commands[CMD_ID_REQ_STATUS_UPDATE].data_length = 0u;

   self->_pending_commands[CMD_ID_REQ_DOSE_EVENT_DATA].ppi = PPI_RD_DOSE_DATA;
   self->_pending_commands[CMD_ID_REQ_DOSE_EVENT_DATA].type = PPI_TYPE_RQ;
   self->_pending_commands[CMD_ID_REQ_DOSE_EVENT_DATA].data_length = sizeof(uint16_t);

   self->_pending_commands[CMD_ID_REQ_RING_DEBUG_LOG_DATA].ppi = PPI_RD_ERROR_DATA;
   self->_pending_commands[CMD_ID_REQ_RING_DEBUG_LOG_DATA].type = PPI_TYPE_RQ;
   self->_pending_commands[CMD_ID_REQ_RING_DEBUG_LOG_DATA].data_length = sizeof(uint16_t);

   self->_pending_commands[CMD_ID_REQ_RING_BATTERY_LEVEL_DATA].ppi = PPI_RD_BATTERY_DATA;
   self->_pending_commands[CMD_ID_REQ_RING_BATTERY_LEVEL_DATA].type = PPI_TYPE_RQ;
   self->_pending_commands[CMD_ID_REQ_RING_BATTERY_LEVEL_DATA].data_length = sizeof(uint16_t);

   self->_pending_commands[CMD_ID_REQ_CAP_DETECTION_STATUS_DATA].ppi = PPI_RD_CAP_DETECTION_CONFIG;
   self->_pending_commands[CMD_ID_REQ_CAP_DETECTION_STATUS_DATA].type = PPI_TYPE_RQ;
   self->_pending_commands[CMD_ID_REQ_CAP_DETECTION_STATUS_DATA].data_length = 0u;

   data_store_t *_data_stores = self->_data_store;

   // Initialize data store array
   // The awkward syntax is to permit the use of constant members in the data_store_t struct
   size_t size = 0u;
   result = dock_data_manager_ifc->get_element_size(dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &size);
   data_store_t dose_event_store = {
      .id = DATA_ID_DOSE_EVENT,
      .element_size = (uint16_t)size,
      .associated_command_id = CMD_ID_REQ_DOSE_EVENT_DATA,
   };
   memcpy(&_data_stores[RING_MANAGER_DATA_ID_DOSE_EVENT], &dose_event_store, sizeof(data_store_t));

   IF_OK_RUN_AND_UPDATE(result,
                        dock_data_manager_ifc->get_element_size(dock_data_manager_ifc, DATA_ID_RING_DEBUG_LOG, &size));
   data_store_t ring_error_store = {
      .id = DATA_ID_RING_DEBUG_LOG,
      .element_size = (uint16_t)size,
      .associated_command_id = CMD_ID_REQ_RING_DEBUG_LOG_DATA,
   };
   memcpy(&_data_stores[RING_MANAGER_DATA_ID_RING_DEBUG_LOG_DATA], &ring_error_store, sizeof(data_store_t));

   IF_OK_RUN_AND_UPDATE(
      result, dock_data_manager_ifc->get_element_size(dock_data_manager_ifc, DATA_ID_RING_BATTERY_LEVEL, &size));
   data_store_t battery_store = {
      .id = DATA_ID_RING_BATTERY_LEVEL,
      .element_size = (uint16_t)size,
      .associated_command_id = CMD_ID_REQ_RING_BATTERY_LEVEL_DATA,
   };
   memcpy(&_data_stores[RING_MANAGER_DATA_ID_RING_BATTERY_LEVEL], &battery_store, sizeof(data_store_t));

   // Initialize last status update
   self->_last_status_update.update_time_unix_s = 0u;
   self->_last_status_update.ring_status = (ring_status_t){0};

   // Initialize last cap detection status update
   self->_cap_detection_poll_period_ms = 0u; // Default to no automatic polling
   self->_last_cap_detection_status_systick_ms = 0u;
   self->_last_cap_detection_status = (cap_detection_status_t){0};

   // Initialize last stored status
   self->_stored_status = (ring_status_t){0};

   // Initialize the last status request and storage systick
   self->_last_status_request_systick_ms = 0u;
   self->_last_status_store_systick_ms = 0u;

   if(IS_ERR(result))
   {
      self->interface.parent = NULL; // Invalidate interface on failure
      self->_systick_ifc = NULL;
      self->_msg_prot_ifc = NULL;
   }

   return result;
}
