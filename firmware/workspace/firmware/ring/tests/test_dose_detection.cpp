/**
 * @file dose_detection_test.cpp
 * @brief Unit tests for the dose detection module
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <gtest/gtest.h>

extern "C"
{
#include "common.h"
#include "dose_detection.h"
#include "queue_interface.h"

#include "mock/cap_detection/cap_detection.h"
#include "mock/system_time/system_time.h"
#include "mock/tilt_detection/tilt_detection.h"

#include <stdint.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define MAKE_RESULT(unit, code) ((result_t)(((unit & 0xFF) << 8) | (code & 0xFF)))

#define DEFAULT_DOSE_START_TIME_MS (1000u)
#define DEFAULT_DOSE_DURATION_MS   (5000u)
#define DEFAULT_TILT_COUNT         (1u)

#define DEFAULT_DOSE_EVENT                                                                                             \
   (dose_detection_event_t)                                                                                            \
   {                                                                                                                   \
      .dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                                                \
      .dose_end_time_ms = DEFAULT_DOSE_START_TIME_MS + DEFAULT_DOSE_DURATION_MS, .tilt_count = DEFAULT_TILT_COUNT,     \
      .state = DOSE_EVENT_STATE_CAP_CLOSED_WITH_TILT                                                                   \
   }

#define MAKE_TILT(_time_ms_, _duration_ms_)                                                                            \
   (tilt_data_t)                                                                                                       \
   {                                                                                                                   \
      .detected_at_time_ms = (_time_ms_), .duration_ms = (_duration_ms_)                                               \
   }

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Input parameters for a process test case
 */
typedef struct
{
   DOSE_DETECTION_STATE initial_state;  /**< Initial state of the dose detection FSM */
   uint16_t initial_tilt_count;         /**< Initial tilt count */
   uint64_t initial_dose_start_time_ms; /**< Initial dose start time */

   CAP_STATE cap_state;                         /**< Mock cap state to be set */
   uint16_t tilt_count;                         /**< Mock tilt count to be set */
   tilt_data_t tilts[TILT_DETECTION_MAX_TILTS]; /**< Mock tilts to be set */
   uint64_t current_time_ms;                    /**< Mock current system time in milliseconds */
} process_test_input_t;

/**
 * @brief Expected output parameters for a process test case
 */
typedef struct
{
   DOSE_DETECTION_STATE state;  /**< Expected state of the dose detection FSM after processing */
   uint64_t dose_start_time_ms; /**< Expected dose start time after processing */
   uint16_t tilt_count;         /**< Expected tilt count after processing */

   bool is_dose_event_available;               /**< Whether a dose event is expected to be available */
   dose_detection_event_t expected_dose_event; /**< Expected dose event to be recorded (if any) */
} process_test_output_t;

/**
 * @brief Test case for the `process` method
 */
typedef struct
{
   const char *name;                      /**< Name of the test case */
   process_test_input_t input;            /**< Input parameters for the test case */
   process_test_output_t expected_output; /**< Expected output parameters for the test case */
} process_test_case_t;

/***********************************************************************************************************************
 * Process Test Cases
 **********************************************************************************************************************/
// Note: for conciseness, unimportant fields are left out, and thus will be zero-initialized

#define TEST_CASE_IDLE_CAP_CLOSED                                                                                      \
   ((process_test_case_t){.name = "IDLE remains IDLE on cap closed",                                                   \
                          .input = {.initial_state = DOSE_DETECTION_STATE_IDLE, .cap_state = CAP_STATE_CLOSED},        \
                          .expected_output = {.state = DOSE_DETECTION_STATE_IDLE}})

#define TEST_CASE_IDLE_CAP_UNKNOWN                                                                                     \
   ((process_test_case_t){.name = "IDLE remains IDLE on cap unknown",                                                  \
                          .input = {.initial_state = DOSE_DETECTION_STATE_IDLE, .cap_state = CAP_STATE_UNKNOWN},       \
                          .expected_output = {.state = DOSE_DETECTION_STATE_IDLE}})

#define TEST_CASE_IDLE_TO_PRIMED_ON_CAP_OPEN                                                                           \
   ((process_test_case_t){.name = "IDLE to PRIMED on cap open",                                                        \
                          .input = {.initial_state = DOSE_DETECTION_STATE_IDLE,                                        \
                                    .cap_state = CAP_STATE_OPEN,                                                       \
                                    .current_time_ms = DEFAULT_DOSE_START_TIME_MS},                                    \
                          .expected_output                                                                             \
                          = {.state = DOSE_DETECTION_STATE_PRIMED, .dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS}})

