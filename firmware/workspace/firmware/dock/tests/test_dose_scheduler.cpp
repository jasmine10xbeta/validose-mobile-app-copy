/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_scheduler_test.cpp
 * @brief Unit tests for the dose scheduler module
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <gtest/gtest.h>

extern "C"
{
#include "common.h"
#include "dose_scheduler.h"
#include "mock/rtc_mock/rtc_mock.h"
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @brief Macro to convert hours and minutes to total minutes since midnight
 *
 * @param hour Hour of the day (0-23)
 * @param minute Minute of the hour (0-59)
 *
 * @return Total minutes since midnight
 */
#define TIME_TO_MINUTES(hour, minute) ((uint16_t)((hour) * 60u + (minute)))

/* Default parameters for dose schedules */
#define DEFAULT_MEDICATION_TYPE              (MEDICATION_TYPE_UNDEFINED)
#define DEFAULT_VALID_DOSAGE_MG              (500u)
#define DEFAULT_TEMP_UPPER_LIMIT_DEG_C       (25)
#define DEFAULT_TEMP_LOWER_LIMIT_DEG_C       (0)
#define DEFAULT_TEMP_AVG_WINDOW_SEC          (300u)
#define DEFAULT_DOSE_DAYS_BITFIELD           (0b10000000) // Daily dosing
#define DEFAULT_DOSE_WINDOW_DURATION_MINUTES (30u)
#define DEFAULT_DOSE_WINDOW_START_TIME       TIME_TO_MINUTES(12, 0)

/* Default system time values */
#define DEFAULT_SYS_TIME    (sys_time_t){0u} // 1 January 2000, Saturday, 00:00:00
#define DEFAULT_EPOCH       (946684800u)     // Unix timestamp for 1 January 2000, 00:00:00
#define DEFAULT_DAY_OF_WEEK (5u)             // Saturday. Day of week for 1 January 2000

#define SKIP_DAYS_BITFIELD_MASK (0x7Fu) // 6-bit maximum for skip days

/* Default dose schedule values used for testing instances. Reduces boilerplate in defining dose schedule instances. */
#define DEFAULT_DOSE_SCHEDULE                                                                                          \
   {                                                                                                                   \
      .medication_type = DEFAULT_MEDICATION_TYPE, .dosage_mg = DEFAULT_VALID_DOSAGE_MG,                                \
      .temp_upper_limit_deg_c = (int8_t)DEFAULT_TEMP_UPPER_LIMIT_DEG_C,                                                \
      .temp_lower_limit_deg_c = (int8_t)DEFAULT_TEMP_LOWER_LIMIT_DEG_C,                                                \
      .temp_avg_window_duration_sec = DEFAULT_TEMP_AVG_WINDOW_SEC, .dose_days_bitfield = DEFAULT_DOSE_DAYS_BITFIELD,   \
      .dose_window_duration_minutes = DEFAULT_DOSE_WINDOW_DURATION_MINUTES, .dose_window_count = 1u,                   \
      .dose_window_start_times_minutes                                                                                 \
         = { DEFAULT_DOSE_WINDOW_START_TIME }                                                                          \
   }

// -----------------------------------------------------------------------------
// Valid dose schedules for testing
// -----------------------------------------------------------------------------

/* Dose schedule with a single dose window */
#define SINGLE_WINDOW_SCHEDULE                                                                                         \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule;                                                                                                        \
   })

/* Dose schedule with a single dose window, using skip days mode */
#define SINGLE_WINDOW_SKIP_DAYS_SCHEDULE                                                                               \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_days_bitfield = 0b10000010; /* Dose every 3rd day */                                               \
      schedule;                                                                                                        \
   })

/* Dose schedule with a single dose window, using skip days mode with zero skip days (i.e., daily dosing) */
#define SINGLE_WINDOW_ZERO_SKIP_DAYS_SCHEDULE                                                                          \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_days_bitfield = 0b10000000;                                                                        \
      schedule;                                                                                                        \
   })

/* Dose schedule with a single dose window that crosses midnight, using skip days mode */
#define SINGLE_WINDOW_SKIP_DAYS_ACROSS_MIDNIGHT_SCHEDULE                                                               \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_days_bitfield = 0b10000010; /* Dose every 3rd day */                                               \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(23, 50);                                           \
      schedule;                                                                                                        \
   })

/* Dose schedule with a single dose window, using day of week mode */
#define SINGLE_WINDOW_DAY_OF_WEEK_SCHEDULE                                                                             \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_days_bitfield = 0b01011001; /* Dose on Monday, Thursday, Friday, and Sunday */                     \
      schedule;                                                                                                        \
   })

/* Dose schedule with a single dose window that crosses midnight, using day of week mode */
#define SINGLE_WINDOW_DAY_OF_WEEK_ACROSS_MIDNIGHT_SCHEDULE                                                             \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_days_bitfield = 0b01011001; /* Dose on Monday, Thursday, Friday, and Sunday */                     \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(23, 50);                                           \
      schedule;                                                                                                        \
   })

/* Dose schedule with the maximum number of dose windows */
#define MAX_WINDOWS_SCHEDULE                                                                                           \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_count = DOSE_SCHEDULE_MAX_DOSES_PER_DAY;                                                    \
      for(uint8_t idx = 0u; idx < DOSE_SCHEDULE_MAX_DOSES_PER_DAY; idx++)                                              \
      {                                                                                                                \
         schedule.dose_window_start_times_minutes[idx]                                                                 \
            = (uint16_t)(idx * (DEFAULT_DOSE_WINDOW_DURATION_MINUTES + 1u));                                           \
      }                                                                                                                \
      schedule;                                                                                                        \
   })

