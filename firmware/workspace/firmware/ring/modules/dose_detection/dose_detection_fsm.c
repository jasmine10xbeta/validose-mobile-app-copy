/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_detection_fsm.c
 * @ingroup modules/dose_detection
 * @brief Source file for the dose detection finite state machine module.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>

// Custom includes
#include "dose_detection_fsm.h"
#include "statemachine.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOSE_DETECTION_FSM;

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

// Note the order of the state names must match the order in the enum defined in application state machine.
const char *dose_detection_fsm_state_names[] = {"IDLE", "PRIMED", "EVALUATING", "DOSE_DETECTED"};

/***********************************************************************************************************************
 * Test and result functions for state transitions
 **********************************************************************************************************************/

/* State: IDLE */

static bool idle_to_primed_test(const void *const inputs)
{
   const dose_detection_fsm_inputs_t *input = (const dose_detection_fsm_inputs_t *)inputs;
   return (false == input->cap_on);
}

static void idle_to_primed_result(void *const outputs)
{
   dose_detection_fsm_outputs_t *output = (dose_detection_fsm_outputs_t *)outputs;
   output->state_changed = true;
}

/* State: PRIMED */

static bool primed_to_idle_test(const void *const inputs)
{
   const dose_detection_fsm_inputs_t *input = (const dose_detection_fsm_inputs_t *)inputs;
   return (true == input->cap_on);
}

static void primed_to_idle_result(void *const outputs)
{
   dose_detection_fsm_outputs_t *output = (dose_detection_fsm_outputs_t *)outputs;
   output->state_changed = true;
}

static bool primed_to_evaluating_test(const void *const inputs)
{
   const dose_detection_fsm_inputs_t *input = (const dose_detection_fsm_inputs_t *)inputs;
   return ((true == input->tilt_detected) && (false == input->cap_on));
}

static void primed_to_evaluating_result(void *const outputs)
{
   dose_detection_fsm_outputs_t *output = (dose_detection_fsm_outputs_t *)outputs;
   output->state_changed = true;
}

/* State: EVALUATING */

static bool evaluating_to_idle_test(const void *const inputs)
{
   const dose_detection_fsm_inputs_t *input = (const dose_detection_fsm_inputs_t *)inputs;

   // Note: uint64_t wrap-around is not handled here, as it is assumed that it will not occur during normal operation.
   // If current_time_ms < dose_start_time_ms, default to staying in EVALUATING state.
   bool valid_timestamps = (input->current_time_ms >= input->dose_start_time_ms);

   bool invalid_dose_duration = false;
   if(valid_timestamps)
   {
      invalid_dose_duration = ((input->current_time_ms - input->dose_start_time_ms) < input->valid_dose_threshold_ms);
   }

   // return ((true == input->cap_on) && (false == dose_duration_valid));
   return (valid_timestamps && input->cap_on && invalid_dose_duration);
}

static void evaluating_to_idle_result(void *const outputs)
{
   dose_detection_fsm_outputs_t *output = (dose_detection_fsm_outputs_t *)outputs;
   output->state_changed = true;
}

static bool evaluating_to_dose_detected_test(const void *const inputs)
{
   const dose_detection_fsm_inputs_t *input = (const dose_detection_fsm_inputs_t *)inputs;

   // Note: uint64_t wrap-around is not handled here, as it is assumed that it will not occur during normal operation.
   // If current_time_ms < dose_start_time_ms, default to staying in EVALUATING state.
   bool valid_timestamps = (input->current_time_ms >= input->dose_start_time_ms);

   bool valid_dose_duration = false;
   if(valid_timestamps)
   {
      valid_dose_duration = ((input->current_time_ms - input->dose_start_time_ms) >= input->valid_dose_threshold_ms);
   }

   return (valid_timestamps && input->cap_on && valid_dose_duration);
}

static void evaluating_to_dose_detected_result(void *const outputs)
{
   dose_detection_fsm_outputs_t *output = (dose_detection_fsm_outputs_t *)outputs;
   output->state_changed = true;
}

/* State: DOSE_DETECTED */

static bool dose_detected_to_idle_test(const void *const inputs)
{
   (void)inputs;

   return true;
}

static void dose_detected_to_idle_result(void *const outputs)
{
   dose_detection_fsm_outputs_t *output = (dose_detection_fsm_outputs_t *)outputs;
   output->state_changed = true;
}

/***********************************************************************************************************************
 * State machine transition table
 **********************************************************************************************************************/

static statemachine_transition_definition _idle_state_transitions[]
   = {STATE_TRANSITION_DEF(DOSE_DETECTION_STATE_PRIMED, idle_to_primed_test, idle_to_primed_result)};

static statemachine_transition_definition _primed_state_transitions[]
   = {STATE_TRANSITION_DEF(DOSE_DETECTION_STATE_IDLE, primed_to_idle_test, primed_to_idle_result),
      STATE_TRANSITION_DEF(DOSE_DETECTION_STATE_EVALUATING, primed_to_evaluating_test, primed_to_evaluating_result)};

static statemachine_transition_definition _evaluating_state_transitions[]
   = {STATE_TRANSITION_DEF(DOSE_DETECTION_STATE_IDLE, evaluating_to_idle_test, evaluating_to_idle_result),
      STATE_TRANSITION_DEF(
         DOSE_DETECTION_STATE_DOSE_DETECTED, evaluating_to_dose_detected_test, evaluating_to_dose_detected_result)};

static statemachine_transition_definition _dose_detected_state_transitions[]
   = {STATE_TRANSITION_DEF(DOSE_DETECTION_STATE_IDLE, dose_detected_to_idle_test, dose_detected_to_idle_result)};

static statemachine_state_transitions _state_transitions[] = {STATE_TRANSITIONS(_idle_state_transitions),
                                                              STATE_TRANSITIONS(_primed_state_transitions),
                                                              STATE_TRANSITIONS(_evaluating_state_transitions),
                                                              STATE_TRANSITIONS(_dose_detected_state_transitions)};

static statemachine_transitions _transitions = STATEMACHINE_TRANSITIONS(_state_transitions);

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t dose_detection_fsm_init(statemachine_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, DOSE_DETECTION_FSM_ERROR_NULL);

   statemachine(p_self, DOSE_DETECTION_STATE_IDLE, &_transitions);

   return RESULT_OK;
}
