
/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file message_protocol.c
 * @ingroup message_protocol
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "message_protocol.h"
#include "crc16.h"
#include "debug.h"
#ifdef NRF_CRYPTO_ENABLED      // Exclude this during testing
#   include "nrf_crypto_rng.h" // For session ID generation.
#endif

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_MESSAGE_PROTOCOL;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define ACK (true)
#define NAK (false)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t send(const message_protocol_interface_t *const interface, mp_packet_payload_t *pkt_payload);
static result_t process(const message_protocol_interface_t *const interface);
static result_t get_rx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_RX_PACKET_STATUS *status);
static result_t get_tx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_TX_PACKET_STATUS *status);
static result_t get_rx_packet(const message_protocol_interface_t *const interface, mp_packet_payload_t *rx_packet);
static result_t get_max_payload_length(const message_protocol_interface_t *const interface, uint16_t *max_payload_len);

// Non-interface functions

/**
 * @brief Processes a received frame from the link layer.
 *
 * This function handles different types of link layer packets: ACK, NAK, and DATA.
 * For ACK and NAK packets, it updates the internal state and sends an ACK if necessary.
 * For DATA packets, it delivers the payload to the application by enqueueing the received packet into the RX queue
 * and sends an ACK.
 *
 * @param self The message protocol instance.
 * @param header The header of the received frame.
 * @param payload The payload of the received frame.
 * @param payload_len The length of the payload.
 *
 * @return A result indicating the success or failure of the function.
 */
static result_t on_link_layer_packet(message_protocol_t *self, const mp_packet_t *packet);

// static result_t serialize_data_frame(message_protocol_t *self, message_protocol_packet_t *packet);

/**
 * @brief Serializes and sends a message protocol packet to the link layer.
 *
 * This function takes a pointer to an mp_packet_t structure, serializes its header and payload fields into a
 * contiguous byte buffer in little-endian format, and transmits the resulting frame over the link interface.
 * The serialization is performed explicitly, field by field, to avoid issues with structure padding and endianness.
 *
 * The function checks that the total packet length does not exceed the maximum allowed by the link layer. If the
 * payload length is valid, the payload is copied into the frame buffer after the header and payload header fields.
 *
 * The function calculates and sets the CRC for the entire packet (excluding the CRC field itself) before sending.
 *
 * @param self    Pointer to the message protocol instance.
 * @param packet  Pointer to the mp_packet_t structure to be serialized and sent.
 *
 * @retval RESULT_OK                If the packet was successfully serialized and sent.
 * @retval MSG_PROT_ERROR_NULL_PTR  If any pointer argument is NULL.
 * @retval MSG_PROT_ERROR_BUFFER_OVERFLOW If the packet length exceeds the allowed maximum.
 * @retval MSG_PROT_ERROR_OUT_OF_RANGE    If the payload length is invalid.
 *
 * @note Any errors from the link layer send operation are logged but not propagated, as the protocol will retry
 *       transmission on failure.
 */
static result_t send_pkt_to_link_layer(const message_protocol_t *self, const mp_packet_t *packet);

/**
 * @brief Keeps track of the time since a message was sent be setting the deadline by which a response should be
 * received before timing out.
 *
 * @param self
 * @return result_t
 */
static result_t start_timer(message_protocol_t *self);

static result_t timer_expired(const message_protocol_t *self, bool *is_expired);

static result_t handle_ack(message_protocol_t *self, uint16_t ack_id);

/**
 * @brief Delivers a received message protocol packet payload to the RX packet buffer.
 *
 * This function is called when a valid DATA packet is received from the link layer. If the internal RX packet buffer
 * is available (status is PROCESSED), it copies the header and payload fields from the received packet into the
 * internal RX buffer and marks the RX packet status as NEW, making it available to the application. If the RX buffer
 * is not available (status is not PROCESSED), the function logs an error and sends a NAK to the sender, requesting
 * retransmission later.
 *
 * @param self   Pointer to the message protocol instance.
 * @param packet Pointer to the received mp_packet_t structure containing the header and payload to deliver.
 *
 * @retval RESULT_OK                If the payload was successfully delivered to the RX buffer.
 * @retval MSG_PROT_ERROR_NULL_PTR  If any pointer argument is NULL.
 *
 * @note If the RX buffer is busy, a NAK is sent to the sender to request a retry.
 */
static result_t deliver_payload(message_protocol_t *self, const mp_packet_t *packet);

// Send the same packet back as ACK or NAK
static result_t send_ack_or_nak(message_protocol_t *self, const mp_packet_t *packet, bool is_ack);

static result_t get_link_layer_pkt(const message_protocol_t *self, uint8_t *data, uint16_t data_buffer_size);
static result_t get_max_link_layer_pkt_len(const message_protocol_t *self, uint16_t *max_packet_len);

/**
 * @brief Serialize a message protocol packet into a raw on-wire buffer.
 *
 * This helper mirrors the production send serialization (little-endian fields + CRC),
 * but does not transmit the buffer. It is used for comparing inbound data against
 * the last transmitted packet.
 */
static result_t encode_packet_raw(const message_protocol_t *self,
                                  const mp_packet_t *packet,
                                  uint8_t *packet_buffer,
                                  uint16_t buffer_size,
                                  uint16_t *packet_length_out);
static bool is_self_tx_echo(message_protocol_t *self, const uint8_t *rx_data, uint16_t rx_len);
static result_t send_pkt_to_link_layer_cached(message_protocol_t *self, const mp_packet_t *packet);

// static result_t parse_data(message_protocol_t *self, uint8_t *serial_data, uint16_t serial_data_len);

