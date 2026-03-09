/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef BUTTON_DRIVER_H_
#define BUTTON_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "app_error.h"
#include "app_timer.h"
#include "button_driver_interface.h"
#include "common.h"
#include "nrf_drv_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
// @note This macro flips the polarity of the button state based on its hardware configuration.
// It is used to determine a button push vs. a button release, depending on the pull configuration, (which is part of
// the button_t struct) and set when setting up the button configuration in the module that makes use of this driver.
#define BUTTON_IS_PRESSED(pull_config, pin)                                                                            \
   ((NRF_GPIO_PIN_PULLUP == (pull_config))                                           ? !(nrf_gpio_pin_read((pin))) :   \
    (NRF_GPIO_PIN_PULLDOWN == (pull_config) || NRF_GPIO_PIN_NOPULL == (pull_config)) ? (nrf_gpio_pin_read((pin))) :    \
                                                                                       (nrf_gpio_pin_read((pin))))
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this module.
 */
typedef enum
{
   BUTTON_DRIVER_ERROR_NONE = 0,
   BUTTON_DRIVER_ERROR_PTR_NULL,
   BUTTON_DRIVER_ERROR_TIMER_INIT,
   BUTTON_DRIVER_ERROR_TIMER,
   BUTTON_DRIVER_ERROR_MAX
} BUTTON_DRIVER_ERROR;

typedef struct
{
   uint8_t index;
   uint8_t pin;
   BUTTON_PRESS press_type;
   nrf_gpio_pin_pull_t pull_config;
   bool is_pressed;
} button_t;

typedef struct
{
   uint8_t index;
   BUTTON_PRESS button_a_press_type;
   BUTTON_PRESS button_b_press_type;
} button_combo_state_t;

// Forward declarations of the structs since they're interdependent.
typedef struct button_driver button_driver_t;
typedef struct button_timer_context button_timer_context_t;
typedef struct button_timer button_timer_t;

/**
 * @brief Definition of custom APP timer for button press
 * @param
 *
 */
struct button_timer_context
{
   uint8_t button_number;
   button_driver_t *driver_instance;
};

/**
 * @brief Definition of timer struct to define app timer instance for button press
 * @param
 *
 */
struct button_timer
{
   app_timer_t timer_storage;
   app_timer_id_t timer_id;
   button_timer_context_t button_timer_context;
};

struct button_driver
{
   button_driver_interface_t interface;
   const uint8_t *_button_pins;
   uint8_t _num_buttons_used;
   button_t _buttons[BUTTON_DRIVER_MAX_BUTTON_COUNT];
   const button_combo_t *_button_combos;
   button_combo_state_t
      _button_combo_states[BUTTON_DRIVER_MAX_BUTTON_COMBO_COUNT]; // Internal buffer for button_combo_t struct.
   uint8_t _num_buttons_combos_used;
   button_timer_t _short_press_timers[BUTTON_DRIVER_MAX_BUTTON_COUNT];
   button_timer_t _long_press_timers[BUTTON_DRIVER_MAX_BUTTON_COUNT];
   button_timer_t _combo_press_timers[BUTTON_DRIVER_MAX_BUTTON_COUNT];

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
 * @details The driver includes a feature for handling multiple concurrent button presses referred to as button combos.
 *
 * @param self Pointer to the LED control instance. This pointer must not be NULL.
 * @param number_of_buttons_used The number of buttons used. Must be <= `BUTTON_DRIVER_MAX_BUTTON_COUNT`.
 * @param button_pins The array of pins numbers corresponding to the button GPIO.
 * @param number_of_button_combos Number of button combination presses used. Must be <=
 * `BUTTON_DRIVER_MAX_BUTTON_COMBO_COUNT`. Set to zero if this feature is not needed.
 * @param button_combos Button combination definitions. Contains the indexes of @p button_pins  for the button pins
 * monitored for simultaneous presses. Set to NULL if this feature is not needed.
 * @param pull_config NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_PULLUP.
 *
 * @note The @p button_pins array and @p button_combos array passed to the @p button_driver_init() function needs to be
 * declared static. This driver does **NOT** copy the values. It simply keeps a pointer to the array.
 *
 * @return result_t
 */
result_t button_driver_init(
   button_driver_t *const self,
   uint8_t number_of_buttons_used,
   const uint8_t *button_pins,
   uint8_t number_of_button_combos,
   const button_combo_t *button_combos,
   nrf_gpio_pin_pull_t
      pull_config); // Todo in device firmware V1.1: nrf_gpio_pin_pull_t refers to internal pull-up or downs
                    // implemented inside the MCU and is usually set during the pin
                    // configuration. In many cases this pull down/up is implemented
                    // external to the MCU, as is the case with Validose. The nrf_gpio_pin_pull_t here needs to change
                    // to something else that's not tied to the SDK, something like a bool is_active_high.

#endif // BUTTON_DRIVER_H_
