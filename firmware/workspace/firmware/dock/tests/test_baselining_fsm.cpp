/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file test_baselining_fsm.cpp
 * @brief Unit tests for the baselining state machine.
 */

#include <gtest/gtest.h>

extern "C"
{
#include "baselining_fsm.h"
#include "common.h"
#include "statemachine.h"
#include "system_time.h"
#include <stddef.h>
#include <string.h>
}

/***********************************************************************************************************************
 * Test utilities
 **********************************************************************************************************************/
static baselining_fsm_inputs_t make_inputs(bool start_baselining,
                                           bool is_baselining_active,
                                           bool is_ring_present,
                                           bool is_medication_uid_avail,
                                           bool is_empty_dock_weight_set,
                                           bool is_ring_dock_full_med_weight_set,
                                           BACKEND_VAL_STATUS backend_status,
                                           DOSE_SCH_STATUS dose_status,
                                           MED_UID_STORAGE med_update_status)
{
   // Helper to build a fully-populated input struct for the FSM.
   // This keeps the test setup readable and ensures all fields are explicitly set.
   baselining_fsm_inputs_t inputs = {0};
   inputs.start_baselining = start_baselining;
   inputs.is_baselining_active = is_baselining_active;
   inputs.is_ring_present = is_ring_present;
   inputs.is_medication_uid_avail = is_medication_uid_avail;
   inputs.is_empty_dock_weight_set = is_empty_dock_weight_set;
   inputs.is_ring_dock_full_med_weight_set = is_ring_dock_full_med_weight_set;
   inputs.backend_validation_status = backend_status;
   inputs.dose_schedule_status = dose_status;
   inputs.med_update_status = med_update_status;
   return inputs;
}

static BASELINING_STATE expected_next_state(BASELINING_STATE current_state,
                                            const baselining_fsm_inputs_t *inputs,
                                            bool timeout_triggered)
{
   // Pure reference model of the transition table in baselining_fsm.c.
   // The tests use this to compute the expected state for any input combination.
   // If this model drifts from production code, the tests will flag it.
   switch(current_state)
   {
      case BASELINING_STATE_WAIT_FOR_RING_REMOVAL:
         if(false == inputs->is_baselining_active)
         {
            return BASELINING_STATE_ERROR;
         }
         if(true == timeout_triggered)
         {
            return BASELINING_STATE_ERROR;
         }
         if(false == inputs->is_ring_present)
         {
            return BASELINING_STATE_SET_DOCK_WEIGHT;
         }
         return BASELINING_STATE_WAIT_FOR_RING_REMOVAL;

      case BASELINING_STATE_SET_DOCK_WEIGHT:
         if(false == inputs->is_baselining_active)
         {
            return BASELINING_STATE_ERROR;
         }
         if(true == timeout_triggered)
         {
            return BASELINING_STATE_ERROR;
         }
         if(true == inputs->is_ring_present)
         {
            return BASELINING_STATE_WAIT_FOR_RING_REMOVAL;
         }
         if(true == inputs->is_empty_dock_weight_set)
         {
            return BASELINING_STATE_WAIT_FOR_RING;
         }
         return BASELINING_STATE_SET_DOCK_WEIGHT;

      case BASELINING_STATE_WAIT_FOR_RING:
         if(false == inputs->is_baselining_active)
         {
            return BASELINING_STATE_ERROR;
         }
         if(true == timeout_triggered)
         {
            return BASELINING_STATE_ERROR;
         }
         if((true == inputs->is_ring_present) && (true == inputs->is_medication_uid_avail))
         {
            return BASELINING_STATE_SET_RING_DOCK_WEIGHT;
         }
         return BASELINING_STATE_WAIT_FOR_RING;

      case BASELINING_STATE_SET_RING_DOCK_WEIGHT:
         if(false == inputs->is_baselining_active)
         {
            return BASELINING_STATE_ERROR;
         }
         if(true == timeout_triggered)
         {
            return BASELINING_STATE_ERROR;
         }
         if(false == inputs->is_ring_present)
         {
            return BASELINING_STATE_WAIT_FOR_RING;
         }
         if(true == inputs->is_ring_dock_full_med_weight_set)
         {
            return BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION;
         }
         return BASELINING_STATE_SET_RING_DOCK_WEIGHT;

      case BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION:
         if(BACKEND_VAL_STATUS_UNSUCCESSFUL == inputs->backend_validation_status)
         {
            return BASELINING_STATE_ERROR;
         }
        if(DOSE_SCH_STATUS_INVALID == inputs->dose_schedule_status)
        {
           return BASELINING_STATE_ERROR;
        }
        if(false == inputs->is_baselining_active)
        {
           return BASELINING_STATE_ERROR;
        }
        if(true == timeout_triggered)
        {
           return BASELINING_STATE_ERROR;
        }
         if((BACKEND_VAL_STATUS_SUCCESS == inputs->backend_validation_status)
            && (DOSE_SCH_STATUS_VALID == inputs->dose_schedule_status))
         {
            return BASELINING_STATE_SET_LOCAL_MED_UID;
         }
         return BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION;

      case BASELINING_STATE_SET_LOCAL_MED_UID:
         if(MED_UID_STORAGE_SUCCESS == inputs->med_update_status)
         {
            return BASELINING_STATE_COMPLETE;
         }
         if(MED_UID_STORAGE_FAILED == inputs->med_update_status)
         {
            return BASELINING_STATE_ERROR;
         }
         return BASELINING_STATE_SET_LOCAL_MED_UID;

      case BASELINING_STATE_COMPLETE:
         if(true == inputs->start_baselining)
         {
            return BASELINING_STATE_WAIT_FOR_RING_REMOVAL;
         }
         return BASELINING_STATE_COMPLETE;

      case BASELINING_STATE_ERROR:
         if(true == inputs->start_baselining)
         {
            return BASELINING_STATE_WAIT_FOR_RING_REMOVAL;
         }
         return BASELINING_STATE_ERROR;

      default:
         return current_state;
   }
}

