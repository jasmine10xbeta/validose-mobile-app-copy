/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file app_statemachine.h
 * @ingroup gc_module
 * @brief General application state machine.
 * @details This module implements a general application state machine that handles various states of the application.
 * It is built on top of the generic state machine.
 */

#ifndef SYSTEM_FSM_H_
#define SYSTEM_FSM_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "statemachine.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#ifdef DEBUG_SLEEP_PERIOD_EN
#   define SLEEP_PERIOD_STORAGE_MS      (2000u)
#   define SLEEP_PERIOD_IDLE_MS         (1000u)
#   define SLEEP_PERIOD_ACTIVE_MS       (1000u)
#   define SLEEP_PERIOD_BLE_PAIRING_MS  (1000u)
#   define SLEEP_PERIOD_INVALID_DOSE_MS (1000u)
#else
// NOTE: SYSTICK_PERIOD < SLEEP_PERIOD and the SYSTICK_PERIOD shall be a factor of SLEEP_PERIOD.

#   define SLEEP_PERIOD_STORAGE_MS   (2000u) // Unbonded state
#   define SYSTICK_PERIOD_STORAGE_MS (500u)
// Compile-time checks for sleep/systick period relationship.
STATIC_ASSERT(SYSTICK_PERIOD_STORAGE_MS < SLEEP_PERIOD_STORAGE_MS,
              "SYSTICK_PERIOD_STORAGE_MS must be less than SLEEP_PERIOD_STORAGE_MS");
STATIC_ASSERT((SLEEP_PERIOD_STORAGE_MS % SYSTICK_PERIOD_STORAGE_MS) == 0u,
              "SYSTICK_PERIOD_STORAGE_MS must be a factor of SLEEP_PERIOD_STORAGE_MS");

#   define SLEEP_PERIOD_IDLE_MS   (100u)
#   define SYSTICK_PERIOD_IDLE_MS (25u)
STATIC_ASSERT(SYSTICK_PERIOD_IDLE_MS < SLEEP_PERIOD_IDLE_MS,
              "SYSTICK_PERIOD_IDLE_MS must be less than SLEEP_PERIOD_IDLE_MS");
STATIC_ASSERT((SLEEP_PERIOD_IDLE_MS % SYSTICK_PERIOD_IDLE_MS) == 0u,
              "SYSTICK_PERIOD_IDLE_MS must be a factor of SLEEP_PERIOD_IDLE_MS");

#   define SLEEP_PERIOD_ACTIVE_MS   (100u)
#   define SYSTICK_PERIOD_ACTIVE_MS (25u)
STATIC_ASSERT(SYSTICK_PERIOD_ACTIVE_MS < SLEEP_PERIOD_ACTIVE_MS,
              "SYSTICK_PERIOD_ACTIVE_MS must be less than SLEEP_PERIOD_ACTIVE_MS");
STATIC_ASSERT((SLEEP_PERIOD_ACTIVE_MS % SYSTICK_PERIOD_ACTIVE_MS) == 0u,
              "SYSTICK_PERIOD_ACTIVE_MS must be a factor of SLEEP_PERIOD_ACTIVE_MS");

#   define SLEEP_PERIOD_CALIBRATE_MS   (SLEEP_PERIOD_ACTIVE_MS)
#   define SYSTICK_PERIOD_CALIBRATE_MS (SYSTICK_PERIOD_ACTIVE_MS)
STATIC_ASSERT(SYSTICK_PERIOD_CALIBRATE_MS < SLEEP_PERIOD_CALIBRATE_MS,
              "SYSTICK_PERIOD_CALIBRATE_MS must be less than SLEEP_PERIOD_CALIBRATE_MS");
STATIC_ASSERT((SLEEP_PERIOD_CALIBRATE_MS % SYSTICK_PERIOD_CALIBRATE_MS) == 0u,
              "SYSTICK_PERIOD_CALIBRATE_MS must be a factor of SLEEP_PERIOD_CALIBRATE_MS");

#   define SLEEP_PERIOD_BLE_PAIRING_MS   (50u)
#   define SYSTICK_PERIOD_BLE_PAIRING_MS (25u)
STATIC_ASSERT(SYSTICK_PERIOD_BLE_PAIRING_MS < SLEEP_PERIOD_BLE_PAIRING_MS,
              "SYSTICK_PERIOD_BLE_PAIRING_MS must be less than SLEEP_PERIOD_BLE_PAIRING_MS");
STATIC_ASSERT((SLEEP_PERIOD_BLE_PAIRING_MS % SYSTICK_PERIOD_BLE_PAIRING_MS) == 0u,
              "SYSTICK_PERIOD_BLE_PAIRING_MS must be a factor of SLEEP_PERIOD_BLE_PAIRING_MS");

#   define SLEEP_PERIOD_INVALID_DOSE_INFO_MS   (200u)
#   define SYSTICK_PERIOD_INVALID_DOSE_INFO_MS (50u)
STATIC_ASSERT(SYSTICK_PERIOD_INVALID_DOSE_INFO_MS < SLEEP_PERIOD_INVALID_DOSE_INFO_MS,
              "SYSTICK_PERIOD_INVALID_DOSE_INFO_MS must be less than SLEEP_PERIOD_INVALID_DOSE_INFO_MS");
STATIC_ASSERT((SLEEP_PERIOD_INVALID_DOSE_INFO_MS % SYSTICK_PERIOD_INVALID_DOSE_INFO_MS) == 0u,
              "SYSTICK_PERIOD_INVALID_DOSE_INFO_MS must be a factor of SLEEP_PERIOD_INVALID_DOSE_INFO_MS");
#endif // DEBUG_SLEEP_PERIOD_EN
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief List of state machine states.
 */
typedef enum
{
   STATEMACHINE_STATE_UNBONDED = 0,
   STATEMACHINE_STATE_IDLE,
   STATEMACHINE_STATE_ACTIVE,
   STATEMACHINE_STATE_INVALID_DOSE_INFO,
   STATEMACHINE_STATE_PAIRING_BLE,
   STATEMACHINE_STATE_CALIBRATION,
   STATEMACHINE_STATE_BASELINING,
   STATEMACHINE_STATE_MAX
} STATEMACHINE_STATE;

typedef enum
{
   SYSTEM_FSM_ERROR_NONE = 0,
   SYSTEM_FSM_ERROR_NULL_PTR,
   SYSTEM_FSM_ERROR_MAX
} SYSTEM_FSM_ERROR;

/**
 * @brief Input data structure for the general control state machine. This data is used by the state machine logic to
 * determnine the next state.
 */
typedef struct
{
   bool is_calibration_active; // Weight calibration process for the scale
   bool is_baselining_active;  // Weight baselining process for the scale
   bool battery_charger_connected;
   BONDING_STATE bonding_state;
   bool is_valid_dose_schedule_detected;
   bool is_dose_window_active;
} system_fsm_inputs_t;

/**
 * @brief Output data structure for the general control state machine.
 *
 *
 * @param current_state_loop_period_ms The loop period in milliseconds for the current state.
 * @param current_state_systick_period_ms The systick period in milliseconds for the current state.
 *
 *
 * @note The @p current_state_loop_period_ms and @p current_state_systick_period_ms allows dynamic adjustment of the
 * state machine's loop period, impacting battery life.
 */
typedef struct
{
   bool changed; // Indicates if the state changed since the last call to the state machine.
   uint32_t current_state_loop_period_ms;
   uint16_t current_state_systick_period_ms;
} system_fsm_outputs_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
extern const char *system_fsm_state_names[];
/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t system_fsm_init(statemachine_t *const self);
#endif // SYSTEM_FSM_H
