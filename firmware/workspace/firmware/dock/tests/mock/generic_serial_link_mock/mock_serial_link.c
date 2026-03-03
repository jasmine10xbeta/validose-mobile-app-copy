/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <string.h>

// Custom includes
#include "message_protocol.h"
#include "mock_serial_link.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_NFC_MOCK_SERIAL_DRIVER;

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
static result_t send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t length);
static result_t get_packet(const comms_driver_interface_t *const interface, uint8_t *data, uint16_t data_buffer_size);
static result_t get_max_packet_length(const comms_driver_interface_t *const interface, uint16_t *max_packet_len);

// Non-interface functions

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MOCK_LL_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, MOCK_LL_ERROR_PTR_NULL);
   (void)length;

   return RESULT_OK;
}

static result_t get_packet(const comms_driver_interface_t *const interface, uint8_t *data, uint16_t data_buffer_size)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MOCK_LL_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, MOCK_LL_ERROR_PTR_NULL);
   (void)data_buffer_size;

   return RESULT_OK;
}

static result_t get_max_packet_length(const comms_driver_interface_t *const interface, uint16_t *max_packet_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, MOCK_LL_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(max_packet_len, MOCK_LL_ERROR_PTR_NULL);

   serial_link_driver_t *self = interface->parent;
   *max_packet_len = self->_max_packet_len;

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t serial_link_driver_init(serial_link_driver_t *const self)
{
   RETURN_ERR_IF_NULL(self, MOCK_LL_ERROR_PTR_NULL);

   self->interface.parent = self;

   self->interface.send_packet = send_packet;
   self->interface.get_packet = get_packet;
   self->interface.get_max_packet_length = get_max_packet_length;

   self->_max_packet_len = MP_MAX_PACKET_LENGTH;
   self->_initialized = true;

   return RESULT_OK;
}
