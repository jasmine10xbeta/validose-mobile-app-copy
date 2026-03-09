/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/*
 * Mock I2C Driver Implementation
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard Includes
// Custom Includes
#include "i2c_driver_mock.h"
#include "common.h"
#include "result.h"
#include <stdio.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MOCK_BUFFER_LEN (256u)
#define THIS_UNIT_ID    SW_UNIT_ID_TWI_DRV // Define unit ID for tagging errors

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

static result_t mock_receive(const i2c_driver_interface_t *const interface,
                             uint8_t address,
                             uint8_t *p_rx_buffer,
                             uint8_t rx_buffer_length,
                             uint32_t timeout_us);

static result_t mock_transmit(const i2c_driver_interface_t *const interface,
                              uint8_t address,
                              const uint8_t *p_tx_buffer,
                              uint8_t tx_buffer_length,
                              uint32_t timeout_us);

static result_t mock_is_transfer_done(const i2c_driver_interface_t *const interface, bool *is_done);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
// Mock variables to simulate the driver's state

static bool m_mock_transfer_done = true;              // Simulates whether the last transfer has completed
static uint8_t m_mock_rx_buffer[MOCK_BUFFER_LEN];     // Simulated RX buffer for incoming data
static uint16_t m_mock_rx_buffer_16[MOCK_BUFFER_LEN]; // Simulated RX buffer for incoming data
static uint8_t m_mock_tx_buffer[MOCK_BUFFER_LEN];     // Simulated TX buffer for outgoing data
static uint16_t m_mock_tx_length = 0;                 // Length of the data in the TX buffer
static uint16_t m_mock_rx_length = 0;                 // Length of the data in the RX buffer
static uint16_t m_mock_device_address = 0;            // Simulated device address

on_mock_i2c_tx_data_ptr m_on_tx_data = NULL;

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t mock_receive(const i2c_driver_interface_t *const interface,
                             uint8_t address,
                             uint8_t *p_rx_buffer,
                             uint8_t rx_buffer_length,
                             uint32_t timeout_us)

{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_RX);
   RETURN_ERR_IF_NULL(p_rx_buffer, TWI_DRV_ERROR_NULL);

   // TODO: Simulate mutex functionality

   if(!m_mock_transfer_done)
   {
      RETURN_ERR(TWI_DRV_ERROR_BUSY);
   }

   m_mock_transfer_done = false; // Simulate the start of a new transfer

   // Simulate data reception
   if((address == m_mock_device_address) && (MOCK_BUFFER_LEN >= rx_buffer_length))
   {
      memcpy(p_rx_buffer, m_mock_rx_buffer, rx_buffer_length);
      m_mock_transfer_done = true; // Simulate transfer completion
      return RESULT_OK;
   }

   RETURN_ERR(TWI_DRV_ERROR_RX);
}

static result_t mock_transmit(const i2c_driver_interface_t *const interface,
                              uint8_t address,
                              const uint8_t *p_tx_buffer,
                              uint8_t tx_buffer_length,
                              uint32_t timeout_us)

{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_TX);
   RETURN_ERR_IF_NULL(p_tx_buffer, TWI_DRV_ERROR_NULL); // OR error: UNIT ID Tag?

   if(!m_mock_transfer_done)
   {
      RETURN_ERR(TWI_DRV_ERROR_BUSY);
   }

   m_mock_transfer_done = false; // Simulate the start of a new transfer

   // Simulate data transmission
   if((address == m_mock_device_address) && (MOCK_BUFFER_LEN >= tx_buffer_length))
   {
      memcpy(m_mock_tx_buffer, p_tx_buffer, tx_buffer_length);
      m_mock_tx_length = tx_buffer_length; // Update the length of the TX buffer
      m_mock_transfer_done = true;         // Simulate transfer completion

      if(NULL != m_on_tx_data)
      {
         m_on_tx_data(p_tx_buffer, tx_buffer_length);
      }

      return RESULT_OK;
   }

   RETURN_ERR(TWI_DRV_ERROR_TX);
}

static result_t mock_is_transfer_done(const i2c_driver_interface_t *const interface, bool *is_done)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TWI_DRV_ERROR_IS_XFER_DONE);
   RETURN_ERR_IF_NULL(is_done, SW_UNIT_ID_TWI_DRV);

   *is_done = m_mock_transfer_done; // Return the mock transfer state

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_i2c_driver_init(i2c_driver_t *const self, on_mock_i2c_tx_data_ptr on_tx_data, uint16_t device_address)
{
   RETURN_ERR_IF_NULL(self, TWI_DRV_ERROR_INIT);

   m_on_tx_data = on_tx_data; //***

   // Initialize the interface with mock function pointers
   self->interface.parent = self;
   self->interface.receive = mock_receive;
   self->interface.transmit = mock_transmit;
   self->interface.is_transfer_done = mock_is_transfer_done;

   // Initialize mock state
   m_mock_transfer_done = true;
   m_mock_device_address = device_address;

   return RESULT_OK;
}

void mock_i2c_set_rx_data(uint8_t *rx_data, uint8_t length)
{
   if(NULL == rx_data)
   {
      return;
   }

   if(length >= MOCK_BUFFER_LEN)
   {
      length = MOCK_BUFFER_LEN - 1;
   }

   memcpy(m_mock_rx_buffer, rx_data, length);
   m_mock_rx_length = length;
}
