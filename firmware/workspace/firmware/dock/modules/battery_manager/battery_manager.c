/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file battery_manager.c
 * @ingroup battery_manager_module
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf_drv_pwm.h"
#include "nrfx_pwm.h"
#include "nrfx_timer.h"
#include <stdint.h>

// Custom includes
#include "../dock/bsp/project.h"
#include "battery_manager.h"
#include "common.h"
#include "debug.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BATTERY_MANAGER_DOCK;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define BATTERY_FULL_TH_VOLTAGE_MV  ((uint32_t)4150u)
#define BATTERY_EMPTY_TH_VOLTAGE_MV ((uint32_t)3400u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t power_flash_memory_and_load_cell(const battery_manager_interface_t *const interface, bool enabled);
static result_t is_power_flash_memory_and_load_cell_enabled(const battery_manager_interface_t *const interface,
                                                            bool *enabled);
static result_t get_battery_status(const battery_manager_interface_t *const interface,
                                   battery_status_t *battery_status);
static result_t enter_ship_mode(const battery_manager_interface_t *const interface);

static result_t power_buzzer_leds_nfc(const battery_manager_interface_t *const interface, bool enabled);
static result_t is_power_buzzer_leds_nfc_enabled(const battery_manager_interface_t *const interface, bool *enabled);
static result_t enable_auto_measurements(const battery_manager_interface_t *const interface);
static result_t disable_auto_measurements(const battery_manager_interface_t *const interface);
static result_t enable_wireless_charging(const battery_manager_interface_t *const interface);
static result_t disable_wireless_charging(const battery_manager_interface_t *const interface);
static result_t get_wireless_charging_status(const battery_manager_interface_t *const interface, bool *is_enabled);

// Internal functions

/**
 * @brief Function to get the battery voltage.
 *
 * This function receives the latest battery voltage ADC measurement, converts it to a real voltage,
 * and adds it to the circular buffer. It then calculates the average battery voltage from the
 * circular buffer and returns it.
 *
 * @param[in] interface Pointer to the battery manager interface. Must not be NULL.
 * @param[out] battery_voltage_mv Pointer to store the calculated average battery voltage in millivolts. Must not be
 * NULL.
 * @param[in] current_battery_measurement_adc The latest battery voltage measurement from the PMIC ADC.
 *
 * @return result_t Returns a result_t indicating the status of the operation.
 */
static result_t get_battery_voltage_mv(const battery_manager_interface_t *const interface,
                                       uint16_t *battery_voltage_mv,
                                       uint16_t current_battery_measurement_adc);

/**
 * @brief Function to Initialize a circular buffer.
 *
 * @param[in,out] cb Pointer to the circular buffer to be initialized. Must not be NULL.
 */
static void circular_buffer_init(circular_buffer_t *cb);

/**
 * @brief Function to add a value to a circular buffer.
 *
 * @param[in,out] cb Pointer to the circular buffer. Must not be NULL.
 * @param[in] value The value to be added to the circular buffer.
 */
static void circular_buffer_add(circular_buffer_t *cb, uint16_t value);

/**
 * @brief Function to calculate the average value from a circular buffer.
 *
 * @param[in,out] cb Pointer to the circular buffer. Must not be NULL.
 * @param[out] average_value Pointer to store the calculated average value. Must not be NULL.
 */
static void circular_buffer_average(const circular_buffer_t *cb, uint16_t *average_value);

/**
 * @brief Function to get the battery level based on the current battery voltage ADC measurement.
 *
 * @note This function calls get_battery_voltage_mv() to convert the ADC value to a real voltage and then calculate the
 * average value of the last X measurements.
 *
 * @param[in] interface Pointer to the battery manager interface. Must not be NULL.
 * @param[in] current_battery_measurement_adc The latest battery voltage measurement from the PMIC ADC.
 * @param[out] battery_level Pointer to store the calculated battery level. Must not be NULL.
 *
 * @return result_t Returns a result_t indicating the status of the operation.
 */
