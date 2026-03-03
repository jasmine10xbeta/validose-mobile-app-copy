/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup battery_manager_module Battery Manager
 * @ingroup modules
 * @brief Battery and charge management for the CSU
 * @details
 *
 * @file battery_manager.h
 * @ingroup battery_manager_module
 * @brief
 */

#ifndef BATTERY_MANAGER_H_
#define BATTERY_MANAGER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "battery_manager_interface.h"
#include "common.h"
#include "npm1300_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define BATTERY_LEVEL_LOW_TH          (20u) // Percentage
#define BATTERY_LEVEL_INCREMENT       (10u)
#define BATTERY_LEVEL_FULL            (100u) // Percentage
#define BATTERY_LEVEL_98_PERCENT_FULL (98u)  // Percentage
#define BATTERY_LEVEL_ENABLE_CHARGER  (90u)  // Percentage

#define CIRCULAR_BUFFER_SIZE (60u) // Battery level taken as average over the last CIRCULAR_BUFFER_SIZE measurements
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   BATTERY_MAN_ERROR_NONE = 0,
   BATTERY_MAN_ERROR_PTR_NULL,
   BATTERY_MAN_ERROR_NRF_ERROR,
   BATTERY_MAN_ERROR_INTERNAL,
   BATTERY_MAN_ERROR_ADC_VALUE_OUT_OF_RANGE,
   BATTERY_MAN_ERROR_NO_BATTERY,
   BATTERY_MAN_ERROR_NOT_INITIALIZED,
   BATTERY_MAN_ERROR_MAX,
} BATTERY_MAN_ERROR;

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

typedef struct battery_manager battery_manager_t;

struct battery_manager
{
   battery_manager_interface_t interface;
   const npm1300_driver_interface_t *_npm1300_driver;
   circular_buffer_t _voltage_buffer; // Circular buffer for storing battery voltage measurements.
   // Indicates whether the flash memory and load cell power is powered. PMIC Switch 1
   bool _is_flash_memory_and_load_cell_powered;
   // Indicates whether the buzzer, LEDs, and NFC power is powered. PMIC Switch 2
   bool _is_buzzer_leds_nfc_powered;
   bool _initialized; // Indicates whether the unit has been initialized.
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t battery_manager_init(battery_manager_t *const self, const npm1300_driver_interface_t *npm1300_driver);

#endif // BATTERY_MANAGER_H_