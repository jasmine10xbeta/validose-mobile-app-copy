/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file statemachine.h
 * @ingroup gc_module
 * @brief Generic state machine implementation.
 */

#ifndef STATEMACHINE_H_
#define STATEMACHINE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdbool.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @def STATE_TRANSITION_DEF
 * Convenience macro to initialize a statemachine_transition_definition inline.
 *
 * @param _destinationState Destination state of the transition.
 * @param _testFunc Function to test if the transition criteria are met.
 * @param _resultFunc Function called when this transition is performed.
 */
#define STATE_TRANSITION_DEF(_destinationState, _testFunc, _resultFunc)                                                \
   {                                                                                                                   \
      .destination_state = (_destinationState), .test_function = (_testFunc), .result_function = (_resultFunc)         \
   }

/**
 * @def
 * Convenience macro to initialize a StatemachineStateTransitions instance inline
 *
 * @param _transitions Array of transitions of type statemachine_transition_definition.
 */
#define STATE_TRANSITIONS(_transitions)                                                                                \
   {                                                                                                                   \
      .transitions = (_transitions), .transition_count = sizeof(_transitions) / sizeof(_transitions[0])                \
   }

/**
 * @def STATEMACHINE_TRANSITIONS
 * Convenience macro to initialize a StatemachineTransitions instance inline.
 *
 * @param _states Array of StatemachineStateTransitions.
 */
#define STATEMACHINE_TRANSITIONS(_states)                                                                              \
   {                                                                                                                   \
      .states = (_states), .states_count = sizeof(_states) / sizeof(_states[0])                                        \
   }
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct statemachine statemachine_t;

typedef bool (*state_transition_test_function)(const void *const inputs);
typedef void (*state_transition_result_function)(void *const outputs);

typedef struct
{
   int destination_state;                            /**<  Destination state if this transition. */
   state_transition_test_function test_function;     /**<  Function to test if the transition criteria are met. */
   state_transition_result_function result_function; /**<  Function called when this transition is performed. */
} statemachine_transition_definition;

typedef struct
{
   statemachine_transition_definition *transitions; /**<  Array of transition definitions. */
   int transition_count;                            /**<  Number of transition definitions in \p transitions array. */
} statemachine_state_transitions;

typedef struct
{
   statemachine_state_transitions *states; /**<  Array of state transitions. */
   int states_count;                       /**<  Number of \p state transitions in \p states array. */
} statemachine_transitions;

struct statemachine
{
   int current_state;                     /**<  Current statemachine state. */
   statemachine_transitions *transitions; /**<  Pointer to transition table. */

   /**
    * @brief Update the statemachine, testing for transitions and calling the corresponding result function.
    *
    * @param self Statemachine instance.
    * @param inputs Pointer to input structure.
    * @param outputs Pointer to output structure.
    */
   void (*update_statemachine)(statemachine_t *const self, const void *const inputs, void *const outputs);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Constructor/Initializer for \p statemachine_t instance
 *
 * @param self statemachine_t instance.
 * @param initial_state Initial state of the \p statemachine_t instance.
 * @param transitions Reference to the transition table to use.
 */
void statemachine(statemachine_t *const self, int initial_state, statemachine_transitions *const transitions);

#endif /* STATE_MACHINE_H_ */
