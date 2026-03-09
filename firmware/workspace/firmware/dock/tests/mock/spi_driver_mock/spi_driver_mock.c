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

// Custom includes
#include "spi_driver_mock.h"

static const uint8_t THIS_UNIT_ID = SW_UNIT_ID_SPI_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

result_t mock_spi_driver_transfer_blocking_success(const spi_driver_interface_t *const interface,
                                                   uint8_t const *p_tx_buffer,
                                                   uint8_t tx_buffer_length,
                                                   uint8_t *p_rx_buffer,
                                                   uint8_t rx_buffer_length,
                                                   uint32_t timeout_us)
{
   return RESULT_OK;
}

result_t mock_spi_driver_transfer_blocking_fail(const spi_driver_interface_t *const interface,
                                                uint8_t const *p_tx_buffer,
                                                uint8_t tx_buffer_length,
                                                uint8_t *p_rx_buffer,
                                                uint8_t rx_buffer_length,
                                                uint32_t timeout_us)
{
   return RESULT_THIS_UNIT_ERROR(SPI_DRV_ERROR_TRANSFER);
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_spi_driver_init(spi_driver_t *const self, bool transfer_success)
{
   result_t result = RESULT_OK;

   self->_initialized = false;
   self->interface.parent = self;

   // Initialize all interface pointers to point to internal static functions by default
   if(transfer_success)
   {
      self->interface.spi_driver_transfer_blocking = mock_spi_driver_transfer_blocking_success;
   }
   else
   {
      self->interface.spi_driver_transfer_blocking = mock_spi_driver_transfer_blocking_fail;
   }

   self->_initialized = true;

   return result;
}