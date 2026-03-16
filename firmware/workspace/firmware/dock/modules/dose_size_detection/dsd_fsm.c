/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
// Custom includes
#include "dsd_fsm.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOSE_SIZE_DETECTION_FSM;

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
// Happy path transitions
/**
 * @brief Test for transition from Ring Present to Ring Absent state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is not present and was previously present.
 */
bool ring_now_absent_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (false == input->is_ring_present);
}

/**
 * @brief Result action for transition from Ring Present to Ring Absent state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Ring Present to Ring Absent state.
 * When the Ring becomes absent, the sampling frequency is set to FAST, the wait time is set to STABLE_WAIT_TIME_S,
 * and the transition is marked as valid.
 */
void ring_now_absent_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_FAST;
   output->wait_time = STABLE_WAIT_TIME_S;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for transition from Ring Absent to Ring Replaced (ring present, again) state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is present and was not previously present.
 */
bool ring_replaced_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (input->is_ring_present);
}

/**
 * @brief Result action for transition from Ring Absent to Ring Replaced (ring present, again) state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Ring Absent to Ring Replaced state.
 * When the Ring becomes present, the sampling frequency is set to FAST, the wait time is set to T_SETTLE_RING_S,
 * and the transition is marked as valid.
 */
void ring_replaced_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_FAST;
   output->wait_time = T_SETTLE_RING_S;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for transition from Ring Replaced to Ring Replaced Settled state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is present, was previously present, and the maximum time (for the system to settle
 * after ring replacement) has elapsed, or if best-sample sigma is below the early-settle threshold.
 */
bool ring_settled_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return ((input->is_ring_present) && (input->is_max_time_elapsed || input->is_sigma_below_best_threshold));
}

/**
 * @brief Result action for transition from Ring Replaced to Ring Replaced Settled state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Ring Replaced to Ring Replaced Settled state.
 * When the Ring has settled after replacement, the sampling frequency is set to SLOW, the wait time is set to 0,
 * and the transition is marked as valid.
 */
void ring_settled_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_SLOW;
   output->wait_time = 0u;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for transition from Ring Replaced Settled to Ring Present state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is present, was previously present, and the dose size has been computed.
 */
bool restart_sequence_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (input->is_ring_present && input->is_dose_size_computed);
}

/**
 * @brief Result action for transition from Ring Replaced Settled to Ring Present state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Ring Replaced Settled to Ring Present state.
 * After dose size computation, the sampling frequency is set to SLOW, the wait time is set to STABLE_WAIT_TIME_S,
 * and the transition is marked as valid.
 */
void restart_sequence_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_SLOW;
   output->wait_time = STABLE_WAIT_TIME_S;
   output->is_sequence_interrupted = false;
   output->is_dose_size_computed = false; // Clear dose size computed input to restart sequence
}

/**
 * @brief Test for transition from Ring Present to Idle state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is present and the maximum time (for obtaining a stable weight measurement) has
 * elapsed.
 * This indicates that the system can transition from Ring Present to Idle state to conserve power.
 */
bool present_to_idle_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (input->is_ring_present && (input->is_max_time_elapsed));
}

/**
 * @brief Result action for transition from Ring Present to Idle state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Ring Present to Idle state.
 * When transitioning to Idle, the sampling frequency is set to IDLE, the wait time is set to 0,
 * and the transition is marked as valid.
 */
void present_to_idle_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_IDLE;
   output->wait_time = 0u;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for transition from Idle to Ring Present state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is present and if at least one valid weight sample has been obtained.
 * This indicates that the system can transition from Idle to Ring Present state and may continue with dose size
 * detection.
 */
bool idle_to_present_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (input->is_ring_present && (input->got_valid_weight_sample));
}

/**
 * @brief Result action for transition from Idle to Ring Present state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Idle to Ring Present state.
 * When transitioning to Ring Present, the sampling frequency is set to SLOW, the wait time is set to
 * STABLE_WAIT_TIME_S, and the transition is marked as valid.
 */
void idle_to_present_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_SLOW;
   output->wait_time = STABLE_WAIT_TIME_S;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for transition from Ring Absent to Idle state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is not present and if the maximum time (for obtaining a stable weight measurement)
 * has elapsed. This indicates that the system can transition from Ring Absent to Idle state to conserve power.
 */
bool absent_to_idle_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (!(input->is_ring_present) && (input->is_max_time_elapsed));
}

/**
 * @brief Result action for transition from Ring Absent to Idle state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Ring Absent to Idle state.
 * When transitioning to Idle, the sampling frequency is set to IDLE, the wait time is set to 0,
 * and the transition is marked as valid.
 */
