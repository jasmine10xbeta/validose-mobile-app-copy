/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file battery_manager_interface.h
 * @ingroup battery_manager_module
 * @brief
 */

#ifndef BATTERY_MANAGER_INTERFACE_H_
#define BATTERY_MANAGER_INTERFACE_H_
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
} battery_status_t;

struct battery_manager; // Forward declaration

typedef struct battery_manager_interface battery_manager_interface_t;

struct battery_manager_interface
{
   struct battery_manager *parent; // Reference to the battery manager instance.

   result_t (*power_buzzer_leds_nfc)(const battery_manager_interface_t *const interface, bool enabled);

   result_t (*is_power_buzzer_leds_nfc_enabled)(const battery_manager_interface_t *const interface, bool *enabled);

   /**
    * @brief This function enables or disables the flash memory IC and load cell IC by enable/disabling power to the
    * circuit/s.
    *
    * @param interface A pointer to the battery manager interface.
    * @param enabled A boolean indicating whether the flash memory IC should be enabled (true) or disabled (false).
    *
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*power_flash_memory_and_load_cell)(const battery_manager_interface_t *const interface, bool enabled);

   /**
    * @brief This function checks if power to the flash memory IC is currently enabled.
    *
    * @param interface A pointer to the battery manager interface.
    * @param enabled A pointer to a boolean that will be set to true if the flash memory IC is enabled, and false
    * otherwise.
    *
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*is_power_flash_memory_and_load_cell_enabled)(const battery_manager_interface_t *const interface,
                                                           bool *enabled);

   /**
    * @brief This function retrieves the current battery level, charger status, and battery state.
    *
    * Battery voltage and level are averaged over the most recent @p CIRCULAR_BUFFER_SIZE VBAT measurements.
    * When PMIC auto measurements are enabled, VBAT is sampled at 1 Hz, so the effective averaging window is up to
    * ~60 seconds (60 samples). If fewer samples are available (e.g., shortly after startup), the average uses only
    * the samples collected so far.
    *
    * When auto measurements are disabled, new VBAT samples are only available after explicit PMIC measurement requests
    * elsewhere in the system. In that case, the averaging window advances only when such a measurement becomes ready,
    * and `is_battery_measurement_stale` will remain true between updates.
    *
    * Updates occur only when the PMIC reports a new VBAT measurement is ready. When no new measurement is available,
    * @p battery_status->is_battery_measurement_stale is set true and voltage/level are not updated for this call.
    *
    * @param interface A pointer to the battery manager interface.
    * @param battery_status A pointer to a battery_status_t structure that will be filled with the current battery
    * status.
    *
    * @return A result_t indicating the success or failure of the operation.
    */
   result_t (*get_battery_status)(const battery_manager_interface_t *const interface, battery_status_t *battery_status);

   /**
    * @brief Enables Auto measurement of battery voltage
    *
    * This configures the PMIC to automatically read the battery voltage and current every second.
    *
    * @param interface Pointer to the battery_manager_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*enable_auto_measurements)(const battery_manager_interface_t *const interface);

   /**
    * @brief Disables Auto measurement of battery voltage
    *
    * This configures the PMIC to read the battery voltage and current only as requested. Implemented as a power saving
    * measure
    *
    * @param interface Pointer to the battery_manager_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*disable_auto_measurements)(const battery_manager_interface_t *const interface);

   /**
    * @brief Places the device in ship mode
    *
    * This function places the PMIC in ship mode. If there is no charger connected, the device will enter ship mode
    * immediately and, in doing so, removes power to the rest of the circuit including the MCU entirely.
    *
    * @param interface A pointer to the battery manager interface.
    *
    * @return Result of the operation.
    */
   result_t (*enter_ship_mode)(const battery_manager_interface_t *const interface);

   /**
    * @brief Enable wireless QI charging.
    *
    * @param interface A pointer to the battery manager interface.
    *
    * @return Result of the operation.
    */
   result_t (*enable_wireless_charging)(const battery_manager_interface_t *const interface);

   /**
    * @brief Disable wireless QI charging.
    *
    * @param interface A pointer to the battery manager interface.
    *
    * @return Result of the operation.
    */
   result_t (*disable_wireless_charging)(const battery_manager_interface_t *const interface);

   /**
    * @brief Disable wireless QI charging.
    *
    * @param interface A pointer to the battery manager interface.
    * @param is_enabled Bool to indicate whether wireless charging is currently enabled or not.
    *
    * @return Result of the operation.
    */
   result_t (*get_wireless_charging_status)(const battery_manager_interface_t *const interface, bool *is_enabled);
};
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // BATTERY_MANAGER_INTERFACE_H_
