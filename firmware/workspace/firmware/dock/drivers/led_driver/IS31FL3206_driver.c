/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file IS31FL3206_driver.c
 * @ingroup drivers/led_driver
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_timer.h"
#include "math.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "result.h"

#include "IS31FL3206_driver.h"
#include "i2c_driver_interface.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_PROX_SENSOR_TMD2635_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
// User configureable
#define TWI_DELAY_MS                                                                                                   \
   (10u) // Delay value experimentally determined. Driver I2C is slow - communication with multiple individual channels
#define I2C_TIMEOUT_US_PER_BYTE (1000u) /// For 400kHz frequency with 40x safety factor

#define MAX_BRIGHTNESS_WHITE (128.0f) // 50%, OR 160.0f (63%) OR brighter option = 192.0f (75%)
#define FIRST_REGISTER_RED   (REG_PWM)
#define FIRST_REGISTER_BLUE  (REG_PWM + 2u)
#define FIRST_REGISTER_GREEN (REG_PWM + 1u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef enum
{
   LED_I2C_STAT_OK = 0,
   LED_I2C_STAT_TX_ERR,
   LED_I2C_STAT_RX_ERR,
   LED_I2C_STAT_MAX
} LED_I2C_STAT;

typedef enum
{
   APP_TIMER_STAT_OK = 0,
   APP_TIMER_STAT_ERR,
   APP_TIMER_STAT_MAX
} APP_TIMER_STAT;

// structs for write to REG_PWM and REG_LED_CONTROL, with uint8_t register address and uint8_t data variable

typedef struct __attribute__((packed, aligned(1)))
{
   uint8_t address;
   uint8_t data;
} led_drv_reg_write_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions

static result_t set_led_driver_config(const led_driver_interface_t *const interface);

static result_t set_led_color_type(const led_driver_interface_t *const interface, const led_config_t *const led_config);

static result_t set_led_update(const led_driver_interface_t *const interface);

static result_t led_blink_timers_start(const led_driver_interface_t *const interface,
                                       const led_config_t *const led_config);

static result_t led_custom_blink_timers_start(const led_driver_interface_t *const interface,
                                              const led_config_t *const led_config);

static result_t led_fade_timers_start(const led_driver_interface_t *const interface,
                                      const led_config_t *const led_config);

static result_t generate_gamma_table(const led_driver_interface_t *const interface,
                                     const led_config_t *const led_config);

static result_t set_off_all(const led_driver_interface_t *const interface);

// Internal (non-interface) functions
static result_t send_led_type(uint8_t i2c_ic_address_type,
                              const led_config_t *const led_config,
                              const uint8_t *_pwm_bool_arr,
                              is31fl3206_driver_t *driver_instance);

static void led_blink_handler(void *p_context);
static void led_custom_blink_handler(void *p_context);
static void led_custom_blink_off_handler(void *p_context);
static void led_fade_handler(void *p_context);

static result_t clear_all_registers(uint8_t i2c_ic_address_clear_all);
static result_t set_on(uint8_t i2c_ic_address_on, const uint8_t *_pwm_bool_arr);
static result_t set_off(uint8_t i2c_ic_address_off, const uint8_t *_pwm_bool_arr);
static result_t send_fade_brightness(uint8_t brightness, uint8_t i2c_ic_address_fade, const uint8_t *_pwm_bool_arr);
static result_t update_registers_fade_blink(uint8_t i2c_ic_address_update);
static result_t do_fade_up(is31fl3206_driver_t *driver_instance, uint8_t gamma_step);
static result_t do_fade_down(is31fl3206_driver_t *driver_instance, uint8_t gamma_step);
static result_t do_blink(is31fl3206_driver_t *driver_instance, bool intrnal_led_on);
static result_t do_custom_blink(is31fl3206_driver_t *driver_instance, led_timer_context_t *context);
static result_t handle_pulse(is31fl3206_driver_t *driver_instance, led_timer_context_t *context);

static result_t led_drv_reg_write(uint8_t i2c_address, uint8_t register_address, uint8_t data);
static result_t led_drv_reg_write_all(uint8_t i2c_address, uint8_t register_address_start, const uint8_t *data_buffer);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
typedef uint32_t m_ret_code_t;

// i2c interface shared by all driver instances
static const i2c_driver_interface_t *mp_i2c_driver_interface;

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

// NOTE: Led driver I2C only supports write operations

/**
 * @brief Writes data to an led driver register via I2c
 *
 * Facilitates less verbose code when setting single registers
 *
 * @param i2c_address      - Address of the LED Driver on the I2c bus
 * @param register_address - Register address that will be written to
 * @param data             - Data that will be written to the address
 *
 * @return indication whether the write was successful
 */

