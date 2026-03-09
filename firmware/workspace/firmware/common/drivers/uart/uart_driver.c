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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf_drv_clock.h"
#include "nrf_libuarte_async.h"

// Custom includes
#include "../../common/common.h"
#include "../../common/modules/debug/debug.h"
#include "uart_driver.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_UART_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

static result_t init_uart(const uart_driver_interface_t *const interface,
                          const nrf_libuarte_async_t *const uart_instance,
                          const nrf_libuarte_async_config_t *const uart_config);

static result_t transmit(const uart_driver_interface_t *const interface, const uint8_t *p_data, size_t length);
static result_t process(const uart_driver_interface_t *const interface);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

void uart_event_handler(void *context, nrf_libuarte_async_evt_t *p_evt)
{
   uart_driver_interface_t *interface = (uart_driver_interface_t *)context;
   uart_driver_t *self = interface->parent;

   switch(p_evt->type)
   {
      case NRF_LIBUARTE_ASYNC_EVT_ERROR:
         break;

      case NRF_LIBUARTE_ASYNC_EVT_RX_DATA:
         if(self->_rx_callback != NULL)
         {
            self->_rx_callback(p_evt->data.rxtx.p_data, p_evt->data.rxtx.length);
            nrf_libuarte_async_rx_free(self->_uart_instance, p_evt->data.rxtx.p_data, p_evt->data.rxtx.length);
         }
         break;

      case NRF_LIBUARTE_ASYNC_EVT_TX_DONE:
         self->is_tx_in_progress = false;
         break;
      case NRF_LIBUARTE_ASYNC_EVT_OVERRUN_ERROR:
         // Fallthrough
      default:
         break;
   }
}

static result_t init_uart(const uart_driver_interface_t *const interface,
                          const nrf_libuarte_async_t *const uart_instance,
                          const nrf_libuarte_async_config_t *const uart_config)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, UART_DRV_ERROR_INIT_HW_NULL);
   RETURN_ERR_IF_NULL(uart_instance, UART_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(uart_config, UART_DRV_ERROR_NULL);

   result_t result = RESULT_OK;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
   uint32_t result_nrf = nrf_libuarte_async_init(
      uart_instance, uart_config, uart_event_handler, (void *)interface); // NOSONAR SDK function
#pragma GCC diagnostic pop

   UPDATE_IF_NRF_ERR(result_nrf, result, UART_DRV_ERROR_INIT_HW);

   if(IS_OK(result))
   {
      nrf_libuarte_async_enable(uart_instance);
   }
   else
   {
      DEBUG_ERROR("Error initializing libuart async %u", result_nrf);
   }

   return result;
}

static result_t transmit(const uart_driver_interface_t *const interface, const uint8_t *p_data, size_t length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, UART_DRV_ERROR_TRANSMIT_NULL);
   RETURN_ERR_IF_NULL(p_data, UART_DRV_ERROR_NULL);

   uart_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   memset(&(self->_enqueueing_tx_msg.tx_data), 0, UART_TX_BUFFER_SIZE);

   if(length > UART_TX_BUFFER_SIZE)
   {
      SET_ERR(result, UART_DRV_ERROR_TRANSMIT_OVERFLOW);
   }

   if(IS_OK(result))
   {
      memcpy(&(self->_enqueueing_tx_msg.tx_data), p_data, length);
      self->_enqueueing_tx_msg.tx_data_len = length;

      result = self->_tx_queue->enqueue(self->_tx_queue, (const void *)(&(self->_enqueueing_tx_msg)));
   }

   return result;
}

static result_t process(const uart_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, UART_DRV_ERROR_PROCESS);
   uart_driver_t *self = interface->parent;

   size_t pending_tx_count = 0;
   result_t result = self->_tx_queue->get_count(self->_tx_queue, &pending_tx_count);

   if((IS_OK(result)) && (pending_tx_count > 0) && (self->is_tx_in_progress == false))
   {
      result = self->_tx_queue->dequeue(self->_tx_queue, (void *)(&(self->_processing_tx_msg)));

      if(IS_OK(result))
      {
         self->is_tx_in_progress = true;
         uint32_t result_nrf = nrf_libuarte_async_tx(
            self->_uart_instance, self->_processing_tx_msg.tx_data, self->_processing_tx_msg.tx_data_len);

         if(result_nrf != NRF_SUCCESS)
         {
            self->is_tx_in_progress = false;
            SET_ERR(result, UART_DRV_ERROR_PROCESS);
         }
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t uart_driver_init(uart_driver_t *const self,
                          const nrf_libuarte_async_t *const uart_instance,
                          const nrf_libuarte_async_config_t *const uart_config,
                          uart_rx_callback rx_callback,
                          const queue_interface_t *tx_queue)
{
   RETURN_ERR_IF_NULL(self, UART_DRV_ERROR_INIT);
   RETURN_ERR_IF_NULL(uart_instance, UART_DRV_ERROR_INIT);
   RETURN_ERR_IF_NULL(uart_config, UART_DRV_ERROR_INIT);
   RETURN_ERR_IF_NULL(tx_queue, UART_DRV_ERROR_INIT);

   self->_initialized = false;
   self->interface.parent = self;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.transmit = transmit;
   self->interface.process = process;

   self->_rx_callback = rx_callback;
   self->_tx_queue = tx_queue;
   self->is_tx_in_progress = false;

   // Init hardware
   result_t result = init_uart(&self->interface, uart_instance, uart_config);

   if(IS_OK(result))
   {
      self->_uart_instance = uart_instance;
      self->_initialized = true;
   }

   return result;
}
