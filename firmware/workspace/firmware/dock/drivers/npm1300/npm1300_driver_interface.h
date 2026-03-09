/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file npm1300_driver_interface.h
 * @ingroup pmic_driver
 * @brief PMIC driver interface to be used by the battery manager module.
 */

#ifndef NPM1300_DRIVER_INTERFACE_H_
#define NPM1300_DRIVER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
// Custom includes
#include "common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define NPM1300_VFS_VBAT_MV          (5000u) // Full scale voltage for VBAT in mV
#define NPM1300_ADC_RESOLUTION_STEPS (1023u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef enum
{
   PMIC_NTC_STATUS_COLD = 0,
   PMIC_NTC_STATUS_COOL,
   PMIC_NTC_STATUS_WARM,
   PMIC_NTC_STATUS_HOT,
   PMIC_NTC_STATUS_UNKNOWN,
   PMIC_NTC_STATUS_MAX
} PMIC_NTC_STATUS;

typedef struct
{
   uint16_t vbat_adc; // ADCVBAT0RESULTMSB
} pmic_battery_measurements_t;

typedef struct
{
   bool charger_connected;         // VBUS detected. VBUSINSTATUS
   bool charger_good;              // False if VBUS OV, UV, OC, suspended, or out active. VBUSINSTATUS
   bool battery_detected;          // BCHGCHARGESTATUS. Checks if battery present when VBUS connected.
   bool charging;                  // BCHGCHARGESTATUS
   bool charging_completed;        // BCHGCHARGESTATUS
   bool charging_stopped_die_temp; // Charging stopped due Die Temp high. BCHGCHARGESTATUS
   bool vbat_measurement_ready;    // EVENTSADCSET
   bool is_reset_detected;         // Flag is set when REG_RSTCAUSE_R is not 0x00
   PMIC_NTC_STATUS ntc_status;     // NTCSTATUS
} pmic_status_t;

struct npm1300_driver;                                              // Forward declaration
typedef struct npm1300_driver_interface npm1300_driver_interface_t; // Forward declaration

struct npm1300_driver_interface
{
   struct npm1300_driver *parent; // Reference to the containing instance.

   /**
    * @brief Retrieves the current status of the PMIC.
    *
    * This function reads the status registers of the PMIC and populates the provided status structure.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param status Pointer to a pmic_status_t structure where the status will be stored.
    *
    * @return Result of the operation.
    */
   result_t (*get_pmic_status)(const npm1300_driver_interface_t *const interface, pmic_status_t *status);

   /**
    * @brief Retrieves the current status of BUCK1.
    *
    * This function reads the BUCK1 status register and populates the provided boolean variable.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param is_enabled Pointer to a boolean variable where the BUCK1 status will be stored.
    *
    * @return Result of the operation.
    */
   result_t (*get_buck1_status)(const npm1300_driver_interface_t *const interface, bool *is_enabled);

   /**
    * @brief Sets the status of BUCK1.
    *
    * This function enables or disables BUCK1 based on the provided boolean value.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param enable Boolean value indicating whether BUCK1 should be enabled (true) or disabled (false).
    *
    * @return Result of the operation.
    */
   result_t (*set_buck1_status)(const npm1300_driver_interface_t *const interface, bool enable);

   /**
    * @brief Sets the status of a specified PMIC load switch.
    *
    * This function enables or disables the specified load switch based on the provided boolean value.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param switch_on Boolean value indicating whether the load switch should be enabled (true) or disabled (false).
    *
    * @return Result of the operation.
    */
   result_t (*set_load_switch1)(const npm1300_driver_interface_t *const interface, bool switch_on);

   /**
    * @brief Sets the status of a specified PMIC load switch.
    *
    * This function enables or disables the specified load switch based on the provided boolean value.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param switch_on Boolean value indicating whether the load switch should be enabled (true) or disabled (false).
    *
    * @return Result of the operation.
    */
   result_t (*set_load_switch2)(const npm1300_driver_interface_t *const interface, bool switch_on);

   /**
    * @brief Places the device in ship mode
    *
    * This function places the PMIC in ship mode and removes power to the rest of the circuit entirely
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*enter_ship_mode)(const npm1300_driver_interface_t *const interface);

   /**
    * @brief Retrieves the current status of PMIC load switch 1.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param power_enabled Bool indicating if the switch is currently set or not: True == Power enabled.
    *
    * @return Result of the operation.
    */
   result_t (*get_load_switch1_status)(const npm1300_driver_interface_t *const interface, bool *power_enabled);
   /**
    * @brief Retrieves the current status of PMIC load switch 2.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param power_enabled Bool indicating if the switch is currently set or not: True == Power enabled.
    *
    * @return Result of the operation.
    */
   result_t (*get_load_switch2_status)(const npm1300_driver_interface_t *const interface, bool *power_enabled);
   /**
    * @brief Enables Auto measurement of battery voltage
    *
    * This configures the PMIC to automatically read the battery voltage and current every second.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*enable_auto_measurements)(const npm1300_driver_interface_t *const interface);

   /**
    * @brief Disables Auto measurement of battery voltage
    *
    * This configures the PMIC to read the battery voltage and current only as requested. Implemented as a power saving
    * measure
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*disable_auto_measurements)(const npm1300_driver_interface_t *const interface);

   /**
    * @brief Retrieves the current battery measurements.
    *
    * This function reads the ADC registers and populates the provided measurements structure.
    * It also starts new measurements for VSYS and clears the VBAT ready interrupt.
    *
    * @param interface Pointer to the npm1300_driver_interface_t instance.
    * @param measurements Pointer to a pmic_battery_measurements_t structure where the measurements will be stored.
    *
    * @return Result of the operation.
    *
    * @note Use @p get_pmic_status() and check the @p vbat_measurement_ready parameter from @p pmic_status_t struct to
    * determine if next vbat measurement is ready.
    * @note The @p vbat_measurement_ready parameter indicates when all other measurements are also ready since it has
    * the lowest sampling rate.
    */
   result_t (*get_measurements)(const npm1300_driver_interface_t *const interface,
                                pmic_battery_measurements_t *measurements);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // NPM1300_DRIVER_INTERFACE_H_