static baselining_fsm_inputs_t timeout_only_inputs(BASELINING_STATE state)
{
   // Build inputs that intentionally *avoid* non-timeout transitions.
   // This ensures any transition is driven strictly by the timeout condition.
   baselining_fsm_inputs_t inputs = {0};
   inputs.start_baselining = false;
   inputs.is_baselining_active = true;
   inputs.is_ring_present = true;
   inputs.med_update_status = MED_UID_STORAGE_NONE;
   inputs.is_empty_dock_weight_set = false;
   inputs.is_ring_dock_full_med_weight_set = false;
   inputs.backend_validation_status = BACKEND_VAL_STATUS_NONE;
   inputs.dose_schedule_status = DOSE_SCH_STATUS_NONE;

   if(BASELINING_STATE_WAIT_FOR_RING == state)
   {
      inputs.is_ring_present = false;
   }

   if(BASELINING_STATE_WAIT_FOR_RING_REMOVAL == state)
   {
      inputs.is_ring_present = true;
   }

   if(BASELINING_STATE_SET_DOCK_WEIGHT == state)
   {
      inputs.is_ring_present = false;
      inputs.is_empty_dock_weight_set = false;
   }

   if(BASELINING_STATE_SET_RING_DOCK_WEIGHT == state)
   {
      inputs.is_ring_present = true;
      inputs.is_ring_dock_full_med_weight_set = false;
   }

   return inputs;
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
class BaseliningFsmTest: public ::testing::Test
{
protected:
   void SetUp() override
   {
      // Zero init for deterministic starting state.
      memset(&m_fsm, 0, sizeof(m_fsm));
      memset(&m_time, 0, sizeof(m_time));
      memset(&m_outputs, 0, sizeof(m_outputs));

      // Initialize the system time module used by the FSM timeout logic.
      result_t result = system_time_init(&m_time);
      ASSERT_TRUE(IS_OK(result));

      // Initialize the FSM under test with the time interface.
      result = baselining_fsm_init(&m_fsm, &m_time.interface);
      ASSERT_TRUE(IS_OK(result));
   }

   void ResetBaselineToTime(uint64_t time_ms)
   {
      // Force system time to a known value so timeout calculations are deterministic.
      result_t result = m_time.interface.set_time_ms(&m_time.interface, time_ms);
      ASSERT_TRUE(IS_OK(result));

      // Drive the FSM from COMPLETE to WAIT_FOR_RING_REMOVAL via start_baselining.
      // This matches expected behavior when a new baselining cycle is requested.
      baselining_fsm_inputs_t inputs = {0};
      inputs.start_baselining = true;

      m_fsm.current_state = BASELINING_STATE_COMPLETE;
      m_outputs.changed = false;
      m_fsm.update_statemachine(&m_fsm, &inputs, &m_outputs);
      ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_WAIT_FOR_RING_REMOVAL);
   }

   void UpdateWithInputs(const baselining_fsm_inputs_t *inputs)
   {
      // Run a single FSM update step with the provided inputs.
      m_outputs.changed = false;
      m_fsm.update_statemachine(&m_fsm, inputs, &m_outputs);
   }

   system_time_t m_time = {0};
   statemachine_t m_fsm = {0};
   baselining_fsm_outputs_t m_outputs = {0};
};