#define TEST_CASE_PRIMED_NO_EVENT                                                                                      \
   ((process_test_case_t){.name = "PRIMED remains PRIMED on cap open with no tilt",                                    \
                          .input = {.initial_state = DOSE_DETECTION_STATE_PRIMED, .cap_state = CAP_STATE_OPEN},        \
                          .expected_output = {.state = DOSE_DETECTION_STATE_PRIMED}})

#define TEST_CASE_PRIMED_TO_IDLE_ON_CAP_CLOSED                                                                         \
   ((process_test_case_t){.name = "PRIMED to IDLE on cap closed with no tilt",                                         \
                          .input = {.initial_state = DOSE_DETECTION_STATE_PRIMED, .cap_state = CAP_STATE_CLOSED},      \
                          .expected_output = {.state = DOSE_DETECTION_STATE_IDLE}})

#define TEST_CASE_PRIMED_TO_IDLE_ON_CAP_CLOSED_WITH_TILT                                                               \
   ((process_test_case_t){.name = "PRIMED to IDLE on cap closed with tilt",                                            \
                          .input                                                                                       \
                          = {.initial_state = DOSE_DETECTION_STATE_PRIMED,                                             \
                             .cap_state = CAP_STATE_CLOSED,                                                            \
                             .tilt_count = 1u,                                                                         \
                             .tilts = {MAKE_TILT(DEFAULT_DOSE_START_TIME_MS, TILT_DETECTION_DURATION_THRESHOLD_MS)}},  \
                          .expected_output = {.state = DOSE_DETECTION_STATE_IDLE, .tilt_count = 0u}})

#define TEST_CASE_PRIMED_TO_EVALUATING_ON_TILT                                                                         \
   ((process_test_case_t){.name = "PRIMED to EVALUATING on tilt detected",                                             \
                          .input                                                                                       \
                          = {.initial_state = DOSE_DETECTION_STATE_PRIMED,                                             \
                             .cap_state = CAP_STATE_OPEN,                                                              \
                             .tilt_count = 1u,                                                                         \
                             .tilts = {MAKE_TILT(DEFAULT_DOSE_START_TIME_MS, TILT_DETECTION_DURATION_THRESHOLD_MS)}},  \
                          .expected_output = {.state = DOSE_DETECTION_STATE_EVALUATING, .tilt_count = 1u}})

#define TEST_CASE_PRIMED_TO_EVALUATING_ON_MULTIPLE_TILTS                                                               \
   ((process_test_case_t){                                                                                             \
      .name = "PRIMED to EVALUATING on multiple tilts",                                                                \
      .input = {.initial_state = DOSE_DETECTION_STATE_PRIMED,                                                          \
                .cap_state = CAP_STATE_OPEN,                                                                           \
                .tilt_count = 3u,                                                                                      \
                .tilts = {MAKE_TILT(DEFAULT_DOSE_START_TIME_MS, TILT_DETECTION_DURATION_THRESHOLD_MS),                 \
                          MAKE_TILT(DEFAULT_DOSE_START_TIME_MS + 200u, TILT_DETECTION_DURATION_THRESHOLD_MS),          \
                          MAKE_TILT(DEFAULT_DOSE_START_TIME_MS + 400u, TILT_DETECTION_DURATION_THRESHOLD_MS)}},        \
      .expected_output                                                                                                 \
      = {.state = DOSE_DETECTION_STATE_EVALUATING, .tilt_count = 3u, .is_dose_event_available = false}})

#define TEST_CASE_EVALUATING_CAP_OPEN                                                                                  \
   ((process_test_case_t){.name = "EVALUATING remains EVALUATING on cap open",                                         \
                          .input = {.initial_state = DOSE_DETECTION_STATE_EVALUATING,                                  \
                                    .cap_state = CAP_STATE_OPEN,                                                       \
                                    .tilt_count = 0u,                                                                  \
                                    .tilts = {0}},                                                                     \
                          .expected_output                                                                             \
                          = {.state = DOSE_DETECTION_STATE_EVALUATING, .is_dose_event_available = false}})

