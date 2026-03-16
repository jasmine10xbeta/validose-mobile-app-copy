/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>

extern "C"
{
// Standard includes
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Custom includes
#include "common.h"
#include "dose_size_detection.h"
#include "dsd_fsm.h"

#include "mock/system_time_mock/system_time.h"
#include "mock/weight_sensor_mock/weight_sensor.h"
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG   (25000u)
#define TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG (0u)

#define TEST_STABLE_STDDEV_MIN_MG (6u)
#define TEST_STABLE_STDDEV_MAX_MG (14u)
#define TEST_UNSTABLE_STDDEV_MG   (1200u)

#define TEST_RING_PRESENT_WEIGHT_MG  (25000)
#define TEST_RING_ABSENT_WEIGHT_MG   (0)
#define TEST_RING_REPLACED_WEIGHT_MG (24700)

#define TEST_IDLE_TIMEOUT_MS (IDLE_STATE_WAIT_MAX_S * COMMON_1K_FACTOR)

#define TEST_DEFAULT_TEMP_DECI_C (250)

/***********************************************************************************************************************
 * Helper functions
 **********************************************************************************************************************/

static void run_process_cycles(dose_size_detection_t *test_dsd,
                               weight_sensor_t *mock_weight,
                               system_time_t *mock_time,
                               bool is_ring_present,
                               int32_t weight_mg,
                               uint16_t stddev_mg,
                               uint16_t cycles,
                               uint32_t advance_after_process_ms)
{
   ASSERT_NE(test_dsd, nullptr);
   ASSERT_NE(mock_weight, nullptr);
   ASSERT_NE(mock_time, nullptr);

   for(uint16_t count = 0u; count < cycles; count++)
   {
      result_t result = mock_weight_sensor_set_weight_data(
         mock_weight, weight_mg, stddev_mg, mock_time->_current_time_ms, is_ring_present, TEST_DEFAULT_TEMP_DECI_C);
      ASSERT_EQ(result, RESULT_OK);

      result = test_dsd->interface.process(&test_dsd->interface, is_ring_present);
      ASSERT_EQ(result, RESULT_OK);

      mock_time->_current_time_ms += advance_after_process_ms;
   }
}

static void
   drive_to_ring_present_state(dose_size_detection_t *test_dsd, weight_sensor_t *mock_weight, system_time_t *mock_time)
{
   run_process_cycles(test_dsd,
                      mock_weight,
                      mock_time,
                      true,
                      TEST_RING_PRESENT_WEIGHT_MG,
                      TEST_STABLE_STDDEV_MIN_MG,
                      4u,
                      COMMON_1K_FACTOR);
}

static void
   drive_to_ring_absent_state(dose_size_detection_t *test_dsd, weight_sensor_t *mock_weight, system_time_t *mock_time)
{
   drive_to_ring_present_state(test_dsd, mock_weight, mock_time);
   result_t result = mock_weight_sensor_set_weight_data(mock_weight,
                                                        TEST_RING_ABSENT_WEIGHT_MG,
                                                        TEST_STABLE_STDDEV_MIN_MG,
                                                        mock_time->_current_time_ms,
                                                        false,
                                                        TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd->interface.process(&test_dsd->interface, false);
   ASSERT_EQ(result, RESULT_OK);

   mock_time->_current_time_ms += 100u;
   run_process_cycles(
      test_dsd, mock_weight, mock_time, false, TEST_RING_ABSENT_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 30u, 100u);
}

static void
   drive_to_ring_replaced_state(dose_size_detection_t *test_dsd, weight_sensor_t *mock_weight, system_time_t *mock_time)
{
   drive_to_ring_absent_state(test_dsd, mock_weight, mock_time);
   result_t result = mock_weight_sensor_set_weight_data(mock_weight,
                                                        TEST_RING_REPLACED_WEIGHT_MG,
                                                        TEST_STABLE_STDDEV_MIN_MG,
                                                        mock_time->_current_time_ms,
                                                        true,
                                                        TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd->interface.process(&test_dsd->interface, true);
   ASSERT_EQ(result, RESULT_OK);

   mock_time->_current_time_ms += 100u;
   run_process_cycles(
      test_dsd, mock_weight, mock_time, true, TEST_RING_REPLACED_WEIGHT_MG, TEST_STABLE_STDDEV_MAX_MG, 25u, 100u);
}

/***********************************************************************************************************************
 * Test Fixtures
 **********************************************************************************************************************/

/**
 * @brief Test suite for Dose Size Detection module
 *
 * Holds a mock weight sensor, mock system time, and a test DSD instance.
 */
class DoseSizeDetectionTestSuite: public testing::Test
{
protected:
   weight_sensor_t mock_weight = {0};
   system_time_t mock_time = {0};
   dose_size_detection_t test_dsd = {0};

   void SetUp() override
   {
      result_t result = system_time_init(&mock_time);
      ASSERT_EQ(result, RESULT_OK);

      result = mock_weight_sensor_init(&mock_weight);
      ASSERT_EQ(result, RESULT_OK);

      result = mock_weight_sensor_set_calibration_state(&mock_weight, WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED);
      ASSERT_EQ(result, RESULT_OK);

      result = mock_weight_sensor_set_is_stable(&mock_weight, true);
      ASSERT_EQ(result, RESULT_OK);

      result = mock_weight_sensor_set_data_stale(&mock_weight, false);
      ASSERT_EQ(result, RESULT_OK);

      result = dose_size_detection_init(&test_dsd,
                                        &mock_weight.interface,
                                        &mock_time.interface,
                                        TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                        TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
      ASSERT_EQ(result, RESULT_OK);

      result = mock_time.interface.set_time_ms(&mock_time.interface, 1000u);
      ASSERT_EQ(result, RESULT_OK);
   }

   void TearDown() override
   {
      memset(&test_dsd, 0, sizeof(test_dsd));
      memset(&mock_weight, 0, sizeof(mock_weight));
      memset(&mock_time, 0, sizeof(mock_time));
   }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

// Initialization
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD initialization correctly sets up the instance
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_init_happy_path_test)
{
   dose_size_detection_t local_dsd = {0};

   // Dirty the local instance
   memset(&local_dsd, 0xFF, sizeof(local_dsd));

   result_t result = dose_size_detection_init(&local_dsd,
                                              &mock_weight.interface,
                                              &mock_time.interface,
                                              TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                              TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_EQ(result, RESULT_OK);

   // Interface
   ASSERT_EQ(local_dsd.interface.parent, &local_dsd);
   ASSERT_NE(local_dsd.interface.process, nullptr);
   ASSERT_NE(local_dsd.interface.get_dose_size_data_state, nullptr);
   ASSERT_NE(local_dsd.interface.get_dose_size_data_bad_reason, nullptr);
   ASSERT_NE(local_dsd.interface.fetch_dose_size_data, nullptr);
   ASSERT_NE(local_dsd.interface.abort_baselining, nullptr);

   // Dependencies
   EXPECT_EQ(local_dsd._system_time_interface, &mock_time.interface);
   EXPECT_EQ(local_dsd._weight_sensor_interface, &mock_weight.interface);

   // Private data
   EXPECT_EQ(local_dsd._last_total_dispensed_weight_mg, TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   EXPECT_EQ(local_dsd._full_assembly_weight_mg, TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG);
   EXPECT_EQ(local_dsd._dose_size_data_state, DSD_DATA_STATE_NO_DATA);
   EXPECT_EQ(local_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_NONE);
   EXPECT_EQ(local_dsd._wait_time_ms, (STABLE_WAIT_TIME_S * COMMON_1K_FACTOR));
   EXPECT_EQ(local_dsd._sampling_frequency, DSD_FREQ_SLOW);
   EXPECT_EQ(local_dsd._previous_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
   EXPECT_FALSE(local_dsd._is_latest_sample_valid);
   EXPECT_FALSE(local_dsd._is_blocked);
   EXPECT_EQ(local_dsd._time_enter_idle_state_ms, 0u);
   EXPECT_FALSE(local_dsd._got_valid_weight_sample);
   EXPECT_TRUE(local_dsd._initialized);

   // Dose size data
   EXPECT_EQ(local_dsd._pre_dose_data.weight_mg, 0);
   EXPECT_FALSE(local_dsd._pre_dose_data.is_valid);
   EXPECT_EQ(local_dsd._pre_dose_data.time_ms, 0u);

   EXPECT_EQ(local_dsd._drift_data.weight_mg, 0);
   EXPECT_FALSE(local_dsd._drift_data.is_valid);
   EXPECT_EQ(local_dsd._drift_data.time_ms, 0u);

   EXPECT_EQ(local_dsd._post_dose_data.weight_mg, 0);
   EXPECT_FALSE(local_dsd._post_dose_data.is_valid);
   EXPECT_EQ(local_dsd._post_dose_data.time_ms, 0u);

   EXPECT_EQ(local_dsd._dose_size_mg, 0);
   EXPECT_EQ(local_dsd._sigma_total_mg, 0u);

   // State machine
   EXPECT_EQ(local_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
   EXPECT_NE(local_dsd._statemachine.transitions, nullptr);
   EXPECT_NE(local_dsd._statemachine.update_statemachine, nullptr);

   EXPECT_FALSE(local_dsd._sm_inputs.is_ring_present);
   EXPECT_FALSE(local_dsd._sm_inputs.got_valid_weight_sample);
   EXPECT_FALSE(local_dsd._sm_inputs.is_max_time_elapsed);
   EXPECT_FALSE(local_dsd._sm_inputs.is_dose_size_computed);

   EXPECT_FALSE(local_dsd._sm_outputs.changed);
   EXPECT_FALSE(local_dsd._sm_outputs.is_sequence_interrupted);
   EXPECT_FALSE(local_dsd._sm_outputs.is_dose_size_computed);
   EXPECT_EQ(local_dsd._sm_outputs.sampling_frequency, DSD_FREQ_SLOW);
   EXPECT_EQ(local_dsd._sm_outputs.wait_time, 0u);

   // Best-sample window
   EXPECT_EQ(local_dsd._best_sample_window_head, 0u);
   EXPECT_EQ(local_dsd._best_sample_window_tail, 0u);
   EXPECT_EQ(local_dsd._best_sample_idx, DSD_BEST_SAMPLE_IDX_INVALID);
   EXPECT_EQ(local_dsd._best_sample_window_latest_second_s, UINT32_MAX);

   // Stability tracker
   EXPECT_EQ(local_dsd._stability_tracker._last_sample_time_ms, 0u);
   EXPECT_FALSE(local_dsd._stability_tracker.is_not_moving);
   EXPECT_FALSE(local_dsd._stability_tracker.is_stable);
   EXPECT_FALSE(local_dsd._stability_tracker.is_settled);
   EXPECT_FALSE(local_dsd._stability_tracker.sample_taken);
   EXPECT_EQ(local_dsd._stability_tracker.max_time_start_time_s, 0u);
   EXPECT_EQ(local_dsd._stability_tracker.last_stable_time_s, 0u);
}

/**
 * @brief Test DSD initialization with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_init_invalid_parameters_test)
{
   // NULL dose size detection instance
   result_t result = dose_size_detection_init(NULL,
                                              &mock_weight.interface,
                                              &mock_time.interface,
                                              TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                              TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_TRUE(IS_ERR(result));

   // NULL weight sensor interface
   result = dose_size_detection_init(&test_dsd,
                                     NULL,
                                     &mock_time.interface,
                                     TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                     TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_TRUE(IS_ERR(result));

   // Uninitialized weight sensor interface
   mock_weight.interface.parent = NULL; // Simulate uninitialized interface
   result = dose_size_detection_init(&test_dsd,
                                     &mock_weight.interface,
                                     &mock_time.interface,
                                     TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                     TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_TRUE(IS_ERR(result));
   mock_weight.interface.parent = &mock_weight; // Restore for other tests

   // NULL system time interface
   result = dose_size_detection_init(&test_dsd,
                                     &mock_weight.interface,
                                     NULL,
                                     TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                     TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_TRUE(IS_ERR(result));

   // Uninitialized system time interface
   mock_time.interface.parent = NULL; // Simulate uninitialized interface
   result = dose_size_detection_init(&test_dsd,
                                     &mock_weight.interface,
                                     &mock_time.interface,
                                     TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG,
                                     TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_TRUE(IS_ERR(result));
   mock_time.interface.parent = &mock_time; // Restore for other tests
}

// `get_dose_size_data_state` function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD get_dose_size_data_state happy path
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_get_data_state_happy_path_test)
{
   const DSD_DATA_STATE expected_states[]
      = {DSD_DATA_STATE_NO_DATA, DSD_DATA_STATE_BUSY, DSD_DATA_STATE_READY, DSD_DATA_STATE_BAD};

   for(uint8_t idx = 0u; idx < ARRAY_LEN(expected_states); idx++)
   {
      DSD_DATA_STATE data_state = DSD_DATA_STATE_MAX;
      test_dsd._dose_size_data_state = expected_states[idx];

      result_t result = test_dsd.interface.get_dose_size_data_state(&test_dsd.interface, &data_state);
      ASSERT_EQ(result, RESULT_OK);
      EXPECT_EQ(data_state, expected_states[idx]);
   }
}

/**
 * @brief Test DSD get_dose_size_data_state with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_get_data_state_invalid_parameters_test)
{
   DSD_DATA_STATE data_state = DSD_DATA_STATE_NO_DATA;

   result_t result = test_dsd.interface.get_dose_size_data_state(NULL, &data_state);
   ASSERT_TRUE(IS_ERR(result));

   result = test_dsd.interface.get_dose_size_data_state(&test_dsd.interface, NULL);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.get_dose_size_data_state(&test_dsd.interface, &data_state);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.get_dose_size_data_state(&test_dsd.interface, &data_state);
   ASSERT_TRUE(IS_ERR(result));
}

// `get_dose_size_data_bad_reason` function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD get_dose_size_data_bad_reason happy path
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_get_bad_reason_happy_path_test)
{
   const DSD_DATA_BAD_REASON expected_reasons[] = {
      DSD_DATA_BAD_REASON_NONE,
      DSD_DATA_BAD_REASON_INVALID_DOSE_DATA,
      DSD_DATA_BAD_REASON_NEGATIVE_DOSE_SIZE,
      DSD_DATA_BAD_REASON_NEGATIVE_TOTAL_DISPENSED,
      DSD_DATA_BAD_REASON_TOTAL_DISPENSED_OVERFLOW,
      DSD_DATA_BAD_REASON_INTERRUPTED,
      DSD_DATA_BAD_REASON_TIMEOUT,
   };

   for(uint8_t idx = 0u; idx < ARRAY_LEN(expected_reasons); idx++)
   {
      DSD_DATA_BAD_REASON bad_reason = DSD_DATA_BAD_REASON_MAX;
      test_dsd._dose_size_data_bad_reason = expected_reasons[idx];
      result_t result = test_dsd.interface.get_dose_size_data_bad_reason(&test_dsd.interface, &bad_reason);
      ASSERT_EQ(result, RESULT_OK);
      EXPECT_EQ(bad_reason, expected_reasons[idx]);
   }
}

/**
 * @brief Test DSD get_dose_size_data_bad_reason with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_get_bad_reason_invalid_parameters_test)
{
   DSD_DATA_BAD_REASON bad_reason = DSD_DATA_BAD_REASON_NONE;

   result_t result = test_dsd.interface.get_dose_size_data_bad_reason(NULL, &bad_reason);
   ASSERT_TRUE(IS_ERR(result));

   result = test_dsd.interface.get_dose_size_data_bad_reason(&test_dsd.interface, NULL);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.get_dose_size_data_bad_reason(&test_dsd.interface, &bad_reason);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.get_dose_size_data_bad_reason(&test_dsd.interface, &bad_reason);
   ASSERT_TRUE(IS_ERR(result));
}

// `fetch_dose_size_data` function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD fetch_dose_size_data
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_fetch_dose_size_data_test)
{
   const struct
   {
      // Inputs
      DSD_DATA_STATE initial_data_state;

      // Expected outputs
      bool expected_error;
      uint8_t expected_error_code;
      bool data_received;
   } test_cases[] = {
      {DSD_DATA_STATE_NO_DATA, true, DOSE_SIZE_DETECTION_ERROR_NO_DATA, false},
      {DSD_DATA_STATE_BUSY, true, DOSE_SIZE_DETECTION_ERROR_BUSY, false},
      {DSD_DATA_STATE_BAD, true, DOSE_SIZE_DETECTION_ERROR_DATA_BAD, true},
      {DSD_DATA_STATE_READY, false, DOSE_SIZE_DETECTION_ERROR_NONE, true},
   };

   for(uint8_t idx = 0u; idx < ARRAY_LEN(test_cases); idx++)
   {
      const uint32_t sentinel_dose_size_mg = 1111u;
      const uint32_t sentinel_total_dispensed_mg = 2222u;
      const uint16_t sentinel_sigma_total_mg = 333u;

      dose_size_data_out_t data_out = {0};
      data_out.dose_size_mg = sentinel_dose_size_mg;
      data_out.total_dispensed_mg = sentinel_total_dispensed_mg;
      data_out.sigma_total_mg = sentinel_sigma_total_mg;

      test_dsd._dose_size_data_state = test_cases[idx].initial_data_state;
      test_dsd._dose_size_data_bad_reason = DSD_DATA_BAD_REASON_MAX; // Arbitrary to verify it gets cleared on fetch

      result_t result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, &data_out);

      if(test_cases[idx].expected_error)
      {
         EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SIZE_DETECTION);
         EXPECT_EQ(GET_ERR_CODE(result), test_cases[idx].expected_error_code);
      }
      else
      {
         ASSERT_EQ(result, RESULT_OK);
      }

      if(test_cases[idx].data_received)
      {
         // With default fixture data, these are expected to be zero.
         EXPECT_EQ(data_out.dose_size_mg, 0u);
         EXPECT_EQ(data_out.total_dispensed_mg, 0u);
         EXPECT_EQ(data_out.sigma_total_mg, 0u);

         EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_NO_DATA);
         EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_NONE);
      }
      else
      {
         // No copy expected for NO_DATA and BUSY
         EXPECT_EQ(data_out.dose_size_mg, sentinel_dose_size_mg);
         EXPECT_EQ(data_out.total_dispensed_mg, sentinel_total_dispensed_mg);
         EXPECT_EQ(data_out.sigma_total_mg, sentinel_sigma_total_mg);

         EXPECT_EQ(test_dsd._dose_size_data_state, test_cases[idx].initial_data_state);
         EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_MAX);
      }
   }
}

/**
 * @brief Test DSD fetch_dose_size_data with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_fetch_data_invalid_parameters_test)
{
   dose_size_data_out_t data_out = {};

   result_t result = test_dsd.interface.fetch_dose_size_data(NULL, &data_out);
   ASSERT_TRUE(IS_ERR(result));

   result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, NULL);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, &data_out);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, &data_out);
   ASSERT_TRUE(IS_ERR(result));
}

// `try_baseline_if_stable` function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD try_baseline_if_stable with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_invalid_parameters_test)
{
   bool baselining_successful = false;

   result_t result = test_dsd.interface.try_baseline_if_stable(NULL, true, &baselining_successful);
   ASSERT_TRUE(IS_ERR(result));

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, NULL);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_TRUE(IS_ERR(result));
}

/**
 * @brief Test DSD try_baseline_if_stable happy path
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_happy_path_test)
{
   dose_size_detection_t local_dsd = {0};
   bool baselining_successful = false;

   result_t result = dose_size_detection_init(&local_dsd, &mock_weight.interface, &mock_time.interface, 0u, 123u);
   ASSERT_EQ(result, RESULT_OK);

   result = mock_weight_sensor_set_calibration_state(&mock_weight, WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_is_stable(&mock_weight, true);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_data_stale(&mock_weight, false);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_weight_data(
      &mock_weight, TEST_RING_PRESENT_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 1000u, true, 250);
   ASSERT_EQ(result, RESULT_OK);

   result = local_dsd.interface.try_baseline_if_stable(&local_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_TRUE(baselining_successful);
   EXPECT_EQ(local_dsd._full_assembly_weight_mg, (uint32_t)TEST_RING_PRESENT_WEIGHT_MG);
   EXPECT_EQ(local_dsd._last_total_dispensed_weight_mg, 0u);
}

/**
 * @brief Test DSD try_baseline_if_stable with invalid baseline weight
 *
 * Invalid (too low) baseline weight should not immediately error. The baselining attempt remains in progress until
 * timeout, after which a timeout error is returned and the previous baseline is restored.
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_invalid_baseline_weight_test)
{
   bool baselining_successful = false;

   result_t result = mock_weight_sensor_set_calibration_state(&mock_weight, WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED);
   ASSERT_EQ(result, RESULT_OK);

   result = mock_weight_sensor_set_is_stable(&mock_weight, true);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_data_stale(&mock_weight, false);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_weight_data(&mock_weight, 0, TEST_STABLE_STDDEV_MIN_MG, 1000u, true, 250);
   ASSERT_EQ(result, RESULT_OK);

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);

   // Advance beyond timeout with still-invalid weight.
   result = mock_time.interface.set_time_ms(&mock_time.interface, 1000u + DSD_BASELINING_TIMEOUT_MS + 1u);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_weight_data(
      &mock_weight, 0, TEST_STABLE_STDDEV_MIN_MG, 1000u + DSD_BASELINING_TIMEOUT_MS + 1u, true, 250);
   ASSERT_EQ(result, RESULT_OK);

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_TRUE(IS_ERR(result));
   EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SIZE_DETECTION);
   EXPECT_EQ(GET_ERR_CODE(result), DOSE_SIZE_DETECTION_ERROR_TIMEOUT);
   EXPECT_FALSE(baselining_successful);
   EXPECT_FALSE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG);
}

/**
 * @brief Test DSD try_baseline_if_stable when weight sensor is not calibrated
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_not_calibrated_test)
{
   bool baselining_successful = false;

   result_t result
      = mock_weight_sensor_set_calibration_state(&mock_weight, WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED);
   ASSERT_EQ(result, RESULT_OK);

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_TRUE(IS_ERR(result));
   EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SIZE_DETECTION);
   EXPECT_EQ(GET_ERR_CODE(result), DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED);
   EXPECT_FALSE(baselining_successful);
   EXPECT_FALSE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG);
   EXPECT_EQ(test_dsd._last_total_dispensed_weight_mg, TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
}

/**
 * @brief Test DSD try_baseline_if_stable when ring is not present
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_ring_not_present_test)
{
   bool baselining_successful = false;

   result_t result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, false, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);
   EXPECT_EQ(test_dsd._last_baselining_sample_time_ms, 0u);
}

/**
 * @brief Test DSD try_baseline_if_stable when weight data is unstable
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_unstable_data_test)
{
   bool baselining_successful = false;

   result_t result = mock_weight_sensor_set_is_stable(&mock_weight, false);
   ASSERT_EQ(result, RESULT_OK);

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);
   EXPECT_EQ(test_dsd._last_baselining_sample_time_ms, 0u);
}

/**
 * @brief Test DSD try_baseline_if_stable when weight data is stale
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_stale_data_test)
{
   bool baselining_successful = false;

   result_t result = mock_weight_sensor_set_is_stable(&mock_weight, true);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_data_stale(&mock_weight, true);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_weight_data(
      &mock_weight, TEST_RING_PRESENT_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 1000u, true, TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);
   EXPECT_EQ(test_dsd._last_baselining_sample_time_ms, 0u);
}

/**
 * @brief Test DSD try_baseline_if_stable when data is below threshold and sample timestamp is not updated
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_try_baseline_if_stable_below_threshold_not_updated_time_test)
{
   bool baselining_successful = false;

   result_t result = mock_weight_sensor_set_is_stable(&mock_weight, true);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_data_stale(&mock_weight, false);
   ASSERT_EQ(result, RESULT_OK);

   // First sample is below threshold.
   result = mock_weight_sensor_set_weight_data(
      &mock_weight, 0, TEST_STABLE_STDDEV_MIN_MG, 1000u, true, TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);
   EXPECT_EQ(test_dsd._last_baselining_sample_time_ms, 1000u);

   // Next call uses same time_ms (not a new sample), even if weight is above threshold.
   result = mock_weight_sensor_set_weight_data(
      &mock_weight, TEST_RING_PRESENT_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 1000u, true, TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   baselining_successful = true; // prove function clears this output each call
   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);
   EXPECT_EQ(test_dsd._last_baselining_sample_time_ms, 1000u);
}

// `abort_baselining` function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD abort_baselining with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_abort_baselining_invalid_parameters_test)
{
   result_t result = test_dsd.interface.abort_baselining(NULL);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.abort_baselining(&test_dsd.interface);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.abort_baselining(&test_dsd.interface);
   ASSERT_TRUE(IS_ERR(result));
}

/**
 * @brief Test DSD abort_baselining while baselining is in progress
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_abort_baselining_while_in_progress_test)
{
   bool baselining_successful = false;

   result_t result = mock_weight_sensor_set_calibration_state(&mock_weight, WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_is_stable(&mock_weight, true);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_data_stale(&mock_weight, false);
   ASSERT_EQ(result, RESULT_OK);
   result = mock_weight_sensor_set_weight_data(&mock_weight, 0, TEST_STABLE_STDDEV_MIN_MG, 1000u, true, 250);
   ASSERT_EQ(result, RESULT_OK);

   result = test_dsd.interface.try_baseline_if_stable(&test_dsd.interface, true, &baselining_successful);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(baselining_successful);
   EXPECT_TRUE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, 0u);

   result = test_dsd.interface.abort_baselining(&test_dsd.interface);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG);
   EXPECT_EQ(test_dsd._last_total_dispensed_weight_mg, TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
}

/**
 * @brief Test DSD abort_baselining when no baselining is in progress
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_abort_baselining_when_not_in_progress_test)
{
   EXPECT_FALSE(test_dsd._is_baselining_in_progress);

   uint32_t full_assembly_weight_before = test_dsd._full_assembly_weight_mg;
   uint32_t total_dispensed_before = test_dsd._last_total_dispensed_weight_mg;

   result_t result = test_dsd.interface.abort_baselining(&test_dsd.interface);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_FALSE(test_dsd._is_baselining_in_progress);
   EXPECT_EQ(test_dsd._full_assembly_weight_mg, full_assembly_weight_before);
   EXPECT_EQ(test_dsd._last_total_dispensed_weight_mg, total_dispensed_before);
}

// `get_full_assembly_weight` function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD get_full_assembly_weight with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_get_full_assembly_weight_invalid_parameters_test)
{
   uint32_t full_assembly_weight_mg = 0u;

   result_t result = test_dsd.interface.get_full_assembly_weight(NULL, &full_assembly_weight_mg);
   ASSERT_TRUE(IS_ERR(result));

   result = test_dsd.interface.get_full_assembly_weight(&test_dsd.interface, NULL);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.get_full_assembly_weight(&test_dsd.interface, &full_assembly_weight_mg);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.get_full_assembly_weight(&test_dsd.interface, &full_assembly_weight_mg);
   ASSERT_TRUE(IS_ERR(result));
}

/**
 * @brief Test DSD get_full_assembly_weight happy path
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_get_full_assembly_weight_happy_path_test)
{
   uint32_t full_assembly_weight_mg = 0u;

   result_t result = test_dsd.interface.get_full_assembly_weight(&test_dsd.interface, &full_assembly_weight_mg);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(full_assembly_weight_mg, TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG);
}

// Process function
// ---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Test DSD process with invalid parameters
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_process_invalid_parameters_test)
{
   result_t result = test_dsd.interface.process(NULL, true);
   ASSERT_TRUE(IS_ERR(result));

   test_dsd.interface.parent = NULL;
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_TRUE(IS_ERR(result));
   test_dsd.interface.parent = &test_dsd;

   test_dsd._initialized = false;
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_TRUE(IS_ERR(result));
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_not_baselined_test)
{
   dose_size_detection_t local_dsd = {0};

   result_t result = dose_size_detection_init(
      &local_dsd, &mock_weight.interface, &mock_time.interface, 0u, TEST_DEFAULT_INITIAL_TOTAL_DISPENSED_WEIGHT_MG);
   ASSERT_EQ(result, RESULT_OK);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = local_dsd.interface.process(&local_dsd.interface, true);
   ASSERT_TRUE(IS_ERR(result));
   EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SIZE_DETECTION);
   EXPECT_EQ(GET_ERR_CODE(result), DOSE_SIZE_DETECTION_ERROR_NOT_BASELINED);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_scale_not_calibrated_test)
{
   result_t result
      = mock_weight_sensor_set_calibration_state(&mock_weight, WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED);
   ASSERT_EQ(result, RESULT_OK);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_TRUE(IS_ERR(result));
   EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SIZE_DETECTION);
   EXPECT_EQ(GET_ERR_CODE(result), DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_ring_present_state_test)
{
   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
   EXPECT_TRUE(test_dsd._pre_dose_data.is_valid);
   EXPECT_EQ(test_dsd._pre_dose_data.weight_mg, TEST_RING_PRESENT_WEIGHT_MG);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_ring_absent_state_test)
{
   drive_to_ring_absent_state(&test_dsd, &mock_weight, &mock_time);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_ABSENT);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_BUSY);
   EXPECT_TRUE(test_dsd._drift_data.is_valid);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_ring_replaced_state_test)
{
   drive_to_ring_replaced_state(&test_dsd, &mock_weight, &mock_time);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED);
   EXPECT_TRUE(test_dsd._post_dose_data.is_valid);
   EXPECT_EQ(test_dsd._post_dose_data.weight_mg, TEST_RING_REPLACED_WEIGHT_MG);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_ring_settled_state_test)
{
   result_t result = RESULT_OK;
   drive_to_ring_replaced_state(&test_dsd, &mock_weight, &mock_time);

   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_READY);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_NONE);
   EXPECT_GT(test_dsd._dose_size_mg, 0);
   EXPECT_GT(test_dsd._last_total_dispensed_weight_mg, 0u);

   mock_time._current_time_ms += COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
   EXPECT_EQ(test_dsd._sampling_frequency, DSD_FREQ_SLOW);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_present_vs_idle_state_test)
{
   const int32_t invalid_present_weight_mg = DOCK_RING_EMPTY_LOWER_W_MG - 1000;
   result_t result = RESULT_OK;

   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   // Force max-wait to appear elapsed in RING_PRESENT.
   test_dsd._stability_tracker.max_time_start_time_s = 1u;
   mock_time._current_time_ms += (STABLE_WAIT_TIME_S + 2u) * COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_IDLE);

   run_process_cycles(
      &test_dsd, &mock_weight, &mock_time, true, invalid_present_weight_mg, TEST_UNSTABLE_STDDEV_MG, 2u, 60000u);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_IDLE);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 60000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_absent_vs_idle_state_test)
{
   const int32_t invalid_absent_weight_mg = DOCK_MAX_DRIFT_MG + 1000;
   result_t result = RESULT_OK;

   drive_to_ring_absent_state(&test_dsd, &mock_weight, &mock_time);

   // Force max-wait to appear elapsed in RING_ABSENT.
   test_dsd._stability_tracker.max_time_start_time_s = 1u;
   mock_time._current_time_ms += (STABLE_WAIT_TIME_S + 2u) * COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_absent_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_IDLE);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_ABSENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 60000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_ABSENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);

   // One extra call to consume got_valid_weight_sample from IDLE and transition out if needed.
   mock_time._current_time_ms += 60000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_ABSENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_ABSENT);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_faster_ring_present_state_test)
{
   result_t result = RESULT_OK;
   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG - 500,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;

   int32_t drb_pre_before_mg = test_dsd._pre_dose_data.weight_mg;

   run_process_cycles(&test_dsd,
                      &mock_weight,
                      &mock_time,
                      true,
                      TEST_RING_PRESENT_WEIGHT_MG - 750,
                      TEST_STABLE_STDDEV_MIN_MG,
                      5u,
                      100u);
   EXPECT_EQ(test_dsd._pre_dose_data.weight_mg, drb_pre_before_mg);

   mock_time._current_time_ms += 500u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG - 250,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._pre_dose_data.weight_mg, TEST_RING_PRESENT_WEIGHT_MG - 250);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_ring_settled_state_unstable_test)
{
   const int32_t invalid_present_weight_mg = DOCK_RING_EMPTY_LOWER_W_MG - 1000;
   const int32_t invalid_absent_weight_mg = DOCK_MAX_DRIFT_MG + 1000;
   result_t result = RESULT_OK;

   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_absent_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_ABSENT);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED);

   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_BAD);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_INVALID_DOSE_DATA);

   mock_time._current_time_ms += COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_interrupted_sequence_sets_bad_reason_test)
{
   result_t result = RESULT_OK;
   drive_to_ring_absent_state(&test_dsd, &mock_weight, &mock_time);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED);

   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_ABSENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_BAD);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_INTERRUPTED);

   mock_time._current_time_ms += COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_PRESENT);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_idle_timeout_sets_bad_reason_test)
{
   const int32_t invalid_present_weight_mg = DOCK_RING_EMPTY_LOWER_W_MG - 1000;
   result_t result = RESULT_OK;

   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   // Force max-wait to appear elapsed in RING_PRESENT.
   test_dsd._stability_tracker.max_time_start_time_s = 1u;
   mock_time._current_time_ms += (STABLE_WAIT_TIME_S + 2u) * COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_IDLE);

   mock_time._current_time_ms += TEST_IDLE_TIMEOUT_MS;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_NO_DATA);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_NONE);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_missing_stable_pre_weight_sets_invalid_drb_data_test)
{
   const int32_t invalid_present_weight_mg = DOCK_RING_EMPTY_LOWER_W_MG - 1000;
   result_t result = RESULT_OK;

   // Keep ring present, but never provide an in-bounds stable pre-dose weight.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += COMMON_1K_FACTOR;

   // Continue sequence and collect valid drift measurements.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_ABSENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   run_process_cycles(
      &test_dsd, &mock_weight, &mock_time, false, TEST_RING_ABSENT_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 40u, 100u);

   // Continue sequence and collect valid post-dose measurement.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   run_process_cycles(
      &test_dsd, &mock_weight, &mock_time, true, TEST_RING_REPLACED_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 30u, 100u);

   // Allow transition to settled and compute.
   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_BAD);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_INVALID_DOSE_DATA);
   EXPECT_FALSE(test_dsd._pre_dose_data.is_valid);
   EXPECT_TRUE(test_dsd._drift_data.is_valid);
   EXPECT_TRUE(test_dsd._post_dose_data.is_valid);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_missing_stable_drift_d1_weight_sets_invalid_drb_data_test)
{
   const int32_t invalid_absent_weight_mg = DOCK_MAX_DRIFT_MG + 1000;
   result_t result = RESULT_OK;

   // First establish a valid pre-dose measurement.
   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   // Transition to absent, but never obtain a valid first drift sample.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_absent_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;

   // Continue sequence regardless and collect a valid post-dose measurement.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   run_process_cycles(
      &test_dsd, &mock_weight, &mock_time, true, TEST_RING_REPLACED_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 30u, 100u);

   // Allow transition to settled and compute.
   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_BAD);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_INVALID_DOSE_DATA);
   EXPECT_TRUE(test_dsd._pre_dose_data.is_valid);
   EXPECT_FALSE(test_dsd._drift_data.is_valid);
   EXPECT_TRUE(test_dsd._post_dose_data.is_valid);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_missing_stable_drift_d2_weight_sets_invalid_drb_data_test)
{
   result_t result = RESULT_OK;

   // First establish a valid pre-dose measurement.
   drive_to_ring_present_state(&test_dsd, &mock_weight, &mock_time);

   // Transition to absent, then wait only until first drift sample becomes valid.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_ABSENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               false,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;

   uint16_t guard_cycles = 0u;
   while(!test_dsd._drift_data.is_valid)
   {
      ASSERT_LT(guard_cycles++, 200u);
      result = mock_weight_sensor_set_weight_data(&mock_weight,
                                                  TEST_RING_ABSENT_WEIGHT_MG,
                                                  TEST_STABLE_STDDEV_MIN_MG,
                                                  mock_time._current_time_ms,
                                                  false,
                                                  TEST_DEFAULT_TEMP_DECI_C);
      ASSERT_EQ(result, RESULT_OK);
      result = test_dsd.interface.process(&test_dsd.interface, false);
      ASSERT_EQ(result, RESULT_OK);
      mock_time._current_time_ms += 100u;
   }
   ASSERT_TRUE(test_dsd._drift_data.is_valid);

   // Continue sequence after first valid drift sample.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   run_process_cycles(
      &test_dsd, &mock_weight, &mock_time, true, TEST_RING_REPLACED_WEIGHT_MG, TEST_STABLE_STDDEV_MIN_MG, 30u, 100u);

   // Allow transition to settled and compute.
   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_READY);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_NONE);
   EXPECT_TRUE(test_dsd._pre_dose_data.is_valid);
   EXPECT_TRUE(test_dsd._drift_data.is_valid);
   EXPECT_TRUE(test_dsd._post_dose_data.is_valid);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_process_missing_stable_post_weight_sets_invalid_drb_data_test)
{
   const int32_t invalid_present_weight_mg = DOCK_RING_EMPTY_LOWER_W_MG - 1000;
   result_t result = RESULT_OK;

   // Establish pre-dose and both drift measurements.
   drive_to_ring_absent_state(&test_dsd, &mock_weight, &mock_time);

   // Transition to replaced, but never obtain a valid post-dose measurement.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;
   run_process_cycles(
      &test_dsd, &mock_weight, &mock_time, true, invalid_present_weight_mg, TEST_UNSTABLE_STDDEV_MG, 30u, 100u);

   // Allow transition to settled and compute.
   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               invalid_present_weight_mg,
                                               TEST_UNSTABLE_STDDEV_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   EXPECT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_BAD);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_INVALID_DOSE_DATA);
   EXPECT_TRUE(test_dsd._pre_dose_data.is_valid);
   EXPECT_TRUE(test_dsd._drift_data.is_valid);
   EXPECT_FALSE(test_dsd._post_dose_data.is_valid);
}

TEST_F(DoseSizeDetectionTestSuite, dsd_fetch_dose_size_data_after_valid_sequence_test)
{
   result_t result = RESULT_OK;
   const int32_t drift_weight_mg = 50;
   const uint16_t pre_stddev_mg = 30u;
   const uint16_t drift_stddev_mg = 5u;
   const uint16_t post_stddev_mg = 7u;
   const uint32_t expected_dose_size_mg
      = TEST_DEFAULT_INITIAL_FULL_ASSEMBLY_WEIGHT_MG - TEST_RING_REPLACED_WEIGHT_MG - (uint32_t)drift_weight_mg;
   const uint16_t expected_sigma_total_mg = 8u; // sqrt(5^2 + 7^2)

   // RING_PRESENT: obtain stable pre-dose measurement.
   for(uint8_t count = 0u; count < 4u; count++)
   {
      result = mock_weight_sensor_set_weight_data(&mock_weight,
                                                  TEST_RING_PRESENT_WEIGHT_MG,
                                                  pre_stddev_mg,
                                                  mock_time._current_time_ms,
                                                  true,
                                                  TEST_DEFAULT_TEMP_DECI_C);
      ASSERT_EQ(result, RESULT_OK);
      result = test_dsd.interface.process(&test_dsd.interface, true);
      ASSERT_EQ(result, RESULT_OK);
      mock_time._current_time_ms += COMMON_1K_FACTOR;
   }

   // Transition to RING_ABSENT.
   result = mock_weight_sensor_set_weight_data(
      &mock_weight, drift_weight_mg, drift_stddev_mg, mock_time._current_time_ms, false, TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, false);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;

   // RING_ABSENT: gather drift_d1 and drift_d2 with stable drift samples.
   for(uint8_t count = 0u; count < 35u; count++)
   {
      result = mock_weight_sensor_set_weight_data(
         &mock_weight, drift_weight_mg, drift_stddev_mg, mock_time._current_time_ms, false, TEST_DEFAULT_TEMP_DECI_C);
      ASSERT_EQ(result, RESULT_OK);
      result = test_dsd.interface.process(&test_dsd.interface, false);
      ASSERT_EQ(result, RESULT_OK);
      mock_time._current_time_ms += 100u;
   }

   // Transition to RING_REPLACED.
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               post_stddev_mg,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   mock_time._current_time_ms += 100u;

   // RING_REPLACED: obtain stable post-dose measurement.
   for(uint8_t count = 0u; count < 35u; count++)
   {
      result = mock_weight_sensor_set_weight_data(&mock_weight,
                                                  TEST_RING_REPLACED_WEIGHT_MG,
                                                  post_stddev_mg,
                                                  mock_time._current_time_ms,
                                                  true,
                                                  TEST_DEFAULT_TEMP_DECI_C);
      ASSERT_EQ(result, RESULT_OK);
      result = test_dsd.interface.process(&test_dsd.interface, true);
      ASSERT_EQ(result, RESULT_OK);
      mock_time._current_time_ms += 100u;
   }

   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               post_stddev_mg,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   ASSERT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_READY);

   dose_size_data_out_t data_out = {0};
   result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, &data_out);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(data_out.dose_size_mg, expected_dose_size_mg);
   EXPECT_EQ(data_out.total_dispensed_mg, expected_dose_size_mg);
   EXPECT_EQ(data_out.sigma_total_mg, expected_sigma_total_mg);

   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_NO_DATA);
   EXPECT_EQ(test_dsd._dose_size_data_bad_reason, DSD_DATA_BAD_REASON_NONE);
}

/**
 * @brief Regression test: Verify that once data is consumed with `fetch_dose_size_data`, it will not immediately be
 * available again after a call to `process`.
 */
TEST_F(DoseSizeDetectionTestSuite, dsd_process_after_fetch_does_not_output_more_data_test)
{
   result_t result = RESULT_OK;

   drive_to_ring_replaced_state(&test_dsd, &mock_weight, &mock_time);

   mock_time._current_time_ms += (T_SETTLE_RING_S * COMMON_1K_FACTOR) + 1000u;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_REPLACED_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_dsd._statemachine.current_state, DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED);
   ASSERT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_READY);

   dose_size_data_out_t first_fetch_out = {0};
   result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, &first_fetch_out);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_NO_DATA);

   const uint32_t total_dispensed_after_first_fetch = first_fetch_out.total_dispensed_mg;

   mock_time._current_time_ms += COMMON_1K_FACTOR;
   result = mock_weight_sensor_set_weight_data(&mock_weight,
                                               TEST_RING_PRESENT_WEIGHT_MG,
                                               TEST_STABLE_STDDEV_MIN_MG,
                                               mock_time._current_time_ms,
                                               true,
                                               TEST_DEFAULT_TEMP_DECI_C);
   ASSERT_EQ(result, RESULT_OK);
   result = test_dsd.interface.process(&test_dsd.interface, true);
   ASSERT_EQ(result, RESULT_OK);

   dose_size_data_out_t second_fetch_out = {0};
   second_fetch_out.dose_size_mg = 1111u;
   second_fetch_out.total_dispensed_mg = 2222u;
   second_fetch_out.sigma_total_mg = 333u;

   result = test_dsd.interface.fetch_dose_size_data(&test_dsd.interface, &second_fetch_out);
   ASSERT_TRUE(IS_ERR(result));
   EXPECT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SIZE_DETECTION);
   EXPECT_EQ(GET_ERR_CODE(result), DOSE_SIZE_DETECTION_ERROR_NO_DATA);
   EXPECT_EQ(second_fetch_out.dose_size_mg, 1111u);
   EXPECT_EQ(second_fetch_out.total_dispensed_mg, 2222u);
   EXPECT_EQ(second_fetch_out.sigma_total_mg, 333u);
   EXPECT_EQ(test_dsd._dose_size_data_state, DSD_DATA_STATE_NO_DATA);
   EXPECT_EQ(test_dsd._last_total_dispensed_weight_mg, total_dispensed_after_first_fetch);
}
