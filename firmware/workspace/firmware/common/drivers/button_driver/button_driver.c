/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "button_driver.h"
#include "custom_board.h"
#include "debug.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_timer.h"
#include <stdbool.h>
#include <stdint.h>

static const uint8_t THIS_UNIT_ID = SW_UNIT_ID_BUTTON_DRIVER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define SHORT_PRESS_DURATION_MS (200u)  // Short press duration in milliseconds
#define LONG_PRESS_DURATION_MS  (1000u) // Long press duration in milliseconds
#define COMBO_PRESS_DURATION_MS                                                                                        \
   (0u) // Max time elapsed, in milliseconds, between individual button events below which they are counted as a combo
        // presss
#define DEBOUNCE_DURATION_MS (50u) // Debounce time in milliseconds // was 50u

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

// Interface functions
static result_t get_button_events(const button_driver_interface_t *const interface,
                                  button_status_t *buttons,
                                  button_status_t *buttons_combos);
static result_t handle_button_event(const button_driver_interface_t *const interface, uint8_t button_number);
// Other
static result_t init_buttons(button_driver_t *const driver_instance, nrf_gpio_pin_pull_t pull_config);
static void debounce_timer_handler(void *p_context);
static void long_press_timer_handler(void *p_context);
static void combo_press_timer_handler(void *p_context);
static void update_combo_button_press_state(button_driver_t *driver_instance,
                                            button_timer_context_t *context,
                                            BUTTON_PRESS press_type);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static result_t init_buttons(button_driver_t *const driver_instance, nrf_gpio_pin_pull_t pull_config)
{
   RETURN_ERR_IF_NULL(driver_instance, BUTTON_DRIVER_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   for(uint8_t button_number = 0; button_number < driver_instance->_num_buttons_used; button_number++)
   {
      driver_instance->_buttons[button_number].index = button_number;
      driver_instance->_buttons[button_number].pin = driver_instance->_button_pins[button_number];
      driver_instance->_buttons[button_number].is_pressed = false;
      driver_instance->_buttons[button_number].press_type = BUTTON_PRESS_NONE;
      driver_instance->_buttons[button_number].pull_config = pull_config;
   }

   // Init button combo states and timers
   for(uint8_t button_combo_number = 0; button_combo_number < driver_instance->_num_buttons_combos_used;
       button_combo_number++)
   {
      driver_instance->_button_combo_states[button_combo_number].index = button_combo_number;
      driver_instance->_button_combo_states[button_combo_number].button_a_press_type = BUTTON_PRESS_NONE;
      driver_instance->_button_combo_states[button_combo_number].button_b_press_type = BUTTON_PRESS_NONE;
   }

   return result;
}

static void debounce_timer_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);

   button_timer_context_t *context = (button_timer_context_t *)p_context;
   button_driver_t *driver_instance = context->driver_instance;

   ret_code_t err_code = NRF_SUCCESS;

   uint8_t button_number = context->button_number;
   nrf_gpio_pin_pull_t pull_config = driver_instance->_buttons[button_number].pull_config;
   uint8_t pin = driver_instance->_buttons[button_number].pin;

   bool pin_state = BUTTON_IS_PRESSED(pull_config, pin);

   if(pin_state != driver_instance->_buttons[button_number].is_pressed)
   {
      driver_instance->_buttons[button_number].is_pressed = pin_state;

      if(pin_state) // button is pressed
      {
         context->button_number = driver_instance->_buttons[button_number].index;
// Button is pressed; start the long press timer
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
         err_code = app_timer_start(driver_instance->_long_press_timers[button_number].timer_id,
                                    APP_TIMER_TICKS(LONG_PRESS_DURATION_MS),
                                    &driver_instance->_long_press_timers[button_number]
                                        .button_timer_context); // NOSONAR - Casting away volatile. Can't modify SDK
                                                                // function prototype. Confirmed safe.
#pragma GCC diagnostic pop

         if(err_code != NRF_SUCCESS)
         {
            DEBUG_ERROR("Failed to start long press timer for button %u", button_number);
         }
      }
      else // Button is released
      {
         // Determine if it was a short press
         if(BUTTON_PRESS_NONE == driver_instance->_buttons[button_number].press_type)
         {
            DEBUG_INFO("Button short press %u", button_number);
            driver_instance->_buttons[button_number].press_type = BUTTON_PRESS_SHORT;
            update_combo_button_press_state(driver_instance, context, BUTTON_PRESS_SHORT);
         }
         else if(BUTTON_PRESS_LONG_READ == driver_instance->_buttons[button_number].press_type)
         {
            // Button is released, and it was from a long press that was read before it was released. Clear press
            // type.
            driver_instance->_buttons[button_number].press_type = BUTTON_PRESS_NONE;
         }

         err_code = app_timer_stop(driver_instance->_long_press_timers[button_number].timer_id);

         if(err_code != NRF_SUCCESS)
         {
            DEBUG_ERROR("Failed to stop long press timer for button %u", button_number);
         }
      }
   }
}

