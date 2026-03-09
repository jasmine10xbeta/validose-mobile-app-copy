/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file calibration_fsm.c
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
#include "calibration_fsm.h"
#include "statemachine.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_CALIBRATION_FSM;

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
static uint32_t m_state_timeouts[CALIBRATION_STATE_CALIBRATING + 1] = {WAIT_FOR_RING_REMOVAL_STATE_TIMEOUT_MS,
                                                                       SET_DOCK_WEIGHT_STATE_TIMEOUT_MS,
                                                                       WAIT_FOR_CALIBRATION_WEIGHT_STATE_TIMEOUT_MS,
                                                                       CALIBRATING_TIMEOUT_MS};

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

static bool is_ring_removed_test(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
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

static bool is_empty_dock_weight_set(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return input->is_empty_dock_weight_set;
}

static bool is_calibration_weight_present(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return input->is_calibration_weight_present;
}

static bool is_calibration_weight_set(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return input->is_calibration_weight_set;
}

static bool is_data_storage_successful(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return (DATA_STORAGE_SUCCESS == input->data_update_status);
}

static bool is_data_storage_unsuccessful(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return (DATA_STORAGE_FAILED == input->data_update_status);
}

static bool is_calibration_starting_test(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return input->start_calibration;
}

static bool is_calibration_canceled_test(const void *const inputs)
{
   const calibration_fsm_inputs_t *input = (const calibration_fsm_inputs_t *)inputs;
   return !input->is_calibration_active;
}

/**
 * A generic result is used for every test function since the output change is always the same.
 */
static void generic_result(void *const outputs)
{
   calibration_fsm_outputs_t *output = (calibration_fsm_outputs_t *)outputs;
   output->changed = true;

   // Update prev state change time
   (void)m_systick_ifc->get_time_ms(m_systick_ifc, &m_prev_state_change_time_ms);
}
////////////////////////////////////////////////////////////////////////////////
// @note The order of the state transitions is important and should match the order of the enum defined in
// app_statemachine.h
static statemachine_transition_definition _wait_for_ring_removal[]
   = {STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_calibration_canceled_test, generic_result),
      STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(CALIBRATION_STATE_SET_DOCK_WEIGHT, is_ring_removed_test, generic_result)};

static statemachine_transition_definition _set_dock_weight[]
   = {STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_calibration_canceled_test, generic_result),
      STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_timeout_test, generic_result),
      STATE_TRANSITION_DEF(CALIBRATION_STATE_WAIT_FOR_CALIBRATION_WEIGHT, is_empty_dock_weight_set, generic_result)};

static statemachine_transition_definition _wait_for_calibration_weight[] = {
   STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_calibration_canceled_test, generic_result),
   STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_timeout_test, generic_result),
   STATE_TRANSITION_DEF(CALIBRATION_STATE_CALIBRATING, is_calibration_weight_present, generic_result),
};

static statemachine_transition_definition _calibrating[] = {
   STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_calibration_canceled_test, generic_result),
   STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_timeout_test, generic_result),
   STATE_TRANSITION_DEF(CALIBRATION_STATE_STORE_UPDATED_PARAM, is_calibration_weight_set, generic_result),
};

static statemachine_transition_definition _store_updated_param[]
   = {STATE_TRANSITION_DEF(CALIBRATION_STATE_COMPLETE, is_data_storage_successful, generic_result),
      STATE_TRANSITION_DEF(CALIBRATION_STATE_ERROR, is_data_storage_unsuccessful, generic_result)};

static statemachine_transition_definition _complete[]
   = {STATE_TRANSITION_DEF(CALIBRATION_STATE_WAIT_FOR_RING_REMOVAL, is_calibration_starting_test, generic_result)};

static statemachine_transition_definition _error[]
   = {STATE_TRANSITION_DEF(CALIBRATION_STATE_WAIT_FOR_RING_REMOVAL, is_calibration_starting_test, generic_result)};

// @note The order of the state transitions is important should and match the order of the enum defined in
// app_statemachine.h
static statemachine_state_transitions _state_transitions[] = {STATE_TRANSITIONS(_wait_for_ring_removal),
                                                              STATE_TRANSITIONS(_set_dock_weight),
                                                              STATE_TRANSITIONS(_wait_for_calibration_weight),
                                                              STATE_TRANSITIONS(_calibrating),
                                                              STATE_TRANSITIONS(_store_updated_param),
                                                              STATE_TRANSITIONS(_complete),
                                                              STATE_TRANSITIONS(_error)};

static statemachine_transitions _transitions = STATEMACHINE_TRANSITIONS(_state_transitions);

result_t calibration_fsm_init(statemachine_t *const self, const system_time_interface_t *systick_ifc)
{
   RETURN_ERR_IF_NULL(self, CALIBRATION_FSM_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(systick_ifc, CALIBRATION_FSM_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   m_systick_ifc = systick_ifc;
   m_self = self;

   statemachine(self, CALIBRATION_STATE_COMPLETE, &_transitions);

   result = m_systick_ifc->get_time_ms(m_systick_ifc, &m_prev_state_change_time_ms);

   return result;
}

// Note the order of the state names must match the order in the enum defined in application state machine.
const char *calibration_fsm_state_names[] = {"WAIT_FOR_RING_REMOVAL",
                                             "SET_DOCK_WEIGHT",
                                             "WAIT_FOR_CALIBRATION_WEIGHT",
                                             "CALIBRATING",
                                             "STORE_UPDATED_PARAM",
                                             "COMPLETE",
                                             "ERROR"};
