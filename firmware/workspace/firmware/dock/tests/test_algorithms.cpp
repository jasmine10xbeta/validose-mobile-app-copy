/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @brief Test File for testing general algorithms
 * This file contains unit tests for various algorithms. It is a sandbox and does not directly test production code.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <cstdio>
#include <gtest/gtest.h>
#include <map>

extern "C"
{
#include "common.h"
#include "debug.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define TEMPERATURE_UPDATE_INTERVAL_MS (10000u) // NOTE: Should be multiples of 1000ms
#define MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE                                                                         \
   (uint16_t(DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC / (TEMPERATURE_UPDATE_INTERVAL_MS / 1000u)))

// Copied constants from general_control.c for cap-off timeout logic.
#define MEDICATION_CAP_OFF_MAX_TIME_S (10u * 60u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
// Test-local copy of status_update_t used by handle_medication_cap_off_for_too_long() in production.
typedef struct status_update
{
   ring_status_t ring_status;
   uint32_t update_time_unix_s;
} status_update_t;

// Minimal test-local notification definitions required by the copied function.
typedef enum
{
   NOTIFICATION_EVENT_ERROR = 0u
} NOTIFICATION_EVENT;

struct test_notification_interface;
typedef struct test_notification_interface test_notification_interface_t;

struct test_notification_interface
{
   void *parent;
   result_t (*set_notification_event)(const test_notification_interface_t *const interface,
                                      NOTIFICATION_EVENT notification_event);
};

typedef struct
{
   test_notification_interface_t interface;
   result_t set_notification_result;
   uint32_t set_notification_call_count;
   NOTIFICATION_EVENT last_notification_event;
} test_hmi_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
// Moving average for temperature measurements
static int16_t m_temperature_ma_buffer[MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE];
static int32_t m_temperature_ma_sum = 0;  // running sum of samples
static uint16_t m_temperature_ma_idx = 0; // ring-buffer write index
static uint16_t m_temperature_ma_cnt = 0; // #samples collected (<= MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE)
static uint16_t m_temperature_ma_len = MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE;

// Needed by debug macros used in copied function.
static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_GENERAL_CONTROL_DOCK;

// Test-local stand-in for the notification module instance used by the copied function.
static test_hmi_t m_hmi = {0};
/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/**
 * @brief Change the active moving-average window length.
 *
 * @param len  Desired window length (0 … MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE).
 *             Values > MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE clamp to MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE.
 *
 * Behaviour:
 *  - When shrinking the window, the sum is recomputed so that only the
 *    newest @p len samples remain in the calculation.
 *  - When growing the window, existing history is retained; new slots
 *    start empty until filled by subsequent calls to moving_average_add().
 *  - Setting @p len to 0 clears all history and disables averaging.
 */
static void moving_average_set_length(uint16_t len)
{
   // Clamp to legal range
   if(len > MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE)
   {
      len = MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE;
   }

   // Handle special case: disable averaging
   if(0 == len)
   {
      m_temperature_ma_len = 0;
      m_temperature_ma_cnt = 0;
      m_temperature_ma_sum = 0;
      m_temperature_ma_idx = 0;
      return;
   }

   // If window grows, no immediate action is needed
   if(len >= m_temperature_ma_cnt)
   {
      m_temperature_ma_len = len;
      return;
   }

   // Window shrinks: recompute sum of newest <len> samples
   if(len < m_temperature_ma_cnt) /* window is shrinking */
   {
      uint16_t prev_window = m_temperature_ma_len;
      int32_t new_sum = 0;
      uint16_t oldest_pos = 0; // will hold index of oldest kept

      for(uint16_t idx = 0; idx < len; idx++)
      {
         int16_t pos = (int16_t)(m_temperature_ma_idx - 1u - idx);
         if(pos < 0)
         {
            pos += (int16_t)prev_window;
         }

         new_sum += m_temperature_ma_buffer[(uint16_t)pos];

         if(idx == len - 1) // last iteration → oldest sample
         {
            oldest_pos = (uint16_t)pos;
         }
      }

      m_temperature_ma_sum = new_sum;
      m_temperature_ma_cnt = len;
      m_temperature_ma_len = len;
      m_temperature_ma_idx = oldest_pos; // point to next slot to overwrite
      return;
   }

   // If the new window is smaller than the previous index, reset pointer
   if(m_temperature_ma_idx >= m_temperature_ma_len)
   {
      m_temperature_ma_idx = 0;
   }
}

/**
 * @brief Fixed-window moving average with runtime-selectable window length The data buffer is allocated once at
 * build-time (MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE). At runtime you may shrink or grow the active window length with
 * @p moving_average_set_length(). If the window length is set to **0**, @p moving_average_add() becomes a pure
 * pass-through and simply echoes the incoming sample.
 *
 * @note All arithmetic is integer-only.
 *
 * @param sample New input value.
 *
 * @return uint16_t Current average (truncated toward zero).
 */
static int16_t moving_average_add(int16_t sample)
{
   // Pass-through mode
   if(0 == m_temperature_ma_len)
   {
      return sample;
   }

   // Subtract outgoing value once buffer is full
   if(m_temperature_ma_cnt == m_temperature_ma_len)
   {
      m_temperature_ma_sum -= m_temperature_ma_buffer[m_temperature_ma_idx];
   }
   else
   {
      m_temperature_ma_cnt++; // still filling
   }

   // Insert new sample
   m_temperature_ma_buffer[m_temperature_ma_idx] = sample;
   m_temperature_ma_sum += sample;

   // Advance circular index
   m_temperature_ma_idx++;
   if(m_temperature_ma_idx >= m_temperature_ma_len) // wrap inside active window
   {
      m_temperature_ma_idx = 0;
   }

   return (int16_t)(m_temperature_ma_sum / m_temperature_ma_cnt);
}

/**
 * @brief Helper that resets the averaging logic to a known state.
 *
 * Calling @c moving_average_set_length(0) clears history and disables the
 * filter; this is sufficient for test isolation because all static state is
 * reset in that path.
 */
static void MA_Reset()
{
   moving_average_set_length(0); // clears buffer & variables
}

static result_t test_set_notification_event(const test_notification_interface_t *const interface,
                                            NOTIFICATION_EVENT notification_event)
{
   if((NULL == interface) || (NULL == interface->parent))
   {
      return RESULT_THIS_UNIT_ERROR(1u);
   }

   test_hmi_t *self = (test_hmi_t *)interface->parent;
   self->set_notification_call_count++;
   self->last_notification_event = notification_event;

   return self->set_notification_result;
}

static status_update_t make_status_update(uint32_t update_time_s, uint8_t cap_detection_status)
{
   status_update_t status_update = {0};
   status_update.update_time_unix_s = update_time_s;
   status_update.ring_status.cap_detection_status = cap_detection_status;
   return status_update;
}

/**
 * @brief Copy of the production function under test.
 *
 * This copy intentionally keeps the same static-state behavior and core logic as production code so that unit tests
 * can validate timing semantics deterministically.
 */
static void handle_medication_cap_off_for_too_long(const status_update_t *status_update)
{
   if(NULL == status_update)
   {
      DEBUG_ERROR("NULL paramater received.");
      return;
   }

   static uint32_t last_processed_timestamp_s = 0u;
   static uint32_t cap_open_start_timestamp_s = 0u;
   static bool has_reported_cap_off_too_long = false;

   uint32_t sample_timestamp_s = status_update->update_time_unix_s; // Timestamp when current status was received
   bool should_process_sample = true;
   bool should_check_cap_open_duration = false;

   if((0u == sample_timestamp_s) || (sample_timestamp_s == last_processed_timestamp_s))
   {
      // Ignore stale samples: only react to newly received timestamps.
      should_process_sample = false;
   }

   if(should_process_sample)
   {
      if(sample_timestamp_s < last_processed_timestamp_s)
      {
         // Update timestamp restarted or moved backwards. Reset local tracking and continue from this sample.
         cap_open_start_timestamp_s = 0u;
         has_reported_cap_off_too_long = false;
      }

      last_processed_timestamp_s = sample_timestamp_s;

      const bool is_cap_open = (CAP_STATE_OPEN == status_update->ring_status.cap_detection_status);
      if(!is_cap_open)
      {
         cap_open_start_timestamp_s = 0u;
         has_reported_cap_off_too_long = false;
      }
      else if(0u == cap_open_start_timestamp_s)
      {
         cap_open_start_timestamp_s = sample_timestamp_s;
      }
      else
      {
         should_check_cap_open_duration = true;
      }
   }

   if(should_check_cap_open_duration)
   {
      const uint32_t cap_open_duration_s = sample_timestamp_s - cap_open_start_timestamp_s;
      if((cap_open_duration_s > MEDICATION_CAP_OFF_MAX_TIME_S) && !has_reported_cap_off_too_long)
      {
         DEBUG_ERROR("Medication cap open too long. Duration: %lus, Threshold: %lus.",
                     (unsigned long)cap_open_duration_s,
                     (unsigned long)MEDICATION_CAP_OFF_MAX_TIME_S);

         result_t result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to set HMI notification. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         if(IS_OK(result))
         {
            has_reported_cap_off_too_long = true;
         }
      }
   }
}

/**
 * @brief Reset the copied function + mock HMI to a deterministic baseline for each test.
 *
 * The function under test has internal static state. Since this is a direct copy, we reset that state indirectly by
 * feeding a known "cap closed" sample at a low timestamp and then clear mock counters.
 */
static void cap_off_test_reset()
{
   // Always re-bind interface callbacks in case a test changed them.
   m_hmi.interface.parent = &m_hmi;
   m_hmi.interface.set_notification_event = test_set_notification_event;
   m_hmi.set_notification_result = RESULT_OK;
   m_hmi.set_notification_call_count = 0u;
   m_hmi.last_notification_event = (NOTIFICATION_EVENT)255u;

   // Drive static algorithm state to known values:
   // - cap closed => timer/latch reset
   // - monotonic timestamps => deterministic baseline for stale/new calculations
   status_update_t seed_sample = make_status_update(1u, CAP_STATE_CLOSED); // 2 == CAP_STATE_CLOSED in ring firmware
   handle_medication_cap_off_for_too_long(&seed_sample);
   seed_sample.update_time_unix_s = 2u;
   handle_medication_cap_off_for_too_long(&seed_sample);

   // Remove setup side effects from assertions.
   m_hmi.set_notification_call_count = 0u;
   m_hmi.last_notification_event = (NOTIFICATION_EVENT)255u;
}
/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
class generalAlgorithmTestSuit: public ::testing::Test
{
protected:
   void SetUp() override
   {
   }

   void TearDown() override
   {
   }
};

/**
 * @test Pass‑through behaviour when window == 0.
 */

TEST_F(generalAlgorithmTestSuit, PassThroughMode)
{
   MA_Reset();
   moving_average_set_length(0);

   EXPECT_EQ(moving_average_add(42), 42u);
   EXPECT_EQ(moving_average_add(INT16_MAX), INT16_MAX);
   EXPECT_EQ(moving_average_add(0), 0u);
}

/**
 * @test Basic averaging while filling the buffer.
 */
TEST_F(generalAlgorithmTestSuit, BasicFillAndAverage)
{
   MA_Reset();
   moving_average_set_length(4);

   EXPECT_EQ(moving_average_add(1), 1u); // avg = 1 / 1
   EXPECT_EQ(moving_average_add(2), 1u); // floor((1+2)/2) = 1
   EXPECT_EQ(moving_average_add(3), 2u); // (1+2+3)/3 = 2
   EXPECT_EQ(moving_average_add(4), 2u); // (1+2+3+4)/4 = 2.5 -> 2

   // Sliding window: buffer now holds {1,2,3,4}
   // Add 5 => buffer {2,3,4,5} => (2+3+4+5)/4 = 3.5 -> 3
   EXPECT_EQ(moving_average_add(5), 3u);
}

/**
 * @test Shrinking the window length discards oldest samples correctly.
 */
TEST_F(generalAlgorithmTestSuit, ShrinkWindowRecomputesSum)
{
   MA_Reset();
   moving_average_set_length(5);

   const uint16_t samples[5] = {10, 20, 30, 40, 50};
   for(uint16_t s: samples)
   {
      moving_average_add(s);
   }
   // Current average: (10+20+30+40+50)/5 = 30
   EXPECT_EQ(moving_average_add(60), 40u); // buffer {20..60} = 200/5 = 40

   // Shrink to last 2 samples (50,60) => avg 55
   moving_average_set_length(2);
   EXPECT_EQ(moving_average_add(70), 65u); // buffer {60,70} => 130/2 = 65 -> 65
}

class medicationCapOffTimeoutTestSuite: public ::testing::Test
{
protected:
   void SetUp() override
   {
      cap_off_test_reset();
   }

   void TearDown() override
   {
      cap_off_test_reset();
   }
};

/**
 * @test Guard clause validation.
 *
 * Intent:
 * - Verify the function safely handles a NULL pointer and does not attempt to notify HMI.
 *
 * Expected outcome:
 * - No notification callback invocation.
 */
TEST_F(medicationCapOffTimeoutTestSuite, NullInputDoesNotNotify)
{
   handle_medication_cap_off_for_too_long(NULL);
   EXPECT_EQ(m_hmi.set_notification_call_count, 0u);
}

/**
 * @test Stale-sample suppression + exact threshold edge behavior.
 *
 * Intent:
 * - Prove that repeated stale samples (same timestamp) do not progress timeout logic.
 * - Prove threshold condition is strictly "greater than 10 minutes" (not >=).
 *
 * Sequence:
 * 1. First OPEN sample starts the timer at t=100.
 * 2. Thousands of stale repeats at t=100 should not trigger.
 * 3. Fresh OPEN sample at t=700 is exactly +600s (10 min) => should NOT trigger.
 * 4. Fresh OPEN sample at t=701 is +601s (>10 min) => should trigger once.
 * 5. Additional fresh OPEN samples should not retrigger because latch is set.
 */
TEST_F(medicationCapOffTimeoutTestSuite, StaleSamplesIgnoredAndThresholdIsStrictlyGreaterThan)
{
   status_update_t sample = make_status_update(100u, CAP_STATE_OPEN);
   handle_medication_cap_off_for_too_long(&sample); // Starts timer

   for(uint32_t idx = 0u; idx < 2000u; idx++)
   {
      handle_medication_cap_off_for_too_long(&sample); // Same timestamp => stale => ignored
   }
   EXPECT_EQ(m_hmi.set_notification_call_count, 0u);

   sample.update_time_unix_s = 700u; // Exactly 600s elapsed
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 0u);

   sample.update_time_unix_s = 701u; // 601s elapsed => timeout exceeded
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);
   EXPECT_EQ(m_hmi.last_notification_event, NOTIFICATION_EVENT_ERROR);

   sample.update_time_unix_s = 702u; // Latch should prevent duplicate notification
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);
}