TEST_F(BaseliningFsmTest, happy_path_reaches_complete)
{
   // This test exercises the intended success path through the FSM.
   // Expected: each valid input advances the FSM to the next "happy path" state,
   // ending in COMPLETE.
   ResetBaselineToTime(0u);

   baselining_fsm_inputs_t inputs = {0};
   inputs.is_baselining_active = true;

   // WAIT_FOR_RING_REMOVAL -> SET_DOCK_WEIGHT
   // Condition: ring removed (is_ring_present == false).
   inputs.is_ring_present = false;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_SET_DOCK_WEIGHT);
   ASSERT_TRUE(m_outputs.changed);

   // SET_DOCK_WEIGHT -> WAIT_FOR_RING
   // Condition: dock weight becomes stable.
   inputs.is_empty_dock_weight_set = true;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_WAIT_FOR_RING);

   // WAIT_FOR_RING -> SET_RING_DOCK_WEIGHT
   // Condition: ring present and medication UID available.
   inputs.is_ring_present = true;
   inputs.is_medication_uid_avail = true;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_SET_RING_DOCK_WEIGHT);

   // SET_RING_DOCK_WEIGHT -> WAIT_FOR_BACKEND_VALIDATION
   // Condition: ring dock weight becomes stable.
   inputs.is_ring_dock_full_med_weight_set = true;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION);

   // WAIT_FOR_BACKEND_VALIDATION -> SET_LOCAL_MED_UID
   // Condition: backend validation succeeds and dose schedule is valid.
   inputs.backend_validation_status = BACKEND_VAL_STATUS_SUCCESS;
   inputs.dose_schedule_status = DOSE_SCH_STATUS_VALID;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_SET_LOCAL_MED_UID);

   // SET_LOCAL_MED_UID -> COMPLETE
   // Condition: approved medication UID is stored successfully.
   inputs.med_update_status = MED_UID_STORAGE_SUCCESS;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_COMPLETE);
}

