/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file comms_interface.h
 * @ingroup driverscomms_driver
 * @brief COMMS driver interface
 *
 * This file contains the interface for the COMMS driver
 *
 * The comms driver handles communication between the dock and the ring
 */

#ifndef COMMS_DRIVER_INTERFACE_H_
#define COMMS_DRIVER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdint.h>

// Custom includes
#include "common.h"
#include "message_protocol_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// TODO Make sure the comms driver max is always larger than the max packet size of all the applicable link layers. This
// check should be enforced at compile time if possible. The two link layers applicable here are NFC and BLE.

typedef enum
{
   COMMS_DRIVER_ERROR_NONE = 0,          // No error
   COMMS_DRIVER_ERROR_PTR_NULL,          // Null pointer error
   COMMS_DRIVER_ERROR_INVALID_TX_LENGTH, // Invalid TX length supplied
   COMMS_DRIVER_ERROR_BUSY,              // Action could not be performed, device busy
   COMMS_DRIVER_MEMORY_ALLOCATION_ERROR, // Memory allocation error
   COMMS_DRIVER_ERROR_INIT_FAILURE,      // Failure to initialize driver
   COMMS_DRIVER_ERROR_NRF_ERR_CHECK,     // Generic Nordic SDK error
   COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND,
   COMMS_DRIVER_ERROR_UNINITIALIZED, // Driver not initialized
   COMMS_DRIVER_ERROR_COMM_RX,       // Communication error on RX
   COMMS_DRIVER_ERROR_COMM_TX,       // Communication error on TX
   COMMS_DRIVER_ERROR_HW_FAULT,      // Hardware fault
   COMMS_DRIVER_ERROR_OVERFLOW,
   COMMS_DRIVER_ERROR_OUT_OF_RANGE,
   COMMS_DRIVER_ERROR_MAX
} COMMS_DRIVER_ERROR;

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct comms_driver; // Forward declaration

typedef struct comms_driver_interface comms_driver_interface_t;

typedef struct comms_driver_interface
{
   void *parent; // Reference to the containing instance.

   /**
    * @brief Send a single link-layer packet.
    *
    * Implementations should treat this as a non-blocking transmit of exactly
    * @p length bytes from @p data. The payload is assumed to already be in the
    * link-layer on-the-wire format expected by the peer.
    *
    * @param[in] interface Pointer to the comms driver interface.
    * @param[in] data      Pointer to the byte buffer to transmit.
    * @param[in] length    Number of bytes to transmit.
    *
    * @retval RESULT_OK                         Packet accepted for transmission.
    * @retval COMMS_DRIVER_ERROR_PTR_NULL       @p interface or @p data is NULL.
    * @retval COMMS_DRIVER_ERROR_INVALID_TX_LENGTH @p length is invalid or exceeds the supported MTU.
    * @retval COMMS_DRIVER_ERROR_BUSY           Link layer is not ready to accept a packet.
    * @retval COMMS_DRIVER_ERROR_UNINITIALIZED  Driver not initialized.
    * @retval COMMS_DRIVER_ERROR_COMM_TX        Transport/hardware error during transmission.
    */
   result_t (*send_packet)(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t length);

   /**
    * @brief Receive a complete link-layer packet into a caller-provided buffer.
    *
    * This function should be non-blocking. It returns RESULT_OK only when a
    * full, self-contained packet has been copied into @p data. If no packet is
    * available, return COMMS_DRIVER_ERROR_BUSY (preferred) so callers can
    * treat it as "no data" without propagating an error.
    *
    * The @p data_buffer_size must be large enough to hold the largest possible
    * packet returned by this link layer (see @ref get_max_packet_length).
    *
    * @param[in]  interface        Pointer to the comms driver interface.
    * @param[out] data             Buffer to receive the packet bytes.
    * @param[in]  data_buffer_size Size of @p data in bytes.
    *
    * @retval RESULT_OK                         Packet copied into @p data.
    * @retval COMMS_DRIVER_ERROR_PTR_NULL       @p interface or @p data is NULL.
    * @retval COMMS_DRIVER_ERROR_OVERFLOW       @p data_buffer_size is too small for a full packet.
    * @retval COMMS_DRIVER_ERROR_BUSY           No packet available right now.
    * @retval COMMS_DRIVER_ERROR_UNINITIALIZED  Driver not initialized.
    * @retval COMMS_DRIVER_ERROR_COMM_RX        Transport/hardware error during reception.
    */
   result_t (*get_packet)(const comms_driver_interface_t *const interface, uint8_t *data, uint16_t data_buffer_size);

   /**
    * @brief Retrieve the maximum supported link-layer packet length.
    *
    * This value represents the maximum on-the-wire packet size (in bytes) that
    * can be sent or received by this link layer, including any link-layer
    * headers/trailers. It may be constant (e.g., mailbox size) or change at
    * runtime (e.g., MTU negotiation).
    *
    * @param[in]  interface       Pointer to the comms driver interface.
    * @param[out] max_packet_len  Receives the maximum packet length in bytes.
    *
    * @retval RESULT_OK                         Maximum packet length returned.
    * @retval COMMS_DRIVER_ERROR_PTR_NULL       @p interface or @p max_packet_len is NULL.
    * @retval COMMS_DRIVER_ERROR_UNINITIALIZED  Driver not initialized.
    */
   result_t (*get_max_packet_length)(const comms_driver_interface_t *const interface, uint16_t *max_packet_len);

} comms_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // COMMS_DRIVER_INTERFACE_H_
