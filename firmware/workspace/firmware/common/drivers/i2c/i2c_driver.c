/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file i2c_driver.c
 * @ingroup i2c_driver
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include "nrf_delay.h"
#include "nrf_mtx.h"
#include "project.h"

// Custom includes
#include "i2c_driver.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_TWI_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
/**
 * @brief This macro checks if the twi interface is initialized
 *        If the twi interface is not initialized, it creates a result_t structure with status set to RESULT_ERROR
 *        and value set to the provided err_val value, along with the current unit's ID. The macro then returns
 *        this result_t immediately.
 *
 * @param[in] is_initialized The Nordic Semiconductor nRF SDK return code to check.
 * @param[in] err_val The error value to return if the nrf_ret_code is not NRF_SUCCESS.
 *
 * @return If nrf_ret_code is not NRF_SUCCESS, it returns a result_t structure with status set to RESULT_ERROR
 *         and value set to the provided err_val value, along with the current unit's ID.
 *         If nrf_ret_code is NRF_SUCCESS, it does not return anything.
 */

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t receive(const i2c_driver_interface_t *const interface,
                        uint8_t address,
                        uint8_t *p_rx_buffer,
                        uint8_t rx_buffer_length,
                        uint32_t timeout_us);
static result_t receive_after_tx_no_stop(const i2c_driver_interface_t *const interface,
                                         uint8_t address,
                                         uint8_t *p_rx_buffer,
                                         uint8_t rx_buffer_length,
                                         uint32_t timeout_us);

static result_t transmit_receive(const i2c_driver_interface_t *const interface,
                                 uint8_t address,
                                 uint8_t *p_tx_buffer,
                                 uint8_t tx_buffer_length,
                                 uint8_t *p_rx_buffer,
                                 uint8_t rx_buffer_length,
                                 uint32_t timeout_us);
static result_t transmit(const i2c_driver_interface_t *const interface,
                         uint8_t address,
                         const uint8_t *p_tx_buffer,
                         uint8_t tx_buffer_length,
                         uint32_t timeout_us);
static result_t transmit_no_stop(const i2c_driver_interface_t *const interface,
                                 uint8_t address,
                                 const uint8_t *p_tx_buffer,
                                 uint8_t tx_buffer_length);
static result_t is_transfer_done(const i2c_driver_interface_t *const interface, bool *is_done);
static result_t i2c_bus_scan(const i2c_driver_interface_t *const interface);

// Non-interface functions
static result_t disable_twi(const i2c_driver_interface_t *const interface);
static result_t init_twi(const i2c_driver_interface_t *const interface);
void twi_event_handler(nrf_drv_twi_evt_t const *p_event,
                       void *p_context); // NOSONAR The function prototype is dictated by the Nordic SDK

static result_t wait_for_xfer(const i2c_driver_interface_t *const interface, uint32_t timeout_us);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
void twi_event_handler(nrf_drv_twi_evt_t const *p_event,
                       void *p_context) // NOSONAR: Callback prototype defined externally
{
   RETURN_VOID_IF_NULL(p_context);
   const i2c_driver_interface_t *const event_interface = (const i2c_driver_interface_t *const)p_context;

   if(event_interface->parent != NULL)
   {
      switch(p_event->type)
      {
         case NRF_DRV_TWI_EVT_DONE:
            event_interface->parent->_twi_transfer_done = true;
            break;

         case NRF_DRV_TWI_EVT_ADDRESS_NACK:
            event_interface->parent->_twi_transfer_done = true;
            event_interface->parent->_anak_received = true;
            break;

         case NRF_DRV_TWI_EVT_DATA_NACK:
            event_interface->parent->_twi_transfer_done = true;
            event_interface->parent->_dnak_received = true;
            break;

         default:
            event_interface->parent->_twi_transfer_done = true;
            break;
      }
   }
}

static result_t init_twi(const i2c_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_NULL);

   result_t result = RESULT_OK;

   if(interface->parent->_initialized)
   {
      return result;
   }