TEST_F(BaseliningFsmTest, timeout_resets_on_transition)
{
   // This test validates that the timeout baseline is updated on a state transition.
   // Expected: timeout should be measured from the last transition time, not from 0.
   const uint64_t start_time_ms = 1234u;
   ResetBaselineToTime(start_time_ms);

   baselining_fsm_inputs_t inputs = {0};
   inputs.is_baselining_active = true;

   // WAIT_FOR_RING_REMOVAL -> SET_DOCK_WEIGHT at start_time_ms.
   inputs.is_ring_present = false;
   UpdateWithInputs(&inputs);
   ASSERT_EQ(m_fsm.current_state, BASELINING_STATE_SET_DOCK_WEIGHT);

   // Advance time to just before the SET_DOCK_WEIGHT timeout threshold.
   const uint64_t timeout_ms = (uint64_t)SET_DOCK_WEIGHT_STATE_TIMEOUT_MS;
   result_t result = m_time.interface.set_time_ms(&m_time.interface, start_time_ms + timeout_ms - 1u);
   ASSERT_TRUE(IS_OK(result));

   inputs.is_empty_dock_weight_set = false;
   UpdateWithInputs(&inputs);
   EXPECT_EQ(m_fsm.current_state, BASELINING_STATE_SET_DOCK_WEIGHT);

   // Now cross the timeout threshold exactly.
   // Expected: the FSM should transition to ERROR on this update.
   result = m_time.interface.set_time_ms(&m_time.interface, start_time_ms + timeout_ms);
   ASSERT_TRUE(IS_OK(result));

   UpdateWithInputs(&inputs);
   EXPECT_EQ(m_fsm.current_state, BASELINING_STATE_ERROR);
}

TEST_F(BaseliningFsmTest, timeout_triggers_error_at_threshold)
{
   // This test checks each timeout-bearing state at the exact timeout boundary.
   // Expected: no transition just before the threshold, ERROR exactly at the threshold.
   struct timeout_case_t
   {
      BASELINING_STATE state;
      uint64_t timeout_ms;
   };

   const timeout_case_t cases[] = {
      {BASELINING_STATE_WAIT_FOR_RING_REMOVAL, (uint64_t)WAIT_FOR_RING_REMOVAL_STATE_TIMEOUT_MS},
      {BASELINING_STATE_SET_DOCK_WEIGHT, (uint64_t)SET_DOCK_WEIGHT_STATE_TIMEOUT_MS},
      {BASELINING_STATE_WAIT_FOR_RING, (uint64_t)WAIT_FOR_RING_STATE_TIMEOUT_MS},
      {BASELINING_STATE_SET_RING_DOCK_WEIGHT, (uint64_t)SET_RING_DOCK_WEIGHT_STATE_TIMEOUT_MS},
      {BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION, (uint64_t)WAIT_FOR_BACKEND_VALIDATION_STATE_TIMEOUT_MS},
   };

   for(size_t i = 0u; i < (sizeof(cases) / sizeof(cases[0])); ++i)
   {
      // Each loop iteration validates one state independently.
      SCOPED_TRACE(::testing::Message() << "state=" << cases[i].state);
      ResetBaselineToTime(0u);

      baselining_fsm_inputs_t inputs = timeout_only_inputs(cases[i].state);
      m_fsm.current_state = cases[i].state;

      // Just before timeout: expect no transition.
      result_t result = m_time.interface.set_time_ms(&m_time.interface, cases[i].timeout_ms - 1u);
      ASSERT_TRUE(IS_OK(result));

      UpdateWithInputs(&inputs);
      EXPECT_EQ(m_fsm.current_state, cases[i].state);
      EXPECT_FALSE(m_outputs.changed);

      // At timeout: expect transition to ERROR.
      result = m_time.interface.set_time_ms(&m_time.interface, cases[i].timeout_ms);
      ASSERT_TRUE(IS_OK(result));

      UpdateWithInputs(&inputs);
      EXPECT_EQ(m_fsm.current_state, BASELINING_STATE_ERROR);
      EXPECT_TRUE(m_outputs.changed);
   }
}

