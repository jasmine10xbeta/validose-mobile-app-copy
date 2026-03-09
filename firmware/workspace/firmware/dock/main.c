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
#include "bsp/project.h" // Have to define it this way otherwise it refers to the ring's project.h
#include "bsp/sdk_config.h"
#include "nrf_delay.h"

// Custom includes
#include "common.h"
#include "custom_board.h"
#include "debug.h"
#include "general_control.h"
#include "result.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_MAIN_DOCK;

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
   // ADC Power
   nrf_gpio_cfg_output(ADS1235_5V_EN_PIN);
   nrf_gpio_pin_set(ADS1235_5V_EN_PIN); // Turn off by default

   // LED Drive pin
   nrf_gpio_cfg_output(LEDDRV_SD1);
   nrf_gpio_pin_set(LEDDRV_SD1);
   nrf_gpio_cfg_output(LEDDRV_SD2);
   nrf_gpio_pin_set(LEDDRV_SD2);

   // NFC pins
   nrf_gpio_cfg_output(NFC_ANT_A_EN);
   nrf_gpio_pin_clear(NFC_ANT_A_EN);
   nrf_gpio_cfg_output(NFC_ANT_B_EN);
   nrf_gpio_pin_set(NFC_ANT_B_EN);
   nrf_gpio_cfg_output(NFC_BSS_EN);
   nrf_gpio_pin_set(NFC_BSS_EN);
   nrf_gpio_cfg_input(NFC_IRQ_EN, NRF_GPIO_PIN_NOPULL);

   // I2C pullup pin
   nrf_gpio_cfg_output(I2C_PULLUP);
   nrf_gpio_pin_set(I2C_PULLUP);

   // Configure test LED pins
   nrf_gpio_cfg_output(LED_RED);
   nrf_gpio_cfg_output(LED_GREEN);
   nrf_gpio_cfg_output(LED_BLUE);

   // Wireless charging - dock
   nrf_gpio_cfg_output(WC_EN1);
   nrf_gpio_cfg_output(WC_EN2);
   nrf_gpio_pin_clear(WC_EN1); // Turn on by default
   nrf_gpio_pin_clear(WC_EN2); // Turn on by default

   // Flash GPIO Pins
   nrf_gpio_cfg_output(FLASH_NRST_PIN);
   nrf_gpio_pin_set(FLASH_NRST_PIN);
   nrf_gpio_cfg_output(FLASH_WP_PIN);
   nrf_gpio_pin_set(FLASH_WP_PIN);
}

int main(void)
{
   DEBUG_INFO("Starting Dock.....");
   gpio_init();

   // Todo: Insert some LED sequence here to show the device was reset.
   nrf_gpio_pin_set(LED_RED);
   nrf_delay_ms(500);
   nrf_gpio_pin_set(LED_BLUE);
   nrf_delay_ms(500);
   nrf_gpio_pin_set(LED_GREEN);
   nrf_delay_ms(500);
   nrf_gpio_pin_clear(LED_RED);
   nrf_gpio_pin_clear(LED_BLUE);
   nrf_gpio_pin_clear(LED_GREEN);

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
