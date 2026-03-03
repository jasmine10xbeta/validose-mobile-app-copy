/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup system_time_module System Time Module
 * @ingroup modules
 * @brief Keeps track of time since the system was initialized.
 * @details
 *
 * @file system_time.h
 * @ingroup system_time_module
 * @brief
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

typedef struct system_time system_time_t;

struct system_time
{
   system_time_interface_t interface;
   uint64_t _current_time_ms;
   bool _initialized;
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initializes the system time module.
 *
 * This function initializes the system time module and sets the current time to zero.
 *
 * @param self A pointer to the system time module instance.
 *
 * @return A result indicating the success or failure of the initialization.
 */
result_t system_time_init(system_time_t *const self);

#endif // SYSTEM_TIME_INTERFACE_H