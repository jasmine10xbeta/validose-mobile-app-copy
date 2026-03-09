/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_sources_interface.h
 * @ingroup ring_sources
 * @brief
 */

#ifndef RING_SOURCES_INTERFACE_H_
#define RING_SOURCES_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef enum
{
   SOURCE_ID_BATTERY,
   SOURCE_ID_ERROR,
   SOURCE_ID_DOSE,
   SOURCE_ID_STATUS,
   SOURCE_ID_MAX,
} SOURCE_ID;

// Error codes specific to the module.
typedef enum
{
   SOURCE_MANAGER_ERROR_NONE = 0,
   SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR,
   SOURCE_MANAGER_ERROR_NULL_INTERFACE_PTR,
   SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID,
   SOURCE_MANAGER_ERROR_INVALID_BYTE_COUNT,
   SOURCE_MANAGER_ERROR_INVALID_DEST_PTR,
   SOURCE_MANAGER_ERROR_INVALID_ARG_PTR,
   SOURCE_MANAGER_ERROR_INVALID_DEST_SIZE,
   SOURCE_MANAGER_ERROR_MAX,
} SOURCE_MANAGER_ERROR;

struct ring_sources;

typedef struct ring_sources_interface ring_sources_interface_t;

typedef struct ring_sources_interface
{
   struct ring_sources *parent; // Reference to the containing instance.

   /**
    * @brief Clear bytes from the source with the given ID.
    *
    * @param id The source ID to clear bytes from.
    * @param byte_count The number of bytes to clear.
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*clear_bytes)(const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t byte_count);

   /**
    * @brief Copy bytes from the source with the given ID.
    *
    * @param id The source ID to copy bytes from.
    * @param max_bytes The maximum number of bytes to copy to the destination buffer.
    * @param dest Pointer to the destination buffer.
    * @param copied_bytes Pointer to store the number of bytes actually copied. Zero indicates no data was available.
    * @return result_t indicating the success or failure of the function.
    */
   result_t (*copy_bytes)(
      const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t max_bytes, uint8_t *dest, uint16_t *copied_bytes);

} ring_sources_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // RING_SOURCES_INTERFACE_H_