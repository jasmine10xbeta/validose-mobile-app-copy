/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file serial_link_protocol_interface.h
 * @ingroup link_layer_protocol
 * @brief
 */

#ifndef SERIAL_LINK_PROTOCOL_INTERFACE_H_
#define SERIAL_LINK_PROTOCOL_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdint.h>
// Custom includes
#include "common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define SOH (0x01)
#define EOT (0x04)

#define HEADER_LENGTH                                                                                                  \
   ((uint16_t)sizeof(link_layer_header_t)) // ## Temp: 9 bytes including SOH and CRC. Last 2 bytes are CRC.
#define TRAILER_LENGTH ((uint16_t)sizeof(link_layer_trailer_t)) // ## Temp: 3 bytes CRC + EOT.

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef enum
{
   LINK_LAYER_PACKET_TYPE_DATA = 0x00,
   LINK_LAYER_PACKET_TYPE_ACK = 0x01,
   LINK_LAYER_PACKET_TYPE_NAK = 0x02,
   LINK_LAYER_PACKET_TYPE_MAX = 0x03
} LINK_LAYER_PACKET_TYPE;

typedef enum
{
   SERIALIZER_ERROR_NONE = 0,
   SERIALIZER_ERROR_NULL_PTR,
   SERIALIZER_ERROR_NULL_INTERFACE,
   SERIALIZER_ERROR_PAYLOAD_BUFFER_TOO_SMALL,
   SERIALIZER_ERROR_NO_PACKET,
   SERIALIZER_ERROR_NO_FRAME,
   SERIALIZER_ERROR_CORRUPTED_PAYLOAD_EOT,
   SERIALIZER_ERROR_CORRUPTED_PAYLOAD_CRC,
   SERIALIZER_ERROR_SWITCH_DEFAULT,
   SERIALIZER_ERROR_MAX
} SERIALIZER_ERROR;

typedef struct __attribute__((packed))
{
   uint8_t soh;         /**< Start of header character */
   uint8_t packet_type; /**< Data, Ack, Nak */
   uint16_t packet_id;
   uint16_t payload_length;
   uint8_t payload_protocol_identifier; /**< PPI contains which higher-layer payload the data belongs to once it is
                                           delivered. */
   uint16_t header_crc;
} link_layer_header_t;

typedef struct __attribute__((packed))
{
   uint16_t payload_crc;
   uint8_t eot; /**< End of trailer character */
} link_layer_trailer_t;

typedef struct
{
   LINK_LAYER_PACKET_TYPE packet_type;
   uint16_t packet_id;
   uint8_t payload_protocol_identifier;
} link_layer_header_data_t;

struct serial_link_protocol_serializer;                     // Forward declaration
typedef struct serializer_interface serializer_interface_t; // Forward declaration

typedef struct serializer_interface
{
   struct serial_link_protocol_serializer *parent; // Reference to the containing instance.

   /**
    * @brief Serializes the given header and payload into a byte stream according to the serial link protocol.
    *
    * @param interface The serializer interface.
    * @param header_data The header data to be serialized.
    * @param payload The payload data to be serialized.
    * @param payload_length The length of the payload data.
    * @param output_buffer The buffer to store the serialized data.
    * @param output_buffer_size The size of the output buffer.
    * @param output_length The length of the serialized data.
    *
    * @return RESULT_OK if the serialization is successful.
    */
   result_t (*serialize)(const serializer_interface_t *const interface,
                         const link_layer_header_data_t *const header_data,
                         const uint8_t *const payload,
                         uint16_t payload_length,
                         uint8_t *const output_buffer,
                         uint16_t output_buffer_size,
                         uint16_t *const output_length);

   /**
    * @brief Processes incoming data to extract a complete frame.
    *
    * This function processes incoming data to extract a complete frame from a stream of bytes.
    * It checks for a valid start-of-header (SOH) byte, validates the header CRC, extracts the payload,
    * validates the payload CRC, and checks for a valid end-of-transmission (EOT) byte.
    *
    * It has an internal state keeping track of the current state of the frame processing, meaning it will detect frames
    * that are split across multiple calls.
    *
    * @param interface Pointer to the serializer interface.
    * @param data Pointer to the incoming data.
    * @param data_length Length of the incoming data.
    * @param header_data Pointer to the extracted header data.
    * @param payload_buffer Pointer to the buffer to store the extracted payload.
    * @param payload_buffer_size Size of the payload buffer.
    * @param payload_length Pointer to store the length of the extracted payload.
    * @param soh_position This contains the position of the SOH found in the received @p data. @p soh_position equals
    * zero if no SOH character was found.
    *
    * @return Returns RESULT_OK if a complete frame is extracted successfully, or an appropriate error code otherwise.
    * @note The function will return SERIALIZER_ERROR_NO_FRAME if no complete frame was detected. This is NOT an
    * error condition.
    */
   result_t (*parser_process)(const serializer_interface_t *const interface,
                              const uint8_t *const data,
                              const uint16_t data_length,
                              link_layer_header_data_t *const header_data,
                              uint8_t *const payload_buffer,
                              uint8_t payload_buffer_size,
                              uint16_t *payload_length,
                              uint16_t *soh_position);

   /**
    * @brief  Return the parser to its power-on state.
    *
    * All index counters are cleared and the state machine goes back to
    * SEARCH_SOH, ready to hunt for the next start-of-header byte.
    *
    * NOTE: We do **not** need to erase @p header_buf or @p frame_buf; their
    *       contents will be overwritten naturally as new bytes arrive.
    */
   result_t (*parser_reset)(const serializer_interface_t *const interface);

} serializer_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // SERIAL_LINK_PROTOCOL_INTERFACE_H_