/**
 * @test Cap-close reset behavior.
 *
 * Intent:
 * - Confirm that when cap transitions away from OPEN, timer and "already-notified" latch are reset.
 * - Confirm a second independent OPEN-too-long interval triggers a second notification.
 *
 * Expected outcome:
 * - One notify in first interval.
 * - After CLOSE sample, next interval can notify again once it exceeds threshold.
 */
TEST_F(medicationCapOffTimeoutTestSuite, CloseSampleResetsTimerAndAllowsFutureNotification)
{
   status_update_t sample = make_status_update(100u, CAP_STATE_OPEN);
   handle_medication_cap_off_for_too_long(&sample); // Start first interval

   sample.update_time_unix_s = 701u;
   handle_medication_cap_off_for_too_long(&sample); // First timeout
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);

   sample.update_time_unix_s = 702u;
   sample.ring_status.cap_detection_status = CAP_STATE_CLOSED; // Closed
   handle_medication_cap_off_for_too_long(&sample);            // Reset interval + latch
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);

   sample.update_time_unix_s = 703u;
   sample.ring_status.cap_detection_status = CAP_STATE_OPEN;
   handle_medication_cap_off_for_too_long(&sample); // Start second interval

   sample.update_time_unix_s = 1303u; // Exactly +600s, still should not trigger
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);

   sample.update_time_unix_s = 1304u; // +601s => second timeout
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 2u);
}