#define MINIMUM_DOSAGE_SCHEDULE                                                                                        \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dosage_mg = 1u;                                                                                         \
      schedule;                                                                                                        \
   })

#define MAXIMUM_DOSAGE_SCHEDULE                                                                                        \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dosage_mg = DOSE_SCHEDULE_MAX_DOSAGE_MG;                                                                \
      schedule;                                                                                                        \
   })

#define MAXIMUM_TEMP_UPPER_THRESHOLD_SCHEDULE                                                                          \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_upper_limit_deg_c = (int8_t)DOSE_SCHEDULE_MAX_TEMP_THRESHOLD_DEG_C;                                \
      schedule;                                                                                                        \
   })

#define MINIMUM_TEMP_LOWER_THRESHOLD_SCHEDULE                                                                          \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_lower_limit_deg_c = (int8_t)DOSE_SCHEDULE_MIN_TEMP_THRESHOLD_DEG_C;                                \
      schedule;                                                                                                        \
   })

#define MAXIMUM_TEMP_AVG_WINDOW_SCHEDULE                                                                               \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_avg_window_duration_sec = DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC;                                   \
      schedule;                                                                                                        \
   })

#define MAXIMUM_DOSE_WINDOW_DURATION_SCHEDULE                                                                          \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = DOSE_SCHEDULE_MAX_DOSE_WINDOW_MINUTES;                                   \
      schedule;                                                                                                        \
   })

#define MINIMUM_DOSE_WINDOW_DURATION_SCHEDULE                                                                          \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 1u;                                                                      \
      schedule;                                                                                                        \
   })

// -----------------------------------------------------------------------------
// Invalid dose schedules for testing
// -----------------------------------------------------------------------------

/* Dose schedule with a zero dosage (invalid) */
#define ZERO_DOSAGE_SCHEDULE                                                                                           \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dosage_mg = 0u;                                                                                         \
      schedule;                                                                                                        \
   })

/* Dose schedule with an excessive dosage (invalid) */
#define EXCESSIVE_DOSAGE_SCHEDULE                                                                                      \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dosage_mg = DOSE_SCHEDULE_MAX_DOSAGE_MG + 1u;                                                           \
      schedule;                                                                                                        \
   })

/* Dose schedule with an excessive upper temperature threshold (invalid) */
#define EXCESSIVE_TEMP_UPPER_THRESHOLD_SCHEDULE                                                                        \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_upper_limit_deg_c = (int8_t)DOSE_SCHEDULE_MAX_TEMP_THRESHOLD_DEG_C + 1;                            \
      schedule;                                                                                                        \
   })

/* Dose schedule with an excessive lower temperature threshold (invalid) */
#define EXCESSIVE_TEMP_LOWER_THRESHOLD_SCHEDULE                                                                        \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_lower_limit_deg_c = (int8_t)DOSE_SCHEDULE_MIN_TEMP_THRESHOLD_DEG_C - 1;                            \
      schedule;                                                                                                        \
   })

/* Dose schedule with invalid temperature threshold order (lower >= upper) */
#define INVALID_TEMP_THRESHOLD_ORDER_SCHEDULE                                                                          \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_upper_limit_deg_c = 10;                                                                            \
      schedule.temp_lower_limit_deg_c = 15;                                                                            \
      schedule;                                                                                                        \
   })

/* Dose schedule with an excessive temperature averaging window (invalid) */
#define EXCESSIVE_TEMP_AVG_WINDOW_SCHEDULE                                                                             \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.temp_avg_window_duration_sec = DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC + 1u;                              \
      schedule;                                                                                                        \
   })

/* Dose schedule with an excessive dose window duration (invalid) */
#define EXCESSIVE_DOSE_WINDOW_DURATION_SCHEDULE                                                                        \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = DOSE_SCHEDULE_MAX_DOSE_WINDOW_MINUTES + 1u;                              \
      schedule;                                                                                                        \
   })

/* Dose schedule with a zero dose window duration (invalid) */
#define ZERO_DOSE_WINDOW_DURATION_SCHEDULE                                                                             \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 0u;                                                                      \
      schedule;                                                                                                        \
   })

/* Dose schedule with a zero dose window count (invalid) */
#define ZERO_DOSE_WINDOW_COUNT_SCHEDULE                                                                                \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_count = 0u;                                                                                 \
      schedule;                                                                                                        \
   })

/* Dose schedule with an excessive dose window count (invalid) */
#define EXCESSIVE_DOSE_WINDOW_COUNT_SCHEDULE                                                                           \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_count = DOSE_SCHEDULE_MAX_DOSES_PER_DAY + 1u;                                               \
      schedule;                                                                                                        \
   })

/* Dose schedule with an invalid dose window start time (invalid) */
#define INVALID_DOSE_WINDOW_START_SCHEDULE                                                                             \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_start_times_minutes[0] = MINUTES_PER_DAY;                                                   \
      schedule;                                                                                                        \
   })

/* Dose schedule with overlapping dose windows (invalid) */
#define OVERLAPPING_DOSE_WINDOWS_SCHEDULE                                                                              \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 30u;                                                                     \
      schedule.dose_window_count = 2u;                                                                                 \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(8, 0);                                             \
      schedule.dose_window_start_times_minutes[1] = TIME_TO_MINUTES(8, 29);                                            \
      schedule;                                                                                                        \
   })

