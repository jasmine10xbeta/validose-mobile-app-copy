/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef BUZZER_H_
#define BUZZER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "buzzer_interface.h"
#include "common.h"
#include "nrf_mtx.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Error definitions for this module.
 */
typedef enum
{
   BUZZER_ERROR_NONE = 0,
   BUZZER_ERROR_NULL,
   BUZZER_ERROR_TIMER,
   BUZZER_ERROR_PWM,
   BUZZER_ERROR_MTX_LOCKED,
   BUZZER_ERROR_MAX
} BUZZER_ERROR;

typedef struct buzzer buzzer_t;

struct buzzer
{
   buzzer_interface_t interface;
   nrf_mtx_t _twi_mutex;
   /// Additional internal state variables
   bool _buzzing;
   bool _initialized;
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes the LED control unit.
 *
 * @param self Pointer to the LED control instance. This pointer must not be NULL.
 *
 * @return result_t
 */
result_t buzzer_init(buzzer_t *const self);

#endif // BUZZER_H_