/**
 * @test HMI failure retry behavior.
 *
 * Intent:
 * - Validate that notification latch is only set when HMI call succeeds.
 * - If HMI returns error, function should retry on later fresh samples while cap is still OPEN.
 *
 * Expected outcome:
 * - Each fresh sample after timeout attempts notification until success.
 * - After success, retries stop.
 */
TEST_F(medicationCapOffTimeoutTestSuite, FailedNotificationRetriesUntilSuccess)
{
   m_hmi.set_notification_result = RESULT_THIS_UNIT_ERROR(7u); // Force failure

   status_update_t sample = make_status_update(100u, CAP_STATE_OPEN);
   handle_medication_cap_off_for_too_long(&sample); // Start interval

   sample.update_time_unix_s = 701u;
   handle_medication_cap_off_for_too_long(&sample); // Attempt #1 (fails)
   sample.update_time_unix_s = 702u;
   handle_medication_cap_off_for_too_long(&sample); // Attempt #2 (fails)
   EXPECT_EQ(m_hmi.set_notification_call_count, 2u);

   m_hmi.set_notification_result = RESULT_OK;
   sample.update_time_unix_s = 703u;
   handle_medication_cap_off_for_too_long(&sample); // Attempt #3 succeeds and latches
   EXPECT_EQ(m_hmi.set_notification_call_count, 3u);

   sample.update_time_unix_s = 704u;
   handle_medication_cap_off_for_too_long(&sample); // Should not retry after success
   EXPECT_EQ(m_hmi.set_notification_call_count, 3u);
}

/**
 * @test Non-monotonic timestamp recovery.
 *
 * Intent:
 * - Validate robustness when update_time_unix_s moves backwards (clock reset or rollover-like event).
 * - Function should reset local timeout tracking and treat the backward sample as a new baseline.
 *
 * Expected outcome:
 * - Existing latch is cleared by rollback path.
 * - A fresh >10 minute OPEN interval after rollback triggers again.
 */
TEST_F(medicationCapOffTimeoutTestSuite, BackwardTimestampResetsTrackingAndRearmsTimeout)
{
   status_update_t sample = make_status_update(100u, CAP_STATE_OPEN);
   handle_medication_cap_off_for_too_long(&sample); // Start interval

   sample.update_time_unix_s = 701u;
   handle_medication_cap_off_for_too_long(&sample); // First timeout
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);

   sample.update_time_unix_s = 10u; // Backward timestamp => reset internal state
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 1u);

   sample.update_time_unix_s = 611u; // 601s since reset baseline (t=10) => should notify again
   handle_medication_cap_off_for_too_long(&sample);
   EXPECT_EQ(m_hmi.set_notification_call_count, 2u);
}
