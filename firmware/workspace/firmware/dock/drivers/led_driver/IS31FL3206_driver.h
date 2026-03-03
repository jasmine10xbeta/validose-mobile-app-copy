/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file IS31FL3206_driver.h (for led driver IS31FL3206)
 * @ingroup drivers/led_driver
 * @brief
 */

#ifndef IS31FL3206_DRIVER_H_
#define IS31FL3206_DRIVER_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_timer.h"
#include "nrf_drv_twi.h"
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"
#include "led_driver_interface.h"
#include "result.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// DRIVER SPECIFIC DEFINITIONS
#define MAX_NUM_DRIVERS          (4u)   // Maximum number of LED drivers per project
#define MAX_PWM_BRIGHTNESS_STEPS (128u) // Maximum steps for gamma corrected fading
#define NUM_CHANNELS             (12u)  // Number of LED channels per driver
#define NUM_CHANNELS_PER_LED     (3u)   // Number of channels per LED (r,g,b)

/* The IS31FL3206 LED DRIVER datasheet used for all register addresses and values:
 https://lumissil.com/assets/pdf/core/IS31FL3206_DS.pdf

 Register addresses are prefixed with REG_
 Pre-calculated configurations are prefixed with CFG_ */

#define REG_SHUTDOWN (0x00) // Set software shutdown mode
#define CFG_SHUTDOWN (0x00) // Default D7:D1 = 0000 000 D0 = 0
/* writing 0x01 to REG_SHUTDOWN --> Normal operation
   hardware shutdown: The chip enters hardware shutdown mode when the SDB pin is pulled low. */

#define REG_PWM (0x04)
/* 0x04-0x0F, 12 channels PWM duty cycle data register, default D7:D0 = 0000 0000
 Each register represents one channel, thus one color of one LED. 3 channels (g,r,b) = 1 LED.
 The value of a channel’s PWM Register decides the average output current for each output, OUT1~OUT12 (BRIGHTNESS). */

// The IOUT of each channel is set by the SL bit of LED Control Register (14h~1Fh)
#define REG_LED_CONTROL (0x17)
/* 0x17-0x22, Channel 1 to 12 enable bit and current setting, default D7:d6 = 00 D5:D0 = 00 0000 (0x00)
 The LED Control Registers store the on or off state of each LED and set the output current.

 The numbers in the variables indicate a fraction (number/12 * Imax), where Imax is:
 Maximum global output current -> typically 38mA for Vcc of 4.2V and Vout of 0.8V */
#define LED_CTRL_IMAX    (0x10)
#define LED_CTRL_I_11    (0x11)
#define LED_CTRL_I_9     (0x12)
#define LED_CTRL_I_7     (0x13)
#define LED_CTRL_ON_IMAX (0x50)
#define LED_CTRL_ON_I_7  (0x53)
#define LED_CTRL_ON_I_9  (0x52)
#define LED_CTRL_ON_I_11 (0x51)

#define REG_UPDATE (0x13)
/* Load PWM Register and LED Control Register's data (they are temporary registers), Default 0x00
 A write operation of 0x00 to the Update Register is required to update the registers (04h~0Fh, 17h~22h) */

#define REG_GLOBAL_CONTROL (0x26)
/* Set all channels enable, default 0x00
 G_EN = 0 -> Normal Operation
 G_EN = 1 -> Shutdown all LEDs */

#define REG_OUTP_FREQ_SET (0x27)
/* Set all channels operating frequency, default 0x00
 The Output Frequency Setting Register selects a fixed PWM operating frequency for all output channels.
 OFS = (0x00) -> 24 kHz, = (0x01) -> 3.6 kHZ */

#define REG_RESET (0x2F)
/* Reset all registers to default value, 0x00
 Once user writes 0x00 to the Reset Register, IS31FL3206 will reset all registers.
 On initial power-up, the IS31FL3206 registers are reset to their default values for a blank display. */

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Definition of LED driver errors.
 */
typedef enum
{
   LED_DRV_ERROR_NONE = 0,             // No error
   LED_DRV_ERROR_PTR_NULL,             // Null pointer error
   LED_DRV_ERROR_INIT_FAILURE,         // Failure to initialise driver
   LED_DRV_ERROR_NRF_ERR_CHECK,        // Generic Nordic SDK error
   LED_DRV_ERROR_I2C_WRITE_FAIL,       // I2C write failure
   LED_DRV_ERROR_INVALID_PARAM,        // Invalid function parameter
   LED_DRV_ERROR_INVALID_REG,          // Invalid register address
   LED_DRV_ERROR_DEVICE_NOT_FOUND,     // LED driver not detected
   LED_DRV_ERROR_TIMEOUT,              // Communication timeout
   LED_DRV_ERROR_OVERFLOW,             // Data overflow
   LED_DRV_ERROR_BUSY,                 // Device busy
   LED_DRV_ERROR_UNINITIALIZED,        // Driver not initialized
   LED_DRV_ERROR_TIMERS_INIT,          // Failed to initialize blink and/ or fade timers
   LED_DRV_ERROR_TIMERS_START,         // Failed to initialize blink and/ or fade timers
   LED_DRV_ERROR_COLOR_TYPE_SET,       // Failed to set color of LED
   LED_DRV_ERROR_GENERATE_GAMMA_TABLE, // Failed to set type of LED
   LED_DRV_ERROR_UNKNOWN               // Unknown error
} LED_DRV_ERROR;

