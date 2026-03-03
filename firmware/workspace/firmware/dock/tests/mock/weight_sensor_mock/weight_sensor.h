/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef MOCK_WEIGHT_SENSOR_H
#define MOCK_WEIGHT_SENSOR_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"
#include "weight_sensor_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Additional error codes used by the mock implementation
 */
typedef enum
{
   MOCK_WEIGHT_SENSOR_ERROR_NONE = WEIGHT_SENSOR_ERROR_ERROR_MAX,
   MOCK_WEIGHT_SENSOR_ERROR_NULL,
   MOCK_WEIGHT_SENSOR_ERROR_FORCED_FAILURE,
   MOCK_WEIGHT_SENSOR_ERROR_MAX,
} MOCK_WEIGHT_SENSOR_ERROR;

/**
 * @brief Weight sensor struct
 */
typedef struct weight_sensor
{
   // Interface
   weight_sensor_interface_t interface;

   // Publicly configurable mock behavior/state
   WEIGHT_SENSOR_CALIBRATION_STATE _calibration_state;
   int32_t _mock_zero_offset;
   int32_t _calibration_factor;

   WEIGHT_SAMPLING_STATE _mock_sampling_state;
   SAMPLING_FREQUENCY _mock_sampling_frequency;

   weight_data_t _latest_weight_data;
   bool _is_weight_data_stale;
   bool _mock_is_stable;
   bool _mock_try_tare_successful;
   bool _mock_try_calibrate_successful;

   // Failure injection flags
   bool _fail_process;
   bool _fail_reset_scale;
   bool _fail_set_zero_offset;
   bool _fail_get_zero_offset;
   bool _fail_set_calibration_factor;
   bool _fail_get_calibration_factor;
   bool _fail_get_calibration_state;
   bool _fail_get_state;
   bool _fail_try_tare_if_stable;
   bool _fail_try_calibrate_if_stable;
   bool _fail_read_weight_data;
   bool _fail_get_is_stable;

   bool _initialized; /**< Whether this instance has been initialized */
} weight_sensor_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initialize an instance of the mock weight sensor module
 *
 * @param self Pointer to the mock weight sensor instance to initialize
 *
 * @return result_t Result of the operation
 */
result_t mock_weight_sensor_init(weight_sensor_t *const self);

/**
 * @brief Set the calibration state returned by the mock
 */
result_t mock_weight_sensor_set_calibration_state(weight_sensor_t *const self, WEIGHT_SENSOR_CALIBRATION_STATE state);

/**
 * @brief Set the weight reading returned by the mock
 */
result_t mock_weight_sensor_set_weight_data(weight_sensor_t *const self,
                                            int32_t weight_mg,
                                            uint16_t stddev_mg,
                                            uint64_t time_ms,
                                            bool is_ring_present,
                                            int16_t temp_deciC);

/**
 * @brief Set the staleness flag returned by the mock
 */
result_t mock_weight_sensor_set_data_stale(weight_sensor_t *const self, bool is_stale);

/**
 * @brief Set the stability value returned by the mock
 */
result_t mock_weight_sensor_set_is_stable(weight_sensor_t *const self, bool is_stable);

#endif /** MOCK_WEIGHT_SENSOR_H */
