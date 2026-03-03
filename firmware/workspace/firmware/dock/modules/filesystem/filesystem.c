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
#include <app_util.h>

// Custom includes
#include "custom_board.h"
#include "filesystem.h"
#include "nrf_delay.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_FILESYSTEM;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define FILESYSTEM_PARTITION_STARTING_BASE_ADDRESS (0x00000000u) // Starting address of partition.
STATIC_ASSERT(FILESYSTEM_PARTITION_STARTING_BASE_ADDRESS <= FLASH_DRIVER_TOTAL_SIZE_BYTES,
              "Partition starting address must be less than flash driver total size.");

#define FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES                                                                             \
   (128u) // Page size is 256, so would want to use that, but limited by nrf spi SDK.
#define FILESYSTEM_LFS_CFG_READ_SIZE_BYTES (16u)
#define FILESYSTEM_LFS_CFG_BLOCK_CYCLES    (500u) // Configures wear-levelling counter for LittleFS

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
static int lfs_erase(const struct lfs_config *c, lfs_block_t block);
static int lfs_sync(const struct lfs_config *c);

// Non-interface functions

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
   // Early returns used below as they are essentially guard conditions.
   RETURN_VALUE_IF_NULL(c, LFS_ERR_IO);
   RETURN_VALUE_IF_NULL(buffer, LFS_ERR_IO);
   RETURN_VALUE_IF_NULL(c->context, LFS_ERR_IO);

   filesystem_t *filesystem_instance = (filesystem_t *)c->context;
   RETURN_VALUE_IF_NULL(filesystem_instance->_flash_interface, LFS_ERR_IO);

   // Check for out of range block index.
   if(block >= c->block_count)
   {
      return LFS_ERR_IO;
   }

   if((off >= c->block_size) || size > (c->block_size - off) || (0 == size))
   {
      return LFS_ERR_IO;
   }

   // LittleFS should guarantee the following, but checking anyway.
   if((c->read_size == 0) || ((off % c->read_size) != 0) || ((size % c->read_size) != 0))
   {
      return LFS_ERR_IO;
   }

   // Calculate absolute flash address.
   uint32_t relative_offset = ((uint32_t)block * c->block_size) + (uint32_t)off;
   uint32_t absolute_address = filesystem_instance->partition_base_addr + relative_offset;

   // Guard against reading past specified partition.
   if((relative_offset > filesystem_instance->partition_size_bytes)
      || (size > (filesystem_instance->partition_size_bytes - relative_offset)))
   {
      return LFS_ERR_IO;
   }

   int return_value = LFS_ERR_OK;
   result_t result = RESULT_THIS_UNIT_ERROR(FILESYSTEM_ERROR_MAX);

   // Guard against limitations of lower level limitations of flash/SPI driver.
   if(FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES < size)
   {
      SET_ERR(result, FILESYSTEM_ERROR_MAX_TRANSACT_LENGTH);
   }
   else
   {
      uint8_t size_u8 = (uint8_t)size;
      result = filesystem_instance->_flash_interface->read_raw_linear_burst_blocking(
         filesystem_instance->_flash_interface, absolute_address, buffer, size_u8);
   }

   if(IS_ERR(result))
   {
      DEBUG_ERROR("lfs_read - Unit: %d, Code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      return_value = LFS_ERR_IO;
   }

   return return_value;
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
   // Early returns used below as they are essentially guard conditions.
   RETURN_VALUE_IF_NULL(c, LFS_ERR_IO);
   RETURN_VALUE_IF_NULL(buffer, LFS_ERR_IO);
   RETURN_VALUE_IF_NULL(c->context, LFS_ERR_IO);

   filesystem_t *filesystem_instance = (filesystem_t *)c->context;
   RETURN_VALUE_IF_NULL(filesystem_instance->_flash_interface, LFS_ERR_IO);

   // Check for out of range block index.
   if(block >= c->block_count)
   {
      return LFS_ERR_IO;
   }

   // Ensure config matches erase sector function being called.
   if(FLASH_DRIVER_SECTOR_SIZE_BYTES != c->block_size)
   {
      return LFS_ERR_IO;
   }

   if((off >= c->block_size) || size > (c->block_size - off) || (0 == size))
   {
      return LFS_ERR_IO;
   }

   // Ensure config is compatible with hw setup
   if(FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES != c->prog_size)
   {
      return LFS_ERR_IO;
   }

   // Double check alignment and sizing - even though LittleFS should take care of this before calling the function.
   if((off % c->prog_size != 0) || (size % c->prog_size != 0))
   {
      return LFS_ERR_IO;
   }

   int return_value = LFS_ERR_OK;
   result_t result = RESULT_OK;

   // Calculate absolute flash address.
   uint32_t relative_offset = ((uint32_t)block * c->block_size) + (uint32_t)off;
   uint32_t absolute_address = filesystem_instance->partition_base_addr + relative_offset;

   // Guard against writing past specified partition.
   if((relative_offset > filesystem_instance->partition_size_bytes)
      || (size > (filesystem_instance->partition_size_bytes - relative_offset)))
   {
      return LFS_ERR_IO;
   }

   const uint8_t *local_buffer = (const uint8_t *)buffer;

   while((IS_OK(result)) && (size > 0))
   {
      uint32_t page_offset = absolute_address % (uint32_t)c->prog_size;
      uint32_t space_in_page = (uint32_t)c->prog_size - page_offset;

      uint32_t chunk = (uint32_t)size;

      if(chunk > space_in_page)
      {
         chunk = space_in_page;
      }

      // Guard against limitations of lower level limitations of flash/SPI driver.
      if(FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES < chunk)
      {
         SET_ERR(result, FILESYSTEM_ERROR_MAX_TRANSACT_LENGTH);
      }
      else
      {
         uint8_t chunk_u8 = (uint8_t)chunk;
         result = filesystem_instance->_flash_interface->prog_page_256b_blocking(
            filesystem_instance->_flash_interface, absolute_address, buffer, chunk_u8);
      }

      if(IS_OK(result))
      {
         absolute_address += chunk;
         local_buffer += chunk;
         size -= chunk;
      }
   }

   if(IS_ERR(result))
   {
      DEBUG_ERROR("lfs_prog - Unit: %d, Code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      return_value = LFS_ERR_IO;
   }

   return return_value;
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
   // Early returns used below as they are essentially guard conditions.
   RETURN_VALUE_IF_NULL(c, LFS_ERR_IO);
   RETURN_VALUE_IF_NULL(c->context, LFS_ERR_IO);

   filesystem_t *filesystem_instance = (filesystem_t *)c->context;
   RETURN_VALUE_IF_NULL(filesystem_instance->_flash_interface, LFS_ERR_IO);

   // Check for out of range block index.
   if(block >= c->block_count)
   {
      return LFS_ERR_IO;
   }

   // Ensure config matches erase sector function being called.
   if(FLASH_DRIVER_SECTOR_SIZE_BYTES != c->block_size)
   {
      return LFS_ERR_IO;
   }

   int return_value = LFS_ERR_OK;

   // Calculate absolute flash address.
   // Given that we check block size and block index, we are sure this math won't overflow.
   uint32_t relative_offset = (uint32_t)block * c->block_size;

   // partition_base_addr is static asserted, therefore math below is also safe.
   uint32_t absolute_address = filesystem_instance->partition_base_addr + relative_offset;
   result_t result = filesystem_instance->_flash_interface->erase_sector_256kb_blocking(
      filesystem_instance->_flash_interface, absolute_address);

   if(IS_ERR(result))
   {
      DEBUG_ERROR("lfs_erase - Unit: %d, Code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      return_value = LFS_ERR_IO;
   }

   return return_value;
}

static int lfs_sync(const struct lfs_config *c)
{
   // Given that we use synchronous / blocking lower level driver, this function is not implemented.
   (void)c;
   return LFS_ERR_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t filesystem_init(filesystem_t *const self, const flash_driver_interface_t *flash_interface)
{
   RETURN_ERR_IF_NULL(self, FILESYSTEM_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(flash_interface, FILESYSTEM_ERROR_NULL);
   RETURN_ERR_IF_TRUE(false == flash_interface->parent->_initialized, FILESYSTEM_ERROR_DEPEND_NOT_INITIALIZED);

   self->_initialized = false;
   self->_flash_interface = flash_interface;
   // We are using full flash memory from address 0.
   self->partition_base_addr = FILESYSTEM_PARTITION_STARTING_BASE_ADDRESS;
   self->partition_size_bytes = FLASH_DRIVER_TOTAL_SIZE_BYTES;

   // This is the lfs_interface setup -> replacing the normal interface as per other modules.
   self->cfg = (struct lfs_config){0};
   self->cfg.context = self;
   self->cfg.read = lfs_read;
   self->cfg.prog = lfs_prog;
   self->cfg.erase = lfs_erase;
   self->cfg.sync = lfs_sync;

   self->cfg.read_size = FILESYSTEM_LFS_CFG_READ_SIZE_BYTES;
   self->cfg.prog_size = FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES; // 128 B to avoid nRF52840 SPI write cap.
   self->cfg.block_size = FLASH_DRIVER_SECTOR_SIZE_BYTES;    // 256 KB S25HL512T erase sector to match LL HW setup.
   self->cfg.block_count = FLASH_DRIVER_TOTAL_SIZE_BYTES / FLASH_DRIVER_SECTOR_SIZE_BYTES; // Use full 64MB of S25HL512T
   self->cfg.cache_size = FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES; // Match program size to avoid wasting RAM and accommodate
                                                              // nRF52840 SPI transaction cap.
   self->cfg.lookahead_size = FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES; // Minimal lookahead bitmap to cover one page.
   self->cfg.block_cycles = FILESYSTEM_LFS_CFG_BLOCK_CYCLES;

   self->_initialized = true;

   DEBUG_INFO("Filesystem Initialized.");

   return RESULT_OK;
}
