/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file serial_link_protocol.c
 * @ingroup link_layer_protocol
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "serial_link_protocol.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_SERIALIZER_PROTOCOL;

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
static result_t serialize(const serializer_interface_t *const interface,
                          const link_layer_header_data_t *const header_data,
                          const uint8_t *const payload,
                          uint16_t payload_length,
                          uint8_t *const output_buffer,
                          uint16_t output_buffer_size,
                          uint16_t *const output_length);

static result_t parser_process(const serializer_interface_t *const interface,
                               const uint8_t *const data,
                               const uint16_t data_length,
                               link_layer_header_data_t *const header_data,
                               uint8_t *const payload_buffer,
                               uint8_t payload_buffer_size,
                               uint16_t *payload_length,
                               uint16_t *soh_position);

static result_t parser_reset(const serializer_interface_t *const interface);

// Non-interface functions
static void set_crc_callback(serial_link_protocol_serializer_t *const self, crc_callback callback);

/**
 * @brief Calculates the CRC-16 Kermit checksum for a given data buffer.
 *
 * This function calculates the CRC-16 Kermit checksum for the provided data buffer.
 * The CRC-16 Kermit algorithm is a widely used error-detecting code.
 *
 * Based on https://www.experts-exchange.com/questions/28271830/How-To-Calculate-CRC16-Using-CRC-CCITT-Kermit.html
 *
 * @param data Pointer to the data buffer for which the CRC-16 Kermit checksum needs to be calculated.
 * @param data_length Length of the data buffer in bytes.
 *
 * @return The calculated CRC-16 Kermit checksum for the provided data buffer.
 */
static uint16_t kermit_crc16(const uint8_t *const data, uint16_t data_length);
static void uint16_to_bytes_little_endian(uint16_t value, void *bytes);
static uint16_t bytes_to_uint16_little_endian(const void *data);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static void uint16_to_bytes_little_endian(uint16_t value, void *bytes)
{
   RETURN_VOID_IF_NULL(bytes);

   ((uint8_t *)bytes)[0] = (uint8_t)(value & 0xff);
   ((uint8_t *)bytes)[1] = (uint8_t)(value >> 8) & 0xff;
}

static uint16_t bytes_to_uint16_little_endian(const void *data)
{
   if(NULL == data)
   {
      return 0;
   }

   const uint8_t *bytes = (const uint8_t *)data;

   return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void set_crc_callback(serial_link_protocol_serializer_t *const self, crc_callback callback)
{
   self->_crc_callback = callback;
}

static uint16_t kermit_crc16(const uint8_t *const data, uint16_t data_length)
{
   RETURN_VALUE_IF_NULL(data, 0);

   uint8_t bit = 0;
   uint8_t carry = 0;

   static const uint16_t polynomial = 0x8408; // reversed 0x1021
   uint16_t crc = 0;

   for(uint16_t count = 0; count < data_length; count++)
   {
      crc = (uint16_t)(crc ^ (uint16_t)data[count]);
      for(bit = 0; bit < 8; bit++)
      {
         carry = crc & 1;
         crc >>= 1;
         if(carry)
         {
            crc ^= polynomial;
         }
      }
   }

   return crc;
}
/***********************************************************************************************************************
 * Static interface function definitions
 *********************************************************************************************************************/
static result_t parser_reset(const serializer_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SERIALIZER_ERROR_NULL_INTERFACE);

   serial_link_protocol_serializer_t *self = interface->parent;

   self->_parser_state = PARSER_STATE_SEARCH_SOH;
   self->_header_index = 0U;
   self->_payload_index = 0U;
   self->_expected_payload_len = 0U;

   return RESULT_OK;
}