// Forward declarations of the structs since they're interdependent.
typedef struct is31fl3206_driver is31fl3206_driver_t;
typedef struct led_timer_context led_timer_context_t;
typedef struct led_timer led_timer_t;

/**
 * @brief Definition of LED driver states.
 */
typedef enum
{
   LED_DRV_UNINITIALIZED = 0,
   LED_DRV_DORMANT,
   LED_DRV_ACTIVE,
   LED_DRV_MAX
} LED_DRV_STATE;

/**
 * @brief Definition of custom APP timer for fading/ blinking LEDs.
 * @param gamma_step The first gamma step in the fading function
 * @param led_on Boolean value indicating status of LED
 * @param stop_timer Boolean value inidacting change to timer status (for blink and fade)
 * @param fade_up Boolean value indicating the direction of fading (Brightness inc-/decreasing)
 * @param pulse Copy of led_config boolean pulse value to pass into timer custom context
 * @param pulse_count To count pulses executed in timer handler
 * @param custom_pulse_count_set Copy of led_config pulse count to pass into timer custom context
 * @param custom_blink_ms Copy of led_config blink interval to pass into timer custom context
 * @param custom_blink_pure_ms Copy of led_config pure blink interval to pass into timer custom context
 * @param custom_blink_on_ms Copy of led_config blink on duration to pass into timer custom context
 * @param custom_repeat_interval_ms Copy of led_config custom repeat interval to pass into timer custom context
 * @param driver_instance Instance of the driver that the timers belong to
 */
struct led_timer_context
{
   uint8_t gamma_step;
   bool led_on;
   bool stop_timer;
   bool fade_up;
   bool pulse;
   uint8_t pulse_count;
   uint8_t custom_pulse_count_set;
   uint32_t custom_blink_ms;
   uint16_t custom_blink_pure_ms;
   uint32_t custom_blink_on_ms;
   uint32_t custom_repeat_interval_ms;
   is31fl3206_driver_t *driver_instance;
};

/**
 * @brief Definition of timer struct to define app timer instance for for fading/ blinking LEDs.
 * @param timer_storage Actual storage for timer
 * @param timer_id Pointer to storage (handle used by SDK)
 * @param led_timer_context LED index (context passed to handler)
 */
struct led_timer
{
   app_timer_t timer_storage;
   app_timer_id_t timer_id;
   led_timer_context_t led_timer_context;
};

/**
 * @brief Definition of LED control (module) instance.
 * @param interface LED driver interface instance
 * @param _i2c_ic_address i2c IC address of LED driver
 * @param _num_gamma_steps Number of steps in the fading function
 * @param _pwm_bool_arr Boolean array to indicate which PWM registers are to be set (corresponding with color)
 * @param _brightness_table The gamma table influencing the fading of LEDs
 * @param _driver_state The state of the LED driver
 * @param _initialized The state of the LED driver
 * @param _led_timer_blink App timer to control the bliking rate of LEDs
 * @param _led_timer_custom_blink App timer to control the custom blink rate (variable on/ off times)
 * @param _led_timer_custom_blink_off App timer to control on duration of custom blink rate
 * @param _led_timer_fade App timer to control the fading rate of LEDs
 *
 */
struct is31fl3206_driver
{
   led_driver_interface_t interface;
   uint8_t _i2c_ic_address;
   uint8_t _num_gamma_steps;
   uint8_t _pwm_bool_arr[NUM_CHANNELS + REG_PWM];
   uint8_t _brightness_table[MAX_PWM_BRIGHTNESS_STEPS];
   LED_DRV_STATE _driver_state;
   bool _initialized;
   led_timer_t _led_timer_blink;
   led_timer_t _led_timer_custom_blink;
   led_timer_t _led_timer_custom_blink_off;
   led_timer_t _led_timer_fade;
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief To initialise instance of LED control (module)
 * @param self instance of control module to initialise
 * @param i2c_interface instance of i2c interface to initialise module with
 * @param led_timer_blink app timer instance for for blinking LEDs
 * @param led_timer_custom_blink app timer instance for custom blinking of LEDs
 * @param led_timer_custom_blink_off app timer instance for custom blinking LEDs (handles LED on duration)
 * @param led_timer_fade app timer instance for for fading LEDs
 * @param _num_gamma_steps number of steps in gamma table to set smoothness of fading
 * @param i2c_address i2c ic address passed to specific driver instance
 */

result_t led_control_init(is31fl3206_driver_t *const self,
                          const i2c_driver_interface_t *i2c_interface,
                          led_timer_t led_timer_blink,
                          led_timer_t led_timer_custom_blink,
                          led_timer_t led_timer_custom_blink_off,
                          led_timer_t led_timer_fade,
                          uint8_t _num_gamma_steps,
                          uint8_t i2c_address);

#endif // IS31FL3206_DRIVER_H_
