/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <cstdio>
#include <gtest/gtest.h>

extern "C"
{
#include "cap_detection.h"
#include "common.h"
#include "mock/ir_proximity_sensor_driver/tmd2635_driver.h"

#include <stdbool.h>
#include <stdint.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define THIS_UNIT_ID SW_UNIT_ID_CAP_DETECTION_MODULE // Define unit ID for tagging errors

#define PROX_THRESHHOLD  (4000u) // Example threshold value for cap ON state
#define HYSTERESIS_VALUE (250u)  // Example hysteresis value

#define CAP_ON_PROX_TEST_VALUE  (PROX_THRESHHOLD + HYSTERESIS_VALUE / 2 + 1)
#define CAP_OFF_PROX_TEST_VALUE (PROX_THRESHHOLD - HYSTERESIS_VALUE / 2 - 1)

#define NON_ZERO_PROX_TEST_VALUE (0x1234) // Arbitrary non-zero proximity value for testing

#define PDATA_MAX_VALUE (0x3FFF) // Maximum valid PDATA value (14-bit)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Utility Functions
 **********************************************************************************************************************/

// Call once at startup (or in test setup)
static inline void seed_rng(void)
{
   srand((unsigned)time(NULL));
}

// Generate a random PDATA in [0 .. 0x3FFF]
static inline uint16_t gen_random_pdata(void)
{
   return (uint16_t)(rand() & 0x3FFF); // 14-bit value
}

/***********************************************************************************************************************
 * Test Fixtures
 **********************************************************************************************************************/

class CapDetectionTestSuit: public ::testing::Test
{
protected:
   cap_detection_t test_cap_det = {0};
   tmd2635_driver_t test_prox_drv = {0};

   void SetUp() override
   {
      result_t result = mock_tmd2635_driver_init(&test_prox_drv);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(test_prox_drv._is_initialized, true);

      result = cap_detection_init(&test_cap_det, &test_prox_drv.interface, PROX_THRESHHOLD, HYSTERESIS_VALUE);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(test_cap_det._is_initialized, true);
   }

   void TearDown() override
   {
   }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

/**
 * @brief Tests cap detection module initialization with invalid parameters.
 *
 * This test verifies that the cap detection module initialization function correctly handles invalid input parameters
 * by returning appropriate error codes.
 */
TEST_F(CapDetectionTestSuit, cap_detection_init_test_invalid_parameters)
{
   result_t result = RESULT_OK;

   // Null self pointer
   result = cap_detection_init(NULL, &test_prox_drv.interface, PROX_THRESHHOLD, HYSTERESIS_VALUE);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);

   // Null proximity interface pointer
   result = cap_detection_init(&test_cap_det, NULL, PROX_THRESHHOLD, HYSTERESIS_VALUE);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);

   // Uninitialized proximity interface
   tmd2635_driver_t uninit_prox_drv = {0};

   result = cap_detection_init(&test_cap_det, &uninit_prox_drv.interface, PROX_THRESHHOLD, HYSTERESIS_VALUE);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);
}

/**
 * @brief Tests cap detection module initialization with proximity driver failures.
 */
TEST_F(CapDetectionTestSuit, cap_detection_init_test_prox_driver_failures)
{
   result_t result = RESULT_OK;

   // Simulate I2C write failure during initialization
   mock_tmd2635_set_behavior_option(&test_prox_drv, MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_WRITE, true);

   result = cap_detection_init(&test_cap_det, &test_prox_drv.interface, PROX_THRESHHOLD, HYSTERESIS_VALUE);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_INIT_FAILURE);
   ASSERT_EQ(test_cap_det._is_initialized, false);
}