static result_t get_battery_level(const battery_manager_interface_t *const interface,
                                  uint16_t current_battery_measurement_adc,
                                  uint8_t *battery_level);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static void circular_buffer_init(circular_buffer_t *cb)
{
   RETURN_VOID_IF_NULL(cb);

   cb->head = 0;
   cb->num_elements = 0;
   for(uint16_t i = 0; i < CIRCULAR_BUFFER_SIZE; i++)
   {
      cb->buffer[i] = 0;
   }
}
static void circular_buffer_add(circular_buffer_t *cb, uint16_t value)
{
   RETURN_VOID_IF_NULL(cb);

   cb->buffer[cb->head] = value;
   // Inc head, wrap around if necessary
   cb->head = (uint16_t)((cb->head + 1u) % (uint16_t)CIRCULAR_BUFFER_SIZE);
   if(cb->num_elements < CIRCULAR_BUFFER_SIZE)
   {
      cb->num_elements++;
   }
}
static void circular_buffer_average(const circular_buffer_t *cb, uint16_t *average_value)
{
   RETURN_VOID_IF_NULL(cb);
   RETURN_VOID_IF_NULL(average_value);
   if(0 == cb->num_elements)
   {
      *average_value = 0; // Avoid division by zero if buffer is empty
   }
   else
   {
      uint32_t sum = 0;
      for(uint16_t i = 0; i < cb->num_elements; i++)
      {
         sum += cb->buffer[i];
      }
      *average_value = (uint16_t)(sum / cb->num_elements);
   }
}

static result_t get_battery_voltage_mv(const battery_manager_interface_t *const interface,
                                       uint16_t *battery_voltage_mv,
                                       uint16_t current_battery_measurement_adc)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(battery_voltage_mv, BATTERY_MAN_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   battery_manager_t *self = interface->parent;

   if(current_battery_measurement_adc > NPM1300_ADC_RESOLUTION_STEPS)
   {
      SET_ERR(result, BATTERY_MAN_ERROR_ADC_VALUE_OUT_OF_RANGE);
      DEBUG_ERROR("ADC value out of range: %d", current_battery_measurement_adc);
   }
   else
   {
      // Convert ADC value to millivolts. The calculation avoids floating point arithmetic. Multiply first, then divide.
      uint16_t current_battery_voltage_mv
         = (uint16_t)(((uint32_t)current_battery_measurement_adc * NPM1300_VFS_VBAT_MV) / NPM1300_ADC_RESOLUTION_STEPS);

      circular_buffer_add(&self->_voltage_buffer, current_battery_voltage_mv);
      circular_buffer_average(&self->_voltage_buffer, battery_voltage_mv);
   }

   return result;
}

static result_t get_battery_level(const battery_manager_interface_t *const interface,
                                  uint16_t current_battery_measurement_adc,
                                  uint8_t *battery_level)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(battery_level, BATTERY_MAN_ERROR_PTR_NULL);

   uint16_t battery_voltage_mv = 0;
   result_t result = get_battery_voltage_mv(interface, &battery_voltage_mv, current_battery_measurement_adc);

   if(IS_OK(result))
   {
      if(battery_voltage_mv < BATTERY_EMPTY_TH_VOLTAGE_MV)
      {
         *battery_level = 0; // Considered as fully discharged
      }
      else if(battery_voltage_mv > BATTERY_FULL_TH_VOLTAGE_MV)
      {
         *battery_level
            = BATTERY_LEVEL_98_PERCENT_FULL; // Only fully charged when the PMIC reports a fully charged battery.
      }
      else
      {
         // Map the voltage range linearly from empty to full
         uint32_t range = BATTERY_FULL_TH_VOLTAGE_MV - BATTERY_EMPTY_TH_VOLTAGE_MV;
         uint32_t adjusted_voltage_mv = (uint32_t)battery_voltage_mv - BATTERY_EMPTY_TH_VOLTAGE_MV;
         uint8_t battery_level_percentage = (uint8_t)((adjusted_voltage_mv * 100u) / range);

         *battery_level = battery_level_percentage;
      }
   }

   return result;
}
/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t get_wireless_charging_status(const battery_manager_interface_t *const interface, bool *is_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_enabled, BATTERY_MAN_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   *is_enabled = nrf_gpio_pin_read(WC_EN1) ? true : false;

   return result;
}