#ifndef RING
   // Re-set the pullup as it was cleared for power savings
   nrf_gpio_pin_set(I2C_PULLUP);
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
   uint32_t result_nrf = nrf_drv_twi_init(interface->parent->_twi_instance,
                                          interface->parent->_twi_config,
                                          twi_event_handler,
                                          (void *)interface); // NOSONAR: Safe and necessary cast
#pragma GCC diagnostic pop

   UPDATE_IF_NRF_ERR(result_nrf, result, TWI_DRV_ERROR_INIT);

   if(IS_OK(result))
   {
      nrf_drv_twi_enable(interface->parent->_twi_instance);
   }

   if(IS_OK(result))
   {
      interface->parent->_initialized = true; // Set enabled flag
   }

   return result;
}

static result_t disable_twi(const i2c_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_NULL);
   RETURN_OK_IF_TRUE(!interface->parent->_initialized);
   result_t result = RESULT_OK;

#ifndef RING
   nrf_gpio_pin_clear(I2C_PULLUP);
#endif
   nrf_drv_twi_disable(interface->parent->_twi_instance);
   nrf_drv_twi_uninit(interface->parent->_twi_instance);

   interface->parent->_initialized = false; // Set enabled flag to false

   return result;
}

static result_t wait_for_xfer(const i2c_driver_interface_t *const interface, uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_INIT);

   i2c_driver_t *self = interface->parent;
   result_t result = RESULT_OK;
   uint32_t wait_iterations = 0u;

   do
   {
      if(wait_iterations > timeout_us)
      {
         SET_ERR(result, TWI_DRV_TIMEOUT);
         self->_twi_transfer_done = true;
      }

      if(self->_twi_transfer_done || IS_ERR(result))
      {
         break;
      }

      nrf_delay_us(1);
      wait_iterations++;
   } while(!(self->_twi_transfer_done) && (IS_OK(result)));

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t receive(const i2c_driver_interface_t *const interface,
                        uint8_t address,
                        uint8_t *p_rx_buffer,
                        uint8_t rx_buffer_length,
                        uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_RX);
   RETURN_ERR_IF_NULL(p_rx_buffer, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_NOT_INITIALIZED);

   i2c_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   bool is_lock_acquired = nrf_mtx_trylock(&(self->_twi_mutex));

   if(!is_lock_acquired || !(self->_twi_transfer_done))
   {
      SET_ERR(result, TWI_DRV_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      self->_dnak_received = false;
      self->_anak_received = false;
      self->_twi_transfer_done = false;
      uint32_t nrf_result = nrf_drv_twi_rx(self->_twi_instance, address, p_rx_buffer, rx_buffer_length);

      if(NRF_ERROR_DRV_TWI_ERR_ANACK == nrf_result)
      {
         SET_ERR(result, TWI_DRV_ERROR_ANAK);
      }
      else if(NRF_ERROR_DRV_TWI_ERR_DNACK == nrf_result)
      {
         SET_ERR(result, TWI_DRV_ERROR_DNAK);
      }
      else if(nrf_result != NRF_SUCCESS)
      {
         SET_ERR(result, TWI_DRV_ERROR_RX);
      }
   }

   if(timeout_us > 0u)
   {
      IF_OK_RUN_AND_UPDATE(result, wait_for_xfer(interface, timeout_us));

      if(self->_dnak_received)
      {
         self->_dnak_received = false; // reset for next transfer
         SET_ERR(result, TWI_DRV_ERROR_DNAK);
      }
      if(self->_anak_received)
      {
         SET_ERR(result, TWI_DRV_ERROR_ANAK);
         self->_anak_received = false; // reset for next transfer
      }
   }

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&(self->_twi_mutex));
   }

   return result;
}

