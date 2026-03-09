/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_ring_battery_manager_interface.h
 * @ingroup modules/ring_battery_manager
 * @brief
 * Works with BMS - PMIC BQ25150 DRIVER
 */

#ifndef RING_BATTERY_MANAGER_INTERFACE_H_
#define RING_BATTERY_MANAGER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct
{
   uint8_t battery_level; // Percentage
   uint16_t battery_voltage_mv;
   bool charger_connected;
   bool is_pmic_reset_detected;
   BATTERY_STATE battery_state;
   bool is_battery_measurement_stale;
   uint16_t vin_mv; // Input voltage from the charger.
   uint16_t iin_ma; // Input current from the charger.
} battery_status_t;

struct ring_battery_manager; // Forward declaration

typedef struct ring_battery_manager_interface ring_battery_manager_interface_t;

struct ring_battery_manager_interface
{
   struct ring_battery_manager *parent; // Reference to the battery manager instance.

   /**
    * @brief This function enables or disables the the power to the RGB LED and IR proximtity sensor IC/s.
    * This includes the IR Transceiver
    *
    * @param interface A pointer to the battery manager interface.
    * @param enabled A boolean indicating whether the prox sensor IC & RGB LED's
    * should be enabled (true) or disabled (false).
    *
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*power_prox_sensor_and_leds)(const ring_battery_manager_interface_t *const interface, bool enabled);

   /**
    * @brief This function checks if power to the flash memory IC is currently enabled.
    *
    * @param interface A pointer to the battery manager interface.
    * @param enabled A pointer to a boolean that will be set to true if the prox sensor IC and RGB LED's are
    * enabled, and false otherwise.
    *
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*is_power_prox_sensor_and_leds_enabled)(const ring_battery_manager_interface_t *const interface,
                                                     bool *enabled);

   /**
    * @brief This function retrieves the current battery level, the charger connected status, and the battery state. If
    * a new measurement isn't available, it returns the last known @p battery_state.
    *
    * @param interface A pointer to the battery manager interface.
    * @param battery_status A pointer to a battery_status_t structure that will be filled with the current battery
    * status.
    *
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*get_battery_status)(const ring_battery_manager_interface_t *const interface,
                                  battery_status_t *battery_status);

   /**
    * @brief Enables charging
    *
    * @param interface Pointer to the ring_battery_manager_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*enable_charging)(const ring_battery_manager_interface_t *const interface);

   /**
    * @brief Disbales charging
    *
    * @param interface Pointer to the ring_battery_manager_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*disable_charging)(const ring_battery_manager_interface_t *const interface);

   /**
    * @brief Enables low power mode
    *
    * @param interface Pointer to the ring_battery_manager_interface_t instance.
    *
    * @return Result of the operation.
    *
    */
   result_t (*enable_low_power_mode)(const ring_battery_manager_interface_t *const interface);

   /**
    * @brief Disables low power mode
    *
    * @param interface Pointer to the ring_battery_manager_interface_t instance.
    *
    * @return Result of the operation.
    *
    */
   result_t (*disable_low_power_mode)(const ring_battery_manager_interface_t *const interface);

   /**
    * @brief Places the device in ship mode
    *
    * This function places the PMIC in ship mode and removes power to the rest of the IC entirely
    *
    * @param interface A pointer to the battery manager interface.
    *
    * @return Result of the operation.
    */
   result_t (*enter_ship_mode)(const ring_battery_manager_interface_t *const interface);
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // RING_BATTERY_MANAGER_INTERFACE_H_