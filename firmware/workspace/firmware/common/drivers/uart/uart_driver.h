/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file uart_driver.h
 * @brief UART driver
 *
 * Second-layer driver providing some additional abstraction to the Nordic UARTE driver.
 */

#ifndef UART_DRIVER_H_
#define UART_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "uart_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   UART_DRV_ERROR_NONE = 0,
   UART_DRV_ERROR_NULL,
   UART_DRV_ERROR_INIT,
   UART_DRV_ERROR_INIT_HW_NULL,
   UART_DRV_ERROR_INIT_HW,
   UART_DRV_ERROR_TRANSMIT_NULL,
   UART_DRV_ERROR_TRANSMIT_OVERFLOW,
   UART_DRV_ERROR_PROCESS,
   UART_DRV_ERROR_MAX,
} UART_DRV_ERROR;

typedef struct uart_driver
{
   uart_driver_interface_t interface;

   const nrf_libuarte_async_t *_uart_instance;

   uart_rx_callback _rx_callback;

   uart_tx_buffer_t _enqueueing_tx_msg;
   uart_tx_buffer_t _processing_tx_msg;
   const queue_interface_t *_tx_queue;

   volatile bool is_tx_in_progress;

   bool _initialized;
} uart_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t uart_driver_init(uart_driver_t *const self,
                          const nrf_libuarte_async_t *const uart_instance,
                          const nrf_libuarte_async_config_t *const uart_config,
                          uart_rx_callback rx_callback,
                          const queue_interface_t *tx_queue);

#endif // UART_DRIVER_H_
