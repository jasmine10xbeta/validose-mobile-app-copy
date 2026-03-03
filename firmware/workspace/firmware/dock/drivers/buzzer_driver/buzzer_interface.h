/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef BUZZER_INTERFACE_H_
#define BUZZER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct buzzer; /// Forward declaration

typedef struct buzzer_interface buzzer_interface_t;

struct buzzer_interface
{
   struct buzzer *parent; /// Reference to the containing instance.
   /**
    * @brief Retrieves the current state of the buzzer.
    *
    * @param interface The pointer to the buzzer interface.
    * @param is_on A pointer to a boolean variable where the current state will be stored.
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*get_state)(const buzzer_interface_t *const interface, bool *is_on);

   /**
    * @brief Turns the buzzer on.
    *
    * @param interface The pointer to the buzzer interface.
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*set_on)(const buzzer_interface_t *const interface);

   /**
    * @brief Turns the buzzer off.
    *
    * @param interface The pointer to the buzzer interface.
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*set_off)(const buzzer_interface_t *const interface);
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // BUZZER_INTERFACE_H_