void absent_to_idle_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_IDLE;
   output->wait_time = 0u;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for transition from Idle to Ring Absent state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is not present and if at least one valid weight sample has been obtained.
 * This indicates that the system can transition from Idle to Ring Absent state and may continue with dose size
 * detection.
 */
bool idle_to_absent_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (!(input->is_ring_present) && (input->got_valid_weight_sample));
}

/**
 * @brief Result action for transition from Idle to Ring Absent state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the transition from Idle to Ring Absent state.
 * When transitioning to Ring Absent, the sampling frequency is set to FAST, the wait time is set to STABLE_WAIT_TIME_S,
 * and the transition is marked as valid.
 */
void idle_to_absent_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->sampling_frequency = DSD_FREQ_FAST;
   output->wait_time = STABLE_WAIT_TIME_S;
   output->is_sequence_interrupted = false;
}

/**
 * @brief Test for invalid transition from Ring Replaced to Ring Absent state.
 * Test for invalid transition from Ring Replaced Settled to Ring Absent state.
 * @param inputs Pointer to the FSM inputs structure.
 * @return true if the transition condition is met, false otherwise.
 *
 * This function checks if the ring is not present and was previously present.
 * If the ring is removed while the system is in the ring replaced/ ring replaced settled state, this is considered an
 * invalid transition. The dose size calculation should not proceed as the ring has been removed unexpectedly and the
 * data for the dose size calculation is no longer valid.
 */
bool replaced_to_absent_test(const void *const inputs)
{
   const dsd_fsm_inputs_t *input = (const dsd_fsm_inputs_t *)inputs;
   return (false == input->is_ring_present);
}

/**
 * @brief Result action for invalid transition from Ring Replaced to Ring Absent state.
 * Result action for invalid transition from Ring Replaced Settled to Ring Absent state.
 * @param outputs Pointer to the FSM outputs structure.
 *
 * This function sets the FSM outputs for the invalid transition from Ring Replaced/ Ring Replaced Settled to Ring
 * Absent state. When the Ring is removed unexpectedly, the transition is marked as invalid and can be handled
 * appropriately by the system.
 */
void replaced_to_absent_result(void *const outputs)
{
   dsd_fsm_outputs_t *output = (dsd_fsm_outputs_t *)outputs;
   output->changed = true;
   output->is_sequence_interrupted = true;
}

////////////////////////////////////////////////////////////////////////////////
// @note The order of the state transitions is important and should match the order of the enum defined in
// app_statemachine.h
static statemachine_transition_definition _ring_present_state_transitions[]
   = {STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_ABSENT, ring_now_absent_test, ring_now_absent_result),
      STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_IDLE, present_to_idle_test, present_to_idle_result)};

static statemachine_transition_definition _ring_absent_state_transitions[]
   = {STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_REPLACED, ring_replaced_test, ring_replaced_result),
      STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_IDLE, absent_to_idle_test, absent_to_idle_result)};

static statemachine_transition_definition _ring_replaced_state_transitions[]
   = {STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED, ring_settled_test, ring_settled_result),
      STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_ABSENT, replaced_to_absent_test, replaced_to_absent_result)};

static statemachine_transition_definition _ring_replaced_settled_state_transitions[]
   = {STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_PRESENT, restart_sequence_test, restart_sequence_result),
      STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_ABSENT, replaced_to_absent_test, replaced_to_absent_result)};

static statemachine_transition_definition _idle_state_transitions[]
   = {STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_PRESENT, idle_to_present_test, idle_to_present_result),
      STATE_TRANSITION_DEF(DSD_STATEMACHINE_STATE_RING_ABSENT, idle_to_absent_test, idle_to_absent_result)};

// @note The order of the state transitions is important should and match the order of the enum defined in
// app_statemachine.h
static statemachine_state_transitions _dsd_state_transitions[]
   = {STATE_TRANSITIONS(_ring_present_state_transitions),
      STATE_TRANSITIONS(_ring_absent_state_transitions),
      STATE_TRANSITIONS(_ring_replaced_state_transitions),
      STATE_TRANSITIONS(_ring_replaced_settled_state_transitions),
      STATE_TRANSITIONS(_idle_state_transitions)};

statemachine_transitions _dsd_transitions = STATEMACHINE_TRANSITIONS(_dsd_state_transitions);

result_t dsd_fsm_init(statemachine_t *const self)
{
   RETURN_ERR_IF_NULL(self, DSD_FSM_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   statemachine(self, DSD_STATEMACHINE_STATE_RING_PRESENT, &_dsd_transitions);
   return result;
}

// Note the order of the state names must match the order in the enum defined in application state machine.
const char *dsd_fsm_state_names[] = {"RING_PRESENT", "RING_ABSENT", "RING_REPLACED", "RING_REPLACED_SETTLED", "IDLE"};