static result_t receive_after_tx_no_stop(const i2c_driver_interface_t *const interface,
                                         uint8_t address,
                                         uint8_t *p_rx_buffer,
                                         uint8_t rx_buffer_length,
                                         uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(p_rx_buffer, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_NOT_INITIALIZED);

   i2c_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   bool is_lock_acquired = nrf_mtx_trylock(&(self->_twi_mutex));

   if(!is_lock_acquired)
   {
      SET_ERR(result, TWI_DRV_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      self->_twi_transfer_done = false;
      ret_code_t nrf_result = nrf_drv_twi_rx(self->_twi_instance, address, p_rx_buffer, rx_buffer_length);

      if(nrf_result != NRF_SUCCESS)
      {
         DEBUG_ERROR("I2C tx error received");
         SET_ERR(result, TWI_DRV_ERROR_RX);
      }
   }

   if(IS_OK(result) && (timeout_us > 0u))
   {
      result = wait_for_xfer(interface, timeout_us);
   }

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&(self->_twi_mutex));
   }

   return result;
}

static result_t transmit_receive(const i2c_driver_interface_t *const interface,
                                 uint8_t address,
                                 uint8_t *p_tx_buffer,
                                 uint8_t tx_buffer_length,
                                 uint8_t *p_rx_buffer,
                                 uint8_t rx_buffer_length,
                                 uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_RX);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(p_tx_buffer, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(p_rx_buffer, TWI_DRV_ERROR_NULL);

   i2c_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   bool is_lock_acquired = nrf_mtx_trylock(&(self->_twi_mutex));

   if(!is_lock_acquired)
   {
      SET_ERR(result, TWI_DRV_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      self->_dnak_received = false;
      self->_anak_received = false;
      self->_twi_transfer_done = false;
      nrf_drv_twi_xfer_desc_t x
         = NRF_DRV_TWI_XFER_DESC_TXRX(address, p_tx_buffer, tx_buffer_length, p_rx_buffer, rx_buffer_length);
      ret_code_t nrf_result = nrf_drv_twi_xfer(self->_twi_instance, &x, 0);

      if(NRF_ERROR_DRV_TWI_ERR_ANACK == nrf_result)
      {
         SET_ERR(result, TWI_DRV_ERROR_ANAK);
      }
      else if(NRF_ERROR_DRV_TWI_ERR_DNACK == nrf_result)
      {
         SET_ERR(result, TWI_DRV_ERROR_DNAK);
      }
      else if(nrf_result != NRF_SUCCESS)
      {
         DEBUG_ERROR("I2C tx error received");
         SET_ERR(result, TWI_DRV_ERROR_RX);
      }
   }

   if(IS_OK(result) && (timeout_us > 0u))
   {
      result = wait_for_xfer(interface, timeout_us);
      if(self->_dnak_received)
      {
         self->_dnak_received = false; // reset for next transfer
         SET_ERR(result, TWI_DRV_ERROR_DNAK);
      }
      if(self->_anak_received)
      {
         SET_ERR(result, TWI_DRV_ERROR_ANAK);
         self->_anak_received = false; // reset for next transfer
      }
   }

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&(self->_twi_mutex));
   }

   return result;
}

static result_t transmit(const i2c_driver_interface_t *const interface,
                         uint8_t address,
                         const uint8_t *p_tx_buffer,
                         uint8_t tx_buffer_length,
                         uint32_t timeout_us)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_TX);
   RETURN_ERR_IF_NULL(p_tx_buffer, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_INIT);

   i2c_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   bool is_lock_acquired = nrf_mtx_trylock(&(self->_twi_mutex));

   if(!is_lock_acquired || !(self->_twi_transfer_done))
   {
      SET_ERR(result, TWI_DRV_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      self->_dnak_received = false;
      self->_anak_received = false;
      self->_twi_transfer_done = false;
      uint32_t nrf_result = nrf_drv_twi_tx(self->_twi_instance, address, p_tx_buffer, tx_buffer_length, false);

      if(NRF_ERROR_DRV_TWI_ERR_ANACK == nrf_result)
      {
         SET_ERR(result, TWI_DRV_ERROR_ANAK);
      }
      else if(NRF_ERROR_DRV_TWI_ERR_DNACK == nrf_result)
      {
         SET_ERR(result, TWI_DRV_ERROR_DNAK);
      }
      else if(nrf_result != NRF_SUCCESS)
      {
         DEBUG_ERROR("I2C tx error received");
         SET_ERR(result, TWI_DRV_ERROR_RX);
      }
   }

   if(timeout_us > 0u)
   {
      IF_OK_RUN_AND_UPDATE(result, wait_for_xfer(interface, timeout_us));
      if(self->_dnak_received)
      {
         self->_dnak_received = false; // reset for next transfer
         SET_ERR(result, TWI_DRV_ERROR_DNAK);
      }
      else if(self->_anak_received)
      {
         SET_ERR(result, TWI_DRV_ERROR_ANAK);
         self->_anak_received = false; // reset for next transfer
      }
   }

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&(self->_twi_mutex));
   }

   return result;
}