#define TEST_CASE_EVALUATING_ADDITIONAL_TILTS                                                                          \
   ((process_test_case_t){                                                                                             \
      .name = "EVALUATING remains EVALUATING on additional tilts",                                                     \
      .input = {.initial_state = DOSE_DETECTION_STATE_EVALUATING,                                                      \
                .initial_tilt_count = 1u,                                                                              \
                .cap_state = CAP_STATE_OPEN,                                                                           \
                .tilt_count = 2u,                                                                                      \
                .tilts = {MAKE_TILT(DEFAULT_DOSE_START_TIME_MS + 1000u, TILT_DETECTION_DURATION_THRESHOLD_MS),         \
                          MAKE_TILT(DEFAULT_DOSE_START_TIME_MS + 2000u, TILT_DETECTION_DURATION_THRESHOLD_MS)}},       \
      .expected_output                                                                                                 \
      = {.state = DOSE_DETECTION_STATE_EVALUATING, .tilt_count = 3u, .is_dose_event_available = false}})

#define TEST_CASE_EVALUATING_TO_DOSE_DETECTED_ON_CAP_CLOSED_AND_DURATION_VALID                                         \
   ((process_test_case_t){                                                                                             \
      .name = "EVALUATING to DOSE_DETECTED on cap closed and duration valid",                                          \
      .input = {.initial_state = DOSE_DETECTION_STATE_EVALUATING,                                                      \
                .initial_tilt_count = 1u,                                                                              \
                .initial_dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                              \
                .cap_state = CAP_STATE_CLOSED,                                                                         \
                .current_time_ms = DEFAULT_DOSE_START_TIME_MS + DOSE_DETECTION_VALID_DOSE_DURATION_MS},                \
      .expected_output = {.state = DOSE_DETECTION_STATE_DOSE_DETECTED,                                                 \
                          .dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                            \
                          .tilt_count = 1u,                                                                            \
                          .is_dose_event_available = true,                                                             \
                          .expected_dose_event                                                                         \
                          = {.dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                         \
                             .dose_end_time_ms = DEFAULT_DOSE_START_TIME_MS + DOSE_DETECTION_VALID_DOSE_DURATION_MS,   \
                             .tilt_count = 1u,                                                                         \
                             .state = DOSE_EVENT_STATE_CAP_CLOSED_WITH_TILT}}})

#define TEST_CASE_EVALUATING_TO_IDLE_ON_CAP_CLOSED_AND_DURATION_INVALID                                                \
   ((process_test_case_t){                                                                                             \
      .name = "EVALUATING to DOSE_DETECTED on cap closed and duration valid",                                          \
      .input = {.initial_state = DOSE_DETECTION_STATE_EVALUATING,                                                      \
                .initial_tilt_count = 1u,                                                                              \
                .initial_dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                              \
                .cap_state = CAP_STATE_CLOSED,                                                                         \
                .current_time_ms = DEFAULT_DOSE_START_TIME_MS + DOSE_DETECTION_VALID_DOSE_DURATION_MS - 1u},           \
      .expected_output = {.state = DOSE_DETECTION_STATE_IDLE, .is_dose_event_available = false}})

#define TEST_CASE_EVALUATION_TO_DOSE_DETECTED_WITH_ADDITIONAL_TILTS                                                    \
   ((process_test_case_t){                                                                                             \
      .name = "EVALUATING to DOSE_DETECTED on cap closed and duration valid with additional tilts",                    \
      .input = {.initial_state = DOSE_DETECTION_STATE_EVALUATING,                                                      \
                .initial_tilt_count = 1u,                                                                              \
                .initial_dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                              \
                .cap_state = CAP_STATE_CLOSED,                                                                         \
                .tilt_count = 2u,                                                                                      \
                .tilts = {MAKE_TILT(DEFAULT_DOSE_START_TIME_MS + 3000u, TILT_DETECTION_DURATION_THRESHOLD_MS),         \
                          MAKE_TILT(DEFAULT_DOSE_START_TIME_MS + 4000u, TILT_DETECTION_DURATION_THRESHOLD_MS)},        \
                .current_time_ms = DEFAULT_DOSE_START_TIME_MS + DOSE_DETECTION_VALID_DOSE_DURATION_MS},                \
      .expected_output = {.state = DOSE_DETECTION_STATE_DOSE_DETECTED,                                                 \
                          .dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                            \
                          .tilt_count = 3u,                                                                            \
                          .is_dose_event_available = true,                                                             \
                          .expected_dose_event                                                                         \
                          = {.dose_start_time_ms = DEFAULT_DOSE_START_TIME_MS,                                         \
                             .dose_end_time_ms = DEFAULT_DOSE_START_TIME_MS + DOSE_DETECTION_VALID_DOSE_DURATION_MS,   \
                             .tilt_count = 3u,                                                                         \
                             .state = DOSE_EVENT_STATE_CAP_CLOSED_WITH_TILT}}})

