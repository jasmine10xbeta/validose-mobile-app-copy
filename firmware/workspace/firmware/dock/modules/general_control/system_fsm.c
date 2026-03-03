/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file app_statemachine.c
 * @ingroup gc_module
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <math.h>
#include <stdbool.h>

// Custom includes
#include "statemachine.h"
#include "system_fsm.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_SYSTEM_FSM;

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
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

// State: Unbonded
static bool ble_bonded_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return (BONDING_STATE_BONDED == input->bonding_state);
}

static void ble_bonded_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

static bool charger_connected_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return input->battery_charger_connected;
}

static void charger_connected_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

// State: Idle
static bool valid_dose_sch_and_active_dose_win_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return ((input->is_valid_dose_schedule_detected) && (input->is_dose_window_active));
}

static void valid_dose_sch_and_active_dose_win_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_ACTIVE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_ACTIVE_MS;
}

static bool bonding_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return ((input->battery_charger_connected) && (BONDING_STATE_BONDING == input->bonding_state));
}

static void bonding_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_BLE_PAIRING_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_BLE_PAIRING_MS;
}

static bool charger_disconnected_unbonded_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return (!(input->battery_charger_connected) && (BONDING_STATE_UNBONDED == input->bonding_state));
}

static void charger_disconnected_unbonded_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_STORAGE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_STORAGE_MS;
}

static bool invalid_dose_sch_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return !input->is_valid_dose_schedule_detected;
}

static void invalid_dose_sch_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_INVALID_DOSE_INFO_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_INVALID_DOSE_INFO_MS;
}

// State: Active
static bool inactive_dose_win_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return !input->is_dose_window_active;
}

static void inactive_dose_win_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

// State: Invalid dose info
static bool valid_dose_sch_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return input->is_valid_dose_schedule_detected;
}

static void valid_dose_sch_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

// State: Pairing
static bool bonded_or_unbonded_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return ((BONDING_STATE_BONDED == input->bonding_state) || (BONDING_STATE_UNBONDED == input->bonding_state));
}

static void bonded_or_unbonded_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

static bool charger_not_connected_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return !(input->battery_charger_connected);
}

static void charger_not_connected_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

// State: Calibration
static bool calibration_enabled_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return (input->is_calibration_active);
}

static void calibration_enabled_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_CALIBRATE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_CALIBRATE_MS;
}

static bool calibration_disabled_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return !(input->is_calibration_active);
}

static void calibration_disabled_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

// Weight baselining process
static bool is_baselining_active_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return input->is_baselining_active;
}

static void baselining_active_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_ACTIVE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_ACTIVE_MS;
}

static bool is_baselining_inactive_test(const void *const inputs)
{
   const system_fsm_inputs_t *input = (const system_fsm_inputs_t *)inputs;
   return !(input->is_baselining_active);
}

static void baselining_inactive_result(void *const outputs)
{
   system_fsm_outputs_t *output = (system_fsm_outputs_t *)outputs;
   output->changed = true;
   output->current_state_loop_period_ms = SLEEP_PERIOD_IDLE_MS;
   output->current_state_systick_period_ms = SYSTICK_PERIOD_IDLE_MS;
}

////////////////////////////////////////////////////////////////////////////////
// @note The order of the state transitions is important and should match the order of the enum defined in
// app_statemachine.h
static statemachine_transition_definition _unbonded_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, charger_connected_test, charger_connected_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, ble_bonded_test, ble_bonded_result)};

static statemachine_transition_definition _idle_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_PAIRING_BLE, bonding_test, bonding_result),
      STATE_TRANSITION_DEF(
         STATEMACHINE_STATE_UNBONDED, charger_disconnected_unbonded_test, charger_disconnected_unbonded_result),
      STATE_TRANSITION_DEF(
         STATEMACHINE_STATE_ACTIVE, valid_dose_sch_and_active_dose_win_test, valid_dose_sch_and_active_dose_win_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_INVALID_DOSE_INFO, invalid_dose_sch_test, invalid_dose_sch_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_CALIBRATION, calibration_enabled_test, calibration_enabled_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_BASELINING, is_baselining_active_test, baselining_active_result)};

static statemachine_transition_definition _active_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, inactive_dose_win_test, inactive_dose_win_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_PAIRING_BLE, bonding_test, bonding_result)};

static statemachine_transition_definition _invalid_dose_info_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, valid_dose_sch_test, valid_dose_sch_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_PAIRING_BLE, bonding_test, bonding_result)};

static statemachine_transition_definition _pairing_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, bonded_or_unbonded_test, bonded_or_unbonded_result),
      STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, charger_not_connected_test, charger_not_connected_result)};

static statemachine_transition_definition _calibration_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, calibration_disabled_test, calibration_disabled_result)};

static statemachine_transition_definition _baselining_state_transitions[]
   = {STATE_TRANSITION_DEF(STATEMACHINE_STATE_IDLE, is_baselining_inactive_test, baselining_inactive_result)};

// @note The order of the state transitions is important should and match the order of the enum defined in
// app_statemachine.h
static statemachine_state_transitions _state_transitions[] = {STATE_TRANSITIONS(_unbonded_state_transitions),
                                                              STATE_TRANSITIONS(_idle_state_transitions),
                                                              STATE_TRANSITIONS(_active_state_transitions),
                                                              STATE_TRANSITIONS(_invalid_dose_info_state_transitions),
                                                              STATE_TRANSITIONS(_pairing_state_transitions),
                                                              STATE_TRANSITIONS(_calibration_state_transitions),
                                                              STATE_TRANSITIONS(_baselining_state_transitions)};

static statemachine_transitions _transitions = STATEMACHINE_TRANSITIONS(_state_transitions);

result_t system_fsm_init(statemachine_t *const self)
{
   RETURN_ERR_IF_NULL(self, SYSTEM_FSM_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   statemachine(self, STATEMACHINE_STATE_IDLE, &_transitions);
   return result;
}

// Note the order of the state names must match the order in the enum defined in application state machine.
const char *system_fsm_state_names[]
   = {"UNBONDED", "IDLE", "ACTIVE", "INVALID DOSE INFO", "PAIRING", "CALIBRATION", "BASELINING"};