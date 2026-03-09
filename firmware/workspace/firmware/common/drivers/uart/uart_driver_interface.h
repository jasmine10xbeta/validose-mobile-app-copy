/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file uart_driver_interface.h
 * @brief Interface for UART driver
 *
 * Defines interface to UART driver.
 */

#ifndef UART_DRIVER_INTERFACE_H_
#define UART_DRIVER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "../queue/queue.h"
#include "nrf_libuarte_async.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define UART_TX_BUFFER_SIZE (128)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct uart_tx_buffer
{
   uint8_t tx_data[UART_TX_BUFFER_SIZE];
   size_t tx_data_len;
} uart_tx_buffer_t;

typedef void (*uart_rx_callback)(const uint8_t *p_data, size_t length);

struct uart_driver;                                           // Forward declaration
typedef struct uart_driver_interface uart_driver_interface_t; // Forward declaration

typedef struct uart_driver_interface
{
   struct uart_driver *parent; // Reference to the containing instance.

   /**
    * @brief Perform nonblocking write
    *
    * @param p_data Buffer storing data to be transmitted
    * @param length Length of data in transmit buffer
    */
   result_t (*transmit)(const uart_driver_interface_t *const interface, const uint8_t *p_data, size_t length);

   /**
    * @brief Process buffers. Needs to be called periodically.
    */
   result_t (*process)(const uart_driver_interface_t *const interface);

} uart_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // UART_DRIVER_INTERFACE_H_