/**
 * @brief Processes outbound messages
 *
 * This function checks if there is an existing transmission being handled or if a new transmission can be made.
 * If an existing transmission is being handled, it checks if the timer has expired. If the timer has expired, it
 * resends the frame if there are retries left. If there are no retries left, it returns a timeout error.
 *
 * @note This function must be called frequently enough to handle timeouts and queued transmissions.
 *
 * @param interface The message protocol instance.
 *
 * @return A result indicating the success or failure of the function.
 *
 * MSG_PROT_ERROR_TIMEOUT: Message TX failed after multiple retries. This error can be used to determine if the LL is
 * down. Other: The MP handles BUSY related errors at lower levels and do not report these errors. They are handled as
 * simple failed attempts. More serious errors are reported from the bottom up.
 *
 */
static result_t process_outbound_packets(message_protocol_t *self);

/**
 * @brief Processes inbound packets
 *
 * This function checks the link layer interface for serial data. If there is new serial data, it parses it via the
 * Serial Link Protocol and delivers the payload to internal message protocol functions for processing. If corrupted
 * payloads are detected, it sends NAKs
 *
 * @note This function must be called frequently enough to prevent the serial input buffer overflowing
 *
 * @param self The message protocol instance.
 *
 * @return A result indicating the success or failure of the function.
 */
static result_t process_inbound_packets(message_protocol_t *self);

static result_t generate_session_id(const message_protocol_t *self, uint32_t *session_id);

static result_t reset_mp_state(message_protocol_t *self);

static result_t mp_sync_start(message_protocol_t *self);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static result_t generate_session_id(const message_protocol_t *self, uint32_t *session_id)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(session_id, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_TRUE(!self->_is_this_mp_instance_master, MSG_PROT_ERROR_INVALID_ARG);
   result_t result = RESULT_OK;

#if defined(NRF_CRYPTO_ENABLED) && NRF_CRYPTO_ENABLED
   uint8_t random_bytes[sizeof(uint32_t)] = {0};
   ret_code_t nrf_result = NRF_SUCCESS;
   nrf_result = nrf_crypto_rng_vector_generate(random_bytes, sizeof(random_bytes));

   if(NRF_SUCCESS == nrf_result)
   {
      *session_id = (uint32_t)random_bytes[0] | ((uint32_t)random_bytes[1] << 8u) | ((uint32_t)random_bytes[2] << 16u)
                    | ((uint32_t)random_bytes[3] << 24u);
   }
   else
   {
      SET_ERR(result, MSG_PROT_ERROR_RNG_FAILURE);
      DEBUG_ERROR("RNG failure in session ID generation. NRF error code: %d", nrf_result);
   }
#else
   {
      SET_ERR(result, MSG_PROT_ERROR_RNG_FAILURE);
      DEBUG_ERROR("RNG not enabled; cannot generate MP session ID.");
   }
#endif

   return result;
}

static result_t reset_mp_state(message_protocol_t *self)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   memset(&self->_rx_packet, 0, sizeof(mp_packet_t));
   self->_rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED;
   memset(&self->_tx_packet, 0, sizeof(mp_packet_t));
   // TX packet status is set to abandoned to indicate to application layer that tx packet was not successfully sent.
   self->_tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_ABANDONED;

   memset(self->_last_rx_packet_raw, 0, sizeof(self->_last_rx_packet_raw));
   self->_last_rx_packet_len = 0u;
   self->_last_rx_packet_valid = false;
   self->_last_rx_packet_deferred = false;

   memset(self->_last_tx_packet_raw, 0, sizeof(self->_last_tx_packet_raw));
   self->_last_tx_packet_len = 0u;
   self->_last_tx_packet_valid = false;

   self->_next_packet_id = 1u;
   self->_pending_id = 0u;
   self->_retries_left = MSG_PROT_MAX_RETRIES;
   self->_current_session_id = 0u;

   return RESULT_OK;
}

static result_t handle_ack(message_protocol_t *self, uint16_t ack_id)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   if(((uint8_t)MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK == self->_tx_packet.header.status)
      && (ack_id == self->_pending_id))
   {
      self->_tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_COMPLETED;
   }

   return RESULT_OK;
}

