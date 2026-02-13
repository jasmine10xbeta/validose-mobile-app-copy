/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file message_protocol_interface.h
 * @ingroup message_protocol
 * @brief
 *
 * @code
 * message_protocol_interface_t *m_msg_prot_ifc;
 * mp_packet_payload_t tx_packet;
 * mp_packet_payload_t rx_packet;
 * MSG_PROT_TX_PACKET_STATUS tx_status;
 * MSG_PROT_RX_PACKET_STATUS rx_status;
 *
 * // Check if TX is ready before sending
 * if (RESULT_OK == m_msg_prot_ifc->get_tx_packet_status(m_msg_prot_ifc, &tx_status))
 * {
 *     if (MSG_PROT_TX_PACKET_STATUS_COMPLETED == tx_status)
 *     {
 *         // Populate tx_packet fields here
 *         result = m_msg_prot_ifc->send(m_msg_prot_ifc, &tx_packet);
 *     }
 * }
 *
 * // Call process frequently (e.g., in main loop or timer)
 * result = m_msg_prot_ifc->process(m_msg_prot_ifc);
 *
 * // Check for new RX packet
 * if (RESULT_OK == m_msg_prot_ifc->get_rx_packet_status(m_msg_prot_ifc, &rx_status))
 * {
 *     if (MSG_PROT_RX_PACKET_STATUS_NEW == rx_status)
 *     {
 *         if (RESULT_OK == m_msg_prot_ifc->get_rx_packet(m_msg_prot_ifc, &rx_packet))
 *         {
 *             // Handle rx_packet here
 *         }
 *     }
 * }
 * @endcode
 */

/**
 * @todo Update all documentation.
 */

#ifndef MESSAGE_PROTOCOL_INTERFACE_H_
#define MESSAGE_PROTOCOL_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <app_util.h>

// Custom includes
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN                                                                               \
   (256u) // Arbitrary limit for max payload size. Can be adjusted as needed depending on the type of link layers to be
          // supported. This refers to the variable payload size within the @ref mp_packet_payload_t packet, excluding
          // the fixed size elements in mp_packet_payload_t.
#define ACK_TIMEOUT_MS                       (500u) // Time to wait before retry
#define MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS (0u)
#define MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE                                                                       \
   (1u + 1u + 2u) // Minimum payload size if pkt_payload_len = 0 in mp_packet_payload_t

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

// TX Packet status enum
typedef enum
{
   MSG_PROT_TX_PACKET_STATUS_NONE = 0,
   MSG_PROT_TX_PACKET_STATUS_NEW,
   MSG_PROT_TX_PACKET_STATUS_COMPLETED,
   MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK,
   MSG_PROT_TX_PACKET_STATUS_ERROR,
   MSG_PROT_TX_PACKET_STATUS_ABANDONED,
   MSG_PROT_TX_PACKET_STATUS_MAX,
} MSG_PROT_TX_PACKET_STATUS;
STATIC_ASSERT(MSG_PROT_TX_PACKET_STATUS_MAX <= UINT8_MAX,
              "MSG_PROT_TX_PACKET_STATUS must fit in uint8_t"); // Static assert to ensure enum fits in uint8_t - as
                                                                // used in mp_packet_t

// RX Packet status enum
typedef enum
{
   MSG_PROT_RX_PACKET_STATUS_NONE = 0,
   MSG_PROT_RX_PACKET_STATUS_NEW,
   MSG_PROT_RX_PACKET_STATUS_PROCESSED,
   MSG_PROT_RX_PACKET_STATUS_MAX,
} MSG_PROT_RX_PACKET_STATUS;
STATIC_ASSERT(MSG_PROT_RX_PACKET_STATUS_MAX <= UINT8_MAX,
              "MSG_PROT_RX_PACKET_STATUS must fit in uint8_t"); // Static assert to ensure enum fits in uint8_t - as
// used in mp_packet_t

/**
 * @note Ensure the MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE is updated if this struct is changed.
 */
typedef struct
{
   uint8_t type;
   uint8_t ppi;              /**< Packet Protocol Identifier. See PPIs for different links defined in common.h */
   uint16_t pkt_payload_len; /**< Length of the payload in bytes */
   uint8_t payload[MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN]; /**< Payload must always be defined last */
} mp_packet_payload_t;

// Error codes specific to the module.
typedef enum
{
   MSG_PROT_ERROR_NONE = 0,
   MSG_PROT_ERROR_NULL_PTR,
   MSG_PROT_ERROR_INVALID_ARG,
   MSG_PROT_ERROR_TIMEOUT,
   MSG_PROT_ERROR_BUSY,
   MSG_PROT_ERROR_TX_QUEUE_FULL,
   MSG_PROT_ERROR_PAYLOAD_TOO_BIG,
   MSG_PROT_ERROR_OUT_OF_RANGE,
   MSG_PROT_ERROR_BUFFER_OVERFLOW,
   MSG_PROT_ERROR_SWITCH_DEFAULT,
   MSG_PROT_ERROR_INVALID_PACKET_STATUS,
   MSG_PROT_ERROR_NO_NEW_PACKET,
   MSG_PROT_ERROR_RNG_FAILURE,
   MSG_PROT_ERROR_ERROR_MAX
} MSG_PROT_ERROR;

struct message_protocol; // Forward declaration
typedef struct message_protocol_interface message_protocol_interface_t;

struct message_protocol_interface
{
   struct message_protocol *parent;

