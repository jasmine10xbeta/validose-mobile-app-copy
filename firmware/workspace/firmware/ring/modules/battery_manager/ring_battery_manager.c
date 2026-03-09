/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_battery_manager.c
 * @ingroup modules/ring_battery_manager
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_error.h"
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "ring_battery_manager.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BATTERY_MANAGER_RING;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t power_prox_sensor_and_leds(const ring_battery_manager_interface_t *const interface, bool enabled);
static result_t is_power_prox_sensor_and_leds_enabled(const ring_battery_manager_interface_t *const interface,
                                                      bool *enabled);
static result_t get_battery_status(const ring_battery_manager_interface_t *const interface,
                                   battery_status_t *battery_status);
static result_t enable_low_power_mode(const ring_battery_manager_interface_t *const interface);
static result_t disable_low_power_mode(const ring_battery_manager_interface_t *const interface);
static result_t enter_ship_mode(const ring_battery_manager_interface_t *const interface);
static result_t enable_charging(const ring_battery_manager_interface_t *const interface);
static result_t disable_charging(const ring_battery_manager_interface_t *const interface);

// Internal functions

/**
 * @brief Function to get the battery voltage.
 *
 * This function receives the latest battery voltage (mv)
 * and adds it to the circular buffer. It then calculates the average battery voltage from the
 * circular buffer and returns it.
 *
 * @param[in] interface Pointer to the battery manager interface. Must not be NULL.
 * @param[in] battery_voltage_mv New battery voltage measurement.
 * @param[out] avg_battery_voltage_mv Pointer to store the calculated average battery voltage in millivolts. Must not be
 * NULL.
 *
 * @return result_t Returns a result_t indicating the status of the operation.
 */
static result_t get_avg_battery_voltage_mv(const ring_battery_manager_interface_t *const interface,
                                           uint16_t *avg_battery_voltage_mv,
                                           uint16_t battery_voltage_mv);

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
 * @brief Function to get the battery level based on the current battery voltage measurement.
 *
 * @param[in] interface Pointer to the battery manager interface. Must not be NULL.
 * @param[in] battery_voltage_mv Battery voltage in mV.
 * @param[out] battery_level Pointer to store the calculated battery level. Must not be NULL.
 *
 * @return result_t Returns a result_t indicating the status of the operation.
 */
static result_t get_battery_level(const ring_battery_manager_interface_t *const interface,
                                  uint16_t battery_voltage_mv,
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

static result_t get_avg_battery_voltage_mv(const ring_battery_manager_interface_t *const interface,
                                           uint16_t *avg_battery_voltage_mv,
                                           uint16_t battery_voltage_mv)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(avg_battery_voltage_mv, RING_BATTERY_MAN_ERROR_PTR_NULL);

   ring_battery_manager_t *self = interface->parent;

   circular_buffer_add(&self->_voltage_buffer, battery_voltage_mv);
   circular_buffer_average(&self->_voltage_buffer, avg_battery_voltage_mv);

   return RESULT_OK;
}

static result_t get_battery_level(const ring_battery_manager_interface_t *const interface,
                                  uint16_t battery_voltage_mv,
                                  uint8_t *battery_level)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(battery_level, RING_BATTERY_MAN_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

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

   return result;
}
/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t power_prox_sensor_and_leds(const ring_battery_manager_interface_t *const interface, bool enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);

   ring_battery_manager_t *self = interface->parent;

   bool is_enabled = false;
   result_t result = self->_bq25150_pmic_driver->get_ldo_status(self->_bq25150_pmic_driver, &is_enabled);

   if((is_enabled != enabled) && IS_OK(result))
   {
      result = self->_bq25150_pmic_driver->set_ldo_status(self->_bq25150_pmic_driver, enabled);

      if(IS_OK(result))
      {
         self->_is_proximity_sensor_circuit_enabled = enabled;
      }
   }

   return result;
}

static result_t is_power_prox_sensor_and_leds_enabled(const ring_battery_manager_interface_t *const interface,
                                                      bool *enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(enabled, RING_BATTERY_MAN_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   const ring_battery_manager_t *self = interface->parent;

   *enabled = self->_is_proximity_sensor_circuit_enabled;

   return result;
}

static result_t enable_charging(const ring_battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);

   ring_battery_manager_t *self = interface->parent;

   result_t result = self->_bq25150_pmic_driver->set_charge_enable(self->_bq25150_pmic_driver, true);

   if(IS_OK(result))
   {
      self->_is_charging_enabled = true;
   }

   return result;
}

static result_t disable_charging(const ring_battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);

   ring_battery_manager_t *self = interface->parent;

   result_t result = self->_bq25150_pmic_driver->set_charge_enable(self->_bq25150_pmic_driver, false);

   if(IS_OK(result))
   {
      self->_is_charging_enabled = false;
   }

   return result;
}