/* Dose schedule with overlapping dose windows across midnight (invalid) */
#define OVERLAPPING_DOSE_WINDOWS_ACROSS_MIDNIGHT_SCHEDULE                                                              \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 30u;                                                                     \
      schedule.dose_window_count = 2u;                                                                                 \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(0, 0);                                             \
      schedule.dose_window_start_times_minutes[1] = TIME_TO_MINUTES(23, 31);                                           \
      schedule;                                                                                                        \
   })

/* Dose schedule with adjacent dose windows (invalid) */
#define ADJACENT_DOSE_WINDOWS_SCHEDULE                                                                                 \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 30u;                                                                     \
      schedule.dose_window_count = 2u;                                                                                 \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(8, 0);                                             \
      schedule.dose_window_start_times_minutes[1] = TIME_TO_MINUTES(8, 30);                                            \
      schedule;                                                                                                        \
   })

/* Dose schedule with adjacent dose windows across midnight (invalid) */
#define ADJACENT_DOSE_WINDOWS_ACROSS_MIDNIGHT_SCHEDULE                                                                 \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 30u;                                                                     \
      schedule.dose_window_count = 2u;                                                                                 \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(0, 0);                                             \
      schedule.dose_window_start_times_minutes[1] = TIME_TO_MINUTES(23, 30);                                           \
      schedule;                                                                                                        \
   })

/* Dose schedule with out-of-order dose windows (invalid) */
#define OUT_OF_ORDER_DOSE_WINDOWS_SCHEDULE                                                                             \
   ({                                                                                                                  \
      dose_schedule_t schedule = DEFAULT_DOSE_SCHEDULE;                                                                \
      schedule.dose_window_duration_minutes = 30u;                                                                     \
      schedule.dose_window_count = 3u;                                                                                 \
      schedule.dose_window_start_times_minutes[0] = TIME_TO_MINUTES(14, 0);                                            \
      schedule.dose_window_start_times_minutes[1] = TIME_TO_MINUTES(8, 0);                                             \
      schedule.dose_window_start_times_minutes[2] = TIME_TO_MINUTES(20, 0);                                            \
      schedule;                                                                                                        \
   })

/***********************************************************************************************************************
 * Private Variables
 **********************************************************************************************************************/

static const dose_schedule_t EMPTY_SCHEDULE = {0};

/***********************************************************************************************************************
 * Helper Functions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Test Fixtures
 **********************************************************************************************************************/

class DoseSchedulerTestSuite: public testing::Test
{
protected:
   rtc_system_time_t mock_rtc = {0};
   dose_scheduler_t test_dose_scheduler = {0};

   void SetUp() override
   {
      result_t result = mock_rtc_system_time_init(&mock_rtc);
      ASSERT_EQ(result, RESULT_OK);

      result = dose_scheduler_init(&test_dose_scheduler, &mock_rtc.interface);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_TRUE(test_dose_scheduler._is_initialized);
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
 * This test ensures that the dose scheduler module initializes correctly with valid parameters.
 */
TEST_F(DoseSchedulerTestSuite, initialization_test_happy_path)
{
   ASSERT_EQ(test_dose_scheduler.interface.parent, &test_dose_scheduler);
   ASSERT_NE(test_dose_scheduler.interface.load_schedule, nullptr);
   ASSERT_NE(test_dose_scheduler.interface.check_dose_window, nullptr);
   ASSERT_NE(test_dose_scheduler.interface.close_dose_window, nullptr);

   ASSERT_EQ(test_dose_scheduler._rtc_interface, &mock_rtc.interface);

   ASSERT_EQ(0, memcmp(&test_dose_scheduler._schedule, &EMPTY_SCHEDULE, sizeof(dose_schedule_t)));
   ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, 0u);
   ASSERT_FALSE(test_dose_scheduler._dose_schedule_loaded);
   ASSERT_EQ(test_dose_scheduler._dose_pending_for_window, true);

   ASSERT_TRUE(test_dose_scheduler._is_initialized);
}

/**
 * @brief Test initialization with invalid parameters
 *
 * This test ensures that the dose scheduler module handles invalid parameters correctly during initialization.
 */
TEST_F(DoseSchedulerTestSuite, initialization_test_invalid_parameters)
{
   // Test null self pointer
   result_t result = dose_scheduler_init(NULL, &mock_rtc.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);

   // Test null RTC interface
   result = dose_scheduler_init(&test_dose_scheduler, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);

   // Test uninitialized RTC interface
   rtc_system_time_t uninit_rtc = {0};
   result = dose_scheduler_init(&test_dose_scheduler, &uninit_rtc.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);
}

/**
 * @brief Test initialization of a "dirty" instance
 *
 * This test ensures that initializing an already initialized instance resets its state correctly.
 */
TEST_F(DoseSchedulerTestSuite, initialization_test_dirty_instance)
{
   // Dirty the instance
   test_dose_scheduler._is_initialized = true;
   test_dose_scheduler._start_date_unix_seconds = 123456789u;
   test_dose_scheduler._dose_schedule_loaded = true;
   test_dose_scheduler._dose_pending_for_window = false;
   memset(&test_dose_scheduler._schedule, 0xFF, sizeof(dose_schedule_t));

   // Re-initialize the instance
   result_t result = dose_scheduler_init(&test_dose_scheduler, &mock_rtc.interface);
   ASSERT_EQ(result, RESULT_OK);

   ASSERT_EQ(test_dose_scheduler.interface.parent, &test_dose_scheduler);
   ASSERT_NE(test_dose_scheduler.interface.load_schedule, nullptr);
   ASSERT_NE(test_dose_scheduler.interface.check_dose_window, nullptr);
   ASSERT_NE(test_dose_scheduler.interface.close_dose_window, nullptr);

   ASSERT_EQ(test_dose_scheduler._rtc_interface, &mock_rtc.interface);

   ASSERT_EQ(0, memcmp(&test_dose_scheduler._schedule, &EMPTY_SCHEDULE, sizeof(dose_schedule_t)));
   ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, 0u);
   ASSERT_FALSE(test_dose_scheduler._dose_schedule_loaded);
   ASSERT_EQ(test_dose_scheduler._dose_pending_for_window, true);

