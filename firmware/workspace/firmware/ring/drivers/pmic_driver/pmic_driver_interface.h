/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file bq25150_driver_interface.h
 * @ingroup pmic_driver
 * @brief PMIC driver interface to be used by the battery manager module.
 *
 * @todo Update documentation.
 */

#ifndef pmic_driver_interface_H_
#define pmic_driver_interface_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

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
   uint16_t vbat_mv; // Battery voltage.
   uint16_t vin_mv;  // Input voltage from the charger.
   uint16_t iin_ma;  // Input current from the charger.

} pmic_battery_measurements_t;

typedef struct
{
   bool charger_connected;
   bool charger_good;
   bool battery_detected;
   bool charging;
   bool charging_completed;
   bool charging_stopped_die_temp;
   bool vbat_measurement_ready;
   bool is_reset_detected; // Not implemented.
   PMIC_NTC_STATUS ntc_status;
   uint8_t flag_reg[4];    // Raw data from the FLAG0 - FLAG3 registers in the PMIC
   uint8_t status_reg[2];  //  Raw data from the STAT0 - STAT1 registers in the PMIC
} pmic_status_t;

struct bq25150_driver;                                              // Forward declaration
typedef struct bq25150_driver_interface bq25150_driver_interface_t; // Forward declaration

typedef struct bq25150_driver_interface
{
   struct bq25150_driver *parent; // Reference to the containing instance.

   /**
    * @brief Retrieves the current status of the PMIC.
    *
    * This function reads the status registers of the PMIC and populates the provided status structure.
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    * @param status Pointer to a pmic_status_t structure where the status will be stored.
    *
    * @return Result of the operation.
    */

   result_t (*get_pmic_status)(const bq25150_driver_interface_t *const interface, pmic_status_t *status);

   /**
    * @brief Retrieves the current pmic measurements.
    *
    * This function reads the ADC registers and populates the provided measurements structure.
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    * @param measurements Pointer to a pmic_battery_measurements_t structure where the measurements will be stored.
    *
    * @return Result of the operation.
    *
    * @note Use @p get_pmic_status() and check the @p vbat_measurement_ready parameter from @p pmic_status_t struct to
    * determine if next vbat measurement is ready.
    * @note The @p vbat_measurement_ready parameter indicates when all other measurements are also ready since it has
    * the lowest sampling rate.
    */
   result_t (*get_measurements)(const bq25150_driver_interface_t *const interface,
                                pmic_battery_measurements_t *measurements);

   /**
    * @brief Places the device in ship mode.
    *
    * This function places the PMIC in ship mode and removes power to the rest of the circuit entirely.
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    *
    * @return Result of the operation.
    */
   result_t (*enter_ship_mode)(const bq25150_driver_interface_t *const interface);

   /**
    * @brief Sets the status of a specified PMIC load switch / LDO.
    *
    * This function enables or disables the specified load LDO switch based on the provided boolean value.
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    * @param switch_on Boolean value indicating whether the load switch / LDO should be enabled (true) or disabled
    * (false).
    *
    * @return Result of the operation.
    */
   result_t (*set_ldo_status)(const bq25150_driver_interface_t *const interface, bool switch_on);

   /**
    * @brief Retrieves the current status of the PMIC load switch / LDO..
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    * @param is_power_enabled Boolean value indicating whether the load switch / LDO is enabled (true) or disabled
    * (false).
    *
    * @return Result of the operation.
    */
   result_t (*get_ldo_status)(const bq25150_driver_interface_t *const interface, bool *is_power_enabled);

   /**
    * @brief Sets the charging enable status of the PMIC.
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    * @param enable_charge Boolean value indicating whether charging should be enabled (true) or disabled (false).
    *
    * @return Result of the operation.
    *
    * @note If the safety timer fault (BQ25150_DRV_ERROR_SAFETY_TIMER) is detected, the charger should be disabled and
    * reenabled to allow further charging.
    */
   result_t (*set_charge_enable)(const bq25150_driver_interface_t *const interface, bool enable_charge);

   /**
    * @brief Sets the low power mode status of the PMIC.
    *
    * @param interface Pointer to the bq25150_driver_interface_t instance.
    * @param enable_lp Boolean value indicating whether the low power mode should be enabled (true) or disabled
    * (false).
    *
    * @return Result of the operation.
    *
    * @note The PMIC I2C bus is disabled in low power mode.
    */
   result_t (*set_low_power_mode)(const bq25150_driver_interface_t *const interface, bool enable_lp);

} bq25150_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // pmic_driver_interface_H_
