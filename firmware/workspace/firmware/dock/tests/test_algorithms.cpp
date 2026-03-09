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
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
// Moving average for temperature measurements
static int16_t m_temperature_ma_buffer[MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE];
static int32_t m_temperature_ma_sum = 0;  // running sum of samples
static uint16_t m_temperature_ma_idx = 0; // ring-buffer write index
static uint16_t m_temperature_ma_cnt = 0; // #samples collected (<= MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE)
static uint16_t m_temperature_ma_len = MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE;
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