#define TEST_CASE_DOSE_DETECTED_TO_IDLE                                                                                \
   ((process_test_case_t){.name = "DOSE_DETECTED to IDLE",                                                             \
                          .input = {.initial_state = DOSE_DETECTION_STATE_DOSE_DETECTED},                              \
                          .expected_output = {.state = DOSE_DETECTION_STATE_IDLE}})

/***********************************************************************************************************************
 * Private Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Helper Functions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Test Fixtures
 **********************************************************************************************************************/

class DoseDetectionTestSuite: public testing::Test
{
protected:
   tilt_detection_t mock_tilt_detection = {0};
   cap_detection_t mock_cap_detection = {0};
   system_time_t mock_system_time = {0};

   dose_detection_t test_dose_detection = {0};

   void SetUp() override
   {
      result_t result = mock_system_time_init(&mock_system_time);
      ASSERT_EQ(result, RESULT_OK) << "Failed to initialize mock system time module";

      result = mock_cap_detection_init(&mock_cap_detection);
      ASSERT_EQ(result, RESULT_OK) << "Failed to initialize mock cap detection module";

      result = mock_tilt_detection_init(&mock_tilt_detection);
      ASSERT_EQ(result, RESULT_OK) << "Failed to initialize mock tilt detection module";

      result = dose_detection_init(&test_dose_detection,
                                   &mock_system_time.interface,
                                   &mock_cap_detection.interface,
                                   &mock_tilt_detection.interface);
      ASSERT_EQ(result, RESULT_OK) << "Failed to initialize dose detection module";
      // Rest of the assertions are in the test cases
   }

   void TearDown() override
   {
   }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

/**
 * @brief Test initialization happy path
 *
 * This test ensures that the dose detection module initializes correctly with valid parameters.
 */
TEST_F(DoseDetectionTestSuite, initialization_happy_path)
{
   ASSERT_EQ(test_dose_detection._is_initialized, true);
   ASSERT_EQ(test_dose_detection._system_time_interface, &mock_system_time.interface);
   ASSERT_EQ(test_dose_detection._cap_interface, &mock_cap_detection.interface);
   ASSERT_EQ(test_dose_detection._tilt_interface, &mock_tilt_detection.interface);
   ASSERT_EQ(test_dose_detection._dose_start_time, 0u);
   ASSERT_EQ(test_dose_detection._tilt_count, 0u);
}

/**
 * @brief Test initialization with invalid parameters
 *
 * This test verifies that the dose detection module handles invalid parameters correctly during initialization.
 */
TEST_F(DoseDetectionTestSuite, initialization_invalid_parameters)
{
   dose_detection_t invalid_dose_detection = {0};
   system_time_t uninitialized_system_time = {0};
   cap_detection_t uninitialized_cap_detection = {0};
   tilt_detection_t uninitialized_tilt_detection = {0};

   result_t result = dose_detection_init(
      nullptr, &mock_system_time.interface, &mock_cap_detection.interface, &mock_tilt_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);

   result = dose_detection_init(
      &invalid_dose_detection, nullptr, &mock_cap_detection.interface, &mock_tilt_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);

   result = dose_detection_init(&invalid_dose_detection,
                                &uninitialized_system_time.interface,
                                &mock_cap_detection.interface,
                                &mock_tilt_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);

   result = dose_detection_init(
      &invalid_dose_detection, &mock_system_time.interface, nullptr, &mock_tilt_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);

   result = dose_detection_init(&invalid_dose_detection,
                                &mock_system_time.interface,
                                &uninitialized_cap_detection.interface,
                                &mock_tilt_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);

   result = dose_detection_init(
      &invalid_dose_detection, &mock_system_time.interface, &mock_cap_detection.interface, nullptr);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);

