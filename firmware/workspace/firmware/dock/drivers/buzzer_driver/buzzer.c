/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "buzzer.h"

#include "../dock/bsp/custom_board.h"
#include "app_error.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_timer.h"
#include "nrf_gpio.h"
#include "nrf_pwm.h"

static const uint8_t THIS_UNIT_ID = SW_UNIT_ID_BUZZER_DOCK;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// pwm parameters:
#define PWM_FREQUENCY_HZ (4000u) // 4 kHz frequency
#define PWM_DUTY_CYCLE   (50u)   // 50% duty cycle
#define PWM_BASE_CLOCK   NRF_PWM_CLK_1MHz
#define PWM_TOP_VALUE    (1000000u / PWM_FREQUENCY_HZ) // Calculate the top value for 4 kHz

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t set_on(const buzzer_interface_t *const interface);
static result_t set_off(const buzzer_interface_t *const interface);
static result_t get_state(const buzzer_interface_t *const interface, bool *is_on);

// Internal functions
static result_t pwm_init(void);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static nrf_drv_pwm_t m_pwm = NRF_DRV_PWM_INSTANCE(BUZZER_PWM_INSTANCE);
// Create a PWM sequence with a 50% duty cycle
static nrf_pwm_values_individual_t s_pwm_seq_values;

// Configure the PWM sequence
static nrf_pwm_sequence_t s_pwm_sequence;

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t pwm_init(void)
{
   result_t result = RESULT_OK;

   s_pwm_seq_values.channel_0 = PWM_TOP_VALUE * PWM_DUTY_CYCLE / 100u;
   s_pwm_seq_values.channel_1 = 0;
   s_pwm_seq_values.channel_2 = 0;
   s_pwm_seq_values.channel_3 = 0;

   s_pwm_sequence.values.p_individual = &s_pwm_seq_values;
   s_pwm_sequence.length = NRF_PWM_VALUES_LENGTH(s_pwm_seq_values);
   s_pwm_sequence.repeats = 0;
   s_pwm_sequence.end_delay = 0;

   nrf_drv_pwm_config_t const config = {
        .output_pins = {
            BUZZER_PIN,                               // Channel 0
            NRF_DRV_PWM_PIN_NOT_USED,                 // Channel 1
            NRF_DRV_PWM_PIN_NOT_USED,                 // Channel 2
            NRF_DRV_PWM_PIN_NOT_USED                  // Channel 3
        },
        .irq_priority = APP_IRQ_PRIORITY_LOW,
        .base_clock   = PWM_BASE_CLOCK,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = PWM_TOP_VALUE,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO
    };

   ret_code_t err_code = nrf_drv_pwm_init(&m_pwm, &config, NULL);

   UPDATE_IF_NRF_ERR(err_code, result, BUZZER_ERROR_PWM);

   return result;
}

static result_t set_on(const buzzer_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BUZZER_ERROR_NULL);

   result_t result = RESULT_OK;

   // TODO: Uncomment when done with testing.
   // buzzer_t *self = interface->parent; /// Get access to the containing unit instance

   // bool is_lock_acquired = nrf_mtx_trylock(&(self->_twi_mutex));

   // if(!is_lock_acquired)
   // {
   //    SET_ERR(result, BUZZER_ERROR_MTX_LOCKED);
   // }

   // if(IS_OK(result))
   // {
   //    // Start the PWM sequence
   //    nrf_drv_pwm_simple_playback(&m_pwm, &s_pwm_sequence, 1, NRF_DRV_PWM_FLAG_LOOP);

   //    self->_buzzing = true;
   // }

   // if(is_lock_acquired)
   // {
   //    nrf_mtx_unlock(&(self->_twi_mutex));
   // }

   return result;
}

static result_t set_off(const buzzer_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BUZZER_ERROR_NULL);
   result_t result = RESULT_OK;

   buzzer_t *self = interface->parent; /// Get access to the containing unit instance

   nrf_drv_pwm_stop(&m_pwm, false);

   self->_buzzing = false;

   return result;
}
static result_t get_state(const buzzer_interface_t *const interface, bool *is_on)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BUZZER_ERROR_NULL);
   result_t result = RESULT_OK;

   const buzzer_t *self = interface->parent; /// Get access to the containing unit instance

   *is_on = self->_buzzing;

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t buzzer_init(buzzer_t *const self)
{
   result_t result = RESULT_OK;
   RETURN_ERR_IF_NULL(self, BUZZER_ERROR_NULL);

   self->_initialized = false;
   self->interface.parent = self; // Connect the interface with the containing unit instance to allow being able to
                                  // reference internal state variables.

   // initialize all interface pointers to point to internal static functions by default
   self->interface.set_on = set_on;
   self->interface.set_off = set_off;
   self->interface.get_state = get_state;

   nrf_mtx_init(&(self->_twi_mutex));

   result = pwm_init();

   if(IS_OK(result))
   {
      self->_buzzing = false;

      self->_initialized = true;
   }

   return result;
}
