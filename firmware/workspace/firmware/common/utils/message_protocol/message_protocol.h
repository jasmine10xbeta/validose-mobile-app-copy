/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup message_protocol Message Protocol
 * @ingroup common
 * @brief Generic, reliable messaging layer built on top of the link‑layer
 * protocol.
 * @details The unit provides:
 * - Asynchronous send with automatic retransmission and ACK/NAK handling.
 * - Configurable time‑outs and retry counters per message.
 * - polling‑based reception API.
 * - Packet IDs. Note that this unit does not enforce packet ID sequencing. It attaches packet IDs used only for
 * managing ACKs and NAKs. E.g. This unit will not check if a received packet ID is the previous packet ID + 1. It will
 * only check respond or check responses in terms of ACKs and NAKs with the same ID to indicate reception status for
 * given packets. It is therefore up to the higher level units to manage sequential packet numbers for larger datasets
 * that span multiple packets.
 *
 * The implementation is transport‑agnostic; it relies on an abstract link‑layer
 * interface as well as abstract FIFO queues supplied by the application.
 *
 * @file message_protocol.h
 * @ingroup message_protocol
 * @brief
 */

#ifndef MODULE_NAME_H_
#define MODULE_NAME_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
#include "comms_driver_interface.h"
#include "message_protocol_interface.h"
#include "queue.h"
#include "system_time.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// Max size for a message protocol packet. Link layer MTUs can be smaller, this is just the absolute max for the MP.
#define MP_MAX_PACKET_LENGTH                                                                                           \
   (sizeof(mp_packet_header_t) + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)

#define MSG_PROT_MAX_RETRIES          (20u) // Number of resends before failure
#define MAX_TIME_BEFORE_SYNC_RETRY_MS (1000u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef enum
{
   MP_PACKET_TYPE_DATA = 0x00,
   MP_PACKET_TYPE_ACK,
   MP_PACKET_TYPE_NAK,
   MP_PACKET_TYPE_SYNC_START,
   MP_PACKET_TYPE_SYNC_ACK,
   MP_PACKET_TYPE_SYNC_MISMATCH,
   MP_PACKET_TYPE_MAX
} MP_PACKET_TYPE;

typedef struct
{
   uint16_t pkt_crc;     /**< CRC of the entire packet including header and payload */
   uint16_t pkt_counter; /**< Packet counter for tracking lost packets */
   uint32_t session_id;  /**< Session identifier */
   uint8_t pkt_type;     /**< Type of packet - see MP_PACKET_TYPE enum */
   uint8_t status;       /**< Status of the packet - Refer to MSG_PROT_PACKET_STATUS enum */
} mp_packet_header_t;

typedef struct
{
   mp_packet_header_t header;
   mp_packet_payload_t payload;
} mp_packet_t;

typedef struct message_protocol message_protocol_t;

typedef result_t (*get_link_layer_pkt_cb_t)(const message_protocol_t *const interface,
                                            uint8_t *data,
                                            uint16_t data_buffer_size);
typedef result_t (*send_pkt_to_link_layer_cb_t)(const message_protocol_t *const interface, const mp_packet_t *packet);
typedef result_t (*get_max_packet_length_cb_t)(const message_protocol_t *const interface, uint16_t *max_packet_len);
typedef result_t (*generate_mp_session_id_cb_t)(const message_protocol_t *self, uint32_t *session_id);
// typedef result_t (*serialize_data_frame_cb_t)(message_protocol_t *self, message_protocol_packet_t *packet);

typedef struct message_protocol
{
   message_protocol_interface_t interface;
   const system_time_interface_t *_systick_ifc;
   const comms_driver_interface_t *_serial_link_ifc;

   uint16_t _next_packet_id; /**< Next packet to be sent to the LL */
   uint16_t _pending_id;     /**< ID of the message previously sent over the LL, awaiting response */
   uint64_t _deadline_ms;    /**< System time by which the previously sent message will timeout. */
   uint32_t _ack_timeout_ms; /**< ACK wait timeout configured for this instance. */
   uint8_t _retries_left;

   // uint8_t _last_frame[MESSAGE_PROTOCOL_MAX_FRAME_LEN];
   // uint16_t _last_len;

   mp_packet_t _last_packet_sent; /**< Last packet sent over the link layer, used for retransmissions */

   mp_packet_t _tx_packet; /**< Current Tx Packet */
   mp_packet_t _rx_packet; /**< Current Rx Packet */

   // Duplicate RX suppression (raw on-wire packet bytes)
   uint8_t _last_rx_packet_raw[MP_MAX_PACKET_LENGTH];
   uint16_t _last_rx_packet_len;
   bool _last_rx_packet_valid;
   bool _last_rx_packet_deferred; // True if last packet was DATA while RX buffer busy

   // Last TX packet bytes (raw on-wire) for mailbox echo suppression
   uint8_t _last_tx_packet_raw[MP_MAX_PACKET_LENGTH];
   uint16_t _last_tx_packet_len;
   bool _last_tx_packet_valid;

   // Sync management
   uint32_t _current_session_id;     // Session identifier
   bool _is_this_mp_instance_master; // Whether this instance is master or slave
   bool _is_syncing;                 // Whether a sync process is ongoing
   bool _has_attempted_sync;         // True after first SYNC_START attempt; bypasses startup throttle once.
   uint64_t _last_resync_time_ms;    // Last time a resync was attempted

   uint16_t _max_packet_payload_len; // Max size for payload member of mp_packet_payload_t
   uint16_t _max_packet_len;         // Max size for mp_packet_t

   // Callback for testing purposes only
   get_link_layer_pkt_cb_t _get_link_layer_pkt;         // Function to get serial data from the serial link interface.
   send_pkt_to_link_layer_cb_t _send_pkt_to_link_layer; // Function to send serial data to serial link interface.
   get_max_packet_length_cb_t _get_max_link_layer_pkt_len;
   generate_mp_session_id_cb_t _generate_mp_session_id;
   // serialize_data_frame_cb_t _serialize_data_frame_func; // Function to send data to serializer
   //   parse_data_cb_t _parse_data_func;                     // Function to parse the data from the serial link
   //   protocol

   bool _initialized;
} message_protocol_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief
 *
 * @param self
 * @param systick_ifc
 * @param serializer_ifc
 * @param serial_link_ifc Interface for the physical serial link driver.
 * @param ack_timeout_ms Timeout in ms to wait for an ACK before retransmitting.
 * @param get_link_layer_pkt_cb_opt OPTIONAL. Only to be used during testing,
 * otherwise pass NULL.
 * @param send_pkt_to_link_layer_cb_opt OPTIONAL. Only to be used during testing,
 * otherwise pass NULL.
 *
 * @return result_t
 */
result_t message_protocol_init(message_protocol_t *const self,
                               const system_time_interface_t *systick_ifc,
                               const comms_driver_interface_t *serial_link_ifc,
                               bool is_master,
                               uint32_t ack_timeout_ms,
                               get_link_layer_pkt_cb_t get_link_layer_pkt_cb_opt,
                               send_pkt_to_link_layer_cb_t send_pkt_to_link_layer_cb_opt,
                               get_max_packet_length_cb_t get_max_packet_length_cb_opt,
                               generate_mp_session_id_cb_t generate_mp_session_id_cb_opt);

#endif // MODULE_NAME_H_
