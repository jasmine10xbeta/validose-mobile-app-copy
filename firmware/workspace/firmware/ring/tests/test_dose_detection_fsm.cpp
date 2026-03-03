#include <cstdio>
#include <gtest/gtest.h>

extern "C"
{
#include "common.h"
#include "dose_detection_fsm.h"
#include "statemachine.h"

#include <stdint.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Requirement condition for boolean inputs of the statemachine.
 */
typedef enum
{
   IN_REQ_BOOL_FALSE = 0,
   IN_REQ_BOOL_TRUE,
   IN_REQ_BOOL_DONT_CARE
} INPUT_REQUIREMENT_BOOL;

/**
 * @brief Requirement condition for the time aspect of the statemachine.
 */
typedef enum
{
   IN_REQ_TIME_FAIL = 0,
   IN_REQ_TIME_PASS,
   IN_REQ_TIME_INVALID,
   IN_REQ_TIME_DONT_CARE
} INPUT_REQUIREMENT_TIME;

/**
 * @brief Input requirements for a single test case.
 */
typedef struct
{
   INPUT_REQUIREMENT_BOOL cap_on;
   INPUT_REQUIREMENT_BOOL tilt_detected;
   INPUT_REQUIREMENT_TIME time_requirement;
} input_requirements_t;

/**
 * @brief Test data for a single state machine state transition test case.
 */
typedef struct
{
   DOSE_DETECTION_STATE initial_state;
   input_requirements_t input_requirements;

   DOSE_DETECTION_STATE expected_end_state;
   dose_detection_fsm_outputs_t expected_outputs;
} test_data_t;

/**
 * @brief Presents a time scenario for testing valid dose duration handling.
 */
typedef struct
{
   uint64_t current_time_ms;         /**< Current timestamp */
   uint64_t dose_start_time_ms;      /**< Timestamp when dose event started */
   uint64_t valid_dose_threshold_ms; /**< Minimum duration for a valid dose event in milliseconds */

   bool is_valid_dose;       /**< Whether the time scenario constitutes a valid dose */
   bool has_valid_timestamp; /**< Whether the timestamps are valid (current_time_ms >= dose_start_time_ms) */
} time_scenario_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/**
 * @brief Set of time scenarios representing all boundary conditions and edge cases
 * @note It is assumed that the timestamps will not wrap around during normal operation. The statemachine should stay in
 * the current state if the timestamps are invalid (i.e., current_time_ms < dose_start_time_ms).
 */
static const time_scenario_t TIME_SCENARIOS[] = {
   {0u, 0u, 0u, true, true},
   {1u, 0u, 0u, true, true},
   {0u, 0u, 1u, false, true},
   {1u, 0u, 1u, true, true},
   {2u, 0u, 1u, true, true},
   {0u, 0u, 100u, false, true},
   {99u, 0u, 100u, false, true},
   {100u, 0u, 100u, true, true},
   {101u, 0u, 100u, true, true},
   {0u, 0u, UINT64_MAX, false, true},
   {1u, 0u, UINT64_MAX, false, true},
   {UINT64_MAX, 0u, UINT64_MAX, true, true},
   {0u, 1u, 0u, false, false},
   {0u, 1u, 1u, false, false},
   {99u, 100u, 1u, false, false},
};

static const size_t NUM_TIME_SCENARIOS = sizeof(TIME_SCENARIOS) / sizeof(TIME_SCENARIOS[0]);

/** Test cases for the IDLE state transitions */
static const test_data_t IDLE_STATE_TEST_CASES[] = {
   // cap_on == false -> transition to PRIMED
   {DOSE_DETECTION_STATE_IDLE,
    {IN_REQ_BOOL_FALSE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_PRIMED,
    {true}},
   // all other conditions -> remain in IDLE
   {DOSE_DETECTION_STATE_IDLE,
    {IN_REQ_BOOL_TRUE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_IDLE,
    {false}},
};

/** Test cases for the PRIMED state transitions */
static const test_data_t PRIMED_STATE_TEST_CASES[] = {
   // cap_on == true -> transition to IDLE
   {DOSE_DETECTION_STATE_PRIMED,
    {IN_REQ_BOOL_TRUE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_IDLE,
    {true}},
   // tilt_detected == true and cap_on == false -> transition to EVALUATING
   {DOSE_DETECTION_STATE_PRIMED,
    {IN_REQ_BOOL_FALSE, IN_REQ_BOOL_TRUE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_EVALUATING,
    {true}},
   // all other conditions -> remain in PRIMED
   {DOSE_DETECTION_STATE_PRIMED,
    {IN_REQ_BOOL_FALSE, IN_REQ_BOOL_FALSE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_PRIMED,
    {false}},
};

/** Test cases for the EVALUATING state transitions */
static const test_data_t EVALUATING_STATE_TEST_CASES[] = {
   // cap_on == true and valid dose duration not met -> transition to IDLE
   {DOSE_DETECTION_STATE_EVALUATING,
    {IN_REQ_BOOL_TRUE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_FAIL},
    DOSE_DETECTION_STATE_IDLE,
    {true}},
   // cap_on == true and valid dose duration met -> transition to DOSE_DETECTED
   {DOSE_DETECTION_STATE_EVALUATING,
    {IN_REQ_BOOL_TRUE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_PASS},
    DOSE_DETECTION_STATE_DOSE_DETECTED,
    {true}},
   // invalid timestamps -> remain in EVALUATING regardless of cap_on
   {DOSE_DETECTION_STATE_EVALUATING,
    {IN_REQ_BOOL_TRUE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_INVALID},
    DOSE_DETECTION_STATE_EVALUATING,
    {false}},
   // all other conditions -> remain in EVALUATING
   {DOSE_DETECTION_STATE_EVALUATING,
    {IN_REQ_BOOL_FALSE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_EVALUATING,
    {false}},

};

/** Test cases for the DOSE_DETECTED state transitions */
static const test_data_t DOSE_DETECTED_STATE_TEST_CASES[] = {
   // Any condition -> transition to IDLE
   {DOSE_DETECTION_STATE_DOSE_DETECTED,
    {IN_REQ_BOOL_DONT_CARE, IN_REQ_BOOL_DONT_CARE, IN_REQ_TIME_DONT_CARE},
    DOSE_DETECTION_STATE_IDLE,
    {true}},
};

/** Number of test cases for each state */
static const size_t NUM_IDLE_STATE_TEST_CASES = ARRAY_LEN(IDLE_STATE_TEST_CASES);
static const size_t NUM_PRIMED_STATE_TEST_CASES = ARRAY_LEN(PRIMED_STATE_TEST_CASES);
static const size_t NUM_EVALUATING_STATE_TEST_CASES = ARRAY_LEN(EVALUATING_STATE_TEST_CASES);
static const size_t NUM_DOSE_DETECTED_STATE_TEST_CASES = ARRAY_LEN(DOSE_DETECTED_STATE_TEST_CASES);

/***********************************************************************************************************************
 * Static Function Declarations
 **********************************************************************************************************************/

static void run_test_case(statemachine_t *p_fsm, const test_data_t *test_case);
static void test_input_combo(statemachine_t *p_fsm,
                             DOSE_DETECTION_STATE initial_state,
                             bool cap_on,
                             bool tilt_detected,
                             const time_scenario_t *p_ts,
                             DOSE_DETECTION_STATE expected_end_state,
                             const dose_detection_fsm_outputs_t *p_expected_outputs);

/***********************************************************************************************************************
 * Static Function Definitions
 **********************************************************************************************************************/

/**
 * @brief Runs a single test case by iterating through all relevant input combinations.
 *
 * This function tests all combinations of inputs that meet the specified requirements for the given test case.
 *
 * @param[in] p_fsm Pointer to the statemachine instance to test.
 * @param[in] test_case Pointer to the test case data to use.
 * @return void
 */
static void run_test_case(statemachine_t *p_fsm, const test_data_t *test_case)
{
   const input_requirements_t *p_reqs = &test_case->input_requirements;

   for(uint8_t cap_on_val = 0u; cap_on_val < 2u; cap_on_val++)
   {
      bool cap_on = cap_on_val == 0u ? false : true;

      // Skip irrelevant cap_on conditions
      if(((false == cap_on) && (IN_REQ_BOOL_TRUE == p_reqs->cap_on))
         || ((true == cap_on) && (IN_REQ_BOOL_FALSE == p_reqs->cap_on)))
      {
         continue;
      }

      for(uint8_t tilt_val = 0u; tilt_val < 2u; tilt_val++)
      {
         bool tilt_detected = tilt_val == 0u ? false : true;

         // Skip irrelevant tilt_detected conditions
         if(((false == tilt_detected) && (IN_REQ_BOOL_TRUE == p_reqs->tilt_detected))
            || ((true == tilt_detected) && (IN_REQ_BOOL_FALSE == p_reqs->tilt_detected)))
         {
            continue;
         }

         for(uint8_t ts_idx = 0u; ts_idx < NUM_TIME_SCENARIOS; ts_idx++)
         {
            const time_scenario_t *p_ts = &TIME_SCENARIOS[ts_idx];

            // Skip irrelevant time conditions
            if(((IN_REQ_TIME_PASS == p_reqs->time_requirement)
                && ((false == p_ts->has_valid_timestamp) || (false == p_ts->is_valid_dose)))
               || ((IN_REQ_TIME_FAIL == p_reqs->time_requirement)
                   && ((false == p_ts->has_valid_timestamp) || (true == p_ts->is_valid_dose)))
               || ((IN_REQ_TIME_INVALID == p_reqs->time_requirement) && (true == p_ts->has_valid_timestamp)))
            {
               continue;
            }

            test_input_combo(p_fsm,
                             test_case->initial_state,
                             cap_on,
                             tilt_detected,
                             p_ts,
                             test_case->expected_end_state,
                             &test_case->expected_outputs);
         }
      }
   }
}

/**
 * @brief Tests a single combination of inputs for the dose detection state machine.
 *
 * This function sets up the state machine with the specified initial state and inputs, runs it,
 * and verifies that the resulting state and outputs match the expected values.
 *
 * @param[in] p_fsm Pointer to the statemachine instance to test.
 * @param[in] initial_state The initial state of the state machine.
 * @param[in] cap_on The cap_on input value.
 * @param[in] tilt_detected The tilt_detected input value.
 * @param[in] p_ts Pointer to the time scenario to use for current_time_ms and dose_start_time_ms.
 * @param[in] expected_end_state The expected state of the state machine after running.
 * @param[in] p_expected_outputs Pointer to the expected outputs after running.
 *
 * @return void
 */
static void test_input_combo(statemachine_t *p_fsm,
                             DOSE_DETECTION_STATE initial_state,
                             bool cap_on,
                             bool tilt_detected,
                             const time_scenario_t *p_ts,
                             DOSE_DETECTION_STATE expected_end_state,
                             const dose_detection_fsm_outputs_t *p_expected_outputs)
{
   // Setup state machine
   p_fsm->current_state = initial_state;
   dose_detection_fsm_inputs_t inputs = {.cap_on = cap_on,
                                         .tilt_detected = tilt_detected,
                                         .current_time_ms = p_ts->current_time_ms,
                                         .dose_start_time_ms = p_ts->dose_start_time_ms,
                                         .valid_dose_threshold_ms = p_ts->valid_dose_threshold_ms};
   dose_detection_fsm_outputs_t outputs = {0};

   // Run state machine
   p_fsm->update_statemachine(p_fsm, &inputs, &outputs);

   // Verify results with descriptive messages
   // clang-format off
   EXPECT_EQ(p_fsm->current_state, expected_end_state)
      << "[State mismatch] Initial: " << dose_detection_fsm_state_names[initial_state] 
      << "\ncap_on: " << cap_on 
      << "\ntilt_detected: " << tilt_detected
      << "\ncurrent_time: " << p_ts->current_time_ms 
      << "\ndose_start_time: " << p_ts->dose_start_time_ms
      << "\nvalid_dose_threshold_ms: " << p_ts->valid_dose_threshold_ms 
      << "\nExpected: " << dose_detection_fsm_state_names[expected_end_state]
      << "\nActual: " << dose_detection_fsm_state_names[p_fsm->current_state];

   EXPECT_EQ(outputs.state_changed, p_expected_outputs->state_changed)
      << "[Output mismatch] Initial: " << dose_detection_fsm_state_names[initial_state]
      << "\ncap_on: " << cap_on
      << "\ntilt_detected: " << tilt_detected 
      << "\ncurrent_time: " << p_ts->current_time_ms
      << "\ndose_start_time: " << p_ts->dose_start_time_ms
      << "\nvalid_dose_threshold_ms: " << p_ts->valid_dose_threshold_ms
      << "\nExpected state_changed: " << p_expected_outputs->state_changed
      << "\nActual state_changed: " << outputs.state_changed;
   // clang-format on
}

/***********************************************************************************************************************
 * Test Fixtures
 **********************************************************************************************************************/

class DoseDetectionFSMTestSuite: public testing::Test
{
protected:
   statemachine_t fsm = {0};

   void SetUp() override
   {
      result_t result = dose_detection_fsm_init(&fsm);
      ASSERT_EQ(result, RESULT_OK);
   }

   void TearDown() override
   {
   }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

/**
 * @brief Test initialization of the dose detection state machine with a null pointer.
 *
 * Expected Result: The function should return DOSE_DETECTION_FSM_ERROR_NULL.
 */
TEST_F(DoseDetectionFSMTestSuite, InitializationNullTest)
{
   result_t result = dose_detection_fsm_init(NULL);
   EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_FSM);
   EXPECT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_FSM_ERROR_NULL);
}

/**
 * @brief Test various state transitions from the IDLE state.
 *
 * This test iterates through the test cases defined for the IDLE state, applying all the appropriate input conditions,
 * and verifies that the state machine transitions to the expected state with the correct outputs.
 *
 * Expected Result: The state machine should transition to the expected state with the correct outputs for each case.
 */
TEST_F(DoseDetectionFSMTestSuite, IdleStateTests)
{
   for(uint8_t case_idx = 0; case_idx < NUM_IDLE_STATE_TEST_CASES; case_idx++)
   {
      run_test_case(&fsm, &IDLE_STATE_TEST_CASES[case_idx]);
   }
}

/**
 * @brief Test various state transitions from the PRIMED state.
 *
 * This test iterates through the test cases defined for the PRIMED state, applying all the appropriate input
 * conditions, and verifies that the state machine transitions to the expected state with the correct outputs.
 *
 * Expected Result: The state machine should transition to the expected state with the correct outputs for each test
 * case.
 */
TEST_F(DoseDetectionFSMTestSuite, PrimedStateTests)
{
   for(uint8_t case_idx = 0; case_idx < NUM_PRIMED_STATE_TEST_CASES; case_idx++)
   {
      run_test_case(&fsm, &PRIMED_STATE_TEST_CASES[case_idx]);
   }
}

/**
 * @brief Test various state transitions from the EVALUATING state.
 *
 * This test iterates through the test cases defined for the EVALUATING state, applying all the appropriate input
 * conditions, and verifies that the state machine transitions to the expected state with the correct outputs.
 *
 * Expected Result: The state machine should transition to the expected state with the correct outputs for each test
 * case.
 */
TEST_F(DoseDetectionFSMTestSuite, EvaluatingStateTests)
{
   for(uint8_t case_idx = 0; case_idx < NUM_EVALUATING_STATE_TEST_CASES; case_idx++)
   {
      run_test_case(&fsm, &EVALUATING_STATE_TEST_CASES[case_idx]);
   }
}

/**
 * @brief Test various state transitions from the DOSE_DETECTED state.
 *
 * This test iterates through the test cases defined for the DOSE_DETECTED state, applying all the appropriate input
 * conditions, and verifies that the state machine transitions to the expected state with the correct outputs.
 *
 * Expected Result: The state machine should transition to the expected state with the correct outputs for each test
 * case.
 */
TEST_F(DoseDetectionFSMTestSuite, DoseDetectedStateTests)
{
   for(uint8_t case_idx = 0; case_idx < NUM_DOSE_DETECTED_STATE_TEST_CASES; case_idx++)
   {
      run_test_case(&fsm, &DOSE_DETECTED_STATE_TEST_CASES[case_idx]);
   }
}
