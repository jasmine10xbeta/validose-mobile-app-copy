/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**

 * @file led_driver_interface.h (for led driver IS31FL3206)
 * @ingroup drivers/led_driver
 * @brief
 */

#ifndef LED_DRIVER_INTERFACE_H_
#define LED_DRIVER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "result.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Predefined LED colors.
 */

typedef enum
{
   LED_COLOR_RED = 0,
   LED_COLOR_GREEN,
   LED_COLOR_BLUE,
   LED_COLOR_AMBER,
   LED_COLOR_WHITE,
   LED_COLOR_TURQUOISE,
   LED_COLOR_MAX
} LED_COLOR;

/**
 * @brief Predefined LED types, i.e. blinking, fading, solid LEDs.
 */

typedef enum
{
   LED_TYPE_SOLID_ON = 0,
   LED_TYPE_BLINK,
   LED_TYPE_CUSTOM_BLINK, // Blinking with variable on/ off durations and/or pulses
   LED_TYPE_FADE,
   LED_TYPE_MAX
} LED_TYPE;

/**
 * @brief Definition of LED driver instance.
 *
 * @param color Color of LED - defined in LED_COLOR enum.
 * @param type Type of LED sequence - defined in LED_TYPE enum.
 * @param blink_interval_ms Interval (in ms) between blinking LED
 * @param blink_pure_ms Blink interval in ms (vs. in app_timer tick count)
 * @param custom_blink_on_duration_ms For custom blink - duration that LED is on for
 * @param custom_pulse_count For custom blink with pulses - number of consecutive pulses
 * @param pulse_repeat_interval_ms For custom blink with pulses - interval between pulses
 * @note e.g. 2 quick pulses (20ms duration, 500ms interval) repeated every 10 seconds
 * @param pulse Boolean variable indicating if the custom blink has pulses
 * @param fade_interval_ms Interval (in ms) between increase/ decrease in brightness of LED during fade
 * @param brightness_pwm_g PWM brightness value of green LED
 * @param brightness_pwm_r PWM brightness value of red LED
 * @param brightness_pwm_b PWM brightness value of blue LED
 */

typedef struct
{
   LED_COLOR color;
   LED_TYPE type;
   uint32_t blink_interval_ms;
   uint16_t blink_pure_ms;
   uint32_t custom_blink_on_duration_ms;
   uint8_t custom_pulse_count;
   bool pulse;
   uint32_t pulse_repeat_interval_ms;
   uint32_t fade_interval_ms;
   uint8_t brightness_pwm_g;
   uint8_t brightness_pwm_r;
   uint8_t brightness_pwm_b;
} led_config_t;

struct is31fl3206_driver; // Forward declaration

typedef struct led_driver_interface led_driver_interface_t;

struct led_driver_interface
{
   struct is31fl3206_driver *parent; // Reference to the containing instance.

   // declaration of all interface functions

   /**
    * @brief Set LED configuration with specific instance variables
    *
    * @param interface The led_driver_interface_t interface handles.
    */
   result_t (*set_led_driver_config)(const led_driver_interface_t *const interface);

   /**
    * @brief Set LED color of specific driver instance - by setting value of PWM register
    * of corresponding driver channels to a certain brightness
    *
    * @param interface The led_driver_interface_t interface handles.
    * @param led_config The led_config_t instance with containing variables, i.e. color
    */
   result_t (*set_led_color_type)(const led_driver_interface_t *const interface, const led_config_t *const led_config);

   /**
    * @brief Push changes to specific LED driver instance's registers - PWM and control registers
    *
    * @param interface The led_driver_interface_t interface handles.
    */
   result_t (*set_led_update)(const led_driver_interface_t *const interface);

   /**
    * @brief Push changes to specific LED driver instance's registers - PWM and control registers
    *
    * @param interface The led_driver_interface_t interface handles.
    * @param led_config The led_config_t instance with containing variables, i.e. color
    */
   result_t (*led_blink_timers_start)(const led_driver_interface_t *const interface,
                                      const led_config_t *const led_config);

   /**
    * @brief Push changes to specific LED driver instance's registers - PWM and control registers
    *
    * @param interface The led_driver_interface_t interface handles.
    * @param led_config The led_config_t instance with containing variables, i.e. color
    */
   result_t (*led_custom_blink_timers_start)(const led_driver_interface_t *const interface,
                                             const led_config_t *const led_config);

   /**
    * @brief Push changes to specific LED driver instance's registers - PWM and control registers
    *
    * @param interface The led_driver_interface_t interface handles.
    * @param led_config The led_config_t instance with containing variables, i.e. color
    */
   result_t (*led_fade_timers_start)(const led_driver_interface_t *const interface,
                                     const led_config_t *const led_config);

   /**
    * @brief Generate table of brightness levels for fading using gamma correction
    *
    * @param interface The led_driver_interface_t interface handles.
    * @param led_config The led_config_t instance with containing variables, i.e. color
    */
   result_t (*generate_gamma_table)(const led_driver_interface_t *const interface,
                                    const led_config_t *const led_config);
   /**
    * @brief Set of all LEDs from notifications module
    *
    * @param interface The led_driver_interface_t interface handles.
    */
   result_t (*set_off_all)(const led_driver_interface_t *const interface);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // LED_DRIVER_INTERFACE_H_