static void long_press_timer_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);
   button_timer_context_t *context = (button_timer_context_t *)p_context;
   button_driver_t *driver_instance = context->driver_instance;
   uint8_t button_number = context->button_number;

   nrf_gpio_pin_pull_t pull_config = driver_instance->_buttons[button_number].pull_config;
   uint8_t pin = driver_instance->_buttons[button_number].pin;

   bool pin_state = BUTTON_IS_PRESSED(pull_config, pin);

   if(pin_state) // button is still being pressed after timer elapse (i.e. it is a long press)
   {
      driver_instance->_buttons[button_number].press_type = BUTTON_PRESS_LONG;
      DEBUG_INFO("Button long press %u", button_number);
      update_combo_button_press_state(driver_instance, context, BUTTON_PRESS_LONG);
   }
}

static void combo_press_timer_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);

   button_timer_context_t *context = (button_timer_context_t *)p_context;
   button_driver_t *driver_instance = context->driver_instance;

   uint8_t button_combo_number = context->button_number;

   // If combo press button states don't match, reset state of both
   if(driver_instance->_button_combo_states[button_combo_number].button_a_press_type
      != driver_instance->_button_combo_states[button_combo_number].button_b_press_type)
   {
      driver_instance->_button_combo_states[button_combo_number].button_a_press_type = BUTTON_PRESS_NONE;
      driver_instance->_button_combo_states[button_combo_number].button_b_press_type = BUTTON_PRESS_NONE;
   }
   else
   {
      DEBUG_INFO("Combo press detected");
   }
}

static result_t handle_button_event(const button_driver_interface_t *const interface, uint8_t button_number)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BUTTON_DRIVER_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   button_driver_t *driver_instance = interface->parent;

   ret_code_t err_code = NRF_SUCCESS;

   // Start debounce timer
   err_code = app_timer_start(driver_instance->_short_press_timers[button_number].timer_id,
                              APP_TIMER_TICKS(DEBOUNCE_DURATION_MS),
                              &driver_instance->_short_press_timers[button_number]
                                  .button_timer_context); // NOSONAR - Casting away volatile. Can't modify SDK
                                                          // function prototype. Confirmed safe.

   UPDATE_IF_NRF_ERR(err_code, result, BUTTON_DRIVER_ERROR_TIMER);

   return result;
}

