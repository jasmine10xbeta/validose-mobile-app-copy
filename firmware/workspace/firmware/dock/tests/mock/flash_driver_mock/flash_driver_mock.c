/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>

#include "flash_driver_mock.h"

static const uint8_t THIS_UNIT_ID = SW_UNIT_ID_FLASH_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

static bool g_read_ok = true;
static bool g_prog_ok = true;
static bool g_erase_ok = true;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

static result_t
   mock_read(const flash_driver_interface_t *const interface, uint32_t address, uint8_t *read_data, uint8_t read_len);
static result_t mock_prog(const flash_driver_interface_t *const interface,
                          uint32_t address,
                          const uint8_t *write_data,
                          uint8_t write_len);
static result_t mock_erase(const flash_driver_interface_t *const interface, uint32_t start_address);

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t
   mock_read(const flash_driver_interface_t *const interface, uint32_t address, uint8_t *read_data, uint8_t read_len)
{
   (void)address;
   (void)read_data;
   (void)read_len;

   if((NULL == interface) || (NULL == interface->parent))
   {
      return RESULT_THIS_UNIT_ERROR(FLASH_DRV_ERROR_NULL);
   }

   if(!g_read_ok)
   {
      return RESULT_THIS_UNIT_ERROR(FLASH_DRV_ERROR_READ_RAW);
   }

   return RESULT_OK;
}

static result_t mock_prog(const flash_driver_interface_t *const interface,
                          uint32_t address,
                          const uint8_t *write_data,
                          uint8_t write_len)
{
   (void)address;
   (void)write_data;
   (void)write_len;

   if((NULL == interface) || (NULL == interface->parent))
   {
      return RESULT_THIS_UNIT_ERROR(FLASH_DRV_ERROR_NULL);
   }

   if(!g_prog_ok)
   {
      return RESULT_THIS_UNIT_ERROR(FLASH_DRV_ERROR_PROG_PAGE);
   }

   return RESULT_OK;
}

static result_t mock_erase(const flash_driver_interface_t *const interface, uint32_t start_address)
{
   (void)start_address;

   if((NULL == interface) || (NULL == interface->parent))
   {
      return RESULT_THIS_UNIT_ERROR(FLASH_DRV_ERROR_NULL);
   }

   if(!g_erase_ok)
   {
      return RESULT_THIS_UNIT_ERROR(FLASH_DRV_ERROR_SECTOR_ERASE);
   }

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_flash_driver_init(flash_driver_t *const self, bool read_ok, bool prog_ok, bool erase_ok)
{
   g_read_ok = read_ok;
   g_prog_ok = prog_ok;
   g_erase_ok = erase_ok;

   self->_initialized = false;
   self->_spi_interface = NULL;
   self->interface.parent = self;
   self->interface.read_raw_linear_burst_blocking = mock_read;
   self->interface.prog_page_256b_blocking = mock_prog;
   self->interface.erase_sector_256kb_blocking = mock_erase;
   self->_initialized = true;

   return RESULT_OK;
}
