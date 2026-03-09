/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file system_time_mock.h
 * @ingroup mock/modules/system_time
 * @brief Header file for the mock system time module for unit testing
 */

#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"
#include "system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   MOCK_SYSTEM_TIME_ERROR_NONE = SYSTEM_TIME_ERROR_MAX, /**< No error. Starts after the real errors */

   MOCK_SYSTEM_TIME_ERROR_NULL, /**< Unexpected NULL reference */

   MOCK_SYSTEM_TIME_ERROR_MAX /**< Sentinel value */
} MOCK_SYSTEM_TIME_ERROR;

/**
 * @brief System time module instance
 */
typedef struct system_time system_time_t;
struct system_time
{
   // Interface
   system_time_interface_t interface;

   // Private data
   uint64_t _current_time_ms;
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initializes an instance of the mock system time module
 *
 * @param[in,out] p_self Pointer to the instance to initialize
 *
 * @return Status code indicating the result of the operation
 */
result_t system_time_init(system_time_t *const p_self);

#endif /* MOCK_SYSTEM_TIME_H */