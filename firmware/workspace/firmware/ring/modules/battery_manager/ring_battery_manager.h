/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_battery_manager.h
 * @ingroup modules/battery_manager
 * @brief
 */

#ifndef RING_BATTERY_MANAGER_H_
#define RING_BATTERY_MANAGER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>
// Custom includes
#include "common.h"
#include "pmic_driver_interface.h" // BQ25150
#include "ring_battery_manager_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define BATTERY_LEVEL_LOW_TH          (20u)  // Percentage
#define BATTERY_LEVEL_INCREMENT       (BATTERY_LEVEL_LOW_TH)
#define BATTERY_LEVEL_FULL            (100u) // Percentage
#define BATTERY_LEVEL_98_PERCENT_FULL (98u)  // Percentage

#define BATTERY_FULL_TH_VOLTAGE_MV  ((uint32_t)4100u)
#define BATTERY_EMPTY_TH_VOLTAGE_MV ((uint32_t)3600u)

#define CIRCULAR_BUFFER_SIZE (60u) // Battery level taken as average over the last CIRCULAR_BUFFER_SIZE measurements
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   RING_BATTERY_MAN_ERROR_NONE = 0,
   RING_BATTERY_MAN_ERROR_PTR_NULL,
   RING_BATTERY_MAN_ERROR_NRF_ERROR,
   RING_BATTERY_MAN_ERROR_INTERNAL,
   RING_BATTERY_MAN_ERROR_VBAT_NOT_AVAILABLE,
   RING_BATTERY_MAN_ERROR_NO_BATTERY,
   RING_BATTERY_MAN_ERROR_MAX,
} RING_BATTERY_MAN_ERROR;

/**
 * @brief Circular buffer structure for storing battery voltage measurements used to calculate average battery voltage
 * over CIRCULAR_BUFFER_SIZE measurements.
 */
typedef struct
{
   uint16_t buffer[CIRCULAR_BUFFER_SIZE];
   uint16_t head;
   uint16_t num_elements;
} circular_buffer_t;

typedef struct ring_battery_manager ring_battery_manager_t;

/**
 * @brief A struct for the battery manager
 *
 * @param interface A pointer to the battery manager interface.
 * @param _bq25150_pmic_driver A pointer to the PMIC interface.
 * @param _voltage_buffer Circular buffer for storing battery voltage measurements.
 * @param _is_proximity_sensor_circuit_enabled Indicates whether the IR prox sensor IC and RGB LEDs are enabled
 * @param _is_low_power_mode_enabled Indicates whether low power mode  is enabled.
 * @param _is_charging_enabled Indicates whether charging is enabled.
 * @param _initialized Indicates whether the unit has been initialized.
 *
 * @return A result_t indicating the success or failure of the operation.
 */
struct ring_battery_manager
{
   ring_battery_manager_interface_t interface;
   const bq25150_driver_interface_t *_bq25150_pmic_driver;
   circular_buffer_t _voltage_buffer;
   bool _is_proximity_sensor_circuit_enabled;
   bool _is_low_power_mode_enabled;
   bool _is_charging_enabled;
   bool _initialized;
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_battery_manager_init(ring_battery_manager_t *const self,
                                   const bq25150_driver_interface_t *_bq25150_pmic_driver);

#endif // RING_BATTERY_MANAGER_H_