   result = dose_detection_init(&invalid_dose_detection,
                                &mock_system_time.interface,
                                &mock_cap_detection.interface,
                                &uninitialized_tilt_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
   ASSERT_EQ(invalid_dose_detection._is_initialized, false);
}

/**
 * @brief Test `clear_dose_events` method happy path
 *
 * This test ensures that the `clear_dose_events` method clears the dose event queue correctly.
 */
TEST_F(DoseDetectionTestSuite, clear_dose_events_happy_path)
{
   // Load dose events into the queue
   dose_detection_event_t dose_event = DEFAULT_DOSE_EVENT;
   queue_interface_t *p_queue = &test_dose_detection._dose_event_queue.interface;
   for(size_t i = 0u; i < MAX_DOSE_EVENTS; i++)
   {
      result_t result = p_queue->enqueue(p_queue, &dose_event);
      ASSERT_EQ(result, RESULT_OK) << "Failed to enqueue dose event into the queue";
   }

   // Clear dose events
   result_t result = test_dose_detection.interface.clear_dose_events(&test_dose_detection.interface);
   ASSERT_EQ(result, RESULT_OK) << "Failed to clear dose events";
   size_t event_count = 0u;
   result = p_queue->get_count(p_queue, &event_count);
   ASSERT_EQ(result, RESULT_OK) << "Failed to get dose event count from the queue";
   ASSERT_EQ(event_count, 0u) << "Dose event queue not cleared properly";
}

/**
 * @brief Test `clear_dose_events` method with invalid parameters
 *
 * This test verifies that the `clear_dose_events` method handles invalid parameters correctly.
 */
TEST_F(DoseDetectionTestSuite, clear_dose_events_invalid_parameters)
{
   // Load dose events into the queue
   dose_detection_event_t dose_event = DEFAULT_DOSE_EVENT;
   queue_interface_t *p_queue = &test_dose_detection._dose_event_queue.interface;
   for(size_t i = 0u; i < MAX_DOSE_EVENTS; i++)
   {
      result_t result = p_queue->enqueue(p_queue, &dose_event);
      ASSERT_EQ(result, RESULT_OK) << "Failed to enqueue dose event into the queue";
   }

   result_t result = test_dose_detection.interface.clear_dose_events(nullptr);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   test_dose_detection._is_initialized = false;
   result = test_dose_detection.interface.clear_dose_events(&test_dose_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NOT_INITIALIZED);
   test_dose_detection._is_initialized = true;

   test_dose_detection.interface.parent = nullptr;
   result = test_dose_detection.interface.clear_dose_events(&test_dose_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   size_t event_count = 0u;
   result = p_queue->get_count(p_queue, &event_count);
   ASSERT_EQ(result, RESULT_OK) << "Failed to get dose event count from the queue";
   ASSERT_EQ(event_count, MAX_DOSE_EVENTS) << "Dose event queue should not be cleared on invalid parameter";
}

/**
 * @brief Test `get_current_state` method happy path
 *
 * This test ensures that the `get_current_state` method retrieves the current state correctly.
 */
TEST_F(DoseDetectionTestSuite, get_current_state_happy_path)
{
   DOSE_DETECTION_STATE states[] = {DOSE_DETECTION_STATE_IDLE,
                                    DOSE_DETECTION_STATE_PRIMED,
                                    DOSE_DETECTION_STATE_EVALUATING,
                                    DOSE_DETECTION_STATE_DOSE_DETECTED};

   for(uint8_t idx = 0u; idx < ARRAY_LEN(states); idx++)
   {
      test_dose_detection._fsm.current_state = (DOSE_DETECTION_STATE)states[idx];
      DOSE_DETECTION_STATE retrieved_state = DOSE_DETECTION_STATE_MAX; // Initialize to invalid state

      result_t result
         = test_dose_detection.interface.get_current_state(&test_dose_detection.interface, &retrieved_state);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(retrieved_state, states[idx]);
   }
}

/**
 * @brief Test `get_current_state` method with invalid parameters
 *
 * This test verifies that the `get_current_state` method handles invalid parameters correctly.
 */
TEST_F(DoseDetectionTestSuite, get_current_state_invalid_parameters)
{
   DOSE_DETECTION_STATE retrieved_state = DOSE_DETECTION_STATE_MAX; // Initialize to invalid state

   result_t result = test_dose_detection.interface.get_current_state(nullptr, &retrieved_state);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   result = test_dose_detection.interface.get_current_state(&test_dose_detection.interface, nullptr);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   test_dose_detection._is_initialized = false;
   result = test_dose_detection.interface.get_current_state(&test_dose_detection.interface, &retrieved_state);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NOT_INITIALIZED);
   test_dose_detection._is_initialized = true;

   test_dose_detection.interface.parent = nullptr;
   result = test_dose_detection.interface.get_current_state(&test_dose_detection.interface, &retrieved_state);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
}

/**
 * @brief Test `is_dose_event_available` method happy path
 */
TEST_F(DoseDetectionTestSuite, is_dose_event_available_happy_path)
{
   bool is_available = false;

   // Initially, no events should be available
   result_t result
      = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, &is_available);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(is_available, false);

