/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @brief This unit detects button presses and generates events. The event can be polled via get_button_events.
 *
 */

#ifndef BUTTON_DRIVER_INTERFACE_H_
#define BUTTON_DRIVER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
#include <stdbool.h>
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define BUTTON_DRIVER_MAX_BUTTON_COUNT (5u) // The maximum number of buttons supported. Update to match use case.
#define BUTTON_DRIVER_MAX_BUTTON_COMBO_COUNT                                                                           \
   (0u) // The maximum number of button long-press combinations that can be detected.

typedef enum
{
   BUTTON_PRESS_NONE = 0,
   BUTTON_PRESS_SHORT,
   BUTTON_PRESS_LONG,
   BUTTON_PRESS_LONG_READ, // Ignore this status, it indicates a long press that has been read but hasn't been released.
   BUTTON_PRESS_MAX
} BUTTON_PRESS;

typedef struct
{
   BUTTON_PRESS button_event;
   bool is_pressed;
} button_status_t;

typedef struct
{
   uint8_t index_a;
   uint8_t index_b;
} button_combo_t;

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct button_driver; // Forward declaration

typedef struct button_driver_interface button_driver_interface_t;

struct button_driver_interface
{
   struct button_driver *parent; // Reference to the containing instance.

   /**
    * @brief Retrieves the current button press events from the button driver.
    *
    * This function is used to poll the button driver for the most current button press events.
    * The function populates an array of button status structs, where each element represents the status of a specific
    * button.
    *
    * @param interface A pointer to the interface instance. This parameter must not be NULL.
    * @param buttons An array of button status structs, where each element represents the status of a specific button.
    * The size of the array must match the number of buttons defined in the button driver interface.
    * This parameter must not be NULL.
    * @param buttons_combos An array of button combo status structs. Same as `buttons`, but for dual-inputs.
    *
    * @return result_t
    */
   result_t (*get_button_events)(const button_driver_interface_t *const interface,
                                 button_status_t *buttons,
                                 button_status_t *buttons_combos);

   /**
    * @brief This function handles the button event because a custom context is required, which cannot be
    *        passed to an NRF event handler
    *
    * @param interface A pointer to the interface instance. This parameter must not be NULL.
    * @param button_number The button number corresponding to the button press
    *
    * @return result_t
    */
   result_t (*handle_button_event)(const button_driver_interface_t *const interface, uint8_t button_number);
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // BUTTON_DRIVER_INTERFACE_H_