   ASSERT_TRUE(test_dose_scheduler._is_initialized);
}

/**
 * @brief Test `validate_dose_schedule` with valid dose schedules
 *
 * This test ensures that the `validate_dose_schedule` function correctly identifies valid dose schedules.
 */
TEST_F(DoseSchedulerTestSuite, validate_dose_schedule_valid_schedules)
{
   const dose_schedule_t test_schedules[] = {
      SINGLE_WINDOW_SCHEDULE,
      SINGLE_WINDOW_SKIP_DAYS_SCHEDULE,
      SINGLE_WINDOW_ZERO_SKIP_DAYS_SCHEDULE,
      SINGLE_WINDOW_SKIP_DAYS_ACROSS_MIDNIGHT_SCHEDULE,
      SINGLE_WINDOW_DAY_OF_WEEK_SCHEDULE,
      SINGLE_WINDOW_DAY_OF_WEEK_ACROSS_MIDNIGHT_SCHEDULE,
      MAX_WINDOWS_SCHEDULE,
      MINIMUM_DOSAGE_SCHEDULE,
      MAXIMUM_DOSAGE_SCHEDULE,
      MAXIMUM_TEMP_UPPER_THRESHOLD_SCHEDULE,
      MINIMUM_TEMP_LOWER_THRESHOLD_SCHEDULE,
      MAXIMUM_TEMP_AVG_WINDOW_SCHEDULE,
      MAXIMUM_DOSE_WINDOW_DURATION_SCHEDULE,
      MINIMUM_DOSE_WINDOW_DURATION_SCHEDULE,
   };

   for(uint8_t idx = 0u; idx < ARRAY_LEN(test_schedules); idx++)
   {
      SCOPED_TRACE("Test case index: " + std::to_string(idx));

      result_t result = validate_dose_schedule(&test_schedules[idx]);
      ASSERT_EQ(result, RESULT_OK);
   }
}

/**
 * @brief Test `validate_dose_schedule` with invalid dose schedules
 *
 * This test ensures that the `validate_dose_schedule` function correctly identifies invalid dose schedules.
 */
TEST_F(DoseSchedulerTestSuite, validate_dose_schedule_invalid_schedules)
{
   const struct
   {
      dose_schedule_t schedule;
      uint8_t expected_error_code;
   } test_cases[] = {
      {ZERO_DOSAGE_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSAGE},
      {EXCESSIVE_DOSAGE_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSAGE},
      {EXCESSIVE_TEMP_UPPER_THRESHOLD_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD},
      {EXCESSIVE_TEMP_LOWER_THRESHOLD_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD},
      {INVALID_TEMP_THRESHOLD_ORDER_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD},
      {EXCESSIVE_TEMP_AVG_WINDOW_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_AVG_WINDOW},
      {EXCESSIVE_DOSE_WINDOW_DURATION_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION},
      {ZERO_DOSE_WINDOW_DURATION_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION},
      {ZERO_DOSE_WINDOW_COUNT_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT},
      {EXCESSIVE_DOSE_WINDOW_COUNT_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT},
      {INVALID_DOSE_WINDOW_START_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_WINDOW_START},
      {OVERLAPPING_DOSE_WINDOWS_SCHEDULE, DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS},
      {OVERLAPPING_DOSE_WINDOWS_ACROSS_MIDNIGHT_SCHEDULE, DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS},
      {ADJACENT_DOSE_WINDOWS_SCHEDULE, DOSE_SCHEDULER_ERROR_ADJACENT_WINDOWS},
      {ADJACENT_DOSE_WINDOWS_ACROSS_MIDNIGHT_SCHEDULE, DOSE_SCHEDULER_ERROR_ADJACENT_WINDOWS},
      {OUT_OF_ORDER_DOSE_WINDOWS_SCHEDULE, DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS},
   };

   for(uint8_t idx = 0u; idx < ARRAY_LEN(test_cases); idx++)
   {
      SCOPED_TRACE("Test case index: " + std::to_string(idx));

      result_t result = validate_dose_schedule(&test_cases[idx].schedule);
      ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
      ASSERT_EQ(GET_ERR_CODE(result), (int)test_cases[idx].expected_error_code);
   }
}

/**
 * @brief Test `load_schedule` method happy path with valid dose schedules
 *
 * This test ensures that the `load_schedule` method functions correctly with valid dose schedules.
 */