   // Enqueue a dose event
   dose_detection_event_t dose_event = DEFAULT_DOSE_EVENT;
   queue_interface_t *p_queue = &test_dose_detection._dose_event_queue.interface;
   result = p_queue->enqueue(p_queue, &dose_event);
   ASSERT_EQ(result, RESULT_OK) << "Failed to enqueue dose event into the queue";

   // Now, an event should be available
   result = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, &is_available);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(is_available, true);
}

/**
 * @brief Test `is_dose_event_available` method with invalid parameters
 */
TEST_F(DoseDetectionTestSuite, is_dose_event_available_invalid_parameters)
{
   bool is_available = false;

   result_t result = test_dose_detection.interface.is_dose_event_available(nullptr, &is_available);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   result = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, nullptr);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   test_dose_detection._is_initialized = false;
   result = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, &is_available);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NOT_INITIALIZED);
   test_dose_detection._is_initialized = true;

   test_dose_detection.interface.parent = nullptr;
   result = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, &is_available);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
}

/**
 * @brief Test `try_get_dose_event` method happy path
 */
TEST_F(DoseDetectionTestSuite, try_get_dose_event_happy_path)
{
   // Enqueue a dose event
   dose_detection_event_t dose_event = DEFAULT_DOSE_EVENT;
   queue_interface_t *p_queue = &test_dose_detection._dose_event_queue.interface;
   result_t result = p_queue->enqueue(p_queue, &dose_event);
   ASSERT_EQ(result, RESULT_OK) << "Failed to enqueue dose event into the queue";

   // Try to get the dose event
   dose_detection_event_t retrieved_event = {0};
   bool event_retrieved = false;
   result = test_dose_detection.interface.try_get_dose_event(
      &test_dose_detection.interface, &retrieved_event, &event_retrieved);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(event_retrieved, true);
   ASSERT_EQ(retrieved_event.dose_start_time_ms, dose_event.dose_start_time_ms);
   ASSERT_EQ(retrieved_event.dose_end_time_ms, dose_event.dose_end_time_ms);
   ASSERT_EQ(retrieved_event.tilt_count, dose_event.tilt_count);
   ASSERT_EQ(retrieved_event.state, dose_event.state);

   // Now the queue should be empty
   result = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, &event_retrieved);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(event_retrieved, false);
}

/**
 * @brief Test `try_get_dose_event` method with invalid parameters
 */
TEST_F(DoseDetectionTestSuite, try_get_dose_event_invalid_parameters)
{
   dose_detection_event_t retrieved_event = {0};
   bool event_retrieved = false;

   result_t result = test_dose_detection.interface.try_get_dose_event(nullptr, &retrieved_event, &event_retrieved);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   result = test_dose_detection.interface.try_get_dose_event(&test_dose_detection.interface, nullptr, &event_retrieved);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   result = test_dose_detection.interface.try_get_dose_event(&test_dose_detection.interface, &retrieved_event, nullptr);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   test_dose_detection._is_initialized = false;
   result = test_dose_detection.interface.try_get_dose_event(
      &test_dose_detection.interface, &retrieved_event, &event_retrieved);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NOT_INITIALIZED);
   test_dose_detection._is_initialized = true;

   test_dose_detection.interface.parent = nullptr;
   result = test_dose_detection.interface.try_get_dose_event(
      &test_dose_detection.interface, &retrieved_event, &event_retrieved);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
}

/**
 * @brief Test `process` method happy path
 *
 * This test verifies that the `process` method correctly processes various input scenarios.
 */
