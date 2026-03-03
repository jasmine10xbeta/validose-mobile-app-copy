/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_error.h"
#include "bsp/project.h"
#include "bsp/sdk_config.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_power.h"

// Custom includes
#include "common.h"
#include "debug.h"
#include "modules/general_control/general_control.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_MAIN_RING;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

void gpio_init(void)
{
   // Batt IC
   nrf_gpio_pin_clear(BM_CE); // Charging on
   nrf_gpio_cfg_output(BM_CE);

   nrf_gpio_pin_set(BM_LP);  // Low power mode off
   nrf_gpio_cfg_output(BM_LP);

   nrf_gpio_pin_set(BM_MR); // Not in reset
   nrf_gpio_cfg_output(BM_MR);

   // Prox IC
   nrf_gpio_cfg_output(PS_VDD);
   nrf_gpio_pin_set(PS_VDD); // Turn on the IR proximity sensor. // Todo set to clear for production and have the ID
                             // proximity unit control this pin.

   // Configure LED pins
   nrf_gpio_cfg_output(LED_RED);
   nrf_gpio_cfg_output(LED_GREEN);
   nrf_gpio_cfg_output(LED_BLUE);

   // Configure IMU interrupt pins as inputs with no pull-up
   nrf_gpio_cfg_input(IMU_INT1, NRF_GPIO_PIN_NOPULL);
   nrf_gpio_cfg_input(IMU_INT2, NRF_GPIO_PIN_NOPULL);
}

static void on_startup_led_seq(void)
{
   nrf_gpio_pin_set(LED_GREEN);
   nrf_delay_ms(10);
   nrf_gpio_pin_clear(LED_GREEN);
   nrf_delay_ms(100);
   nrf_gpio_pin_set(LED_BLUE);
   nrf_delay_ms(10);
   nrf_gpio_pin_clear(LED_BLUE);
   nrf_delay_ms(100);
   nrf_gpio_pin_set(LED_RED);
   nrf_delay_ms(10);
   nrf_gpio_pin_clear(LED_RED);
   nrf_gpio_pin_clear(LED_BLUE);
   nrf_gpio_pin_clear(LED_GREEN);
}

void lfclk_request(void)
{
   if(!nrf_drv_clock_lfclk_is_running())
   {
      ret_code_t err_code = nrf_drv_clock_init();
      APP_ERROR_CHECK(err_code);
   }
   else
   {
      nrf_drv_clock_lfclk_request(NULL);
      nrfx_clock_lfclk_start();

      nrf_drv_clock_lfclk_request(NULL);
      nrfx_clock_lfclk_start();

      while(!nrf_drv_clock_lfclk_is_running())
      {
         // Wait
      }
   }
}

int main(void)
{
   // Enable the nRF DCDC converter. This line setsup the MCU at startup so the bootloader will also run using the DCDC
   nrf_power_dcdcen_vddh_set(true);

   // Enable Low frequency clock, if not already enabled by Softdevice. This function setsup the MCU at startup as the
   // bootloader also uses less power
   //   lfclk_request();

   DEBUG_INFO("Starting Ring.....");
   gpio_init();

   on_startup_led_seq();

   result_t result = RESULT_OK;
   result = general_control_init();

   if(IS_OK(result))
   {
      result = general_control_run();
      DEBUG_CRITICAL(
         "General control run failed. Result.unit = %d, result.code = %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   else
   {
      DEBUG_CRITICAL(
         "General control init failed. Result.unit = %d, result.code = %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   // delay to allow SEGGER_RTT_printf to finish
   nrf_delay_ms(5000u);

   // The code should never reach this point. If it does, an error occurred.
   NVIC_SystemReset(); // Reset the device
}
/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/