TEST_F(DoseSchedulerTestSuite, load_dose_schedule_valid_schedules)
{
   const dose_schedule_t test_schedules[] = {
      SINGLE_WINDOW_SCHEDULE,
      SINGLE_WINDOW_SKIP_DAYS_SCHEDULE,
      SINGLE_WINDOW_ZERO_SKIP_DAYS_SCHEDULE,
      SINGLE_WINDOW_SKIP_DAYS_ACROSS_MIDNIGHT_SCHEDULE,
      SINGLE_WINDOW_DAY_OF_WEEK_SCHEDULE,
      SINGLE_WINDOW_DAY_OF_WEEK_ACROSS_MIDNIGHT_SCHEDULE,
      MAX_WINDOWS_SCHEDULE,
      MINIMUM_DOSAGE_SCHEDULE,
      MAXIMUM_DOSAGE_SCHEDULE,
      MAXIMUM_TEMP_UPPER_THRESHOLD_SCHEDULE,
      MINIMUM_TEMP_LOWER_THRESHOLD_SCHEDULE,
      MAXIMUM_TEMP_AVG_WINDOW_SCHEDULE,
      MAXIMUM_DOSE_WINDOW_DURATION_SCHEDULE,
      MINIMUM_DOSE_WINDOW_DURATION_SCHEDULE,
   };

   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   for(uint8_t idx = 0u; idx < ARRAY_LEN(test_schedules); idx++)
   {
      SCOPED_TRACE("Test case index: " + std::to_string(idx));

      test_dose_scheduler._schedule = EMPTY_SCHEDULE;
      test_dose_scheduler._start_date_unix_seconds = 0u;
      test_dose_scheduler._dose_schedule_loaded = false;

      result_t result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &test_schedules[idx], DEFAULT_EPOCH);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(0, memcmp(&test_dose_scheduler._schedule, &test_schedules[idx], sizeof(dose_schedule_t)));
      ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, DEFAULT_EPOCH);
      ASSERT_TRUE(test_dose_scheduler._dose_schedule_loaded);
   }
}

/**
 * @brief Test `load_schedule` method with invalid dose schedules
 *
 * This test ensures that the `load_schedule` method correctly identifies and rejects invalid dose schedules.
 */
TEST_F(DoseSchedulerTestSuite, load_dose_schedule_invalid_schedules)
{
   const struct
   {
      dose_schedule_t schedule;
      uint8_t expected_error_code;
   } test_cases[] = {
      {ZERO_DOSAGE_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSAGE},
      {EXCESSIVE_DOSAGE_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSAGE},
      {EXCESSIVE_TEMP_UPPER_THRESHOLD_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD},
      {EXCESSIVE_TEMP_LOWER_THRESHOLD_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD},
      {INVALID_TEMP_THRESHOLD_ORDER_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD},
      {EXCESSIVE_TEMP_AVG_WINDOW_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_TEMP_AVG_WINDOW},
      {EXCESSIVE_DOSE_WINDOW_DURATION_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION},
      {ZERO_DOSE_WINDOW_DURATION_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION},
      {ZERO_DOSE_WINDOW_COUNT_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT},
      {EXCESSIVE_DOSE_WINDOW_COUNT_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT},
      {INVALID_DOSE_WINDOW_START_SCHEDULE, DOSE_SCHEDULER_ERROR_INVALID_WINDOW_START},
      {OVERLAPPING_DOSE_WINDOWS_SCHEDULE, DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS},
      {OVERLAPPING_DOSE_WINDOWS_ACROSS_MIDNIGHT_SCHEDULE, DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS},
      {ADJACENT_DOSE_WINDOWS_SCHEDULE, DOSE_SCHEDULER_ERROR_ADJACENT_WINDOWS},
      {ADJACENT_DOSE_WINDOWS_ACROSS_MIDNIGHT_SCHEDULE, DOSE_SCHEDULER_ERROR_ADJACENT_WINDOWS},
      {OUT_OF_ORDER_DOSE_WINDOWS_SCHEDULE, DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS},
   };

   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   for(uint8_t idx = 0u; idx < ARRAY_LEN(test_cases); idx++)
   {
      SCOPED_TRACE("Test case index: " + std::to_string(idx));

      result_t result
         = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &test_cases[idx].schedule, DEFAULT_EPOCH);
      ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
      ASSERT_EQ(GET_ERR_CODE(result), (int)test_cases[idx].expected_error_code);
   }
}

/**
 * @brief Test `load_schedule` method correctly stored start date
 *
 * This test ensures that the `load_schedule` method correctly stores the provided start date. It should strip out the
 * time component.
 */
TEST_F(DoseSchedulerTestSuite, load_dose_schedule_correct_start_date)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   dose_schedule_t valid_schedule = SINGLE_WINDOW_SCHEDULE;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Test with a start timestamp that includes time component
   uint32_t start_date_with_time = DEFAULT_EPOCH + (TIME_TO_MINUTES(12, 34) * SECONDS_PER_MINUTE);

   result_t result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &valid_schedule, start_date_with_time);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, DEFAULT_EPOCH);
   ASSERT_TRUE(test_dose_scheduler._dose_schedule_loaded);

   valid_schedule = SINGLE_WINDOW_SKIP_DAYS_SCHEDULE;

   result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &valid_schedule, start_date_with_time);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, DEFAULT_EPOCH);
   ASSERT_TRUE(test_dose_scheduler._dose_schedule_loaded);
}

/**
 * @brief Test `load_schedule` method erases the previous schedule when passed an invalid schedule
 *
 * This test ensures that when an invalid schedule is loaded, the previous valid schedule is erased.
 */
