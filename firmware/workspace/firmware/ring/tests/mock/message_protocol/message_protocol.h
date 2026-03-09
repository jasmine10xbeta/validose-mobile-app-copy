/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup message_protocol Mock Message Protocol
 * @brief Mock of a Generic, reliable messaging layer
 *
 * @file mock_message_protocol.h
 * @ingroup mock_message_protocol

 */

#ifndef MODULE_NAME_H_
#define MODULE_NAME_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"

#include "message_protocol_interface.h"
#include "system_time.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MESSAGE_PROTOCOL_MAX_FRAME_LEN                                                                                 \
   (sizeof(link_layer_header_t) + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN + sizeof(link_layer_trailer_t))

#define MSG_PROT_MAX_RETRIES (20u) // Number of resends before failure

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct message_protocol message_protocol_t;

typedef struct message_protocol
{
   message_protocol_interface_t interface;

   mp_packet_payload_t _tx_packet; /**< Current Tx Packet */
   mp_packet_payload_t _rx_packet; /**< Current Rx Packet */

   MSG_PROT_RX_PACKET_STATUS _rx_packet_status; /**< Status of current RX packet */
   MSG_PROT_TX_PACKET_STATUS _tx_packet_status; /**< Status of current TX packet */

   result_t _result;

} message_protocol_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t message_protocol_init(message_protocol_t *const self);

#endif // MODULE_NAME_H_