static void update_combo_button_press_state(button_driver_t *driver_instance,
                                            button_timer_context_t *context,
                                            BUTTON_PRESS press_type)
{
   RETURN_VOID_IF_NULL(driver_instance);
   RETURN_VOID_IF_NULL(context);

   uint8_t button_number = context->button_number;

   for(uint8_t button_combo_number = 0; button_combo_number < driver_instance->_num_buttons_combos_used;
       button_combo_number++)
   {
      bool was_combo_start_already_detected
         = (driver_instance->_button_combo_states[button_combo_number].button_a_press_type != BUTTON_PRESS_NONE)
           || (driver_instance->_button_combo_states[button_combo_number].button_b_press_type != BUTTON_PRESS_NONE);
      bool is_combo_start_now_detected = false;

      if(button_number == driver_instance->_button_combos[button_combo_number].index_a)
      {
         driver_instance->_button_combo_states[button_combo_number].button_a_press_type = press_type;
         is_combo_start_now_detected = true;
      }

      if(button_number == driver_instance->_button_combos[button_combo_number].index_b)
      {
         driver_instance->_button_combo_states[button_combo_number].button_b_press_type = press_type;
         is_combo_start_now_detected = true;
      }

      if((false == was_combo_start_already_detected) && is_combo_start_now_detected)
      {
// Start combo timer
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
         ret_code_t err_code
            = app_timer_start(driver_instance->_combo_press_timers[button_combo_number].timer_id,
                              APP_TIMER_TICKS(COMBO_PRESS_DURATION_MS),
                              &driver_instance->_combo_press_timers[button_combo_number].button_timer_context);
#pragma GCC diagnostic pop

         if(err_code != NRF_SUCCESS)
         {
            DEBUG_ERROR("Failed to start combo timer for button combo %u", button_combo_number);
         }
      }
   }
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t get_button_events(const button_driver_interface_t *const interface,
                                  button_status_t *buttons,
                                  button_status_t *buttons_combos)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BUTTON_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(buttons, BUTTON_DRIVER_ERROR_PTR_NULL);
   // No NULL check for the button_combos, according to VOD documentation. Combos are optional and may be NULL.
   result_t result = RESULT_OK;

   for(uint8_t button_number = 0; button_number < interface->parent->_num_buttons_used; button_number++)
   {
      buttons[button_number].button_event = interface->parent->_buttons[button_number].press_type;
      buttons[button_number].is_pressed = interface->parent->_buttons[button_number].is_pressed;

      if(BUTTON_PRESS_LONG == interface->parent->_buttons[button_number].press_type)
      {
         interface->parent->_buttons[button_number].press_type = BUTTON_PRESS_LONG_READ; // Cleared in debounce timer.
      }
      if(BUTTON_PRESS_SHORT == interface->parent->_buttons[button_number].press_type)
      {
         // Clear event
         interface->parent->_buttons[button_number].press_type = BUTTON_PRESS_NONE;
      }
   }

   for(uint8_t button_combo_number = 0; button_combo_number < interface->parent->_num_buttons_combos_used;
       button_combo_number++)
   {
      // If both buttons have the same press event, and that event is not `BUTTON_PRESS_NONE`
      if((BUTTON_PRESS_NONE != interface->parent->_button_combo_states[button_combo_number].button_a_press_type)
         && (interface->parent->_button_combo_states[button_combo_number].button_a_press_type
             == interface->parent->_button_combo_states[button_combo_number].button_b_press_type))
      {
         buttons_combos[button_combo_number].button_event
            = interface->parent->_button_combo_states[button_combo_number].button_a_press_type;
         buttons_combos[button_combo_number].is_pressed = true;

         if(BUTTON_PRESS_SHORT == interface->parent->_button_combo_states[button_combo_number].button_a_press_type)
         {
            DEBUG_INFO("SHORT BUTTON COMBO %u", button_combo_number);
         }
         else
         {
            DEBUG_INFO("LONG BUTTON COMBO %u", button_combo_number);
         }

         // Clear event
         interface->parent->_button_combo_states[button_combo_number].button_a_press_type = BUTTON_PRESS_NONE;
         interface->parent->_button_combo_states[button_combo_number].button_b_press_type = BUTTON_PRESS_NONE;
      }
      else
      {
         buttons_combos[button_combo_number].button_event = BUTTON_PRESS_NONE;
         buttons_combos[button_combo_number].is_pressed = false;
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t button_driver_init(button_driver_t *const self,
                            uint8_t number_of_buttons_used,
                            const uint8_t *button_pins,
                            uint8_t number_of_button_combos,
                            const button_combo_t *button_combos,
                            nrf_gpio_pin_pull_t pull_config)
{
   // Todo: change the init function so that it copies the button pins so that you don't need to keep a static array
   // somewhere outside of this unit - One less mistake that can be made by a developer.
   result_t result = RESULT_OK;

   RETURN_ERR_IF_NULL(self, BUTTON_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(button_pins, BUTTON_DRIVER_ERROR_PTR_NULL);
   // No NULL check for the button_combos, according to VOD documentation. Combos are optional and may be NULL.

   self->_initialized = false;
   self->interface.parent = self;

   self->_button_pins = button_pins;
   self->_num_buttons_used = number_of_buttons_used;
   self->_button_combos = button_combos;
   self->_num_buttons_combos_used = number_of_button_combos;

   static button_timer_t short_press_timers[BUTTON_DRIVER_MAX_BUTTON_COUNT] = {0};
   static button_timer_t long_press_timers[BUTTON_DRIVER_MAX_BUTTON_COUNT] = {0};
   static button_timer_t combo_press_timers[BUTTON_DRIVER_MAX_BUTTON_COUNT] = {0};

   // Init Timers

   for(uint8_t button_number = 0; button_number < self->_num_buttons_used; button_number++)
   {
      self->_short_press_timers[button_number] = short_press_timers[button_number];
      self->_long_press_timers[button_number] = long_press_timers[button_number];
      self->_combo_press_timers[button_number] = combo_press_timers[button_number];
      self->_short_press_timers[button_number].button_timer_context.driver_instance = self;
      self->_long_press_timers[button_number].button_timer_context.driver_instance = self;
      self->_combo_press_timers[button_number].button_timer_context.driver_instance = self;
      self->_short_press_timers[button_number].timer_id = &self->_short_press_timers[button_number].timer_storage;
      self->_long_press_timers[button_number].timer_id = &self->_long_press_timers[button_number].timer_storage;
      self->_combo_press_timers[button_number].timer_id = &self->_combo_press_timers[button_number].timer_storage;
      self->_short_press_timers[button_number].button_timer_context.button_number = button_number;
      self->_long_press_timers[button_number].button_timer_context.button_number = button_number;
      self->_combo_press_timers[button_number].button_timer_context.button_number = button_number;
   }

   for(uint8_t button_number = 0; button_number < self->_num_buttons_used; button_number++)
   {
      // Create the timers
      // Create debounce timer
      ret_code_t error_code = app_timer_create(
         &self->_short_press_timers[button_number].timer_id, APP_TIMER_MODE_SINGLE_SHOT, debounce_timer_handler);

      UPDATE_IF_NRF_ERR(error_code, result, BUTTON_DRIVER_ERROR_TIMER_INIT);

      // Create long press timer
      if(IS_OK(result))
      {
         error_code = app_timer_create(
            &self->_long_press_timers[button_number].timer_id, APP_TIMER_MODE_SINGLE_SHOT, long_press_timer_handler);
         UPDATE_IF_NRF_ERR(error_code, result, BUTTON_DRIVER_ERROR_TIMER_INIT);
      }

      // Create combo timer
      if(IS_OK(result))
      {
         error_code = app_timer_create(
            &self->_combo_press_timers[button_number].timer_id, APP_TIMER_MODE_SINGLE_SHOT, combo_press_timer_handler);
         UPDATE_IF_NRF_ERR(error_code, result, BUTTON_DRIVER_ERROR_TIMER_INIT);
      }

      BREAK_ON_ERR(result);
   }

   // initialize all interface pointers to point to internal static functions by default
   self->interface.get_button_events = get_button_events;
   self->interface.handle_button_event = handle_button_event;

   result = init_buttons(self, pull_config);

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}