TEST_F(DoseSchedulerTestSuite, load_dose_schedule_invalid_erases_previous)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   dose_schedule_t valid_schedule = SINGLE_WINDOW_SCHEDULE;
   dose_schedule_t invalid_schedule = ZERO_DOSAGE_SCHEDULE;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Load a valid schedule first
   result_t result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &valid_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(0, memcmp(&test_dose_scheduler._schedule, &valid_schedule, sizeof(dose_schedule_t)));

   // Now load an invalid schedule
   result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &invalid_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_INVALID_DOSAGE);

   // Verify that the previous schedule has been erased
   ASSERT_EQ(0, memcmp(&test_dose_scheduler._schedule, &EMPTY_SCHEDULE, sizeof(dose_schedule_t)));
   ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, 0u);
   ASSERT_FALSE(test_dose_scheduler._dose_schedule_loaded);
}

/**
 * @brief Test `load_schedule` method with invalid parameters
 *
 * This test ensures that the `load_schedule` method handles invalid parameters correctly.
 */
TEST_F(DoseSchedulerTestSuite, load_dose_schedule_invalid_parameters)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   const dose_schedule_t valid_schedule = SINGLE_WINDOW_SCHEDULE;

   // Test null interface
   result_t result = p_dose_scheduler_ifc->load_schedule(NULL, &valid_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);

   // Test null parent pointer
   p_dose_scheduler_ifc->parent = NULL;
   result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &valid_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);
   p_dose_scheduler_ifc->parent = &test_dose_scheduler;

   // Test uninitialized instance
   test_dose_scheduler._is_initialized = false;
   result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &valid_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_UNINITIALIZED);
   test_dose_scheduler._is_initialized = true;

   // Test null schedule pointer
   result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, NULL, DEFAULT_EPOCH);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);

   // Test start timestamp in the future (whole day ahead needed)
   // Note: Must erase the previous valid schedule
   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;
   uint32_t future_timestamp = DEFAULT_EPOCH + SECONDS_PER_DAY;

   test_dose_scheduler._schedule = SINGLE_WINDOW_SCHEDULE;
   test_dose_scheduler._start_date_unix_seconds = DEFAULT_EPOCH;
   test_dose_scheduler._dose_schedule_loaded = true;

   result = p_dose_scheduler_ifc->load_schedule(p_dose_scheduler_ifc, &valid_schedule, future_timestamp);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_INVALID_TIMESTAMP);
   ASSERT_EQ(0, memcmp(&test_dose_scheduler._schedule, &EMPTY_SCHEDULE, sizeof(dose_schedule_t)));
   ASSERT_EQ(test_dose_scheduler._start_date_unix_seconds, 0u);
   ASSERT_FALSE(test_dose_scheduler._dose_schedule_loaded);
}

/**
 * @brief Test `check_dose_window` method with no schedule loaded
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_without_loaded_schedule)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   // Ensure no schedule is loaded
   test_dose_scheduler._dose_schedule_loaded = false;

   bool is_dose_window_open = true;
   result_t result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);

   ASSERT_EQ(result, RESULT_OK);
   ASSERT_FALSE(is_dose_window_open);
}

/**
 * @brief Test `close_dose_window` method happy path
 *
 * This test ensures that the `close_dose_window` method functions correctly under normal conditions.
 */
TEST_F(DoseSchedulerTestSuite, close_dose_window_happy_path)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   test_dose_scheduler._dose_pending_for_window = true;

   result_t result = p_dose_scheduler_ifc->close_dose_window(p_dose_scheduler_ifc);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_FALSE(test_dose_scheduler._dose_pending_for_window);
}

/**
 * @brief Test `close_dose_window` method with invalid parameters
 *
 * This test ensures that the `close_dose_window` method handles invalid parameters correctly.
 */
TEST_F(DoseSchedulerTestSuite, close_dose_window_invalid_parameters)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   // Test null interface
   result_t result = p_dose_scheduler_ifc->close_dose_window(NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);
   ASSERT_TRUE(test_dose_scheduler._dose_pending_for_window);

   // Test null parent pointer
   p_dose_scheduler_ifc->parent = NULL;
   result = p_dose_scheduler_ifc->close_dose_window(p_dose_scheduler_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);
   ASSERT_TRUE(test_dose_scheduler._dose_pending_for_window);
   p_dose_scheduler_ifc->parent = &test_dose_scheduler;

   // Test uninitialized instance
   test_dose_scheduler._is_initialized = false;
   result = p_dose_scheduler_ifc->close_dose_window(p_dose_scheduler_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_UNINITIALIZED);
   ASSERT_TRUE(test_dose_scheduler._dose_pending_for_window);
}

