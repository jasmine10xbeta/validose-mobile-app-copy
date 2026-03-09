#include <cstdio>
#include <gtest/gtest.h>

extern "C"
{
#include "common.h"

#include "system_time.h"
#include "system_time_interface.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define INITIAL_VALUE     (0u)
#define INC_RESULT_1MS    (1u)
#define INC_RESULT_10MS   (10u)
#define INC_RESULT_100MS  (100u)
#define INC_RESULT_1000MS (1000u)

#define INC_RESULT_SET_VAL (1234u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
class SystemTimeTestSuit: public testing::Test
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
 * @brief Tests the initialization of the system time module
 *
 * This function initializes the module and verifies that the initialization was successful.
 * It then checks that the initial values are set correctly
 *
 * @return None
 */
TEST_F(SystemTimeTestSuit, system_time_init_test)
{
   result_t result = RESULT_OK;

   system_time_t time = {0};

   result = system_time_init(&time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INITIAL_VALUE);
}

/**
 * @brief Tests the increment functions of the system time module
 *
 * This function initializes the module and verifies that the increment functions work correctly
 * This is confirmed by checking the internal variable directly and also through the interface function
 *
 * @return None
 */
TEST_F(SystemTimeTestSuit, system_time_increment_test)
{
   result_t result = RESULT_OK;

   system_time_t time = {0};
   uint64_t current_time = INITIAL_VALUE;

   result = system_time_init(&time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INITIAL_VALUE);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INITIAL_VALUE);

   result = time.interface.inc_time_by_1_ms(&time.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INC_RESULT_1MS);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_1MS);

   time._current_time_ms = INITIAL_VALUE;
   result = time.interface.inc_time_by_10_ms(&time.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INC_RESULT_10MS);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_10MS);

   time._current_time_ms = INITIAL_VALUE;
   result = time.interface.inc_time_by_100_ms(&time.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INC_RESULT_100MS);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_100MS);

   time._current_time_ms = INITIAL_VALUE;
   result = time.interface.inc_time_by_1000_ms(&time.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INC_RESULT_1000MS);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_1000MS);

   time._current_time_ms = INITIAL_VALUE;
   result = time.interface.inc_time_by_set_val_ms(&time.interface, INC_RESULT_SET_VAL);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time._current_time_ms, INC_RESULT_SET_VAL);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_SET_VAL);
}

/**
 * @brief Tests the ability of the system time module to handle and report overflow
 *
 * @return None
 */
TEST_F(SystemTimeTestSuit, system_time_overflow_test)
{
   result_t result = RESULT_OK;

   system_time_t time = {0};
   uint64_t current_time = 0;

   result = system_time_init(&time);
   ASSERT_TRUE(IS_OK(result));

   time._current_time_ms = UINT64_MAX;
   result = time.interface.inc_time_by_1_ms(&time.interface);
   ASSERT_TRUE(IS_ERR(result));
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(time._current_time_ms, INC_RESULT_1MS - 1);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_1MS - 1);

   time._current_time_ms = UINT64_MAX;
   result = time.interface.inc_time_by_10_ms(&time.interface);
   ASSERT_TRUE(IS_ERR(result));
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(time._current_time_ms, INC_RESULT_10MS - 1);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_10MS - 1);

   time._current_time_ms = UINT64_MAX;
   result = time.interface.inc_time_by_100_ms(&time.interface);
   ASSERT_TRUE(IS_ERR(result));
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(time._current_time_ms, INC_RESULT_100MS - 1);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_100MS - 1);

   time._current_time_ms = UINT64_MAX;
   result = time.interface.inc_time_by_1000_ms(&time.interface);
   ASSERT_TRUE(IS_ERR(result));
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(time._current_time_ms, INC_RESULT_1000MS - 1);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_1000MS - 1);

   time._current_time_ms = UINT64_MAX;
   result = time.interface.inc_time_by_set_val_ms(&time.interface, INC_RESULT_SET_VAL);
   ASSERT_TRUE(IS_ERR(result));
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(time._current_time_ms, INC_RESULT_SET_VAL - 1);
   result = time.interface.get_time_ms(&time.interface, &current_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(current_time, INC_RESULT_SET_VAL - 1);
}