   /**
    * @brief Queue a payload for transmission.
    *
    * This populates the internal TX packet. Transmission begins on the next call to @ref process. The function returns
    * an error if a previous TX packet is still in progress (i.e., not completed or abandoned). It is preferred to check
    * that the current TX packet is completed by calling @ref get_tx_packet_status before sending a new packet.
    *
    * The payload length is validated against the current maximum payload length. This value is updated during
    * @ref process, so call @ref process at least once (or call @ref get_max_payload_length) before sending.
    *
    * @param[in] interface Pointer to the message protocol interface.
    * @param[in] tx_packet Pointer to the payload to be sent.
    *
    * @retval RESULT_OK               Packet accepted for transmission.
    * @retval MSG_PROT_ERROR_NULL_PTR One or more input pointers are NULL.
    * @retval MSG_PROT_ERROR_BUSY     A previous packet is still in progress.
    * @retval MSG_PROT_ERROR_PAYLOAD_TOO_BIG Payload exceeds the current max payload length.
    */
   result_t (*send)(const message_protocol_interface_t *const interface, mp_packet_payload_t *tx_pkt_payload);

   /**
    * @brief Processes the message protocol interface.
    *
    * This function checks if there is an existing transmission being handled or if a new transmission can be made.
    * If an existing transmission is being handled, it checks if the timer has expired. If the timer has expired, it
    * resends the packet if there are retries left. If there are no retries left, it updates the status of the TX packet
    * appropriately. If a new transmission can be made, it checks the status of the currently populated TX packet and
    * sends it if it is new.
    *
    * It also checks the link layer for inbound packets, validates CRCs, and delivers valid packets to the internal
    * message protocol state machine. Corrupted packets are ignored and will be retried by the sender. The application
    * must fetch new RX packets via @ref get_rx_packet, which frees the internal RX slot for subsequent packets.
    *
    * @note This function must be called frequently enough to handle timeouts status updates of the current TX/RX
    * Packet. Errors regarding packet sending and receiving are reported via the TX/RX packet statuses. Retries are
    * also handled internally. Serious errors are propagated up via the return code of this function.
    *
    * @param[in] interface Pointer to the message protocol interface.
    * @return Result of operation - see @ref MSG_PROT_ERROR for possible error codes.
    */
   result_t (*process)(const message_protocol_interface_t *const interface);

   /**
    * @brief Get the status of the current RX packet being processed.
    *
    * This function retrieves the current status of the RX packet.
    * The caller must provide a pointer to a MSG_PROT_RX_PACKET_STATUS variable to receive the status.
    *
    * @param[in] interface Pointer to the message protocol interface.
    * @param[in] status Pointer to MSG_PROT_RX_PACKET_STATUS to receive the RX packet status.
    * @return Result of operation - see @ref MSG_PROT_ERROR for possible error codes.
    */
   result_t (*get_rx_packet_status)(const message_protocol_interface_t *const interface,
                                    MSG_PROT_RX_PACKET_STATUS *status);

   /**
    * @brief Retrieve the latest received packet if available.
    *
    * This function should be called when a call to @ref get_rx_packet_status returned a status of @ref
    * MSG_PROT_RX_PACKET_STATUS_NEW. The new incoming packet is copied into the provided pointer. The caller must
    * provide a valid pointer to a mp_packet_payload_t structure for the packet to be copied into. Upon successful
    * copy, the internal RX packet status is set to @ref MSG_PROT_RX_PACKET_STATUS_PROCESSED, indicating that the
    * internal RX packet is freed for new incoming messages.
    *
    * @param[in] interface Pointer to the message protocol interface.
    * @param[in] rx_packet Pointer to a mp_packet_payload_t structure to receive the packet data.
    * @retval RESULT_OK                  Packet copied and RX slot released.
    * @retval MSG_PROT_ERROR_NULL_PTR    One or more input pointers are NULL.
    * @retval MSG_PROT_ERROR_NO_NEW_PACKET No new packet is available.
    */
   result_t (*get_rx_packet)(const message_protocol_interface_t *const interface, mp_packet_payload_t *rx_packet);

   /**
    * @brief Get the status of the current TX packet being processed.
    *
    * This function retrieves the current status of the TX packet (e.g., WAITING_FOR_ACK, COMPLETED).
    * The caller must provide a pointer to a MSG_PROT_TX_PACKET_STATUS variable to receive the status.
    *
    * @param[in] interface Pointer to the message protocol interface.
    * @param[in] status Pointer to MSG_PROT_TX_PACKET_STATUS to receive the TX packet status.
    * @return Result of operation - see @ref MSG_PROT_ERROR for possible error codes.
    */
   result_t (*get_tx_packet_status)(const message_protocol_interface_t *const interface,
                                    MSG_PROT_TX_PACKET_STATUS *status);

   /**
    * @brief Get the maximum allowed payload length for a message protocol packet.
    *
    * This function retrieves the maximum payload length (in bytes) that can be sent in a single message protocol
    * packet, accounting for the current link layer MTU and protocol overhead. The value returned is the largest payload
    * size that can be safely transmitted without exceeding the underlying transport's maximum packet size.
    *
    * The maximum payload length may change at runtime if the link layer MTU changes. The value is refreshed during
    * @ref process, so call @ref process at least once before relying on this value.
    *
    * @param[in]  interface         Pointer to the message protocol interface.
    * @param[out] max_payload_len   Pointer to a uint16_t variable to receive the maximum payload length in bytes.
    *
    * @retval RESULT_OK             The maximum payload length was successfully retrieved.
    * @retval MSG_PROT_ERROR_NULL_PTR  One or more input pointers are NULL.
    *
    * @note The returned value excludes protocol header and minimum payload size overhead.
    * @note Always check this value before sending large packets to avoid buffer overflows or transmission errors.
    */
   result_t (*get_max_payload_length)(const message_protocol_interface_t *const interface, uint16_t *max_payload_len);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // MESSAGE_PROTOCOL_INTERFACE_H_