/**
 * @brief Test `check_dose_window` method with skip days scheduling
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_skip_days)
{
   const dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   const dose_schedule_t test_schedule = SINGLE_WINDOW_SKIP_DAYS_SCHEDULE;
   const uint8_t skip_days = test_schedule.dose_days_bitfield & SKIP_DAYS_BITFIELD_MASK;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Load a valid schedule
   result_t result
      = test_dose_scheduler.interface.load_schedule(&test_dose_scheduler.interface, &test_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(result, RESULT_OK);

   // Test over the maximum possible skip days
   for(uint8_t day = 0u; day < (SKIP_DAYS_BITFIELD_MASK + 1u); day++)
   {
      SCOPED_TRACE("Day offset: " + std::to_string(day));

      // Determine if the dose window should be open based on the skip days bitfield
      bool should_be_open = (day % (skip_days + 1u) == 0u);

      // Set time to the start of the dose window on the given day
      mock_rtc._epoch = DEFAULT_EPOCH + (day * SECONDS_PER_DAY) + DEFAULT_DOSE_WINDOW_START_TIME * SECONDS_PER_MINUTE;

      // Check dose window
      bool is_dose_window_open = false;
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(is_dose_window_open, should_be_open);

      // Set time to the end of the dose window on the given day
      mock_rtc._epoch += DEFAULT_DOSE_WINDOW_DURATION_MINUTES * SECONDS_PER_MINUTE;

      // Check the dose window (should be closed now)
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_FALSE(is_dose_window_open);
   }
}

/**
 * @brief Test `check_dose_window` method with zero skip days scheduling
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_zero_skip_days)
{
   const dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   const dose_schedule_t test_schedule = SINGLE_WINDOW_ZERO_SKIP_DAYS_SCHEDULE;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Load a valid schedule
   result_t result
      = test_dose_scheduler.interface.load_schedule(&test_dose_scheduler.interface, &test_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(result, RESULT_OK);

   // Test over the maximum possible skip days
   for(uint8_t day = 0u; day < (SKIP_DAYS_BITFIELD_MASK + 1u); day++)
   {
      SCOPED_TRACE("Day offset: " + std::to_string(day));

      // Set time to the start of the dose window on the given day
      mock_rtc._epoch = DEFAULT_EPOCH + (day * SECONDS_PER_DAY) + DEFAULT_DOSE_WINDOW_START_TIME * SECONDS_PER_MINUTE;

      // Check dose window
      bool is_dose_window_open = false;
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_TRUE(is_dose_window_open);

      // Set time to the end of the dose window on the given day
      mock_rtc._epoch += DEFAULT_DOSE_WINDOW_DURATION_MINUTES * SECONDS_PER_MINUTE;

      // Check the dose window (should be closed now)
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_FALSE(is_dose_window_open);
   }
}

/**
 * @brief Test `check_dose_window` method with skip days scheduling and a window across midnight
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_skip_days_across_midnight)
{
   const dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   const dose_schedule_t test_schedule = SINGLE_WINDOW_SKIP_DAYS_ACROSS_MIDNIGHT_SCHEDULE;
   const uint8_t skip_days = test_schedule.dose_days_bitfield & SKIP_DAYS_BITFIELD_MASK;
   const uint16_t dose_window_start_minutes = test_schedule.dose_window_start_times_minutes[0];
   const uint16_t dose_window_end_minutes
      = (dose_window_start_minutes + test_schedule.dose_window_duration_minutes) % MINUTES_PER_DAY;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Load a valid schedule
   result_t result
      = test_dose_scheduler.interface.load_schedule(&test_dose_scheduler.interface, &test_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(result, RESULT_OK);

   // Test over the maximum possible skip days
   for(uint8_t day = 0u; day < (SKIP_DAYS_BITFIELD_MASK + 1u); day++)
   {
      SCOPED_TRACE("Day offset: " + std::to_string(day));

      // Determine if the dose window should be open based on the skip days bitfield
      bool is_dose_day = (day % (skip_days + 1u) == 0u);

      SCOPED_TRACE("Is dose day: " + std::to_string(is_dose_day));

      // Set time to the start of the dose window on the given day
      mock_rtc._epoch = DEFAULT_EPOCH + (day * SECONDS_PER_DAY) + (dose_window_start_minutes * SECONDS_PER_MINUTE);

      // Check dose window - should be open if it's a dose day
      bool is_dose_window_open = false;
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(is_dose_window_open, is_dose_day);

      // Set time to one second before the end of the dose window (next day)
      mock_rtc._epoch
         = DEFAULT_EPOCH + ((day + 1u) * SECONDS_PER_DAY) + (dose_window_end_minutes * SECONDS_PER_MINUTE) - 1u;

      // Check dose window - should still be open if it was a dose day
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(is_dose_window_open, is_dose_day);

      // Set time to the end of the dose window (next day)
      mock_rtc._epoch = DEFAULT_EPOCH + ((day + 1u) * SECONDS_PER_DAY) + (dose_window_end_minutes * SECONDS_PER_MINUTE);

      // Check dose window - should be closed now
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_FALSE(is_dose_window_open);
   }
}

/**
 * @brief Test `check_dose_window` method with days of week scheduling
 *
 * This test ensures that the `check_dose_window` method functions correctly with schedules that specify days of the
 * week.
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_days_of_week)
{
   const dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   const dose_schedule_t test_schedule = SINGLE_WINDOW_DAY_OF_WEEK_SCHEDULE;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Load a valid schedule
   result_t result
      = test_dose_scheduler.interface.load_schedule(&test_dose_scheduler.interface, &test_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(result, RESULT_OK);

   // Test over 14 days to cover two full weeks
   for(uint8_t day = 0u; day < 14u; day++)
   {
      SCOPED_TRACE("Day offset: " + std::to_string(day));

      // Determine the day of the week (0=Monday, 1=Tuesday, ..., 6=Sunday)
      uint8_t day_of_week = (day + DEFAULT_DAY_OF_WEEK) % 7u; // Adjusting for default start day
      bool should_be_open = (0u != (test_schedule.dose_days_bitfield & (1u << day_of_week)));

      // Set time to the start of the dose window on the given day
      mock_rtc._epoch = DEFAULT_EPOCH + (day * SECONDS_PER_DAY) + (DEFAULT_DOSE_WINDOW_START_TIME * SECONDS_PER_MINUTE);

      // Check dose window
      bool is_dose_window_open = false;
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);

      ASSERT_EQ(is_dose_window_open, should_be_open);

      // Set time to the end of the dose window on the given day
      mock_rtc._epoch += (DEFAULT_DOSE_WINDOW_DURATION_MINUTES * SECONDS_PER_MINUTE);

      // Check the dose window (should be closed now)
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_FALSE(is_dose_window_open);
   }
}

/**
 * @brief Test `check_dose_window` method with days of week scheduling and a window across midnight
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_days_of_week_across_midnight)
{
   const dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   const dose_schedule_t test_schedule = SINGLE_WINDOW_DAY_OF_WEEK_ACROSS_MIDNIGHT_SCHEDULE;
   const uint16_t dose_window_start_minutes = test_schedule.dose_window_start_times_minutes[0];
   const uint16_t dose_window_end_minutes
      = (dose_window_start_minutes + test_schedule.dose_window_duration_minutes) % MINUTES_PER_DAY;

   mock_rtc._epoch = DEFAULT_EPOCH;
   mock_rtc._system_time = DEFAULT_SYS_TIME;

   // Load a valid schedule
   result_t result
      = test_dose_scheduler.interface.load_schedule(&test_dose_scheduler.interface, &test_schedule, DEFAULT_EPOCH);
   ASSERT_EQ(result, RESULT_OK);

   // Test over 14 days to cover two full weeks
   for(uint8_t day = 0u; day < 14u; day++)
   {
      // Determine the day of the week (0=Monday, 1=Tuesday, ..., 6=Sunday)
      uint8_t day_of_week = (day + DEFAULT_DAY_OF_WEEK) % 7u; // Adjusting for default start day
      bool is_dose_day = (0u != (test_schedule.dose_days_bitfield & (1u << day_of_week)));

      SCOPED_TRACE("Day offset: " + std::to_string(day));
      SCOPED_TRACE("Day of week: " + std::to_string(day_of_week));
      SCOPED_TRACE("Is dose day: " + std::to_string(is_dose_day));

      // Set time to the start of the dose window on the given day
      mock_rtc._epoch = DEFAULT_EPOCH + (day * SECONDS_PER_DAY) + (dose_window_start_minutes * SECONDS_PER_MINUTE);

      // Check dose window - should be open if it's a dose day
      bool is_dose_window_open = false;
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(is_dose_window_open, is_dose_day);

      // Set time to one second before the end of the dose window (next day)
      mock_rtc._epoch
         = DEFAULT_EPOCH + ((day + 1u) * SECONDS_PER_DAY) + (dose_window_end_minutes * SECONDS_PER_MINUTE) - 1u;

      // Check dose window - should still be open if it was a dose day
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(is_dose_window_open, is_dose_day);

      // Set time to the end of the dose window (next day)
      mock_rtc._epoch = DEFAULT_EPOCH + ((day + 1u) * SECONDS_PER_DAY) + (dose_window_end_minutes * SECONDS_PER_MINUTE);

      // Check dose window - should be closed now
      result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_FALSE(is_dose_window_open);
   }
}

/**
 * @brief Test `check_dose_window` method with invalid parameters
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_invalid_parameters)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   bool is_dose_window_open = false;

   // Test null interface
   result_t result = p_dose_scheduler_ifc->check_dose_window(NULL, &is_dose_window_open);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);

   // Test null parent pointer
   p_dose_scheduler_ifc->parent = NULL;
   result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);
   p_dose_scheduler_ifc->parent = &test_dose_scheduler;

   // Test uninitialized instance
   test_dose_scheduler._is_initialized = false;
   result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_UNINITIALIZED);
   test_dose_scheduler._is_initialized = true;

   // Test null output pointer
   result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_NULL);
}

/**
 * @brief Test `check_dose_window` handles zero window count in internal schedule safely
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_zero_window_count_internal_schedule)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   // Create an internal malformed schedule that would have previously underflowed at count-1.
   test_dose_scheduler._schedule = SINGLE_WINDOW_SKIP_DAYS_SCHEDULE;
   test_dose_scheduler._schedule.dose_window_count = 0u;
   test_dose_scheduler._start_date_unix_seconds = DEFAULT_EPOCH;
   test_dose_scheduler._dose_schedule_loaded = true;
   mock_rtc._epoch = DEFAULT_EPOCH + SECONDS_PER_DAY; // day after start

   bool is_dose_window_open = true;
   result_t result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT);
}

/**
 * @brief Test `check_dose_window` rejects internal schedules with excessive window count
 */
TEST_F(DoseSchedulerTestSuite, check_dose_window_excessive_window_count_internal_schedule)
{
   dose_scheduler_interface_t *p_dose_scheduler_ifc = &test_dose_scheduler.interface;

   test_dose_scheduler._schedule = SINGLE_WINDOW_SCHEDULE;
   test_dose_scheduler._schedule.dose_window_count = DOSE_SCHEDULE_MAX_DOSES_PER_DAY + 1u;
   test_dose_scheduler._dose_schedule_loaded = true;
   mock_rtc._epoch = DEFAULT_EPOCH;

   bool is_dose_window_open = false;
   result_t result = p_dose_scheduler_ifc->check_dose_window(p_dose_scheduler_ifc, &is_dose_window_open);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_DOSE_SCHEDULER_DOCK);
   ASSERT_EQ(GET_ERR_CODE(result), DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT);
}