static result_t transmit_no_stop(const i2c_driver_interface_t *const interface,
                                 uint8_t address,
                                 const uint8_t *p_tx_buffer,
                                 uint8_t tx_buffer_length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_TX);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(p_tx_buffer, TWI_DRV_ERROR_NULL);

   i2c_driver_t *self = interface->parent;

   result_t result = RESULT_OK;

   bool is_lock_acquired = nrf_mtx_trylock(&(self->_twi_mutex));

   if(!is_lock_acquired || !(self->_twi_transfer_done))
   {
      SET_ERR(result, TWI_DRV_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      self->_twi_transfer_done = false;
      uint32_t nrf_result = nrf_drv_twi_tx(self->_twi_instance, address, p_tx_buffer, tx_buffer_length, true);

      if(nrf_result != NRF_SUCCESS)
      {
         SET_ERR(result, TWI_DRV_ERROR_TX);
      }
   }

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&(self->_twi_mutex));
   }

   return result;
}

static result_t is_transfer_done(const i2c_driver_interface_t *const interface, bool *is_done)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_IS_XFER_DONE);
   RETURN_ERR_IF_NULL(is_done, TWI_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_INIT);
   result_t result = RESULT_OK;

   const i2c_driver_t *self = interface->parent;

   *is_done = self->_twi_transfer_done;

   return result;
}

static result_t i2c_bus_scan(const i2c_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_IS_XFER_DONE);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, TWI_DRV_ERROR_NOT_INITIALIZED);

   const uint8_t first_address = 0x03;
   const uint8_t last_address = 0x77;
   bool detected_device = false;
   uint8_t sample_data = 0;
   result_t result = RESULT_OK;

   DEBUG_INFO("Start I2C bus scan:");
   for(uint8_t address = first_address; address <= last_address; address++)
   {
      result = transmit(interface, address, &sample_data, sizeof(sample_data), 100);

      if(IS_OK(result)) // device ACKed
      {
         detected_device = true;
         DEBUG_INFO("Address: %d", address);
      }
   }

   if(!detected_device)
   {
      DEBUG_INFO("No device was found.");
   }

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t
   i2c_driver_init(i2c_driver_t *const self, const nrf_drv_twi_t *twi_instance, const nrf_drv_twi_config_t *twi_config)
{
   RETURN_ERR_IF_NULL(self, TWI_DRV_ERROR_INIT_GENERAL);
   RETURN_ERR_IF_NULL(twi_instance, TWI_DRV_ERROR_INIT_GENERAL);
   RETURN_ERR_IF_NULL(twi_config, TWI_DRV_ERROR_INIT_GENERAL);

   self->_initialized = false;
   self->interface.parent = self;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.receive = receive;
   self->interface.transmit = transmit;
   self->interface.disable_twi = disable_twi;
   self->interface.is_transfer_done = is_transfer_done;
   self->interface.i2c_bus_scan = i2c_bus_scan;
   self->interface.transmit_no_stop = transmit_no_stop;
   self->interface.receive_after_tx_no_stop = receive_after_tx_no_stop;
   self->interface.transmit_receive = transmit_receive;

   self->_twi_transfer_done = true;
   self->_twi_instance = twi_instance;
   self->_twi_config = twi_config;

   nrf_mtx_init(&(self->_twi_mutex));

   // Init hardware
   result_t result = init_twi(&self->interface);
   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