static result_t encode_packet_raw(const message_protocol_t *self,
                                  const mp_packet_t *packet,
                                  uint8_t *packet_buffer,
                                  uint16_t buffer_size,
                                  uint16_t *packet_length_out)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(packet, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(packet_buffer, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(packet_length_out, MSG_PROT_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   const uint16_t header_len = 2u + 2u + 4u + 1u + 1u; // See mp_packet_header_t
   const uint16_t payload_hdr_len = MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
   const uint16_t packet_length = (uint16_t)(header_len + payload_hdr_len + packet->payload.pkt_payload_len);

   if((packet_length > buffer_size) || (packet_length > MP_MAX_PACKET_LENGTH))
   {
      SET_ERR(result, MSG_PROT_ERROR_BUFFER_OVERFLOW);
   }

   if(IS_OK(result) && (packet_length > self->_max_packet_len))
   {
      SET_ERR(result, MSG_PROT_ERROR_BUFFER_OVERFLOW);
   }

   if(IS_OK(result))
   {
      uint8_t crc_offset = sizeof(packet->header.pkt_crc);
      size_t offset = crc_offset;

      // Little-endian wire format. Explicit serialization to avoid padding and endianess issues on wire.
      packet_buffer[offset] = (uint8_t)(packet->header.pkt_counter & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)((packet->header.pkt_counter >> 8u) & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)(packet->header.session_id & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)((packet->header.session_id >> 8u) & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)((packet->header.session_id >> 16u) & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)((packet->header.session_id >> 24u) & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = packet->header.pkt_type;
      offset += 1u;
      packet_buffer[offset] = packet->header.status;
      offset += 1u;

      packet_buffer[offset] = packet->payload.type;
      offset += 1u;
      packet_buffer[offset] = packet->payload.ppi;
      offset += 1u;
      packet_buffer[offset] = (uint8_t)(packet->payload.pkt_payload_len & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)((packet->payload.pkt_payload_len >> 8u) & 0xFFu);
      offset += 1u;

      if(packet->payload.pkt_payload_len > 0u)
      {
         memcpy(&packet_buffer[offset], packet->payload.payload, packet->payload.pkt_payload_len);
      }

      // Calculate and set CRC for the on-the-wire packet
      uint16_t crc = crc16_update(&packet_buffer[crc_offset], packet_length - crc_offset, NULL);
      offset = 0u;
      packet_buffer[offset] = (uint8_t)(crc & 0xFFu);
      offset += 1u;
      packet_buffer[offset] = (uint8_t)((crc >> 8u) & 0xFFu);

      *packet_length_out = packet_length;
   }

   return result;
}

static bool is_self_tx_echo(message_protocol_t *self, const uint8_t *rx_data, uint16_t rx_len)
{
   // Guard clause.
   if(NULL == self || NULL == rx_data)
   {
      return false;
   }

   bool is_echo = false;

   if(self->_last_tx_packet_valid)
   {
      if((self->_last_tx_packet_len == rx_len)
         && (0 == memcmp(self->_last_tx_packet_raw, rx_data, self->_last_tx_packet_len)))
      {
         is_echo = true;
      }
      else
      {
         // Clear echo tracking once a different valid packet is seen.
         self->_last_tx_packet_valid = false;
      }
   }

   return is_echo;
}

static result_t send_pkt_to_link_layer_cached(message_protocol_t *self, const mp_packet_t *packet)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(packet, MSG_PROT_ERROR_NULL_PTR);

   result_t result = RESULT_OK;
   uint8_t packet_buffer[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t packet_length = 0u;

   result = encode_packet_raw(self, packet, packet_buffer, sizeof(packet_buffer), &packet_length);

   IF_OK_RUN_AND_UPDATE(result, self->_send_pkt_to_link_layer(self, packet));

   if(IS_OK(result))
   {
      memcpy(self->_last_tx_packet_raw, packet_buffer, packet_length);
      self->_last_tx_packet_len = packet_length;
      self->_last_tx_packet_valid = true;
   }

   return result;
}

static result_t send_pkt_to_link_layer(const message_protocol_t *self, const mp_packet_t *packet)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(packet, MSG_PROT_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   // Serialize the mp_packet_t into a frame buffer for the link layer to send
   uint8_t packet_buffer[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t packet_length = 0u;

   result = encode_packet_raw(self, packet, packet_buffer, sizeof(packet_buffer), &packet_length);

   IF_OK_RUN_AND_UPDATE(result,
                        self->_serial_link_ifc->send_packet(self->_serial_link_ifc, packet_buffer, packet_length));

   // This should propagate the buffer overflow error but ignore errors from the link layer.
   if(IS_ERR(result) && (SW_UNIT_ID_MESSAGE_PROTOCOL != GET_ERR_UNIT(result)))
   {
      DEBUG_ERROR("Failed to send data over LL. See comms_driver_interface.h error unit %d, code %d",
                  GET_ERR_UNIT(result),
                  GET_ERR_CODE(result));
      // Do not propagate the error. The message transmit will be retried and eventually time out. When this happens,
      // the link layer will be reported as being down.
      result = RESULT_OK;
   }

   return result;
}

static result_t start_timer(message_protocol_t *self)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   uint64_t current_time = 0;
   result_t result = self->_systick_ifc->get_time_ms(self->_systick_ifc, &current_time);

   if(IS_OK(result))
   {
      self->_deadline_ms = current_time + self->_ack_timeout_ms;
   }

   return result;
}

static result_t timer_expired(const message_protocol_t *self, bool *is_expired)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(is_expired, MSG_PROT_ERROR_NULL_PTR);

   uint64_t current_time = 0;
   result_t result = self->_systick_ifc->get_time_ms(self->_systick_ifc, &current_time);

   if(IS_OK(result))
   {
      if(self->_deadline_ms < current_time)
      {
         *is_expired = true;
      }
      else
      {
         *is_expired = false;
      }
   }

   return result;
}

static result_t deliver_payload(message_protocol_t *self, const mp_packet_t *packet)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(packet, MSG_PROT_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   if((uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED == self->_rx_packet.header.status)
   {
      // Current RX packet was processed, load the new packet.
      memcpy(&self->_rx_packet.header, &packet->header, sizeof(mp_packet_header_t));
      self->_rx_packet.payload.type = packet->payload.type;
      self->_rx_packet.payload.ppi = packet->payload.ppi;
      self->_rx_packet.payload.pkt_payload_len = packet->payload.pkt_payload_len;
      memcpy(self->_rx_packet.payload.payload, packet->payload.payload, packet->payload.pkt_payload_len);

      // Set rx packet status to new.
      self->_rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_NEW;
   }
   else
   {
      DEBUG_ERROR("Previous RX Packet not yet processed. NAK-ing new packet.");

      // NAK so sender retries later.
      result = send_ack_or_nak(self, packet, NAK);
   }
   return result;
}

static result_t send_ack_or_nak(message_protocol_t *self, const mp_packet_t *packet, bool is_ack)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   // Make a copy of the received packet to modify for ACK/NAK
   mp_packet_t ack_nak_packet = {0};
   memcpy(&ack_nak_packet.header, &packet->header, sizeof(mp_packet_header_t));
   ack_nak_packet.payload.type = packet->payload.type; // if needed
   ack_nak_packet.payload.ppi = packet->payload.ppi;   // if needed
   ack_nak_packet.payload.pkt_payload_len = 0;         // No payload for ACK/NAK

   ack_nak_packet.header.pkt_type = is_ack ? MP_PACKET_TYPE_ACK : MP_PACKET_TYPE_NAK;

   result_t result = send_pkt_to_link_layer_cached(self, &ack_nak_packet);

   return result;
}

static result_t get_link_layer_pkt(const message_protocol_t *self, uint8_t *data, uint16_t data_buffer_size)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(data, MSG_PROT_ERROR_NULL_PTR);

   result_t result = self->_serial_link_ifc->get_packet(self->_serial_link_ifc, data, data_buffer_size);

   // Link-layer: treat BUSY as no data received.
   if(IS_ERR(result) && (COMMS_DRIVER_ERROR_BUSY == GET_ERR_CODE(result)))
   {
      result = RESULT_OK; // Not considered an error, but no valid data was received either.
   }

   return result;
}

static result_t get_max_link_layer_pkt_len(const message_protocol_t *self, uint16_t *max_packet_len)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(max_packet_len, MSG_PROT_ERROR_NULL_PTR);

   result_t result = self->_serial_link_ifc->get_max_packet_length(self->_serial_link_ifc, max_packet_len);

   if(IS_ERR(result))
   {
      DEBUG_ERROR("Failed to get data over LL. unit %d error %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   return result;
}

static result_t mp_sync_start(message_protocol_t *self)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   uint64_t current_ms = 0;
   result = self->_systick_ifc->get_time_ms(self->_systick_ifc, &current_ms);
   uint64_t time_since_last_sync = current_ms - self->_last_resync_time_ms;
   bool is_first_sync_attempt = !self->_has_attempted_sync;

   if(IS_OK(result) && (is_first_sync_attempt || (time_since_last_sync > MAX_TIME_BEFORE_SYNC_RETRY_MS)))
   {
      // Reset MP state
      result = reset_mp_state(self);
      if(IS_OK(result))
      {
         // Generate new session ID
         result = self->_generate_mp_session_id(self, &self->_current_session_id);
      }
      if(IS_OK(result))
      {
         self->_last_resync_time_ms = current_ms;
         self->_has_attempted_sync = true;
         DEBUG_INFO(" Generated new session ID: %d", self->_current_session_id);
      }
      if(IS_OK(result))
      {
         // Set the new packet header fields
         mp_packet_t packet = {0};

         packet.header.pkt_counter = self->_next_packet_id;
         self->_next_packet_id++;
         packet.header.session_id = self->_current_session_id;
         packet.header.pkt_type = MP_PACKET_TYPE_SYNC_START; // Initiate sync process
         packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;

         packet.payload.type = 0u;            // No specific type for sync packets
         packet.payload.ppi = 0u;             // No specific PPI for sync packets
         packet.payload.pkt_payload_len = 0u; // No payload for sync packets

         result = send_pkt_to_link_layer_cached(self, &(packet));
         self->_is_syncing = true;
         DEBUG_INFO(" Sent SYNC_START packet to initiate sync process.");
      }
   }
   return result;
}

static result_t on_link_layer_packet(message_protocol_t *self, const mp_packet_t *packet)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   switch(packet->header.pkt_type)
   {
      case MP_PACKET_TYPE_ACK:
      {
         if(packet->header.session_id != self->_current_session_id)
         {
            // Session mismatch
            if(self->_is_this_mp_instance_master)
            {
               DEBUG_ERROR(" Session ID mismatch. Expected: %d, Received: %d",
                           self->_current_session_id,
                           packet->header.session_id);

               result = mp_sync_start(self);
            }
            else
            {
               // Slave received data with wrong session ID, send SYNC_MISMATCH to master
               mp_packet_t response_packet = {0};
               memcpy(&response_packet.header, &packet->header, sizeof(mp_packet_header_t));
               response_packet.header.session_id = self->_current_session_id; // Indicate expected session ID
               response_packet.header.pkt_type = MP_PACKET_TYPE_SYNC_MISMATCH;
               response_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;

               response_packet.payload.type = 0;            // No specific type for sync packets
               response_packet.payload.ppi = 0;             // No specific PPI for sync packets
               response_packet.payload.pkt_payload_len = 0; // No payload for sync packets

               result = send_pkt_to_link_layer_cached(self, &(response_packet));
               DEBUG_INFO(" Sent SYNC_MISMATCH packet to indicate session ID mismatch.");
            }
         }
         else
         {
            // No mismatch, handle ack normally
            result = handle_ack(self, packet->header.pkt_counter);
         }
      }
      break;

      case MP_PACKET_TYPE_NAK:
      {
         if(packet->header.session_id != self->_current_session_id)
         {
            // Session mismatch
            if(self->_is_this_mp_instance_master)
            {
               DEBUG_ERROR(" Session ID mismatch. Expected: %d, Received: %d",
                           self->_current_session_id,
                           packet->header.session_id);

               result = mp_sync_start(self);
            }
            else
            {
               // Slave received data with wrong session ID, send SYNC_MISMATCH to master
               mp_packet_t response_packet = {0};
               memcpy(&response_packet.header, &packet->header, sizeof(mp_packet_header_t));
               response_packet.header.session_id = self->_current_session_id; // Indicate expected session ID
               response_packet.header.pkt_type = MP_PACKET_TYPE_SYNC_MISMATCH;
               response_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;

               response_packet.payload.type = 0;            // No specific type for sync packets
               response_packet.payload.ppi = 0;             // No specific PPI for sync packets
               response_packet.payload.pkt_payload_len = 0; // No payload for sync packets

               result = send_pkt_to_link_layer_cached(self, &(response_packet));
               DEBUG_INFO(" Sent SYNC_MISMATCH packet to indicate session ID mismatch.");
            }
         }
         else
         {
            // No mismatch, handle NAK normally
            // Immediate retransmission, preserve retries_left. Receiving a NAK indicates that the link is active, but
            // there was a LL fault.
            result = send_pkt_to_link_layer_cached(self, &self->_last_packet_sent);

            if(IS_OK(result))
            {
               result = start_timer(self);
            }
         }
      }
      break;

      case MP_PACKET_TYPE_DATA:
      {
         if(packet->header.session_id != self->_current_session_id)
         {
            // Session mismatch
            if(self->_is_this_mp_instance_master)
            {
               DEBUG_ERROR(" Session ID mismatch. Expected: %d, Received: %d",
                           self->_current_session_id,
                           packet->header.session_id);

               result = mp_sync_start(self);
            }
            else
            {
               // Slave received data with wrong session ID, send SYNC_MISMATCH to master
               mp_packet_t response_packet = {0};
               memcpy(&response_packet.header, &packet->header, sizeof(mp_packet_header_t));
               response_packet.header.session_id = self->_current_session_id; // Indicate expected session ID
               response_packet.header.pkt_type = MP_PACKET_TYPE_SYNC_MISMATCH;
               response_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;

               response_packet.payload.type = 0;            // No specific type for sync packets
               response_packet.payload.ppi = 0;             // No specific PPI for sync packets
               response_packet.payload.pkt_payload_len = 0; // No payload for sync packets

               result = send_pkt_to_link_layer_cached(self, &(response_packet));
               DEBUG_INFO(" Sent SYNC_MISMATCH packet to indicate session ID mismatch.");
            }
         }
         else
         {
            bool rx_busy = ((uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED != self->_rx_packet.header.status);

            // No mismatch, deliver payload
            result = deliver_payload(self, packet);
            if(IS_OK(result) && !rx_busy)
            {
               result = send_ack_or_nak(self, packet, ACK);
            }
         }
      }
      break;
      case MP_PACKET_TYPE_SYNC_START:
      {
         // Is this the master? Only the master should be sending the SYNC_START packet type
         if(self->_is_this_mp_instance_master)
         {
            DEBUG_ERROR("Received SYNC_START packet from another master instance.");
            SET_ERR(result, MSG_PROT_ERROR_INVALID_PACKET_STATUS);
         }
         else
         {
            // Slave received SYNC_START, respond with SYNC_ACK
            // Reset MP state
            result = reset_mp_state(self);

            if(IS_OK(result))
            {
               self->_current_session_id = packet->header.session_id; // Adopt master's session ID

               mp_packet_t response_packet = {0};
               memcpy(&response_packet.header, &packet->header, sizeof(mp_packet_header_t));
               response_packet.header.pkt_type = MP_PACKET_TYPE_SYNC_ACK;
               response_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
               response_packet.header.session_id = self->_current_session_id; // Verify session ID

               response_packet.payload.type = 0;            // No specific type for sync packets
               response_packet.payload.ppi = 0;             // No specific PPI for sync packets
               response_packet.payload.pkt_payload_len = 0; // No payload for sync packets

               result = send_pkt_to_link_layer_cached(self, &(response_packet));
               DEBUG_INFO(" Sent SYNC_ACK packet to acknowledge sync process.");
            }
         }
      }
      break;
      case MP_PACKET_TYPE_SYNC_MISMATCH:
      {
         if(self->_is_this_mp_instance_master)
         {
            // Master received SYNC_MISMATCH, restart sync process
            result = mp_sync_start(self);
         }
         else
         {
            // Slave received SYNC_MISMATCH. This should never happen
            DEBUG_ERROR(" Slave instance received SYNC_MISMATCH packet from master.");
            SET_ERR(result, MSG_PROT_ERROR_INVALID_PACKET_STATUS);
         }
      }
      break;
      case MP_PACKET_TYPE_SYNC_ACK:
      {
         if(self->_is_this_mp_instance_master)
         {
            if(packet->header.session_id != self->_current_session_id)
            {
               DEBUG_ERROR(" Session ID mismatch. Expected: %d, Received: %d",
                           self->_current_session_id,
                           packet->header.session_id);

               result = mp_sync_start(self);
            }
            else if(self->_is_syncing)
            {
               // Sync successful
               self->_is_syncing = false;
               DEBUG_INFO(" Sync process completed. New session received ID: %d", packet->header.session_id);
            }
            else
            {
               // Unexpected SYNC_ACK while not syncing. Resync
               DEBUG_ERROR("Received unexpected SYNC_ACK packet while not syncing.");
               result = mp_sync_start(self);
            }
         }
         else
         {
            // Slave received SYNC_ACK. This should never happen
            DEBUG_ERROR(" Slave instance received SYNC_ACK packet from master.");
            SET_ERR(result, MSG_PROT_ERROR_INVALID_PACKET_STATUS);
         }
      }
      break;
      case MP_PACKET_TYPE_MAX:
      // Fall through
      default:
      {
         DEBUG_ERROR(" Unknown packet type");
         SET_ERR(result, MSG_PROT_ERROR_SWITCH_DEFAULT);
      }
      break;
   }

   return result;
}

static result_t process_outbound_packets(message_protocol_t *self)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   // Existing transmission being handled.
   bool is_timer_expired = false;
   result_t result = timer_expired(self, &is_timer_expired);

   // Handle retry counter and message abandonment upfront and separate.
   if(IS_OK(result) && is_timer_expired
      && (((uint8_t)MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK == self->_tx_packet.header.status)
          || (MSG_PROT_TX_PACKET_STATUS_ERROR == self->_tx_packet.header.status)))
   {
      if(self->_retries_left > 0u)
      {
         self->_retries_left--;
      }
      else
      {
         DEBUG_ERROR("Message timed out.");
         // Note: This discards the current message and updates the tx_packet status to abandoned. It prevents a
         // lock up scenario at the cost of discarding the current packet. It is up to the higher level unit
         // managing this unit to handle the reported error appropriately. The higher level unit should also
         // implement logic to prevent packet loss if it cannot be tolerated. This unit must be able to move on.
         SET_ERR(result, MSG_PROT_ERROR_TIMEOUT);
         self->_tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_ABANDONED;
      }
   }

   // Resend last packet if waiting for ACK and timer expired
   if((IS_OK(result)) && (is_timer_expired)
      && ((uint8_t)MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK == self->_tx_packet.header.status))
   {
      result = send_pkt_to_link_layer_cached(self, &self->_last_packet_sent);

      if(IS_ERR(result))
      {
         DEBUG_ERROR("Failed to send serialized data over link layer.");
      }

      // Start timer regardless of whether the send operation was successful or not to ensure a retry later.
      result = start_timer(self);
   }
   else
   {
      // Check whether a new transmission must be started or whether a previous transmission failed and needs to be
      // retried due to errors.
      if(((IS_OK(result)) && ((uint8_t)MSG_PROT_TX_PACKET_STATUS_NEW == self->_tx_packet.header.status))
         || (((uint8_t)MSG_PROT_TX_PACKET_STATUS_ERROR == self->_tx_packet.header.status) && (is_timer_expired)))
      {
         // Only reset retries if this is a new packet, not a retry after error.
         if((uint8_t)MSG_PROT_TX_PACKET_STATUS_NEW == self->_tx_packet.header.status)
         {
            self->_retries_left = MSG_PROT_MAX_RETRIES;
         }

         // Attempt to send packet
         if(IS_OK(result))
         {
            result = send_pkt_to_link_layer_cached(self, &self->_tx_packet);
            if(IS_OK(result))
            {
               memcpy(&self->_last_packet_sent, &self->_tx_packet, sizeof(mp_packet_t));
            }

            if(IS_OK(result))
            {
               self->_pending_id = self->_next_packet_id;
               self->_next_packet_id++;
               self->_tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK;
            }
            else
            {
               self->_tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_ERROR;
               DEBUG_ERROR("Failed to send data over link layer.");
            }
         }

         // Start timer regardless of whether the send operation was successful or not to ensure a retry later.
         result = start_timer(self);
      }
   }
   return result;
}

