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

#define THIS_UNIT_ID SW_UNIT_ID_SYSTEM_TIME // Define unit ID for tagging errors

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
   system_time_t sys_time = {0};
   uint64_t time = INITIAL_VALUE;
   result_t result = RESULT_OK;

   void SetUp() override
   {
   }

   void TearDown() override
   {
   }

   void StandardInit()
   {
      result = system_time_init(&sys_time);
      ASSERT_TRUE(IS_OK(result));
      ASSERT_EQ(sys_time._current_time_ms, INITIAL_VALUE);
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

   system_time_t sys_time = {0};

   result = system_time_init(&sys_time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(sys_time._current_time_ms, INITIAL_VALUE);
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
   StandardInit();

   // Initial
   result = sys_time.interface.get_time_ms(&sys_time.interface, &time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time, INITIAL_VALUE);

   // 1ms
   ASSERT_EQ(RESULT_OK, sys_time.interface.inc_time_by_1_ms(&sys_time.interface));
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_1MS);
   ASSERT_EQ(RESULT_OK, sys_time.interface.get_time_ms(&sys_time.interface, &time));
   ASSERT_EQ(time, INC_RESULT_1MS);

   // 10ms
   sys_time._current_time_ms = INITIAL_VALUE;
   ASSERT_EQ(RESULT_OK, sys_time.interface.inc_time_by_10_ms(&sys_time.interface));
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_10MS);
   ASSERT_EQ(RESULT_OK, sys_time.interface.get_time_ms(&sys_time.interface, &time));
   ASSERT_EQ(time, INC_RESULT_10MS);

   // 100ms
   sys_time._current_time_ms = INITIAL_VALUE;
   ASSERT_EQ(RESULT_OK, sys_time.interface.inc_time_by_100_ms(&sys_time.interface));
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_100MS);
   ASSERT_EQ(RESULT_OK, sys_time.interface.get_time_ms(&sys_time.interface, &time));
   ASSERT_EQ(time, INC_RESULT_100MS);

   // 1000ms
   sys_time._current_time_ms = INITIAL_VALUE;
   ASSERT_EQ(RESULT_OK, sys_time.interface.inc_time_by_1000_ms(&sys_time.interface));
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_1000MS);
   ASSERT_EQ(RESULT_OK, sys_time.interface.get_time_ms(&sys_time.interface, &time));
   ASSERT_EQ(time, INC_RESULT_1000MS);

   // set val
   sys_time._current_time_ms = INITIAL_VALUE;
   ASSERT_EQ(RESULT_OK, sys_time.interface.inc_time_by_set_val_ms(&sys_time.interface, INC_RESULT_SET_VAL));
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_SET_VAL);
   ASSERT_EQ(RESULT_OK, sys_time.interface.get_time_ms(&sys_time.interface, &time));
   ASSERT_EQ(time, INC_RESULT_SET_VAL);
}

/**
 * @brief Tests the ability of the system time module to handle and report overflow
 *
 * @return None
 */
TEST_F(SystemTimeTestSuit, system_time_overflow_test)
{
   StandardInit();
   time = 0;

   sys_time._current_time_ms = UINT64_MAX;
   result = sys_time.interface.inc_time_by_1_ms(&sys_time.interface);
   ASSERT_NE(result, RESULT_OK);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_1MS - 1);
   result = sys_time.interface.get_time_ms(&sys_time.interface, &time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time, INC_RESULT_1MS - 1);

   sys_time._current_time_ms = UINT64_MAX;
   result = sys_time.interface.inc_time_by_10_ms(&sys_time.interface);
   ASSERT_NE(result, RESULT_OK);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_10MS - 1);
   result = sys_time.interface.get_time_ms(&sys_time.interface, &time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time, INC_RESULT_10MS - 1);

   sys_time._current_time_ms = UINT64_MAX;
   result = sys_time.interface.inc_time_by_100_ms(&sys_time.interface);
   ASSERT_NE(result, RESULT_OK);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_100MS - 1);
   result = sys_time.interface.get_time_ms(&sys_time.interface, &time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time, INC_RESULT_100MS - 1);

   sys_time._current_time_ms = UINT64_MAX;
   result = sys_time.interface.inc_time_by_1000_ms(&sys_time.interface);
   ASSERT_NE(result, RESULT_OK);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_1000MS - 1);
   result = sys_time.interface.get_time_ms(&sys_time.interface, &time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time, INC_RESULT_1000MS - 1);

   sys_time._current_time_ms = UINT64_MAX;
   result = sys_time.interface.inc_time_by_set_val_ms(&sys_time.interface, INC_RESULT_SET_VAL);
   ASSERT_NE(result, RESULT_OK);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_OVERFLOW);
   ASSERT_EQ(sys_time._current_time_ms, INC_RESULT_SET_VAL - 1);
   result = sys_time.interface.get_time_ms(&sys_time.interface, &time);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(time, INC_RESULT_SET_VAL - 1);
}

TEST_F(SystemTimeTestSuit, system_time_null_pointer_handling)
{
   // system_time_init should fail with null
   result_t result = system_time_init(nullptr);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = system_time_init(&sys_time);
   // All interface functions should fail with null interface
   result = sys_time.interface.inc_time_by_1_ms(nullptr);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = sys_time.interface.inc_time_by_10_ms(nullptr);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = sys_time.interface.inc_time_by_100_ms(nullptr);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = sys_time.interface.inc_time_by_1000_ms(nullptr);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = sys_time.interface.inc_time_by_set_val_ms(nullptr, 5);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = sys_time.interface.set_time_ms(nullptr, 5);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   uint64_t dummy = 0;
   result = sys_time.interface.get_time_ms(nullptr, &dummy);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);

   result = sys_time.interface.get_time_ms(&sys_time.interface, nullptr);
   ASSERT_EQ(GET_ERR_CODE(result), SYSTEM_TIME_ERROR_PTR_NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_SYSTEM_TIME);
}

TEST_F(SystemTimeTestSuit, system_time_set_test)
{
   StandardInit();

   uint64_t time = 0;
   uint64_t new_time_ms = 123456789u;
   ASSERT_EQ(RESULT_OK, sys_time.interface.set_time_ms(&sys_time.interface, new_time_ms));
   ASSERT_EQ(sys_time._current_time_ms, new_time_ms);
   ASSERT_EQ(RESULT_OK, sys_time.interface.get_time_ms(&sys_time.interface, &time));
   ASSERT_EQ(new_time_ms, time);
}