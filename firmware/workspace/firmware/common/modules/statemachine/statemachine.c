/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file statemachine.c
 * @ingroup gc_module
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <string.h>
// Custom includes
#include "common.h"
#include "debug.h"
#include "statemachine.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_STATE_MACHINE;
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
static void s_update_statemachine(statemachine_t *const self, const void *const inputs, void *const outputs);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
void statemachine(statemachine_t *const self, int initial_state, statemachine_transitions *const transitions)
{
   RETURN_VOID_IF_NULL(self);
   RETURN_VOID_IF_NULL(transitions);

   memset(self, 0, sizeof(statemachine_t));
   self->current_state = initial_state;
   self->transitions = transitions;
   self->update_statemachine = s_update_statemachine;
}

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/
static void s_update_statemachine(statemachine_t *const self, const void *const inputs, void *const outputs)
{
   if((NULL == self) || (NULL == inputs) || (NULL == outputs))
   {
      DEBUG_ERROR("NULL pointer detected in state machine update function");
      return;
   }

   if(self->current_state >= self->transitions->states_count)
   {
      DEBUG_ERROR("Invalid input parameters");
      return;
   }

   const statemachine_state_transitions *state = &self->transitions->states[self->current_state];

   for(int i = 0; i < state->transition_count; ++i)
   {
      if(state->transitions[i].test_function(inputs))
      {
         state->transitions[i].result_function(outputs);
         self->current_state = state->transitions[i].destination_state;
         break;
      }
   }
}