/**
 * @brief Tests the failure handling of `get_cap_status` with incorrect input parameters.
 *
 * This test verifies that the function returns an error value and does not proceed with cap detection,
 * if any incorrect input parameters are supplied.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_incorrect_parameters)
{
   result_t result = RESULT_OK;
   result_t err_result;
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = NON_ZERO_PROX_TEST_VALUE;

   // Null interface pointer
   result = test_cap_det.interface.get_cap_status(NULL, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, NON_ZERO_PROX_TEST_VALUE);

   // Uninitialized interface
   cap_detection_t uninit_cap_det = {0};
   result = test_cap_det.interface.get_cap_status(&uninit_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, NON_ZERO_PROX_TEST_VALUE);

   // Null cap_state pointer
   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, NULL, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, NON_ZERO_PROX_TEST_VALUE);

   // Null prox_val pointer
   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, NON_ZERO_PROX_TEST_VALUE);

   // All null pointers
   result = test_cap_det.interface.get_cap_status(NULL, NULL, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_PTR_NULL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, NON_ZERO_PROX_TEST_VALUE);
}

/**
 * @brief Test the failure handling of `get_cap_status` with various ir proximity sensor driver failures.
 *
 * This test exercises all relevant failure modes of the ir proximity sensor driver and verifies that `get_cap_status`
 * correctly propagates errors when they occur.
 *
 * @note Note: I2C write and register update failures can occur in `get_cap_status` only when `set_sampling_rate` has
 * been called. `set_sampling_rate` does not affect the logic or return value of `get_cap_status`.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_prox_driver_failures)
{
   result_t result = RESULT_OK;
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Set prox data to a valid value
   mock_tmd2635_set_proximity_data(&test_prox_drv, NON_ZERO_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, TMD2635_STATUS_RESET);

   // I2C Read Failure
   mock_tmd2635_set_behavior_option(&test_prox_drv, MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_READ, true);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_GET_PROXIMITY_FAIL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, 0u);

   mock_tmd2635_set_behavior_option(&test_prox_drv, MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_READ, false);

   // Invalid Proximity Data (greater than 14-bit max)
   mock_tmd2635_set_proximity_data(&test_prox_drv, UINT16_MAX); // Invalid PDATA. Should return error.

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), CAP_MODULE_ERROR_GET_PROXIMITY_FAIL);
   ASSERT_EQ(cap_state, CAP_STATE_UNKNOWN);
   ASSERT_EQ(prox, 0u);
}

/**
 * @brief Tests cap OFF classification for a clean, unsaturated reading.
 *
 * This test feeds a proximity value below the configured cap-off threshold with
 * no saturation flags set. It verifies that the cap is reported as OFF (OPEN) and the
 * measurement is marked as reliable (no saturation error set - RESULT_OK).
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_cap_off)
{
   // Initialization
   result_t result = RESULT_OK;

   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // For pdata = 2000 -> Cap OFF
   mock_tmd2635_set_proximity_data(&test_prox_drv, 2000u);
   mock_tmd2635_set_status_value(&test_prox_drv, TMD2635_STATUS_RESET);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS);
   ASSERT_EQ(cap_state, CAP_STATE_OPEN);
}

/**
 * @brief Tests cap ON classification for a clean, unsaturated reading.
 *
 * This test feeds a proximity value above the configured cap-on threshold with
 * no saturation flags set. It verifies that the cap is reported as ON (CLOSED) and the
 * measurement is marked as reliable (no saturation error set - RESULT_OK).
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_cap_on)
{
   // Initialization
   result_t result = RESULT_OK;

   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap ON
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_ON_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, TMD2635_STATUS_RESET);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
}

/**
 * @brief Tests boundary behavior for cap ON classification for a clean, unsaturated reading.
 *
 * This test feeds a proximity value equal to the configured cap-on/ cap-off threshold with
 * no saturation flags set. It verifies that the cap is reported as ON (CLOSED) and the
 * measurement is marked as reliable (no saturation error set - RESULT_OK).
 *
 * This test also calls the get_cap_status function again, to ensure that when the cap is alread ON
 * and pdata does not change/ drop below the threshold, the cap state does not change.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_cap_on_on_threshold)
{
   result_t result = RESULT_OK;

   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap ON at threshold
   mock_tmd2635_set_proximity_data(&test_prox_drv, PROX_THRESHHOLD + HYSTERESIS_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, TMD2635_STATUS_RESET);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);

   // cap state = previous cap state
   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);
}

/**
 * @brief Tests handling of reflective saturation.
 *
 * This test feeds a proximity value in the cap-ON range while setting the
 * reflective saturation flag in the status register. It verifies that the
 * reading is reported as saturated and unreliable (PROX_DRV_ERROR_REFLECTIVE_SATURATION),
 * regardless of the raw PDATA value.
 *
 * However, it also checks that the cap state is changed to cap ON, irregardless of the saturation.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_reflective_saturation)
{
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap ON with reflective saturation
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_ON_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, (TMD2635_STATUS_PSAT | TMD2635_STATUS_PSAT_REFLECTIVE));

   result_t result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), PROX_DRV_ERROR_REFLECTIVE_SATURATION);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);
}

/**
 * @brief Tests handling of ambient saturation.
 *
 * This test feeds a proximity value in the cap-OFF range while setting the
 * ambient saturation flag in the status register. It verifies that the
 * measurement is treated as saturated and unreliable (PROX_DRV_ERROR_AMBIENT_SATURATION), rather than trusted
 * as a valid cap-OFF reading.
 *
 * However, it also checks that the cap state is changed to cap OFF, irregardless of the saturation.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_ambient_saturation)
{
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap OFF with ambient saturation
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_OFF_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, (TMD2635_STATUS_PSAT | TMD2635_STATUS_PSAT_AMBIENT));

   result_t result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), PROX_DRV_ERROR_AMBIENT_SATURATION);
   ASSERT_EQ(cap_state, CAP_STATE_OPEN);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS);
}

/**
 * @brief Tests handling of simultaneous ambient & reflective saturation.
 *
 * This test feeds a proximity value in the cap-ON & cap-OFF range while setting both the
 * ambient & reflective saturation flag in the status register. It verifies that the
 * measurement is treated as saturated and unreliable (PROX_DRV_ERROR_AMBIENT_REFLECTIVE_SATURATION), rather than
 * trusted as a valid reading.
 *
 * However, it also checks that the cap state is changed, irregardless of the saturation.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_ambient_reflective_saturation)
{
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap ON with ambient & reflective saturation
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_ON_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv,
                                 (TMD2635_STATUS_PSAT | TMD2635_STATUS_PSAT_AMBIENT | TMD2635_STATUS_PSAT_REFLECTIVE));

   result_t result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), PROX_DRV_ERROR_AMBIENT_REFLECTIVE_SATURATION);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);

   // Cap OFF with ambient & reflective saturation
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_OFF_PROX_TEST_VALUE);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), PROX_DRV_ERROR_AMBIENT_REFLECTIVE_SATURATION);
   ASSERT_EQ(cap_state, CAP_STATE_OPEN);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS);
}

/**
 * @brief Tests behavior when saturation summary bit is set with inconsistent detail bits.
 *
 * This test feeds a proximity value in the cap-ON & cap-OFF range while setting only the
 * PSAT (saturation) flag in the status register. It verifies that get_cap_status()
 * handles inconsistent status gracefully and that the measurement is treated as saturated and
 * unreliable (PROX_DRV_ERROR_UNKNOWN_PROX_SATURATION), rather than trusted as a valid reading.
 *
 * However, it also checks that the cap state is changed, irregardless of the saturation.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_unknown_saturation)
{
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap ON with unknown saturation
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_ON_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, (TMD2635_STATUS_PSAT));

   result_t result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), PROX_DRV_ERROR_UNKNOWN_PROX_SATURATION);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);

   // Cap OFF
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_OFF_PROX_TEST_VALUE);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_CAP_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), PROX_DRV_ERROR_UNKNOWN_PROX_SATURATION);
   ASSERT_EQ(cap_state, CAP_STATE_OPEN);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS);
}

/**
 * @brief Tests handling of a random status value with no saturation bits set.
 *
 * This test feeds a proximity value in the cap-ON & cap-OFF range while setting the high two bits “randomly” in the
 * status register. No saturation bits are set. It verifies that the measurement is treated as reliable (RESULT_OK).
 *
 * It also checks that the cap state is changed, irregardless of the random status bits being set.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_random_status_value)
{
   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint16_t prox = 0u;

   // Cap ON with random status bits set
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_ON_PROX_TEST_VALUE);
   mock_tmd2635_set_status_value(&test_prox_drv, (TMD2635_STATUS_PHIGH | TMD2635_STATUS_PLOW));

   result_t result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);

   // Cap OFF with random status bits set
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_OFF_PROX_TEST_VALUE);

   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state, CAP_STATE_OPEN);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS);
}

/**
 * @brief Tests hysteresis behavior of get_cap_status().
 *
 * This test verifies that the cap state only changes when the proximity data crosses the respective hysteresis
 * thresholds:
 * - If cap_state is CAP_STATE_CLOSED, it only changes to CAP_STATE_OPEN when prox < CAP_OFF_PROX_THRESHOLD.
 * - If cap_state is CAP_STATE_OPEN, it only changes to CAP_STATE_CLOSED when prox >= PROX_THRESHHOLD +
 * HYSTERESIS_VALUE.
 */
