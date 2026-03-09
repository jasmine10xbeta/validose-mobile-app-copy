/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup mock_serial_link_protocol Link Layer Protocol
 * @ingroup common
 * @brief Implements the link layer protocol for communication between devices over a serial channel.
 * @details
 *
 * @file mock_serial_link_protocol.h
 * @ingroup mock_serial_link_protocol
 * @brief
 */

#ifndef MOCK_SERIAL_LINK_PROTOCOL_H_
#define MOCK_SERIAL_LINK_PROTOCOL_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "serial_link_protocol.h"
#include "serial_link_protocol_interface.h"


/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t mock_serial_link_protocol_serializer_init(serial_link_protocol_serializer_t *const self,
                                                   bool serializer_success,
                                                   bool parser_success);
#endif // SERIAL_LINK_PROTOCOL_H_