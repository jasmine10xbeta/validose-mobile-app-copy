/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file mock_message_protocol.c
 * @ingroup mock_message_protocol
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "message_protocol.h"
#include <stdio.h>
#include <string.h>

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
static result_t send(const message_protocol_interface_t *const interface, mp_packet_payload_t *packet);
static result_t get_rx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_RX_PACKET_STATUS *status);
static result_t get_tx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_TX_PACKET_STATUS *status);
static result_t get_rx_packet(const message_protocol_interface_t *const interface, mp_packet_payload_t *rx_packet);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t send(const message_protocol_interface_t *const interface, mp_packet_payload_t *packet)
{
   memcpy(&interface->parent->_tx_packet, packet, sizeof(mp_packet_payload_t));
   return interface->parent->_result;
}

static result_t get_rx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_RX_PACKET_STATUS *status)
{
   *status = interface->parent->_rx_packet_status;
   return interface->parent->_result;
}

static result_t get_tx_packet_status(const message_protocol_interface_t *const interface,
                                     MSG_PROT_TX_PACKET_STATUS *status)
{
   *status = interface->parent->_tx_packet_status;
   return interface->parent->_result;
}

static result_t get_rx_packet(const message_protocol_interface_t *const interface, mp_packet_payload_t *rx_packet)
{
   memcpy(rx_packet, &interface->parent->_rx_packet, sizeof(mp_packet_payload_t));
   return interface->parent->_result;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t message_protocol_init(message_protocol_t *const self)
{
   // Assign parent
   self->interface.parent = self;

   self->interface.send = send;
   self->interface.get_rx_packet_status = get_rx_packet_status;
   self->interface.get_tx_packet_status = get_tx_packet_status;
   self->interface.get_rx_packet = get_rx_packet;

   return RESULT_OK;
}
