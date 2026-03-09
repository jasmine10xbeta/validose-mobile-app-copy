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
#include "mock_serial_link_protocol.h"

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
static result_t serialize_fail(const serializer_interface_t *const interface,
                               const link_layer_header_data_t *const header_data,
                               const uint8_t *const payload,
                               uint16_t payload_length,
                               uint8_t *const output_buffer,
                               uint16_t output_buffer_size,
                               uint16_t *const output_length);

static result_t parser_process_fail(const serializer_interface_t *const interface,
                                    const uint8_t *const data,
                                    const uint16_t data_length,
                                    link_layer_header_data_t *const header_data,
                                    uint8_t *const payload_buffer,
                                    uint8_t payload_buffer_size,
                                    uint16_t *payload_length,
                                    uint16_t *soh_position);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 *********************************************************************************************************************/

static result_t parser_process_fail(const serializer_interface_t *const interface,
                                    const uint8_t *const data,
                                    const uint16_t data_length,
                                    link_layer_header_data_t *const header_data,
                                    uint8_t *const payload_buffer,
                                    uint8_t payload_buffer_size,
                                    uint16_t *payload_length,
                                    uint16_t *soh_position)
{
   return RESULT_THIS_UNIT_ERROR(SERIALIZER_ERROR_MAX);
}

static result_t serialize_fail(const serializer_interface_t *const interface,
                               const link_layer_header_data_t *const header_data,
                               const uint8_t *const payload,
                               uint16_t payload_length,
                               uint8_t *const output_buffer,
                               uint16_t output_buffer_size,
                               uint16_t *const output_length)
{
   return RESULT_THIS_UNIT_ERROR(SERIALIZER_ERROR_MAX);
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t mock_serial_link_protocol_serializer_init(serial_link_protocol_serializer_t *const self,
                                                   bool serializer_success,
                                                   bool parser_success)
{
   RETURN_ERR_IF_NULL(self, SERIALIZER_ERROR_NULL_PTR);

   result_t result = serial_link_protocol_serializer_init(self);

   if(false == serializer_success)
   {
      self->interface.serialize = serialize_fail;
   }

   if(false == parser_success)
   {
      self->interface.parser_process = parser_process_fail;
   }

   return result;
}