static result_t process_inbound_packets(message_protocol_t *self)
{
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   // Check the link layer for incoming data.
   uint8_t link_layer_data[MP_MAX_PACKET_LENGTH] = {0};
   uint8_t blank_data[MP_MAX_PACKET_LENGTH] = {0};
   result = self->_get_link_layer_pkt(self, link_layer_data, sizeof(link_layer_data));

   if(IS_OK(result) && (0 != memcmp(link_layer_data, blank_data, sizeof(blank_data))))
   {
      // Data received from the link layer: parse it
      // Expected format from link layer data: |[mp_packet_header_t][mp_packet_payload_t]............|
      // Expect for first byte in the message protocol header to be the crc

      const uint16_t header_len = 2u + 2u + 4u + 1u + 1u; // See mp_packet_header_t
      const uint16_t payload_hdr_len = MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
      const uint16_t min_packet_len = header_len + payload_hdr_len;

      if(MP_MAX_PACKET_LENGTH < min_packet_len)
      {
         SET_ERR(result, MSG_PROT_ERROR_BUFFER_OVERFLOW);
      }

      if(IS_OK(result))
      {
         size_t offset = 0u;
         mp_packet_t packet = {0};

         // populate packet header
         packet.header.pkt_crc
            = (uint16_t)((uint16_t)link_layer_data[offset] | ((uint16_t)link_layer_data[offset + 1u] << 8u));
         offset += 2u;
         packet.header.pkt_counter
            = (uint16_t)((uint16_t)link_layer_data[offset] | ((uint16_t)link_layer_data[offset + 1u] << 8u));
         offset += 2u;
         packet.header.session_id
            = (uint32_t)((uint32_t)link_layer_data[offset] | ((uint32_t)link_layer_data[offset + 1u] << 8u)
                         | ((uint32_t)link_layer_data[offset + 2u] << 16u)
                         | ((uint32_t)link_layer_data[offset + 3u] << 24u));
         offset += 4u;
         packet.header.pkt_type = link_layer_data[offset];
         offset += 1u;
         packet.header.status = link_layer_data[offset];
         offset += 1u;

         // populate packet payload
         packet.payload.type = link_layer_data[offset];
         offset += 1u;
         packet.payload.ppi = link_layer_data[offset];
         offset += 1u;
         packet.payload.pkt_payload_len
            = (uint16_t)((uint16_t)link_layer_data[offset] | ((uint16_t)link_layer_data[offset + 1u] << 8u));
         offset += 2u;

         if(packet.payload.pkt_payload_len > self->_max_packet_payload_len)
         {
            SET_ERR(result, MSG_PROT_ERROR_OUT_OF_RANGE);
         }

         const uint16_t packet_length = (uint16_t)(min_packet_len + packet.payload.pkt_payload_len);
         if(IS_OK(result)
            && ((packet_length > MP_MAX_PACKET_LENGTH)
                || ((self->_max_packet_len > 0u) && (packet_length > self->_max_packet_len))))
         {
            SET_ERR(result, MSG_PROT_ERROR_BUFFER_OVERFLOW);
         }

         if(IS_OK(result) && (packet.payload.pkt_payload_len > 0u))
         {
            // Valid length, copy payload
            memcpy(packet.payload.payload, &link_layer_data[offset], packet.payload.pkt_payload_len);
         }

         if(IS_OK(result))
         {
            // Calculate received packet CRC
            uint8_t crc_offset = sizeof(packet.header.pkt_crc); // Skip pkt_crc field in mp_packet_header_t
            uint16_t calc_crc = crc16_update(&link_layer_data[crc_offset], packet_length - crc_offset, NULL);
            // Compare CRCs
            if(calc_crc != packet.header.pkt_crc)
            {
               DEBUG_WARNING("Corrupted frame payload CRC detected.");
               // Ignore packet so that the sender will timeout and retry.
            }
            else
            {
               if(is_self_tx_echo(self, link_layer_data, packet_length))
               {
                  // DEBUG_TRACE("Dropping self TX echo packet.");
               }
               else
               {
                  bool is_duplicate = false;

                  if(self->_last_rx_packet_valid && (packet_length == self->_last_rx_packet_len)
                     && (0 == memcmp(link_layer_data, self->_last_rx_packet_raw, packet_length)))
                  {
                     if(self->_last_rx_packet_deferred)
                     {
                        // Only ignore deferred duplicates while RX buffer is still busy.
                        if((uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED != self->_rx_packet.header.status)
                        {
                           is_duplicate = true;
                        }
                     }
                     else
                     {
                        is_duplicate = true;
                     }
                  }

                  if(is_duplicate)
                  {
                     if(MP_PACKET_TYPE_DATA == packet.header.pkt_type)
                     {
                        if(packet.header.session_id == self->_current_session_id)
                        {
                           // Re-ACK duplicate DATA packets to suppress sender retries for this active session.
                           result = send_ack_or_nak(self, &packet, ACK);
                        }
                        else
                        {
                           // Duplicate stale-session DATA indicates peer may have missed our prior SYNC_MISMATCH.
                           // Re-send SYNC_MISMATCH from slave to improve recovery from a lost control packet.
                           if(!self->_is_this_mp_instance_master)
                           {
                              mp_packet_t response_packet = {0};
                              memcpy(&response_packet.header, &packet.header, sizeof(mp_packet_header_t));
                              response_packet.header.session_id = self->_current_session_id; // Expected session ID.
                              response_packet.header.pkt_type = MP_PACKET_TYPE_SYNC_MISMATCH;
                              response_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
                              response_packet.payload.type = 0u;
                              response_packet.payload.ppi = 0u;
                              response_packet.payload.pkt_payload_len = 0u;
                              result = send_pkt_to_link_layer_cached(self, &response_packet);
                              DEBUG_INFO(" Re-sent SYNC_MISMATCH for duplicate stale-session DATA packet.");
                           }
                           else
                           {
                              DEBUG_WARNING(" Not ACKing duplicate stale-session DATA packet. Expected session ID: %u, "
                                            "Received: %u",
                                            self->_current_session_id,
                                            packet.header.session_id);
                           }
                        }
                     }
                  }
                  else
                  {
                     bool rx_busy
                        = ((MP_PACKET_TYPE_DATA == packet.header.pkt_type)
                           && ((uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED != self->_rx_packet.header.status));

                     // Valid packet received, process it
                     result = on_link_layer_packet(self, &packet);

                     // Cache raw packet bytes to suppress duplicates
                     memcpy(self->_last_rx_packet_raw, link_layer_data, packet_length);
                     self->_last_rx_packet_len = packet_length;
                     self->_last_rx_packet_valid = true;
                     self->_last_rx_packet_deferred = rx_busy;
                  }
               }
            }
         }
      }

      if(IS_ERR(result))
      {
         DEBUG_TRACE("Failed to parse inbound packet. Unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
   }

   // Nothing happens if result is ok but no data was received. Errors get reported as usual.
   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t process(const message_protocol_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MSG_PROT_ERROR_NULL_PTR);

   message_protocol_t *self = interface->parent;

   // Get the most up to date link layer MTU in case it has changed.
   result_t result = self->_get_max_link_layer_pkt_len(self, &self->_max_packet_len);
   uint16_t max_mp_payload_size
      = (uint16_t)(self->_max_packet_len - sizeof(mp_packet_header_t) - MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE);
   // Clamp max payload size if necessary
   if(max_mp_payload_size > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)
   {
      max_mp_payload_size = MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN;
   }

   self->_max_packet_payload_len = max_mp_payload_size;

   // Process inbound and outbound packets
   if(IS_OK(result))
   {
      result = process_inbound_packets(self);
      if(IS_ERR(result))
      {
         DEBUG_WARNING(
            "Message protocol process() failure. Unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
   }

   if(IS_OK(result))
   {
      result = process_outbound_packets(self);

      if(IS_ERR(result))
      {
         DEBUG_WARNING(
            "Message protocol process() failure. Unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
   }

   return result;
}

static result_t send(const message_protocol_interface_t *const interface, mp_packet_payload_t *pkt_payload)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(pkt_payload, MSG_PROT_ERROR_NULL_PTR);
   message_protocol_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(
      ((pkt_payload->pkt_payload_len + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE) > self->_max_packet_payload_len),
      MSG_PROT_ERROR_PAYLOAD_TOO_BIG);

   result_t result = RESULT_OK;
   if(self->_tx_packet.header.status != (uint8_t)MSG_PROT_TX_PACKET_STATUS_ABANDONED
      && self->_tx_packet.header.status != (uint8_t)MSG_PROT_TX_PACKET_STATUS_COMPLETED)
   {
      DEBUG_ERROR("Cannot send new packet while another is being processed.");
      SET_ERR(result, MSG_PROT_ERROR_BUSY);
   }

   if(self->_is_syncing)
   {
      DEBUG_ERROR("Cannot send new packet while syncing.");
      SET_ERR(result, MSG_PROT_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      // Set the new tx packet header fields
      self->_tx_packet.header.pkt_counter = self->_next_packet_id;
      self->_tx_packet.header.session_id = self->_current_session_id;
      self->_tx_packet.header.pkt_type = MP_PACKET_TYPE_DATA;
      self->_tx_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;

      // Copy payload
      self->_tx_packet.payload.type = pkt_payload->type;
      self->_tx_packet.payload.ppi = pkt_payload->ppi;
      self->_tx_packet.payload.pkt_payload_len = pkt_payload->pkt_payload_len;
      memcpy(self->_tx_packet.payload.payload, pkt_payload->payload, pkt_payload->pkt_payload_len);
   }

   return result;
}

static result_t get_rx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_RX_PACKET_STATUS *status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(status, MSG_PROT_ERROR_NULL_PTR);

   message_protocol_t *self = interface->parent;

   *status = (MSG_PROT_RX_PACKET_STATUS)(self->_rx_packet.header.status);

   return RESULT_OK;
}

static result_t get_tx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_TX_PACKET_STATUS *status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(status, MSG_PROT_ERROR_NULL_PTR);

   message_protocol_t *self = interface->parent;

   if(self->_is_syncing)
   {
      // If currently syncing, override status to indicate that the link is busy. This prevents new packets from
      // being sent while syncing is in progress.
      *status = MSG_PROT_TX_PACKET_STATUS_ERROR;
   }
   else
   {
      *status = (MSG_PROT_TX_PACKET_STATUS)(self->_tx_packet.header.status);
   }

   return RESULT_OK;
}

static result_t get_rx_packet(const message_protocol_interface_t *const interface, mp_packet_payload_t *rx_packet)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(rx_packet, MSG_PROT_ERROR_NULL_PTR);

   message_protocol_t *self = interface->parent;
   result_t result = RESULT_OK;

   if((uint8_t)MSG_PROT_RX_PACKET_STATUS_NEW != self->_rx_packet.header.status)
   {
      DEBUG_ERROR("No new RX packet available");
      SET_ERR(result, MSG_PROT_ERROR_NO_NEW_PACKET);
   }

   if(IS_OK(result))
   {
      // Copy the RX packet data to the provided pointer
      rx_packet->type = self->_rx_packet.payload.type;
      rx_packet->ppi = self->_rx_packet.payload.ppi;
      rx_packet->pkt_payload_len = self->_rx_packet.payload.pkt_payload_len;
      memcpy(rx_packet->payload, self->_rx_packet.payload.payload, self->_rx_packet.payload.pkt_payload_len);

      // Mark internal RX packet as processed
      self->_rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED;
   }

   return result;
}

static result_t get_max_payload_length(const message_protocol_interface_t *const interface, uint16_t *max_payload_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(max_payload_len, MSG_PROT_ERROR_NULL_PTR);

   message_protocol_t *self = interface->parent;

   *max_payload_len = self->_max_packet_payload_len;

   return RESULT_OK;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t message_protocol_init(message_protocol_t *const self,
                               const system_time_interface_t *systick_ifc,
                               const comms_driver_interface_t *serial_link_ifc,
                               bool is_master,
                               uint32_t ack_timeout_ms,
                               get_link_layer_pkt_cb_t get_link_layer_pkt_cb_opt, // These callbacks can and should
                                                                                  // be NULL during normal operation.
                               send_pkt_to_link_layer_cb_t send_pkt_to_link_layer_cb_opt,
                               get_max_packet_length_cb_t get_max_packet_length_cb_opt,
                               generate_mp_session_id_cb_t generate_mp_session_id_cb_opt)
{
   // Example implementation of message_protocol_init below
   RETURN_ERR_IF_NULL(self, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(systick_ifc, MSG_PROT_ERROR_NULL_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(serial_link_ifc, MSG_PROT_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   self->_initialized = false;
   self->interface.parent = self;

   self->_systick_ifc = systick_ifc;
   self->_ack_timeout_ms = ack_timeout_ms;

   self->_is_this_mp_instance_master = is_master;
   self->_is_syncing = false;
   self->_has_attempted_sync = false;
   self->_last_resync_time_ms = 0u;

   self->_tx_packet = (mp_packet_t){0};
   self->_tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_COMPLETED; // No pending TX packet
   self->_rx_packet = (mp_packet_t){0};
   self->_rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED; // No pending RX packet
   self->_serial_link_ifc = serial_link_ifc;

   memset(self->_last_rx_packet_raw, 0u, sizeof(self->_last_rx_packet_raw));
   self->_last_rx_packet_len = 0u;
   self->_last_rx_packet_valid = false;
   self->_last_rx_packet_deferred = false;

   memset(self->_last_tx_packet_raw, 0u, sizeof(self->_last_tx_packet_raw));
   self->_last_tx_packet_len = 0u;
   self->_last_tx_packet_valid = false;

   self->interface.send = send;
   self->interface.process = process;
   self->interface.get_rx_packet_status = get_rx_packet_status;
   self->interface.get_tx_packet_status = get_tx_packet_status;
   self->interface.get_rx_packet = get_rx_packet;
   self->interface.get_max_payload_length = get_max_payload_length;

   self->_next_packet_id = 0u;
   self->_pending_id = 0u;
   self->_retries_left = 0u;
   self->_deadline_ms = 0u;
   self->_current_session_id = 0u;

   memset(&self->_last_packet_sent, 0u, sizeof(mp_packet_t));

   // Test dependency injection
   self->_get_link_layer_pkt = (NULL == get_link_layer_pkt_cb_opt) ? get_link_layer_pkt : get_link_layer_pkt_cb_opt;
   self->_send_pkt_to_link_layer
      = (NULL == send_pkt_to_link_layer_cb_opt) ? send_pkt_to_link_layer : send_pkt_to_link_layer_cb_opt;
   self->_get_max_link_layer_pkt_len
      = (NULL == get_max_packet_length_cb_opt) ? get_max_link_layer_pkt_len : get_max_packet_length_cb_opt;

   self->_generate_mp_session_id
      = (NULL == generate_mp_session_id_cb_opt) ? generate_session_id : generate_mp_session_id_cb_opt;

   self->_initialized = true;

   return result;
}