static result_t parser_process(const serializer_interface_t *const interface,
                               const uint8_t *const data,
                               const uint16_t data_length,
                               link_layer_header_data_t *const header_data,
                               uint8_t *const payload_buffer,
                               uint8_t payload_buffer_size,
                               uint16_t *payload_length,
                               uint16_t *soh_position)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SERIALIZER_ERROR_NULL_INTERFACE);
   RETURN_ERR_IF_NULL(data, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(header_data, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(payload_buffer, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(payload_length, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(soh_position, SERIALIZER_ERROR_NULL_PTR);

   result_t result = RESULT_OK;

   bool first_retry = true;

   serial_link_protocol_serializer_t *self = interface->parent;

   *soh_position = 0;

   // Loop through all the incoming bytes
   for(uint16_t idx = 0; idx < data_length; ++idx)
   {
      uint8_t incoming = data[idx]; // Check every byte

      switch(self->_parser_state)
      {
         case PARSER_STATE_SEARCH_SOH: // Waiting for start of header
            if(SOH == incoming)
            {
               self->_header_buf[0] = incoming;
               self->_header_index = 1;
               self->_parser_state = PARSER_STATE_READ_HEADER;
               *soh_position = idx;
            }
            break;

         case PARSER_STATE_READ_HEADER: // Collect bytes required for header

            //  1. Collect the next byte into the running header buffer
            self->_header_buf[self->_header_index] = incoming;
            self->_header_index++;
            // 2. If we still have fewer than HEADER_LENGTH bytes, wait for more data
            if(self->_header_index < HEADER_LENGTH)
            {
               break;
            }

            /** 3. Ensure the byte at position-0 is SOH.
             * If it is not, shrink the window left until either:
             *  - SOH is now at position-0, or
             *  - we have <HEADER_LENGTH bytes left, in which case we exit and wait for more data before computing CRC
             * again. */
            while((self->_header_index >= HEADER_LENGTH) && (self->_header_buf[0] != SOH))
            {
               self->_header_index -= 1;
               memmove(self->_header_buf, self->_header_buf + 1, self->_header_index);
            }

            // Not enough bytes to hold a full header after shrinking?
            if(self->_header_index < HEADER_LENGTH)
            {
               break; // Back to start of loop
            }

            // 4. From here: header_buf[0] == SOH  &&  header_index == HEADER_LENGTH
            // Parse received header
            link_layer_header_t received_header = {
               .soh = self->_header_buf[0],
               .packet_type = self->_header_buf[1],
               .packet_id = (uint16_t)((uint16_t)(self->_header_buf[2]) | (uint16_t)(self->_header_buf[3] << 8)),
               .payload_length = (uint16_t)((uint16_t)(self->_header_buf[4]) | (uint16_t)(self->_header_buf[5] << 8)),
               .payload_protocol_identifier = self->_header_buf[6],
               .header_crc = (uint16_t)((uint16_t)(self->_header_buf[7]) | (uint16_t)(self->_header_buf[8] << 8))};

            // Calculate CRC
            uint16_t calc_crc = kermit_crc16(self->_header_buf, HEADER_LENGTH - sizeof(uint16_t));

            if(calc_crc != received_header.header_crc)
            {
               //  CRC fail → slide entire 9-byte window by ONE byte and stay in READ_HEADER to re-attempt.
               memmove(self->_header_buf, self->_header_buf + 1, HEADER_LENGTH - 1);
               self->_header_index = HEADER_LENGTH - 1;
               break;
            }

            // 5. Header is valid.  Extract payload length and sanity check.
            // CRC good → extract payload length and sanity-check
            self->_expected_payload_len = bytes_to_uint16_little_endian(&received_header.payload_length);

            if((uint16_t)(HEADER_LENGTH + self->_expected_payload_len + TRAILER_LENGTH) > SERIAL_LINK_LAYER_MAX_FRAME)
            {
               SET_ERR(result, SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL);
               (void)parser_reset(
                  &self->interface); // Ignore result to prioritize SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL result
            }

            //  6. Copy verified header into the full-frame buffer so the payload stage can append data directly.
            if(IS_OK(result))
            {
               memcpy(self->_frame_buf, self->_header_buf, HEADER_LENGTH);
               self->_payload_index = 0;
               self->_parser_state = PARSER_STATE_READ_PAYLOAD;
            }

            break;

         case PARSER_STATE_READ_PAYLOAD: // Payload + trailer
         {
            size_t frame_offset = HEADER_LENGTH + self->_payload_index;
            self->_frame_buf[frame_offset] = incoming;
            self->_payload_index++;

            // All bytes of payload + trailer received?
            if(self->_payload_index == (self->_expected_payload_len + TRAILER_LENGTH))
            {
               // Trailer pointer inside frame_buf
               size_t trailer_offset = HEADER_LENGTH + self->_expected_payload_len;

               // Parse received trailer
               link_layer_trailer_t received_trailer
                  = {.payload_crc = (uint16_t)((uint16_t)(self->_frame_buf[trailer_offset])
                                               | (uint16_t)(self->_frame_buf[trailer_offset + 1] << 8)),
                     .eot = self->_frame_buf[trailer_offset + 2]};

               // Parse received header
               link_layer_header_t received_header = {
                  .soh = self->_frame_buf[0],
                  .packet_type = self->_frame_buf[1],
                  .packet_id = (uint16_t)((uint16_t)(self->_frame_buf[2]) | (uint16_t)(self->_frame_buf[3] << 8)),
                  .payload_length = (uint16_t)((uint16_t)(self->_frame_buf[4]) | (uint16_t)(self->_frame_buf[5] << 8)),
                  .payload_protocol_identifier = self->_frame_buf[6],
                  .header_crc = (uint16_t)((uint16_t)(self->_frame_buf[7]) | (uint16_t)(self->_frame_buf[8] << 8))};

               // Valid Header, so we always fill out header_data
               header_data->packet_id = bytes_to_uint16_little_endian(&received_header.packet_id);
               header_data->payload_protocol_identifier = received_header.payload_protocol_identifier;

               // Check EOT byte
               if(received_trailer.eot != EOT)
               {
                  result = parser_reset(&self->interface);

                  if(IS_OK(result) && first_retry)
                  {
                     idx = 0;
                     first_retry = false;
                     // continue; // Todo, refactor this continue out
                  }
                  else
                  {
                     SET_ERR(result, SERIALIZER_ERROR_CORRUPTED_PAYLOAD_EOT);
                     header_data->packet_type = LINK_LAYER_PACKET_TYPE_NAK;
                  }
               }

               // Verify payload CRC
               if(first_retry)
               {
                  if(IS_OK(result))
                  {
                     uint16_t crc_calc = kermit_crc16(&self->_frame_buf[HEADER_LENGTH], self->_expected_payload_len);
                     if(crc_calc != bytes_to_uint16_little_endian(&received_trailer.payload_crc))
                     {
                        (void)parser_reset(&self->interface); // Ignore result to prioritize
                                                              // SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL result
                        SET_ERR(result, SERIALIZER_ERROR_CORRUPTED_PAYLOAD_CRC);
                        header_data->packet_type = LINK_LAYER_PACKET_TYPE_NAK;
                     }
                  }
                  if(IS_OK(result))
                  {
                     header_data->packet_type = received_header.packet_type;

                     if(self->_expected_payload_len > payload_buffer_size)
                     {
                        (void)parser_reset(&self->interface); // Ignore result to prioritize
                                                              // SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL result
                        SET_ERR(result, SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL);
                        header_data->packet_type = LINK_LAYER_PACKET_TYPE_NAK;
                     }
                  }
                  if(IS_OK(result))
                  {
                     memcpy(payload_buffer, &self->_frame_buf[HEADER_LENGTH], self->_expected_payload_len);
                     if(payload_length)
                     {
                        *payload_length = self->_expected_payload_len;
                     }
                     result = parser_reset(&self->interface);
                  }
               }

               if(IS_OK(result))
               {
                  header_data->packet_type = received_header.packet_type;
               }
               else
               {
                  // On any error, send NAK
                  header_data->packet_type = LINK_LAYER_PACKET_TYPE_NAK;
               }

               return result; // Todo: refactor out this early return.
            }
         }
         break;
         default:
         {
            SET_ERR(result, SERIALIZER_ERROR_SWITCH_DEFAULT);
         }
      }
   }
   if(IS_OK(result))
   {
      RETURN_ERR(SERIALIZER_ERROR_NO_FRAME);
   }
   else
   {
      return result;
   }
}

static result_t serialize(const serializer_interface_t *const interface,
                          const link_layer_header_data_t *const header_data,
                          const uint8_t *const payload,
                          uint16_t payload_length,
                          uint8_t *const output_buffer,
                          uint16_t output_buffer_size,
                          uint16_t *const output_length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SERIALIZER_ERROR_NULL_INTERFACE);
   RETURN_ERR_IF_NULL(header_data, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(payload, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(output_buffer, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(output_length, SERIALIZER_ERROR_NULL_PTR);
   RETURN_ERR_IF_TRUE((sizeof(link_layer_header_t) + sizeof(link_layer_trailer_t) + payload_length)
                         > output_buffer_size,
                      SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL);

   result_t result = RESULT_OK;

   const serial_link_protocol_serializer_t *self = interface->parent;

   link_layer_header_t header;
   header.soh = SOH;
   header.packet_type = (uint8_t)header_data->packet_type;
   uint16_to_bytes_little_endian(header_data->packet_id, &header.packet_id);

   if(LINK_LAYER_PACKET_TYPE_DATA == header_data->packet_type)
   {
      uint16_to_bytes_little_endian(payload_length, &header.payload_length);
   }
   else
   {
      header.payload_length = 0;
   }

   header.payload_protocol_identifier = header_data->payload_protocol_identifier;
   uint16_t header_crc = self->_crc_callback((uint8_t *)&header, HEADER_LENGTH - sizeof(uint16_t));
   uint16_to_bytes_little_endian(header_crc, &header.header_crc);

   memcpy(output_buffer, &header, HEADER_LENGTH);
   *output_length = HEADER_LENGTH;

   if(LINK_LAYER_PACKET_TYPE_DATA == header_data->packet_type)
   {
      memcpy(&output_buffer[*output_length], payload, payload_length);
      *output_length += payload_length;
   }

   {
      link_layer_trailer_t trailer;
      trailer.payload_crc = 0;
      trailer.eot = EOT;

      if(LINK_LAYER_PACKET_TYPE_DATA == header_data->packet_type)
      {
         uint16_t crc = self->_crc_callback(payload, payload_length);
         uint16_to_bytes_little_endian(crc, &trailer.payload_crc);
      }

      memcpy(&output_buffer[*output_length], &trailer, TRAILER_LENGTH);
      *output_length += TRAILER_LENGTH;
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t serial_link_protocol_serializer_init(serial_link_protocol_serializer_t *const self)
{
   RETURN_ERR_IF_NULL(self, SERIALIZER_ERROR_NULL_PTR);

   self->interface.parent = self;

   self->interface.serialize = serialize;
   self->interface.parser_process = parser_process;
   self->interface.parser_reset = parser_reset;

   self->set_crc_callback = set_crc_callback;
   self->_crc_callback = kermit_crc16;

   result_t result = parser_reset(&self->interface);

   return result;
}