static result_t led_drv_reg_write(uint8_t i2c_address, uint8_t register_address, uint8_t data)
{
   led_drv_reg_write_t tx_data = {0};

   // Write the register address and data into transmit buffer
   tx_data.address = register_address; // Address is 8 bit wide
   tx_data.data = data;

   uint8_t bytes = sizeof(tx_data);
   uint32_t wait_us = I2C_TIMEOUT_US_PER_BYTE * sizeof(tx_data);

   // Transmit the data over TWI Bus
   result_t result
      = mp_i2c_driver_interface->transmit(mp_i2c_driver_interface, i2c_address, ((uint8_t *)&tx_data), bytes, wait_us);

   UPDATE_ERR(result, LED_I2C_STAT_TX_ERR);

   if(IS_OK(result))
   {
      nrf_delay_ms(TWI_DELAY_MS);
   }

   return result;
}

/**
 * @brief Writes data to multiple led driver registers via I2c
 *
 * Facilitates less verbose code when setting multiple registers
 * uses address auto increment to write multiple bytes of data to consecutive registers
 *
 * @param i2c_address      - Address of the LED Driver on the I2c bus
 * @param register_address_start - First register address that will be written to
 * @param data_buffer             - Data that will be written to consecutive addresses as a buffer
 *
 * @return indication whether the write was successful
 */

static result_t led_drv_reg_write_all(uint8_t i2c_address, uint8_t register_address_start, const uint8_t *data_buffer)
{
   RETURN_ERR_IF_NULL(data_buffer, LED_DRV_ERROR_PTR_NULL);

   uint8_t data_buffer_send[NUM_CHANNELS + 1u];
   data_buffer_send[0] = register_address_start; // First register in consecutive sequence
   memcpy(&data_buffer_send[1u], data_buffer, NUM_CHANNELS);

   uint8_t bytes = sizeof(data_buffer_send);
   uint32_t wait_us = I2C_TIMEOUT_US_PER_BYTE * sizeof(data_buffer_send);

   // Transmit the data over TWI Bus
   result_t result
      = mp_i2c_driver_interface->transmit(mp_i2c_driver_interface, i2c_address, data_buffer_send, bytes, wait_us);

   UPDATE_ERR(result, LED_I2C_STAT_TX_ERR);

   if(IS_OK(result))
   {
      nrf_delay_ms(TWI_DELAY_MS);
   }

   return result;
}

// Timer Callback for LED Custom Blink Updates
static void led_custom_blink_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);

   led_timer_context_t *context = (led_timer_context_t *)p_context;
   is31fl3206_driver_t *driver_instance = context->driver_instance;

   if(true == context->pulse)
   {
      handle_pulse(driver_instance, context);
   }
   else
   {
      do_custom_blink(driver_instance, context);
   }
}

// Timer Callback for LED Custom Blink On Duration Handling
static void led_custom_blink_off_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);

   led_timer_context_t *context = (led_timer_context_t *)p_context;
   is31fl3206_driver_t *driver_instance = context->driver_instance;

   result_t result = set_off(driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);
   IF_OK_RUN_AND_UPDATE(result, update_registers_fade_blink(driver_instance->_i2c_ic_address));

   if(IS_OK(result))
   {
      driver_instance->_led_timer_custom_blink.led_timer_context.led_on = false;
      // Start the timer
      m_ret_code_t error_code = app_timer_stop(driver_instance->_led_timer_custom_blink_off.timer_id);

      UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);
   }
}

static result_t do_custom_blink(is31fl3206_driver_t *driver_instance, led_timer_context_t *context)
{
   RETURN_ERR_IF_NULL(driver_instance, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(context, LED_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   result = set_on(driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);

   if(IS_OK(result))
   {
      driver_instance->_led_timer_custom_blink.led_timer_context.led_on = true;
      // Start the timer
      m_ret_code_t error_code
         = app_timer_start(driver_instance->_led_timer_custom_blink_off.timer_id,
                           driver_instance->_led_timer_custom_blink_off.led_timer_context.custom_blink_on_ms,
                           &driver_instance->_led_timer_custom_blink_off.led_timer_context);

      UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);
   }

   IF_OK_RUN_AND_UPDATE(result, update_registers_fade_blink(driver_instance->_i2c_ic_address));

   return result;
}

static result_t handle_pulse(is31fl3206_driver_t *driver_instance, led_timer_context_t *context)
{
   RETURN_ERR_IF_NULL(driver_instance, LED_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;
   uint32_t second_ms = 1000;

   uint8_t compare_pulse_count
      = (uint8_t)(context->custom_repeat_interval_ms * (second_ms / context->custom_blink_pure_ms));

   if(context->pulse_count < context->custom_pulse_count_set)
   {
      result = do_custom_blink(driver_instance, context);

      if(IS_OK(result))
      {
         context->pulse_count++;
      }
   }
   else if(compare_pulse_count == context->pulse_count)
   {
      context->pulse_count = 0;
   }
   else
   {
      context->pulse_count++;
   }

   return result;
}

// Timer Callback for LED Blink Updates
static void led_blink_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);

   led_timer_context_t *context = (led_timer_context_t *)p_context;
   bool led_on = context->led_on;
   is31fl3206_driver_t *driver_instance = context->driver_instance;

   do_blink(driver_instance, led_on);
}

