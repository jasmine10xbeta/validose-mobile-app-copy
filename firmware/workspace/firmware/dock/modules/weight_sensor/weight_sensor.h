/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef WEIGHT_SENSOR_H
#define WEIGHT_SENSOR_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "ads1235.h"
#include "common.h"
#include "sts30_dis_temp_sensor.h"
#include "system_time.h"
#include "weight_sensor_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Weight sensor struct
 */
typedef struct weight_sensor
{
   // Interface
   weight_sensor_interface_t interface;

   // Dependencies
   const system_time_interface_t *_system_time;
   const sts30_dis_temp_sensor_interface_t *_temp_sensor;

   // Drivers
   ads1235_driver_t _adc_driver;

   // Calibration
   WEIGHT_SENSOR_CALIBRATION_STATE _calibration_state; /**< Current calibration state of the weight sensor */
   int32_t _zero_offset;                               /**< Calibrated zero offset for the scale (empty weight) */
   int32_t _calibration_factor; /**< Scale factor to convert ADC units to grams, determined during factory calibration.
                                   Must be non-zero. */

   // Stability tracking
   int32_t _adc_ma[WS_MOVING_AVERAGE_SAMPLES]; /**< Moving average buffer of ADC samples used for stability checks */
   uint32_t _adc_ma_head;                      /**< Write index for the moving average buffer */
   uint32_t _adc_ma_count;                     /**< Number of samples currently in the moving average buffer */
   int64_t _adc_ma_sum;                        /**< Rolling sum of samples in the moving average window */
   uint64_t _adc_ma_sum2;                      /**< Rolling sum of squares of samples in the moving average window */

   // Calibration process tracking
   uint64_t _last_calibration_sample_time_ms; /**< Timestamp in milliseconds of the last sample used for
                                                 tare/calibration stability checks */
   uint64_t _calibration_start_time_ms; /**< Timestamp in milliseconds of when the tare/calibration process started,
                                           used for timeout */
   WEIGHT_SENSOR_CALIBRATION_STATE
   _calibration_state_before_start; /**< Calibration state before starting tare/calibration process */

   // Sampling state tracking
   WEIGHT_SAMPLING_STATE _current_sampling_state;  /**< Current sampling state (ring present/absent/replaced) */
   SAMPLING_FREQUENCY _current_sampling_frequency; /**< Current sampling frequency, determined by sampling state */
   uint64_t _ring_replaced_time_ms; /**< Timestamp when ring was replaced, used for debounce logic (ms) */

   // Latest sample
   int32_t _latest_adc_value;         /**< Latest raw ADC value reported by the ADS1235 driver */
   uint16_t _latest_adc_stddev;       /**< Latest raw ADC standard deviation reported by the ADS1235 driver */
   weight_data_t _latest_weight_data; /**< Latest weight data sample */
   bool _is_weight_data_stale;    /** Whether the latest weight data is stale and needs to be updated by process() */
   uint64_t _last_sample_time_ms; /**< Timestamp of last sample, used to determine when to take the next sample (ms) */

   bool _is_adc_enabled; /**< Whether ADC conversion is enabled, based on ring presence and DSD state */

   bool _initialized; /**< Whether this instance has been initialized */
} weight_sensor_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initialize an instance of the weight sensor module
 *
 * Initializes the weight sensor instance with the provided dependencies and configuration. The weight sensor will be
 * considered uncalibrated until full calibration is set using the appropriate interface functions.
 *
 * @param self Pointer to the weight sensor instance to initialize
 * @param system_time Pointer to the system time interface
 * @param spi_interface Pointer to the SPI driver interface
 * @param temp_sensor_interface Pointer to the temperature sensor interface
 *
 * @return result_t Result of the operation
 */
result_t weight_sensor_init(weight_sensor_t *const self,
                            const system_time_interface_t *const system_time,
                            const spi_driver_interface_t *const spi_interface,
                            const sts30_dis_temp_sensor_interface_t *const temp_sensor_interface);

#endif // WEIGHT_SENSOR_H