static result_t enable_wireless_charging(const battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   bool is_enabled = false;
   result_t result = interface->get_wireless_charging_status(interface, &is_enabled);

   if(IS_OK(result) && !is_enabled)
   {
      // Enable wireless charging for the Dock
      nrf_gpio_pin_clear(WC_EN1);
      nrf_gpio_pin_clear(WC_EN2);
   }

   return result;
}
static result_t disable_wireless_charging(const battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   bool is_enabled = false;
   result_t result = interface->get_wireless_charging_status(interface, &is_enabled);

   if(IS_OK(result) && is_enabled)
   {
      // Disable wireless charging for the Dock
      nrf_gpio_pin_set(WC_EN1);
      nrf_gpio_pin_set(WC_EN2);
   }

   return result;
}

static result_t power_buzzer_leds_nfc(const battery_manager_interface_t *const interface, bool enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   battery_manager_t *self = interface->parent;

   result_t result = self->_npm1300_driver->set_load_switch2(self->_npm1300_driver, enabled);

   if(IS_OK(result))
   {
      self->_is_buzzer_leds_nfc_powered = enabled;
   }

   return result;
}
static result_t is_power_buzzer_leds_nfc_enabled(const battery_manager_interface_t *const interface, bool *enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(enabled, BATTERY_MAN_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   const battery_manager_t *self = interface->parent;

   *enabled = self->_is_buzzer_leds_nfc_powered;

   return result;
}
static result_t enable_auto_measurements(const battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   result_t result = interface->parent->_npm1300_driver->enable_auto_measurements(interface->parent->_npm1300_driver);
   return result;
}

static result_t disable_auto_measurements(const battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   result_t result = interface->parent->_npm1300_driver->disable_auto_measurements(interface->parent->_npm1300_driver);
   return result;
}

static result_t power_flash_memory_and_load_cell(const battery_manager_interface_t *const interface, bool enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   battery_manager_t *self = interface->parent;

   result_t result = self->_npm1300_driver->set_load_switch1(self->_npm1300_driver, enabled);

   if(IS_OK(result))
   {
      self->_is_flash_memory_and_load_cell_powered = enabled;
   }

   return result;
}
static result_t is_power_flash_memory_and_load_cell_enabled(const battery_manager_interface_t *const interface,
                                                            bool *enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(enabled, BATTERY_MAN_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   const battery_manager_t *self = interface->parent;

   *enabled = self->_is_flash_memory_and_load_cell_powered;

   return result;
}

static result_t enter_ship_mode(const battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   const battery_manager_t *self = interface->parent;

   result_t result = self->_npm1300_driver->enter_ship_mode(self->_npm1300_driver);

   return result;
}

static result_t get_battery_status(const battery_manager_interface_t *const interface, battery_status_t *battery_status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(battery_status, BATTERY_MAN_ERROR_PTR_NULL);

   battery_manager_t *self = interface->parent;

   pmic_status_t pmic_status = {0};
   result_t result = self->_npm1300_driver->get_pmic_status(self->_npm1300_driver, &pmic_status);

   battery_status->is_pmic_reset_detected = pmic_status.is_reset_detected;
   battery_status->is_battery_measurement_stale = true;

   pmic_battery_measurements_t measurements;
   uint8_t battery_level = 0u;
   uint16_t battery_voltage_mv = 0u;
   static bool was_full_recently = true; // Captures transition from battery level being full to not full

   if(IS_OK(result))
   {
      // Get battery measurements from PMIC if vbat is available.
      if(pmic_status.vbat_measurement_ready)
      {
         result = self->_npm1300_driver->get_measurements(self->_npm1300_driver, &measurements);
         IF_OK_RUN_AND_UPDATE(result, get_battery_level(interface, measurements.vbat_adc, &battery_level));
         IF_OK_RUN_AND_UPDATE(result, get_battery_voltage_mv(interface, &battery_voltage_mv, measurements.vbat_adc));
      }

      if(IS_OK(result) && (battery_status->battery_level < BATTERY_LEVEL_ENABLE_CHARGER) && was_full_recently)
      {
         was_full_recently = false;
         result = enable_wireless_charging(interface);
      }

      if((pmic_status.vbat_measurement_ready) && IS_OK(result))
      {
         battery_status->battery_voltage_mv = battery_voltage_mv;
         battery_status->battery_level = battery_level;
         battery_status->charger_connected = pmic_status.charger_connected;

         battery_status->is_battery_measurement_stale = false;

         if(pmic_status.charging_completed)
         {
            battery_status->battery_state = BATTERY_STATE_CHARGING_COMPLETED;
            battery_status->battery_level = BATTERY_LEVEL_FULL;
            was_full_recently = true;
            result = disable_wireless_charging(interface); // Disable charging to prevent unnecessary heat buildup
         }
         else if(pmic_status.charging)
         {
            battery_status->battery_state = BATTERY_STATE_CHARGING;
         }
         else if(battery_status->battery_level <= BATTERY_LEVEL_LOW_TH)
         {
            battery_status->battery_state = BATTERY_STATE_SOC_LOW;
         }
         else
         {
            battery_status->battery_state = BATTERY_STATE_SOC_GOOD;
         }
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t battery_manager_init(battery_manager_t *const self, const npm1300_driver_interface_t *npm1300_driver)
{
   RETURN_ERR_IF_NULL(self, BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(npm1300_driver, BATTERY_MAN_ERROR_PTR_NULL);

   self->interface.parent = self; // Connect the interface with the containing unit instance to allow being able to
                                  // reference internal state variables.

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.get_battery_status = get_battery_status;
   self->interface.is_power_flash_memory_and_load_cell_enabled = is_power_flash_memory_and_load_cell_enabled;
   self->interface.power_flash_memory_and_load_cell = power_flash_memory_and_load_cell;
   self->interface.disable_auto_measurements = disable_auto_measurements;
   self->interface.enable_auto_measurements = enable_auto_measurements;
   self->interface.enter_ship_mode = enter_ship_mode;
   self->interface.is_power_buzzer_leds_nfc_enabled = is_power_buzzer_leds_nfc_enabled;
   self->interface.power_buzzer_leds_nfc = power_buzzer_leds_nfc;
   self->interface.enable_wireless_charging = enable_wireless_charging;
   self->interface.disable_wireless_charging = disable_wireless_charging;
   self->interface.get_wireless_charging_status = get_wireless_charging_status;

   self->_npm1300_driver = npm1300_driver;

   // Initialize the internal buffer for battery voltage measurements
   circular_buffer_init(&self->_voltage_buffer);

   // Set flash memory and load cell power to off initially.
   result_t result = self->_npm1300_driver->set_load_switch1(self->_npm1300_driver, true);
   // Set buzzer, LEDs, and NFC power to on initially.
   IF_OK_RUN_AND_UPDATE(result, self->_npm1300_driver->set_load_switch2(self->_npm1300_driver, true));

   if(IS_OK(result))
   {
      self->_is_buzzer_leds_nfc_powered = true;
      self->_is_flash_memory_and_load_cell_powered = true;

      // Initialize the battery status structure
      battery_status_t battery_status = {0};

      IF_OK_RUN_AND_UPDATE(result, get_battery_status(&self->interface, &battery_status));
      IF_OK_RUN_AND_UPDATE(result, enable_auto_measurements(&self->interface));
      IF_OK_RUN_AND_UPDATE(result, enable_wireless_charging(&self->interface));
   }

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