static result_t do_blink(is31fl3206_driver_t *driver_instance, bool led_on)
{
   RETURN_ERR_IF_NULL(driver_instance, LED_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   if(false == led_on)
   {
      result = set_on(driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);

      if(IS_OK(result))
      {
         driver_instance->_led_timer_blink.led_timer_context.led_on = true;
      }
   }
   else if(true == led_on)
   {
      result = set_off(driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);
      if(IS_OK(result))
      {
         driver_instance->_led_timer_blink.led_timer_context.led_on = false;
      }
   }

   IF_OK_RUN_AND_UPDATE(result, update_registers_fade_blink(driver_instance->_i2c_ic_address));

   return result;
}

// Timer Callback for LED fade Updates
static void led_fade_handler(void *p_context) // NOSONAR: The function prototype is dictated by the Nordic SDK
{
   RETURN_VOID_IF_NULL(p_context);

   const led_timer_context_t *context = (led_timer_context_t *)p_context;
   uint8_t gamma_step = context->gamma_step;
   bool fade_up = context->fade_up;
   is31fl3206_driver_t *driver_instance = context->driver_instance;

   if(true == fade_up)
   {
      do_fade_up(driver_instance, gamma_step);
   }
   else
   {
      do_fade_down(driver_instance, gamma_step);
   }

   driver_instance->_led_timer_fade.led_timer_context.led_on = true;
}

static result_t do_fade_up(is31fl3206_driver_t *driver_instance, uint8_t gamma_step)
{
   RETURN_ERR_IF_NULL(driver_instance, LED_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   // Adjust gamma index based on fade direction
   uint8_t send_pwm = 0;

   if(gamma_step < driver_instance->_num_gamma_steps)
   {
      send_pwm = (uint8_t)driver_instance->_brightness_table[gamma_step];
      driver_instance->_led_timer_fade.led_timer_context.gamma_step++;

      // Update LED brightness
      result = send_fade_brightness(send_pwm, driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);

      IF_OK_RUN_AND_UPDATE(result, update_registers_fade_blink(driver_instance->_i2c_ic_address));
   }
   else
   {
      driver_instance->_led_timer_fade.led_timer_context.fade_up = false;
      driver_instance->_led_timer_fade.led_timer_context.gamma_step--;
   }

   return result;
}

static result_t do_fade_down(is31fl3206_driver_t *driver_instance, uint8_t gamma_step)
{
   RETURN_ERR_IF_NULL(driver_instance, LED_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   // Adjust gamma index based on fade direction
   uint8_t send_pwm = 0;

   if(0 < gamma_step)
   {
      send_pwm = (uint8_t)driver_instance->_brightness_table[gamma_step];
      driver_instance->_led_timer_fade.led_timer_context.gamma_step--;

      // Update LED brightness
      result = send_fade_brightness(send_pwm, driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);
      IF_OK_RUN_AND_UPDATE(result, update_registers_fade_blink(driver_instance->_i2c_ic_address));
   }
   else
   {
      // for gamma step of 0 (brightness = 0)
      send_pwm = (uint8_t)driver_instance->_brightness_table[gamma_step];
      driver_instance->_led_timer_fade.led_timer_context.gamma_step--;
      result = send_fade_brightness(send_pwm, driver_instance->_i2c_ic_address, driver_instance->_pwm_bool_arr);
      IF_OK_RUN_AND_UPDATE(result, update_registers_fade_blink(driver_instance->_i2c_ic_address));

      if(IS_OK(result))
      {
         driver_instance->_led_timer_fade.led_timer_context.fade_up = true;
         driver_instance->_led_timer_fade.led_timer_context.gamma_step++;
      }
   }

   return result;
}

// Set registers for LED brightness for bidirectional fading
static result_t send_fade_brightness(uint8_t brightness, uint8_t i2c_ic_address_fade, const uint8_t *_pwm_bool_arr)
{
   RETURN_ERR_IF_NULL(_pwm_bool_arr, LED_DRV_ERROR_PTR_NULL);

   // Send brightness level (pwm value) to the LED driver via I2C, using address auto increment
   uint8_t pwm_data_buffer[NUM_CHANNELS] = {0};
   uint8_t _pwm_bool_arr_receive[NUM_CHANNELS + REG_PWM] = {0};
   memcpy(&_pwm_bool_arr_receive, _pwm_bool_arr, NUM_CHANNELS + REG_PWM);
   bool is_amber = false;

   // Check first three registers to see if the color is amber
   if(1u == pwm_data_buffer[FIRST_REGISTER_RED] && 0 == pwm_data_buffer[FIRST_REGISTER_BLUE]
      && 1u == pwm_data_buffer[FIRST_REGISTER_GREEN])
   {
      is_amber = true;
   }

   for(uint8_t count = 0; count < NUM_CHANNELS; count++)
   {
      if(_pwm_bool_arr_receive[REG_PWM + count] != 0)
      {
         pwm_data_buffer[count] = brightness;
      }
      else
      {
         pwm_data_buffer[count] = 0x00;
      }
   }

   // Fine tune brightness to ensure color stays amber while fading
   if(true == is_amber)
   {
      for(uint8_t count = 0; count < NUM_CHANNELS; count++)
      {
         if((REG_PWM + count) % 3 == 0)
         {
            pwm_data_buffer[count] = (uint8_t)(brightness * 0.5);
         }
      }
   }

   result_t result = led_drv_reg_write_all(i2c_ic_address_fade, REG_PWM, pwm_data_buffer);

   return result;
}

// Set on multiple, specific channels/ registers

static result_t set_on(uint8_t i2c_ic_address_on, const uint8_t *_pwm_bool_arr)
{
   RETURN_ERR_IF_NULL(_pwm_bool_arr, LED_DRV_ERROR_PTR_NULL);

   uint8_t control_data_buffer[NUM_CHANNELS] = {0};
   uint8_t _pwm_bool_arr_receive[NUM_CHANNELS + REG_PWM] = {0};
   memcpy(&_pwm_bool_arr_receive, _pwm_bool_arr, NUM_CHANNELS + REG_PWM);

   for(uint8_t count = 0; count < NUM_CHANNELS; count++)
   {
      if(_pwm_bool_arr_receive[REG_PWM + count] != 0)
      {
         control_data_buffer[count] = LED_CTRL_ON_IMAX;
      }
      else
      {
         control_data_buffer[count] = 0x00;
      }
   }

   result_t result = led_drv_reg_write_all(i2c_ic_address_on, REG_LED_CONTROL, control_data_buffer);

   return result;
}

// Set off multiple specific channels/ registers
static result_t set_off(uint8_t i2c_ic_address_off, const uint8_t *_pwm_bool_arr)
{
   RETURN_ERR_IF_NULL(_pwm_bool_arr, LED_DRV_ERROR_PTR_NULL);

   uint8_t control_data_buffer[NUM_CHANNELS] = {0};
   uint8_t _pwm_bool_arr_receive[NUM_CHANNELS + REG_PWM] = {0};
   memcpy(&_pwm_bool_arr_receive, _pwm_bool_arr, NUM_CHANNELS + REG_PWM);

   for(uint8_t count = 0; count < NUM_CHANNELS; count++)
   {
      if(_pwm_bool_arr_receive[REG_PWM + count] != 0)
      {
         control_data_buffer[count] = 0x00;
      }
   }

   result_t result = led_drv_reg_write_all(i2c_ic_address_off, REG_LED_CONTROL, control_data_buffer);

   return result;
}

// Clear all LED registers related to control and PWM
static result_t clear_all_registers(uint8_t i2c_ic_address_clear_all)
{
   uint8_t control_data_buffer[NUM_CHANNELS] = {0};
   uint8_t pwm_data_buffer[NUM_CHANNELS] = {0};

   for(uint8_t count = 0; count < NUM_CHANNELS; count++)
   {
      control_data_buffer[count] = 0x00;
      pwm_data_buffer[count] = 0x00;
   }

   result_t result = led_drv_reg_write_all(i2c_ic_address_clear_all, REG_LED_CONTROL, control_data_buffer);
   IF_OK_RUN_AND_UPDATE(result, led_drv_reg_write_all(i2c_ic_address_clear_all, REG_PWM, pwm_data_buffer));
   IF_OK_RUN_AND_UPDATE(result, led_drv_reg_write(i2c_ic_address_clear_all, REG_UPDATE, 0x00));

   return result;
}

// Set off multiple channels/ registers
static result_t set_off_all(const led_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);

   uint8_t data_buffer[NUM_CHANNELS] = {0};

   for(uint8_t count = 0; count < NUM_CHANNELS; count++)
   {
      data_buffer[count] = 0x00;
   }

   interface->parent->_led_timer_fade.led_timer_context.gamma_step = 0;
   interface->parent->_led_timer_fade.led_timer_context.led_on = false;
   interface->parent->_led_timer_blink.led_timer_context.led_on = false;
   interface->parent->_led_timer_custom_blink.led_timer_context.led_on = false;
   interface->parent->_led_timer_custom_blink_off.led_timer_context.led_on = false;
   app_timer_stop(interface->parent->_led_timer_blink.timer_id);
   app_timer_stop(interface->parent->_led_timer_custom_blink.timer_id);
   app_timer_stop(interface->parent->_led_timer_custom_blink_off.timer_id);
   app_timer_stop(interface->parent->_led_timer_fade.timer_id);

   result_t result = led_drv_reg_write_all(interface->parent->_i2c_ic_address, REG_LED_CONTROL, data_buffer);
   IF_OK_RUN_AND_UPDATE(result, led_drv_reg_write_all(interface->parent->_i2c_ic_address, REG_PWM, data_buffer));

   if(IS_OK(result))
   {
      memset(interface->parent->_pwm_bool_arr, 0, sizeof(interface->parent->_pwm_bool_arr));
   }

   return result;
}

// Push changes to control and PWM registers
static result_t update_registers_fade_blink(uint8_t i2c_ic_address_update)
{
   result_t result = led_drv_reg_write(i2c_ic_address_update, REG_UPDATE, 0x00);

   return result;
}

static result_t send_led_type(uint8_t i2c_ic_address_type,
                              const led_config_t *const led_config,
                              const uint8_t *_pwm_bool_arr,
                              is31fl3206_driver_t *self)
{
   RETURN_ERR_IF_NULL(led_config, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(_pwm_bool_arr, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(self, LED_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   uint8_t _pwm_bool_arr_send[NUM_CHANNELS + REG_PWM] = {0};
   memcpy(&_pwm_bool_arr_send, _pwm_bool_arr, NUM_CHANNELS + REG_PWM);

   if(LED_TYPE_SOLID_ON == led_config->type)
   {
      result = set_on(i2c_ic_address_type, _pwm_bool_arr_send);
   }
   else if(LED_TYPE_BLINK == led_config->type)
   {
      self->_led_timer_blink.led_timer_context.stop_timer = false;
   }
   else if(LED_TYPE_CUSTOM_BLINK == led_config->type)
   {
      self->_led_timer_custom_blink.led_timer_context.stop_timer = false;
      self->_led_timer_custom_blink_off.led_timer_context.stop_timer = false;

      self->_led_timer_custom_blink.led_timer_context.custom_blink_ms = led_config->blink_interval_ms;
      self->_led_timer_custom_blink.led_timer_context.custom_blink_pure_ms = led_config->blink_pure_ms;
      self->_led_timer_custom_blink.led_timer_context.custom_pulse_count_set = led_config->custom_pulse_count;
      self->_led_timer_custom_blink.led_timer_context.custom_repeat_interval_ms = led_config->pulse_repeat_interval_ms;
      self->_led_timer_custom_blink.led_timer_context.pulse = led_config->pulse;

      self->_led_timer_custom_blink_off.led_timer_context.custom_blink_on_ms = led_config->custom_blink_on_duration_ms;
      self->_led_timer_custom_blink_off.led_timer_context.pulse = led_config->pulse;
   }
   else if(LED_TYPE_FADE == led_config->type)
   {
      self->_led_timer_fade.led_timer_context.stop_timer = false;
      result = set_on(i2c_ic_address_type, _pwm_bool_arr_send);
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t set_led_driver_config(const led_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);

   is31fl3206_driver_t *self = interface->parent;

   clear_all_registers(self->_i2c_ic_address);

   // SET DRIVER TO KNOWN STATE (NORMAL OPERATION)
   result_t result = led_drv_reg_write(self->_i2c_ic_address, REG_SHUTDOWN, 0x01);

   // CONFIGURE PWM FREQ
   IF_OK_RUN_AND_UPDATE(result, led_drv_reg_write(self->_i2c_ic_address, REG_OUTP_FREQ_SET, 0x00));

   IF_OK_RUN_AND_UPDATE(result, led_drv_reg_write(self->_i2c_ic_address, REG_GLOBAL_CONTROL, 0x00));

   return result;
}

// To start fade and blink app timers
static result_t led_blink_timers_start(const led_driver_interface_t *const interface,
                                       const led_config_t *const led_config)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(led_config, LED_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   is31fl3206_driver_t *self = interface->parent;

   // Start the timer
   m_ret_code_t error_code = app_timer_start(
      self->_led_timer_blink.timer_id, led_config->blink_interval_ms, &self->_led_timer_blink.led_timer_context);

   UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);

   return result;
}

static result_t led_custom_blink_timers_start(const led_driver_interface_t *const interface,
                                              const led_config_t *const led_config)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(led_config, LED_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   is31fl3206_driver_t *self = interface->parent;

   // Start the timer
   m_ret_code_t error_code = app_timer_start(self->_led_timer_custom_blink.timer_id,
                                             led_config->blink_interval_ms,
                                             &self->_led_timer_custom_blink.led_timer_context);

   UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);

   return result;
}

static result_t led_fade_timers_start(const led_driver_interface_t *const interface,
                                      const led_config_t *const led_config)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(led_config, LED_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   is31fl3206_driver_t *self = interface->parent;

   // Start the timer
   m_ret_code_t error_code = app_timer_start(
      self->_led_timer_fade.timer_id, led_config->fade_interval_ms, &self->_led_timer_fade.led_timer_context);

   UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);

   return result;
}

static result_t generate_gamma_table(const led_driver_interface_t *const interface,
                                     const led_config_t *const led_config)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(led_config, LED_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   is31fl3206_driver_t *self = interface->parent;

   /* Lowering the gamma makes the steps more evenly perceptual — especially in low/mid range where the steps are
    too coarse. 1.8f can be used, or 1.6f can also be used if steps (especially for white) are still too perceptible */
   float gamma = 2.2f;
   float gamma_brightness_max = 0.0f;

   switch(led_config->color)
   {
      case LED_COLOR_RED:
      case LED_COLOR_GREEN:
      case LED_COLOR_BLUE:
      case LED_COLOR_AMBER:
      case LED_COLOR_TURQUOISE:
         gamma_brightness_max = 255.0f;
         break;
      case LED_COLOR_WHITE:
         gamma_brightness_max = MAX_BRIGHTNESS_WHITE;
         break;
      case LED_COLOR_MAX:
         SET_ERR(result, LED_DRV_ERROR_GENERATE_GAMMA_TABLE);
         break;
      default:
         gamma_brightness_max = 255.0f;
   }

   if(IS_OK(result))
   {
      self->_brightness_table[0] = 0; // First value explicitly set to 0

      for(uint8_t count = 1u; count < self->_num_gamma_steps; count++)
      {
         // Generate values using a gamma-like function (ensuring smoothness)
         float base = ((float)(count - 1u)) / ((float)(self->_num_gamma_steps - 2u));
         float result = gamma_brightness_max * powf(base, gamma);
         self->_brightness_table[count] = (uint8_t)result;

         // Ensure values are increasing (prevent rounding issues i.e. duplicates)
         if(self->_brightness_table[count] <= self->_brightness_table[count - 1u])
         {
            self->_brightness_table[count] = self->_brightness_table[count - 1u] + 1u;
         }
      }

      // Ensure last value is exactly the maximum brightness value
      self->_brightness_table[self->_num_gamma_steps - 1u] = (uint8_t)gamma_brightness_max;

      if(LED_COLOR_GREEN == led_config->color)
      {
         for(uint8_t count = 0; count < self->_num_gamma_steps; count++)
         {
            double half = self->_brightness_table[count] * 0.5;
            self->_brightness_table[count] = (uint8_t)half;
         }
      }

      if(LED_COLOR_BLUE == led_config->color)
      {
         for(uint8_t count = 0; count < self->_num_gamma_steps; count++)
         {
            double fraction = self->_brightness_table[count] * 0.8;
            self->_brightness_table[count] = (uint8_t)fraction;
         }
      }
   }

   return result;
}

static result_t set_led_color_type(const led_driver_interface_t *const interface, const led_config_t *const led_config)
{
   /* note: channel -> color mapping set in datasheet is incorrect.
     the first channel of every led is red, then blue, then green */
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(led_config, LED_DRV_ERROR_PTR_NULL);
   is31fl3206_driver_t *self = interface->parent;
   // check state of driver (guard clause)
   RETURN_ERR_IF_TRUE(self->_driver_state != LED_DRV_ACTIVE, LED_DRV_UNINITIALIZED);

   result_t result = RESULT_OK;

   uint8_t pwm_red = led_config->brightness_pwm_r;
   uint8_t pwm_green = led_config->brightness_pwm_g;
   uint8_t pwm_blue = led_config->brightness_pwm_b;

   if(LED_COLOR_RED == led_config->color)
   {
      uint8_t inc_per_repetition = 0;
      for(uint8_t count = 0; count < NUM_CHANNELS / NUM_CHANNELS_PER_LED; count++)
      {
         uint8_t reg_to_send = REG_PWM + inc_per_repetition;
         result = led_drv_reg_write(self->_i2c_ic_address, reg_to_send, pwm_red);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[reg_to_send] = 1u;
         inc_per_repetition += NUM_CHANNELS_PER_LED;
      }
   }
   else if(LED_COLOR_GREEN == led_config->color)
   {
      uint8_t inc_per_repetition = 0;
      for(uint8_t count = 0; count < NUM_CHANNELS / NUM_CHANNELS_PER_LED; count++)
      {
         uint8_t reg_to_send = FIRST_REGISTER_GREEN + inc_per_repetition;
         result = led_drv_reg_write(self->_i2c_ic_address, reg_to_send, pwm_green);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[reg_to_send] = 1u;
         inc_per_repetition += NUM_CHANNELS_PER_LED;
      }
   }
   else if(LED_COLOR_BLUE == led_config->color)
   {
      uint8_t inc_per_repetition = 0;
      for(uint8_t count = 0; count < NUM_CHANNELS / NUM_CHANNELS_PER_LED; count++)
      {
         uint8_t reg_to_send = FIRST_REGISTER_BLUE + inc_per_repetition;
         result = led_drv_reg_write(self->_i2c_ic_address, reg_to_send, pwm_blue);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[reg_to_send] = 1u;
         inc_per_repetition += NUM_CHANNELS_PER_LED;
      }
   }
   else if(LED_COLOR_AMBER == led_config->color)
   {
      uint8_t inc_per_repetition = 0;
      for(uint8_t count = 0; count < NUM_CHANNELS / NUM_CHANNELS_PER_LED; count++)
      {
         result = led_drv_reg_write(self->_i2c_ic_address, REG_PWM + inc_per_repetition, pwm_red);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[REG_PWM + inc_per_repetition] = 1u;
         result = led_drv_reg_write(self->_i2c_ic_address, FIRST_REGISTER_GREEN + inc_per_repetition, pwm_green);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[FIRST_REGISTER_GREEN + inc_per_repetition] = 1u;
         inc_per_repetition += NUM_CHANNELS_PER_LED;
      }
   }
   else if(LED_COLOR_TURQUOISE == led_config->color)
   {
      uint8_t inc_per_repetition = 0;
      for(uint8_t count = 0; count < NUM_CHANNELS / NUM_CHANNELS_PER_LED; count++)
      {
         result = led_drv_reg_write(self->_i2c_ic_address, REG_PWM + inc_per_repetition, pwm_red);

         if(IS_OK(result))
         {
            self->_pwm_bool_arr[REG_PWM + inc_per_repetition] = 1u;

            result = led_drv_reg_write(self->_i2c_ic_address, FIRST_REGISTER_GREEN + inc_per_repetition, pwm_green);
         }

         if(IS_OK(result))
         {
            self->_pwm_bool_arr[FIRST_REGISTER_GREEN + inc_per_repetition] = 1u;

            result = led_drv_reg_write(self->_i2c_ic_address, FIRST_REGISTER_BLUE + inc_per_repetition, pwm_blue);
         }

         if(IS_OK(result))
         {
            self->_pwm_bool_arr[FIRST_REGISTER_BLUE + inc_per_repetition] = 1u;

            inc_per_repetition += NUM_CHANNELS_PER_LED;
         }

         BREAK_ON_ERR(result);
      }
   }
   else if(LED_COLOR_WHITE == led_config->color)
   {
      for(uint8_t count = 0; count < NUM_CHANNELS; count++)
      {
         result = led_drv_reg_write(self->_i2c_ic_address, REG_PWM + count, pwm_red);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[REG_PWM + count] = 1u;
      }
   }
   else
   {
      // No color (off)
      for(uint8_t count = 0; count < NUM_CHANNELS; count++)
      {
         result = led_drv_reg_write(self->_i2c_ic_address, REG_PWM + count, 0x00);
         BREAK_ON_ERR(result);
         self->_pwm_bool_arr[REG_PWM + count] = 0;
      }
   }

   if(IS_OK(result))
   {
      send_led_type(self->_i2c_ic_address, led_config, self->_pwm_bool_arr, self);
   }

   return result;
}

// To access update function from main
static result_t set_led_update(const led_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, LED_DRV_ERROR_PTR_NULL);
   is31fl3206_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(self->_driver_state != LED_DRV_ACTIVE, LED_DRV_UNINITIALIZED);

   result_t result = led_drv_reg_write(self->_i2c_ic_address, REG_UPDATE, 0x00);

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t led_control_init(is31fl3206_driver_t *const self,
                          const i2c_driver_interface_t *i2c_interface,
                          led_timer_t led_timer_blink,
                          led_timer_t led_timer_custom_blink,
                          led_timer_t led_timer_custom_blink_off,
                          led_timer_t led_timer_fade,
                          uint8_t _num_gamma_steps,
                          uint8_t i2c_address)
{
   result_t result = RESULT_OK;

   RETURN_ERR_IF_NULL(self, LED_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(i2c_interface, LED_DRV_ERROR_PTR_NULL);

   self->_initialized = false;
   self->interface.parent = self;
   self->_i2c_ic_address = i2c_address;
   self->_num_gamma_steps = _num_gamma_steps;
   self->_led_timer_blink = led_timer_blink;
   self->_led_timer_custom_blink = led_timer_custom_blink;
   self->_led_timer_custom_blink_off = led_timer_custom_blink_off;
   self->_led_timer_fade = led_timer_fade;

   self->_led_timer_blink.led_timer_context.driver_instance = self->interface.parent;
   self->_led_timer_custom_blink.led_timer_context.driver_instance = self->interface.parent;
   self->_led_timer_custom_blink_off.led_timer_context.driver_instance = self->interface.parent;
   self->_led_timer_fade.led_timer_context.driver_instance = self->interface.parent;

   self->_led_timer_blink.timer_id = &self->_led_timer_blink.timer_storage;
   self->_led_timer_custom_blink.timer_id = &self->_led_timer_custom_blink.timer_storage;
   self->_led_timer_custom_blink_off.timer_id = &self->_led_timer_custom_blink_off.timer_storage;
   self->_led_timer_fade.timer_id = &self->_led_timer_fade.timer_storage;

   self->_led_timer_blink.led_timer_context.stop_timer = false;
   self->_led_timer_custom_blink.led_timer_context.stop_timer = false;
   self->_led_timer_custom_blink_off.led_timer_context.stop_timer = false;
   self->_led_timer_fade.led_timer_context.stop_timer = false;

   self->_led_timer_blink.led_timer_context.custom_blink_ms = 0;
   self->_led_timer_blink.led_timer_context.custom_blink_pure_ms = 0;
   self->_led_timer_blink.led_timer_context.custom_pulse_count_set = 0;
   self->_led_timer_blink.led_timer_context.custom_blink_on_ms = 0;
   self->_led_timer_blink.led_timer_context.pulse_count = 0;
   self->_led_timer_blink.led_timer_context.custom_repeat_interval_ms = 0;
   self->_led_timer_blink.led_timer_context.pulse = false;

   self->_led_timer_fade.led_timer_context.custom_blink_ms = 0;
   self->_led_timer_fade.led_timer_context.custom_blink_pure_ms = 0;
   self->_led_timer_fade.led_timer_context.custom_pulse_count_set = 0;
   self->_led_timer_fade.led_timer_context.custom_blink_on_ms = 0;
   self->_led_timer_fade.led_timer_context.pulse_count = 0;
   self->_led_timer_fade.led_timer_context.custom_repeat_interval_ms = 0;
   self->_led_timer_fade.led_timer_context.pulse = false;

   self->_led_timer_custom_blink_off.led_timer_context.custom_blink_ms = 0;
   self->_led_timer_custom_blink_off.led_timer_context.custom_blink_pure_ms = 0;
   self->_led_timer_custom_blink_off.led_timer_context.custom_pulse_count_set = 0;
   self->_led_timer_custom_blink_off.led_timer_context.custom_blink_on_ms = 0;
   self->_led_timer_custom_blink_off.led_timer_context.pulse_count = 0;
   self->_led_timer_custom_blink_off.led_timer_context.custom_repeat_interval_ms = 0;
   self->_led_timer_custom_blink_off.led_timer_context.pulse = false;

   self->_led_timer_custom_blink.led_timer_context.custom_blink_ms = 0;
   self->_led_timer_custom_blink.led_timer_context.custom_blink_pure_ms = 0;
   self->_led_timer_custom_blink.led_timer_context.custom_pulse_count_set = 0;
   self->_led_timer_custom_blink.led_timer_context.custom_blink_on_ms = 0;
   self->_led_timer_custom_blink.led_timer_context.pulse_count = 0;
   self->_led_timer_custom_blink.led_timer_context.custom_repeat_interval_ms = 0;
   self->_led_timer_custom_blink.led_timer_context.pulse = false;

   self->interface.set_led_driver_config = set_led_driver_config;
   self->interface.set_led_color_type = set_led_color_type;
   self->interface.set_led_update = set_led_update;
   self->interface.led_blink_timers_start = led_blink_timers_start;
   self->interface.led_custom_blink_timers_start = led_custom_blink_timers_start;
   self->interface.led_fade_timers_start = led_fade_timers_start;
   self->interface.generate_gamma_table = generate_gamma_table;
   self->interface.set_off_all = set_off_all;

   mp_i2c_driver_interface = i2c_interface;

   memset(self->_pwm_bool_arr, 0, sizeof(self->_pwm_bool_arr));
   memset(self->_brightness_table, 0, sizeof(self->_brightness_table));

   // Create the timers
   m_ret_code_t error_code
      = app_timer_create(&self->_led_timer_blink.timer_id, APP_TIMER_MODE_REPEATED, led_blink_handler);
   UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);

   if(IS_OK(result))
   {
      error_code
         = app_timer_create(&self->_led_timer_custom_blink.timer_id, APP_TIMER_MODE_REPEATED, led_custom_blink_handler);
      UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);
   }

   if(IS_OK(result))
   {
      error_code = app_timer_create(
         &self->_led_timer_custom_blink_off.timer_id, APP_TIMER_MODE_SINGLE_SHOT, led_custom_blink_off_handler);
      UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);
   }

   if(IS_OK(result))
   {
      error_code = app_timer_create(&self->_led_timer_fade.timer_id, APP_TIMER_MODE_REPEATED, led_fade_handler);
      UPDATE_IF_NRF_ERR(error_code, result, APP_TIMER_STAT_ERR);
   }

   if(IS_OK(result))
   {
      self->_driver_state = LED_DRV_ACTIVE;
      self->_initialized = true;
   }

   return result;
}
