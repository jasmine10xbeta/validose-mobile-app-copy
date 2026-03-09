/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup serial_link_protocol Link Layer Protocol
 * @ingroup common
 * @brief Implements the link layer protocol for communication between devices over a serial channel.
 * @details
 *
 * @file serial_link_protocol.h
 * @ingroup serial_link_protocol
 * @brief
 */

#ifndef SERIAL_LINK_PROTOCOL_H_
#define SERIAL_LINK_PROTOCOL_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "serial_link_protocol_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Maximum frame length we accept (header + payload + trailer). */
#ifndef SERIAL_LINK_LAYER_MAX_FRAME
#   define SERIAL_LINK_LAYER_MAX_FRAME (256u)
#endif

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/* typedef enum
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
} SERIALIZER_ERROR; */

/**
 * @brief Parser internal states.
 */
typedef enum
{
   PARSER_STATE_SEARCH_SOH,
   PARSER_STATE_READ_HEADER,
   PARSER_STATE_READ_PAYLOAD,
} parser_state_t;

typedef uint16_t (*crc_callback)(const uint8_t *const data, uint16_t data_length);

typedef struct serial_link_protocol_serializer serial_link_protocol_serializer_t;

struct serial_link_protocol_serializer
{
   serializer_interface_t interface;
   crc_callback _crc_callback;

   parser_state_t _parser_state;
   uint8_t _header_buf[HEADER_LENGTH]; /**< Buffer in which the header is stored.*/
   size_t _header_index;

   uint16_t _expected_payload_len;
   uint16_t _payload_index;
   uint8_t _frame_buf[SERIAL_LINK_LAYER_MAX_FRAME]; /**< Holds entire frame until done */

   void (*set_crc_callback)(serial_link_protocol_serializer_t *const self, crc_callback callback);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t serial_link_protocol_serializer_init(serial_link_protocol_serializer_t *const self);
#endif // SERIAL_LINK_PROTOCOL_H_