TEST_F(CapDetectionTestSuit, get_cap_status_test_hysteresis)
{
   result_t result = RESULT_OK;
   CAP_STATE cap_state = CAP_STATE_UNKNOWN; // Start with cap_state = CAP_STATE_OPEN since prox = 0
   uint16_t prox = 0u;

   // Sweep up: cap should only close when crossing PROX_THRESHHOLD + HYSTERESIS_VALUE
   for(uint16_t pdata = 0u; pdata <= PDATA_MAX_VALUE; pdata++)
   {
      mock_tmd2635_set_proximity_data(&test_prox_drv, pdata);
      mock_tmd2635_set_status_value(&test_prox_drv, TMD2635_STATUS_RESET);

      result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
      ASSERT_EQ(prox, pdata);
      ASSERT_EQ(result, RESULT_OK);

      if(cap_state == CAP_STATE_OPEN)
      {
         if(pdata >= PROX_THRESHHOLD + HYSTERESIS_VALUE)
         {
            ASSERT_EQ(cap_state, CAP_STATE_CLOSED); // Should transition to CLOSED
         }
         else
         {
            ASSERT_EQ(cap_state, CAP_STATE_OPEN);
         }
      }
      else if(cap_state == CAP_STATE_CLOSED)
      {
         // Should only open if crossing below CAP_OFF_PROX_THRESHOLD
         if(pdata < PROX_THRESHHOLD - HYSTERESIS_VALUE)
         {
            ASSERT_EQ(cap_state, CAP_STATE_OPEN);
         }
         else
         {
            ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
         }
      }
   }

   // Sweep down: cap should only open when crossing CAP_OFF_PROX_THRESHOLD
   cap_state = CAP_STATE_CLOSED;
   for(int pdata = PDATA_MAX_VALUE; pdata >= 0; pdata--)
   {
      mock_tmd2635_set_proximity_data(&test_prox_drv, (uint16_t)pdata);
      mock_tmd2635_set_status_value(&test_prox_drv, TMD2635_STATUS_RESET);

      result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state, &prox);
      ASSERT_EQ(prox, pdata);
      ASSERT_EQ(result, RESULT_OK);

      if(cap_state == CAP_STATE_CLOSED)
      {
         if(pdata < PROX_THRESHHOLD - HYSTERESIS_VALUE)
         {
            ASSERT_EQ(cap_state, CAP_STATE_OPEN); // Should transition to OPEN
         }
         else
         {
            ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
         }
      }
      else if(cap_state == CAP_STATE_OPEN)
      {
         // Should only close if crossing above PROX_THRESHHOLD + HYSTERESIS_VALUE
         if(pdata >= PROX_THRESHHOLD + HYSTERESIS_VALUE)
         {
            ASSERT_EQ(cap_state, CAP_STATE_CLOSED);
         }
         else
         {
            ASSERT_EQ(cap_state, CAP_STATE_OPEN);
         }
      }
   }
}

