/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file flash_driver_interface.h
 * @ingroup drivers/flash_driver
 * @brief Interface for low-level flash memory driver operations.
 *
 * This header defines the interface for interacting with the flash memory driver, including
 * reading, programming, and erasing flash sectors. It provides function pointers for hardware-specific
 * implementations and error codes for robust error handling.
 */

#ifndef FLASH_DRIVER_INTERFACE_H_
#define FLASH_DRIVER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <app_util.h>
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "result.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES UINT8_MAX // This is limited by nrf SDK spi functions.
STATIC_ASSERT(FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES <= UINT8_MAX,
              "SPI buffer length exceeds underlying max limitations");
#define FLASH_DRIVER_SECTOR_SIZE_BYTES (0x40000u) // 256kB blocks
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @enum FLASH_DRV_ERROR
 * @brief Error codes returned by flash driver operations.
 *
 * These error codes indicate the status of flash memory operations, such as read, write,
 * erase, and initialization errors. Used for diagnostics and robust error handling.
 */
typedef enum
{
   FLASH_DRV_ERROR_NONE = 0,                // No Error
   FLASH_DRV_ERROR_NULL,                    // Generic Null Pointer Error
   FLASH_DRV_ERROR_DEPEND_NOT_INITIALIZED,  // Error to indicate a dependency was not initialized.
   FLASH_DRV_ERROR_READ_REGISTERS_VIA_CMD,  // Read Registers via CMD Error
   FLASH_DRV_ERROR_WRITE_REGISTERS_VIA_CMD, // Write Registers via CMD Error
   FLASH_DRV_ERROR_ID,                      // Device Identification Error
   FLASH_DRV_ERROR_INVALID_ADDR,            // Read/Write to Invalid Address
   FLASH_DRV_ERROR_READ_ANY_REGISTER,       // Read Any Register Error
   FLASH_DRV_ERROR_WRITE_CMD,               // Write CMD Error
   FLASH_DRV_ERROR_WRITE_ANY_REGISTER,      // Write Any Register Error
   FLASH_DRV_ERROR_ARCH_SETUP,              // Error during architecture setup
   FLASH_DRV_ERROR_WAIT_BUSY_TIMEOUT,       // Error to indicate timeout while waiting for device
   FLASH_DRV_ERROR_DATA_LENGTH,             // Error to indicate length mismatch
   FLASH_DRV_ERROR_CROSS_PAGE_WRITE,        // Error to indicate cross page write
   FLASH_DRV_ERROR_READ_RAW,                // Error to indicate Raw Read transaction failure.
   FLASH_DRV_ERROR_PROG_PAGE,               // Error to indicate Page Programming transaction failure.
   FLASH_DRV_ERROR_SECTOR_ERASE,            // Error to indicate sector erase failed
   FLASH_DRV_ERROR_PROG,                    // Error to indicate STR1V[6] was set during last programming action
   FLASH_DRV_ERROR_ERASE,                   // Error to indicate STR1V[5] was set during erase operation
   FLASH_DRV_ERROR_MAX,
} FLASH_DRV_ERROR;

struct flash_driver;                                            // Forward declaration
typedef struct flash_driver_interface flash_driver_interface_t; // Forward declaration

/**
 * @struct flash_driver_interface
 * @brief Structure defining the flash driver interface for memory operations.
 *
 * This structure contains function pointers for flash memory operations and a reference to the parent driver instance.
 */
typedef struct flash_driver_interface
{
   /**
    * @brief Reference to the parent flash driver instance.
    */
   struct flash_driver *parent;

   /**
    * @brief Read a contiguous region directly from flash memory in linear burst mode.
    *
    * Reads data from the specified absolute address in flash memory into the provided buffer.
    * No page wrapping occurs; the operation is linear across the memory array.This is a blocking call.
    *
    * @param interface Pointer to the flash driver interface instance.
    * @param address Absolute flash address to begin reading from.
    * @param read_data Destination buffer for the data that is read.
    * @param read_len Number of bytes to read into @p read_data.
    * @return Driver result code (see @ref FLASH_DRV_ERROR).
    */
   result_t (*read_raw_linear_burst_blocking)(const flash_driver_interface_t *const interface,
                                              uint32_t address,
                                              uint8_t *read_data,
                                              uint8_t read_len);

   /**
    * @brief Program up to 256 bytes to a flash page at a specified address.
    *
    * Writes data from the provided buffer to the specified address in flash memory.
    * The address must be pre-erased and aligned to the page size. This is a blocking call.
    *
    * @param interface Pointer to the flash driver interface instance.
    * @param address Absolute flash address to write to (must be pre-erased).
    * @param write_data Buffer containing the bytes to program.
    * @param write_len Number of bytes to write from @p write_data (max 256).
    * @return Driver result code (see @ref FLASH_DRV_ERROR).
    */
   result_t (*prog_page_256b_blocking)(const flash_driver_interface_t *const interface,
                                       uint32_t address,
                                       const uint8_t *write_data,
                                       uint8_t write_len);

   /**
    * @brief Erase a 256 kB sector starting at the provided address.
    *
    * Erases the flash sector at the specified address. The address must be 256 kB aligned. This is a blocking call.
    *
    * @param interface Pointer to the flash driver interface instance.
    * @param start_address Absolute address of the sector to erase (must be 256 kB aligned).
    * @return Driver result code (see @ref FLASH_DRV_ERROR).
    */
   result_t (*erase_sector_256kb_blocking)(const flash_driver_interface_t *const interface, uint32_t start_address);

} flash_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // FLASH_DRIVER_INTERFACE_H_
