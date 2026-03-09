/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file baselining_fsm.c
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
#include "baselining_fsm.h"
#include "statemachine.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BASELINING_FSM;

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
static const system_time_interface_t *m_systick_ifc;
static statemachine_t const *m_self;
static uint64_t m_prev_state_change_time_ms = 0;

// Note: The m_state_timeouts depend on the order of the states in BASELINING_STATE. Make sure to keep them in sync.
static uint32_t m_state_timeouts[BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION + 1]
   = {WAIT_FOR_RING_REMOVAL_STATE_TIMEOUT_MS,
      SET_DOCK_WEIGHT_STATE_TIMEOUT_MS,
      WAIT_FOR_RING_STATE_TIMEOUT_MS,
      SET_RING_DOCK_WEIGHT_STATE_TIMEOUT_MS,
      WAIT_FOR_BACKEND_VALIDATION_STATE_TIMEOUT_MS};

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

static bool is_backend_validation_and_dose_sch_valid_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;

   return ((BACKEND_VAL_STATUS_SUCCESS == input->backend_validation_status)
           && (DOSE_SCH_STATUS_VALID == input->dose_schedule_status));
}

static bool is_backend_validation_unsuccessful_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return (BACKEND_VAL_STATUS_UNSUCCESSFUL == input->backend_validation_status);
}

static bool is_invalid_dose_schedule_detected_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return (DOSE_SCH_STATUS_INVALID == input->dose_schedule_status);
}

static bool is_ring_present_with_med_uid_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return ((input->is_ring_present) && (input->is_medication_uid_avail));
}

static bool is_ring_present_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return (input->is_ring_present);
}

static bool is_ring_removed_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return (!input->is_ring_present);
}

static bool is_timeout_test(const void *const inputs)
{
   (void)inputs;
   bool ret_val = false;
   uint64_t current_time_ms = 0;
   result_t result = m_systick_ifc->get_time_ms(m_systick_ifc, &current_time_ms);
   if(IS_OK(result))
   {
      // Check if the current time has exceeded the timeout threshold for the current state.
      ret_val = (current_time_ms - m_prev_state_change_time_ms >= m_state_timeouts[m_self->current_state]);
   }
   else
   {
      // If there was an error getting the time, we don't want to trigger a timeout, so return false.
      ret_val = false;
   }

   return ret_val;
}

static bool is_baselining_canceled_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return !input->is_baselining_active;
}

static bool is_baseline_starting_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return input->start_baselining;
}

static bool is_approved_med_uid_store_successful_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return (MED_UID_STORAGE_SUCCESS == input->med_update_status);
}

static bool is_approved_med_uid_store_unsuccessful_test(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return (MED_UID_STORAGE_FAILED == input->med_update_status);
}

static bool is_empty_dock_weight_set(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return input->is_empty_dock_weight_set;
}

static bool is_dock_ring_full_med_weight_set(const void *const inputs)
{
   const baselining_fsm_inputs_t *input = (const baselining_fsm_inputs_t *)inputs;
   return input->is_ring_dock_full_med_weight_set;
}

/**
 * A generic result is used for every test function since the output change is always the same.
 */
static void generic_result(void *const outputs)
{
   baselining_fsm_outputs_t *output = (baselining_fsm_outputs_t *)outputs;
   output->changed = true;

   // Update prev state change time
   (void)m_systick_ifc->get_time_ms(m_systick_ifc, &m_prev_state_change_time_ms);
}
////////////////////////////////////////////////////////////////////////////////
// @note The order of the state transitions is important and should match the order of the enum defined in
// app_statemachine.h
static statemachine_transition_definition _wait_for_ring_removal[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_baselining_canceled_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_SET_DOCK_WEIGHT, is_ring_removed_test, generic_result)};

static statemachine_transition_definition _set_dock_weight[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_baselining_canceled_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_WAIT_FOR_RING_REMOVAL, is_ring_present_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_WAIT_FOR_RING, is_empty_dock_weight_set, generic_result)};

static statemachine_transition_definition _wait_for_ring[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_baselining_canceled_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_SET_RING_DOCK_WEIGHT, is_ring_present_with_med_uid_test, generic_result)};

static statemachine_transition_definition _set_ring_dock_weight[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_baselining_canceled_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_WAIT_FOR_RING, is_ring_removed_test, generic_result),
      STATE_TRANSITION_DEF(
         BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION, is_dock_ring_full_med_weight_set, generic_result)};

static statemachine_transition_definition _wait_for_backend_validation[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_backend_validation_unsuccessful_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_invalid_dose_schedule_detected_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_baselining_canceled_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(
         BASELINING_STATE_SET_LOCAL_MED_UID, is_backend_validation_and_dose_sch_valid_test, generic_result)};

static statemachine_transition_definition _store_med_uid[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_COMPLETE, is_approved_med_uid_store_successful_test, generic_result),
      STATE_TRANSITION_DEF(BASELINING_STATE_ERROR, is_approved_med_uid_store_unsuccessful_test, generic_result)};

static statemachine_transition_definition _complete[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_WAIT_FOR_RING_REMOVAL, is_baseline_starting_test, generic_result)};

static statemachine_transition_definition _error[]
   = {STATE_TRANSITION_DEF(BASELINING_STATE_WAIT_FOR_RING_REMOVAL, is_baseline_starting_test, generic_result)};

// @note The order of the state transitions is important should and match the order of the enum defined in
// app_statemachine.h
static statemachine_state_transitions _state_transitions[] = {STATE_TRANSITIONS(_wait_for_ring_removal),
                                                              STATE_TRANSITIONS(_set_dock_weight),
                                                              STATE_TRANSITIONS(_wait_for_ring),
                                                              STATE_TRANSITIONS(_set_ring_dock_weight),
                                                              STATE_TRANSITIONS(_wait_for_backend_validation),
                                                              STATE_TRANSITIONS(_store_med_uid),
                                                              STATE_TRANSITIONS(_complete),
                                                              STATE_TRANSITIONS(_error)};

static statemachine_transitions _transitions = STATEMACHINE_TRANSITIONS(_state_transitions);

result_t baselining_fsm_init(statemachine_t *const self, const system_time_interface_t *systick_ifc)
{
   RETURN_ERR_IF_NULL(self, BASELINING_FSM_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(systick_ifc, BASELINING_FSM_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   m_systick_ifc = systick_ifc;
   m_self = self;

   statemachine(self, BASELINING_STATE_COMPLETE, &_transitions);

   result = m_systick_ifc->get_time_ms(m_systick_ifc, &m_prev_state_change_time_ms);

   return result;
}

// Note the order of the state names must match the order in the enum defined in application state machine.
const char *baselining_fsm_state_names[] = {"WAIT_FOR_RING_REMOVAL",
                                            "SET_DOCK_WEIGHT",
                                            "WAIT_FOR_RING",
                                            "SET_RING_DOCK_WEIGHT",
                                            "WAIT_FOR_BACKEND_VALIDATION",
                                            "SET_LOCAL_MED_UID",
                                            "COMPLETE",
                                            "ERROR"};