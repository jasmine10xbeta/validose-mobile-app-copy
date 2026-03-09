/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "flash_driver_s25hl512t.h"

// External includes
#include "lfs.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Definition of filesystem errors.
 */
typedef enum
{
   FILESYSTEM_ERROR_NONE = 0,               // No Error
   FILESYSTEM_ERROR_NULL,                   // Error to indicate NULL pointer.
   FILESYSTEM_ERROR_DEPEND_NOT_INITIALIZED, // Error to indicate a dependency was not initialized.
   FILESYSTEM_ERROR_MAX_TRANSACT_LENGTH,
   FILESYSTEM_ERROR_MAX,
} FILESYSTEM_ERROR;

typedef struct filesystem
{
   const flash_driver_interface_t *_flash_interface;
   struct lfs_config cfg;
   uint32_t partition_base_addr;  // start of the filesystem region in flash (absolute)
   uint32_t partition_size_bytes; // size of the filesystem region in bytes
   bool _initialized;
} filesystem_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t filesystem_init(filesystem_t *const self, const flash_driver_interface_t *flash_interface);

#endif // FILESYSTEM_H_