TEST_F(BaseliningFsmTest, all_input_combinations_without_timeout)
{
   // Exhaustive input-space test (within reasonable bounds).
   // Expected: the FSM transitions exactly as defined in expected_next_state()
   // when no timeouts are asserted.
   const BASELINING_STATE states[] = {BASELINING_STATE_WAIT_FOR_RING_REMOVAL,
                                      BASELINING_STATE_SET_DOCK_WEIGHT,
                                      BASELINING_STATE_WAIT_FOR_RING,
                                      BASELINING_STATE_SET_RING_DOCK_WEIGHT,
                                      BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION,
                                      BASELINING_STATE_SET_LOCAL_MED_UID,
                                      BASELINING_STATE_COMPLETE,
                                      BASELINING_STATE_ERROR};

   const BACKEND_VAL_STATUS backend_statuses[] = {BACKEND_VAL_STATUS_NONE,
                                                  BACKEND_VAL_STATUS_SUCCESS,
                                                  BACKEND_VAL_STATUS_UNSUCCESSFUL};

   const DOSE_SCH_STATUS dose_statuses[] = {DOSE_SCH_STATUS_NONE, DOSE_SCH_STATUS_VALID, DOSE_SCH_STATUS_INVALID};
   const MED_UID_STORAGE med_uid_statuses[]
      = {MED_UID_STORAGE_NONE, MED_UID_STORAGE_SUCCESS, MED_UID_STORAGE_FAILED};

   // Lock time at 0 to ensure timeout never triggers.
   ResetBaselineToTime(0u);
   ASSERT_TRUE(IS_OK(m_time.interface.set_time_ms(&m_time.interface, 0u)));

   // Iterate all states and all combinations of boolean inputs plus backend/dose status.
   for(size_t s = 0u; s < (sizeof(states) / sizeof(states[0])); ++s)
   {
      for(size_t b = 0u; b < (sizeof(backend_statuses) / sizeof(backend_statuses[0])); ++b)
      {
         for(size_t d = 0u; d < (sizeof(dose_statuses) / sizeof(dose_statuses[0])); ++d)
         {
            for(size_t m = 0u; m < (sizeof(med_uid_statuses) / sizeof(med_uid_statuses[0])); ++m)
            {
               for(uint8_t start = 0u; start < 2u; ++start)
               {
                  for(uint8_t active = 0u; active < 2u; ++active)
                  {
                     for(uint8_t ring_present = 0u; ring_present < 2u; ++ring_present)
                     {
                        for(uint8_t med_uid_avail = 0u; med_uid_avail < 2u; ++med_uid_avail)
                        {
                           for(uint8_t empty_dock_weight_set = 0u; empty_dock_weight_set < 2u; ++empty_dock_weight_set)
                           {
                              for(uint8_t ring_dock_full_med_weight_set = 0u; ring_dock_full_med_weight_set < 2u;
                                  ++ring_dock_full_med_weight_set)
                              {
                                 baselining_fsm_inputs_t inputs = make_inputs((0u != start),
                                                                              (0u != active),
                                                                              (0u != ring_present),
                                                                              (0u != med_uid_avail),
                                                                              (0u != empty_dock_weight_set),
                                                                              (0u != ring_dock_full_med_weight_set),
                                                                              backend_statuses[b],
                                                                              dose_statuses[d],
                                                                              med_uid_statuses[m]);

                                 // Force FSM into a known state for this test vector.
                                 m_fsm.current_state = states[s];
                                 UpdateWithInputs(&inputs);

                                 BASELINING_STATE expected
                                    = expected_next_state(states[s], &inputs, false /* timeout */);

                                 // Provide context on failures for fast diagnosis.
                                 SCOPED_TRACE(::testing::Message() << "state=" << states[s]
                                                                  << " start=" << (0u != start)
                                                                  << " active=" << (0u != active)
                                                                  << " ring=" << (0u != ring_present)
                                                                  << " med_uid_avail=" << (0u != med_uid_avail)
                                                                  << " empty_set=" << (0u != empty_dock_weight_set)
                                                                  << " ring_med_set=" << (0u != ring_dock_full_med_weight_set)
                                                                  << " backend=" << backend_statuses[b]
                                                                  << " dose=" << dose_statuses[d]
                                                                  << " med_uid_store=" << med_uid_statuses[m]);

                                 EXPECT_EQ(m_fsm.current_state, expected);
                                 EXPECT_EQ(m_outputs.changed, (expected != states[s]));
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }
}