TEST_F(DoseDetectionTestSuite, process_happy_path)
{
   const process_test_case_t test_cases[] = {
      TEST_CASE_IDLE_CAP_CLOSED,
      TEST_CASE_IDLE_CAP_UNKNOWN,
      TEST_CASE_IDLE_TO_PRIMED_ON_CAP_OPEN,
      TEST_CASE_PRIMED_NO_EVENT,
      TEST_CASE_PRIMED_TO_IDLE_ON_CAP_CLOSED,
      TEST_CASE_PRIMED_TO_IDLE_ON_CAP_CLOSED_WITH_TILT,
      TEST_CASE_PRIMED_TO_EVALUATING_ON_TILT,
      TEST_CASE_PRIMED_TO_EVALUATING_ON_MULTIPLE_TILTS,
      TEST_CASE_EVALUATING_CAP_OPEN,
      TEST_CASE_EVALUATING_ADDITIONAL_TILTS,
      TEST_CASE_EVALUATING_TO_DOSE_DETECTED_ON_CAP_CLOSED_AND_DURATION_VALID,
      TEST_CASE_EVALUATING_TO_IDLE_ON_CAP_CLOSED_AND_DURATION_INVALID,
      TEST_CASE_EVALUATION_TO_DOSE_DETECTED_WITH_ADDITIONAL_TILTS,
      TEST_CASE_DOSE_DETECTED_TO_IDLE,
   };

   for(uint8_t idx = 0u; idx < ARRAY_LEN(test_cases); idx++)
   {
      const process_test_case_t *test_case = &test_cases[idx];
      SCOPED_TRACE("Failed on test case: " + std::string(test_case->name));

      // Set initial state
      test_dose_detection._fsm.current_state = test_case->input.initial_state;
      test_dose_detection._tilt_count = test_case->input.initial_tilt_count;
      test_dose_detection._dose_start_time = test_case->input.initial_dose_start_time_ms;

      // Set mocks
      mock_cap_detection._mock_cap_state = test_case->input.cap_state;
      mock_tilt_detection._mock_tilt_count = test_case->input.tilt_count;
      memcpy(&mock_tilt_detection._mock_tilts, test_case->input.tilts, sizeof(mock_tilt_detection._mock_tilts));
      mock_system_time._current_time_ms = test_case->input.current_time_ms;

      // Call process
      result_t result = test_dose_detection.interface.process(&test_dose_detection.interface);

      // Verify result
      EXPECT_EQ(result, RESULT_OK);
      EXPECT_EQ(test_dose_detection._fsm.current_state, test_case->expected_output.state);
      EXPECT_EQ(test_dose_detection._dose_start_time, test_case->expected_output.dose_start_time_ms);
      EXPECT_EQ(test_dose_detection._tilt_count, test_case->expected_output.tilt_count);

      bool is_event_available = false;
      result
         = test_dose_detection.interface.is_dose_event_available(&test_dose_detection.interface, &is_event_available);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(is_event_available, test_case->expected_output.is_dose_event_available);

      if(test_case->expected_output.is_dose_event_available)
      {
         dose_detection_event_t retrieved_event = {0};
         bool event_retrieved = false;
         result = test_dose_detection.interface.try_get_dose_event(
            &test_dose_detection.interface, &retrieved_event, &event_retrieved);
         ASSERT_EQ(result, RESULT_OK);
         ASSERT_EQ(event_retrieved, true);
         ASSERT_EQ(retrieved_event.dose_start_time_ms,
                   test_case->expected_output.expected_dose_event.dose_start_time_ms);
         ASSERT_EQ(retrieved_event.dose_end_time_ms, test_case->expected_output.expected_dose_event.dose_end_time_ms);
         ASSERT_EQ(retrieved_event.tilt_count, test_case->expected_output.expected_dose_event.tilt_count);
         ASSERT_EQ(retrieved_event.state, test_case->expected_output.expected_dose_event.state);
      }
   }
}

/**
 * @brief Test `process` method with invalid parameters
 *
 * This test verifies that the `process` method handles invalid parameters correctly.
 */
TEST_F(DoseDetectionTestSuite, process_invalid_parameters)
{
   result_t result = test_dose_detection.interface.process(nullptr);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);

   test_dose_detection._is_initialized = false;
   result = test_dose_detection.interface.process(&test_dose_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NOT_INITIALIZED);
   test_dose_detection._is_initialized = true;

   test_dose_detection.interface.parent = nullptr;
   result = test_dose_detection.interface.process(&test_dose_detection.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_DETECTION_ERROR_NULL_PTR);
}