/**
 * @brief Tests instance isolation between multiple cap detection module instances.
 *
 * This test creates two separate instances of the cap detection module, each with its own
 * proximity sensor driver. It verifies that operations on one instance do not affect the other,
 * ensuring proper isolation of internal states in interface methods (i.e. no global static/local static variables).
 */
TEST_F(CapDetectionTestSuit, cap_detection_multiple_instance_isolation)
{
   // Initialization
   result_t result = RESULT_OK;

   cap_detection_t cap_det_2 = {0};
   tmd2635_driver_t prox_drv_2 = {0};

   CAP_STATE cap_state_1 = CAP_STATE_UNKNOWN;
   CAP_STATE cap_state_2 = CAP_STATE_UNKNOWN;
   uint16_t prox_1 = 0;
   uint16_t prox_2 = 0;

   // Initialize second proximity driver
   result = mock_tmd2635_driver_init(&prox_drv_2);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(prox_drv_2._is_initialized, true);

   // Initialize second cap detection module
   result = cap_detection_init(&cap_det_2, &prox_drv_2.interface, PROX_THRESHHOLD, HYSTERESIS_VALUE);
   ASSERT_EQ(result, RESULT_OK);
   EXPECT_EQ(cap_det_2._is_initialized, true);

   // Cap ON for instance 1
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_ON_PROX_TEST_VALUE);
   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state_1, &prox_1);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state_1, CAP_STATE_CLOSED);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_ON_SAMPLING_PERIOD_MS);

   // Cap OFF for instance 2
   mock_tmd2635_set_proximity_data(&prox_drv_2, CAP_OFF_PROX_TEST_VALUE);
   result = cap_det_2.interface.get_cap_status(&cap_det_2.interface, &cap_state_2, &prox_2);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state_2, CAP_STATE_OPEN);
   ASSERT_EQ(prox_drv_2._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS);

   // Change cap state for instance 1 to OFF
   // Note: If instance data is not isolated, the sampling rate would incorrectly remain at CAP_ON_SAMPLING_PERIOD_MS
   mock_tmd2635_set_proximity_data(&test_prox_drv, CAP_OFF_PROX_TEST_VALUE);
   result = test_cap_det.interface.get_cap_status(&test_cap_det.interface, &cap_state_1, &prox_1);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(cap_state_1, CAP_STATE_OPEN);
   ASSERT_EQ(test_prox_drv._mock_sampling_rate, CAP_OFF_SAMPLING_PERIOD_MS); // Should change to OFF rate
}