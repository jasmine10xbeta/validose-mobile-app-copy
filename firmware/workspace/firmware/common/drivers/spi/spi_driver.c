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

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_mtx.h"

// Custom includes
#include "../../common.h"
#include "../common/modules/debug/debug.h"
#include "spi_driver.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_SPI_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
void spi_event_handler(nrf_drv_spi_evt_t const *p_event,
                       void *p_context); // NOSONAR The function prototype is dictated by the Nordic SDK

static result_t init_chip_select(const spi_driver_interface_t *const interface);
static result_t init_spi(const spi_driver_interface_t *const interface, nrf_drv_spi_config_t const *spi_config);
static result_t wait_for_xfer(const spi_driver_interface_t *const interface, uint32_t timeout_us);

static result_t spi_driver_transfer_blocking(const spi_driver_interface_t *const interface,
                                             uint8_t const *p_tx_buffer,
                                             uint8_t tx_buffer_length,
                                             uint8_t *p_rx_buffer,
                                             uint8_t rx_buffer_length,
                                             uint32_t timeout_us);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
void spi_event_handler(nrf_drv_spi_evt_t const *p_event,
                       void *p_context) // NOSONAR: Callback prototype defined externally
{
   RETURN_VOID_IF_NULL(p_context);

   const spi_driver_interface_t *const event_interface = (const spi_driver_interface_t *const)p_context;

   if((event_interface->parent != NULL) && (p_event->type == NRF_DRV_SPI_EVENT_DONE))
   {
      event_interface->parent->_spi_transfer_done = true;
   }
}

static result_t init_chip_select(const spi_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SPI_DRV_ERROR_NULL);
   result_t result = RESULT_OK;

   nrf_gpio_cfg_output(interface->parent->_spi_chip_select);
   nrf_gpio_pin_set(interface->parent->_spi_chip_select);

   return result;
}

static result_t init_spi(const spi_driver_interface_t *const interface, nrf_drv_spi_config_t const *spi_config)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SPI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(spi_config, SPI_DRV_ERROR_NULL);

   result_t result = init_chip_select(interface);

   if(IS_OK(result))
   {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
      uint32_t result_nrf = nrf_drv_spi_init(interface->parent->_spi_instance,
                                             spi_config,
                                             spi_event_handler,
                                             (void *)interface); // NOSONAR: Safe and necessary cast
#pragma GCC diagnostic pop

      UPDATE_IF_NRF_ERR(result_nrf, result, SPI_DRV_ERROR_INIT_SPI);
   }

   return result;
}

static result_t wait_for_xfer(const spi_driver_interface_t *const interface, uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SPI_DRV_ERROR_NULL);
   spi_driver_t *self = interface->parent;
   result_t result = RESULT_OK;
   uint32_t wait_iterations = 0u;

   do
   {
      if(wait_iterations > timeout_us)
      {
         SET_ERR(result, SPI_DRV_ERROR_TIMEOUT);
         self->_spi_transfer_done = true;
         DEBUG_ERROR("SPI timeout on channel %u", self->_spi_instance->inst_idx);
      }

      if(self->_spi_transfer_done || IS_ERR(result))
      {
         break;
      }

      nrf_delay_us(1);
      wait_iterations++;
   } while(!(self->_spi_transfer_done) && (IS_OK(result)));

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t spi_driver_transfer_blocking(const spi_driver_interface_t *const interface,
                                             uint8_t const *p_tx_buffer,
                                             uint8_t tx_buffer_length,
                                             uint8_t *p_rx_buffer,
                                             uint8_t rx_buffer_length,
                                             uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SPI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(p_tx_buffer, SPI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(p_rx_buffer, SPI_DRV_ERROR_NULL);

   spi_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   bool is_lock_acquired = nrf_mtx_trylock(&(self->_mutex));

   if(!is_lock_acquired || !(self->_spi_transfer_done))
   {
      SET_ERR(result, SPI_DRV_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      self->_spi_transfer_done = false;

      uint32_t nrf_result
         = nrf_drv_spi_transfer(self->_spi_instance, p_tx_buffer, tx_buffer_length, p_rx_buffer, rx_buffer_length);

      UPDATE_IF_NRF_ERR(nrf_result, result, SPI_DRV_ERROR_TRANSFER);

      if(IS_OK(result) && (timeout_us > 0))
      {
         result = wait_for_xfer(interface, timeout_us);
      }
   }

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&(self->_mutex));
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t
   spi_driver_init(spi_driver_t *const self, const nrf_drv_spi_t *spi_instance, const nrf_drv_spi_config_t *spi_config)
{
   result_t result = RESULT_OK;
   RETURN_ERR_IF_NULL(self, SPI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(spi_instance, SPI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(spi_config, SPI_DRV_ERROR_NULL);

   self->_initialized = false;
   self->interface.parent = self;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.spi_driver_transfer_blocking = spi_driver_transfer_blocking;

   self->_spi_transfer_done = true;
   self->_spi_instance = spi_instance;
   self->_spi_chip_select = spi_config->ss_pin;

   nrf_mtx_init(&(self->_mutex));

   // Init hardware
   result = init_spi(&self->interface, spi_config);

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