static result_t enter_ship_mode(const ring_battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   const ring_battery_manager_t *self = interface->parent;

   result_t result = self->_bq25150_pmic_driver->enter_ship_mode(self->_bq25150_pmic_driver);

   return result;
}

static result_t enable_low_power_mode(const ring_battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   ring_battery_manager_t *self = interface->parent;

   result_t result = self->_bq25150_pmic_driver->set_low_power_mode(self->_bq25150_pmic_driver, true);

   if(IS_OK(result))
   {
      self->_is_low_power_mode_enabled = true;
   }

   return result;
}

static result_t disable_low_power_mode(const ring_battery_manager_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   ring_battery_manager_t *self = interface->parent;

   result_t result = self->_bq25150_pmic_driver->set_low_power_mode(self->_bq25150_pmic_driver, false);

   if(IS_OK(result))
   {
      self->_is_low_power_mode_enabled = false;
   }

   return result;
}

static result_t get_battery_status(const ring_battery_manager_interface_t *const interface,
                                   battery_status_t *battery_status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RING_BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(battery_status, RING_BATTERY_MAN_ERROR_PTR_NULL);

   ring_battery_manager_t *self = interface->parent;

   pmic_status_t pmic_status = {0};
   result_t result = self->_bq25150_pmic_driver->get_pmic_status(self->_bq25150_pmic_driver, &pmic_status);

   if(IS_OK(result))
   {
      battery_status->is_pmic_reset_detected = pmic_status.is_reset_detected; // Not used
      battery_status->is_battery_measurement_stale = true;
   }
   // Get battery measurements from PMIC if vbat is available. Note: For BQ25150, Vbat flag in PMIC is suppressed when
   // the charger is connected. VBAT measurement is continuous when the charger is connected and is therefore always
   // ready.
   if(IS_OK(result)) // && (pmic_status.vbat_measurement_ready || pmic_status.charger_connected))
   {
      pmic_battery_measurements_t measurements;
      result = self->_bq25150_pmic_driver->get_measurements(self->_bq25150_pmic_driver, &measurements);

      uint16_t battery_voltage_mv = 0u;
      IF_OK_RUN_AND_UPDATE(result, get_avg_battery_voltage_mv(interface, &battery_voltage_mv, measurements.vbat_mv));

      uint8_t battery_level = 0u;
      IF_OK_RUN_AND_UPDATE(result, get_battery_level(interface, battery_voltage_mv, &battery_level));

      if(IS_OK(result))
      {
         battery_status->battery_voltage_mv = battery_voltage_mv;
         battery_status->battery_level = battery_level;
         battery_status->charger_connected = pmic_status.charger_connected;
         battery_status->iin_ma = measurements.iin_ma;
         battery_status->vin_mv = measurements.vin_mv;

         battery_status->is_battery_measurement_stale = false;

         if(pmic_status.charging_completed)
         {
            battery_status->battery_state = BATTERY_STATE_CHARGING_COMPLETED;
            battery_status->battery_level = BATTERY_LEVEL_FULL;
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
result_t ring_battery_manager_init(ring_battery_manager_t *const self,
                                   const bq25150_driver_interface_t *_bq25150_pmic_driver)
{
   RETURN_ERR_IF_NULL(self, RING_BATTERY_MAN_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(_bq25150_pmic_driver, RING_BATTERY_MAN_ERROR_PTR_NULL);

   self->interface.parent = self;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.get_battery_status = get_battery_status;
   self->interface.disable_charging = disable_charging;
   self->interface.disable_low_power_mode = disable_low_power_mode;
   self->interface.enable_charging = enable_charging;
   self->interface.enable_low_power_mode = enable_low_power_mode;
   self->interface.enter_ship_mode = enter_ship_mode;
   self->interface.is_power_prox_sensor_and_leds_enabled = is_power_prox_sensor_and_leds_enabled;
   self->interface.power_prox_sensor_and_leds = power_prox_sensor_and_leds;

   self->_bq25150_pmic_driver = _bq25150_pmic_driver;

   // Initialize the internal buffer for battery voltage measurements
   circular_buffer_init(&self->_voltage_buffer);

   // Initialize hardware
   // Set proximity sensor, IR transceiver and RGB LED power on, initially. This is to allow proper initialization of
   // all of the relevant modules.
   self->_is_proximity_sensor_circuit_enabled = false;
   result_t result = self->interface.power_prox_sensor_and_leds(&self->interface, true);

   // Set low power mode off initially.
   IF_OK_RUN_AND_UPDATE(result, self->interface.disable_low_power_mode(&self->interface));

   if(IS_OK(result))
   {
      self->_is_low_power_mode_enabled = false;
   }

   // Initialize the battery status structure
   battery_status_t battery_status = {0};
   IF_OK_RUN_AND_UPDATE(result, get_battery_status(&self->interface, &battery_status));

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
