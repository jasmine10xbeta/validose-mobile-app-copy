/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file weight_sensor_interface.h
 * @brief Interface file for the weight sensor module
 *
 * Expected operation:
 *
 * main loop:
 *    process()
 *    read_weight_data(&weight_data, &is_stale)
 *    if (!is_stale && is_new(weight_data)) // If timestamp of weight_data is newer than last processed timestamp
 *        // use weight data
 *
 *
 * calibration procedure:
 *    while (step != FINISHED)
 *       if (step == GET_ZERO_OFFSET)
 *          try_tare_if_stable(&successful)
 *          if (successful) step = GET_CALIBRATION_FACTOR
 *       if (step == GET_CALIBRATION_FACTOR)
 *           try_calibrate_if_stable(known_weight, &successful)
 *          if (successful) step = FINISHED
 *
 * baselining procedure:
 *    while (step != FINISHED)
 *       if (step == GET_BASELINE_WEIGHT)
 *          try_tare_if_stable(&successful)
 *          if (successful) step = GET_FULL_ASSEMBLY_WEIGHT
 *
 *       // No more interaction with weight sensor module required
 *
 * @todo Investigate if the is_stale flag in read_weight_data is necessary. Currently it is not being used and cannot
 * serve as a method for detecting if the samples are new, due to multiple consumers of the weight data.
 */

#ifndef WEIGHT_SENSOR_INTERFACE_H
#define WEIGHT_SENSOR_INTERFACE_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"

#include <app_util.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define WS_MOVING_AVERAGE_SAMPLES (10u) /**< Number of ADC samples across which the weight sensor judges stability */

/** Standard deviation threshold in raw ADC units under which a sample is considered stable */
#define WS_STABLE_STDDEV_THRESHOLD_RAW (200u)

/** Weight present detection threshold in raw ADC units to determine if the calibration weight is present */
#define WS_WEIGHT_PRESENT_DETECTION_THRESHOLD_RAW (50000u)

/** Maximum time in milliseconds to wait for stable samples during calibration */
#define WS_CALIBRATION_TIMEOUT_MS (60000u)

STATIC_ASSERT(WS_STABLE_STDDEV_THRESHOLD_RAW > 0, "Stable stddev threshold must be greater than 0");
STATIC_ASSERT(WS_WEIGHT_PRESENT_DETECTION_THRESHOLD_RAW > 0,
              "Weight present detection threshold must be greater than 0");

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Structure to hold weight measurement data for dose size detection.
 */
typedef struct weight_data
{
   int32_t weight_mg;    /**< The weight measurement in milligrams. Contains raw ADC values if scale is uncalibrated */
   uint16_t stddev_mg;   /**< The standard deviation of the weight measurements in milligrams */
   int16_t temp_deciC;   /**< The temperature in deci-degrees Celsius for temperature compensation */
   bool is_ring_present; /**< Whether the ring was present at the time of measurement */
   uint64_t time_ms;     /**< When the weight measurement was taken, in milliseconds since system start */
} weight_data_t;

/**
 * @brief Error definitions for the weight sensor module
 */
typedef enum
{
   WEIGHT_SENSOR_ERROR_NONE = 0,
   WEIGHT_SENSOR_ERROR_NULL_POINTER,     /**< Unexpected null pointer */
   WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT, /**< Invalid argument */
   WEIGHT_SENSOR_ERROR_INTERNAL,         /**< Internal error such as calculation overflow */
   WEIGHT_SENSOR_ERROR_NOT_INITIALIZED,  /**< The weight sensor has not been initialized */
   WEIGHT_SENSOR_ERROR_ADC_DATA_STALE,   /**< The ADC data is stale */
   WEIGHT_SENSOR_ERROR_ZERO_NOT_SET,     /**< Zero offset has not been set during calibration */
   WEIGHT_SENSOR_ERROR_NOT_IMPLEMENTED,
   WEIGHT_SENSOR_ERROR_CONVERSION_NOT_ENABLED,
   WEIGHT_SENSOR_ERROR_INVALID,
   WEIGHT_SENSOR_ERROR_TIMEOUT,
   WEIGHT_SENSOR_ERROR_ERROR_MAX,
} WEIGHT_SENSOR_ERROR;

// NOTE: The IDLE sampling state/ frequency is to be implemented at a later stage, as part of the dose size detection
// upgrades (as decided by Herman and Cara on 05/02/2026)
typedef enum
{
   SAMPLING_FREQUENCY_FAST_RING_OFF_HZ = 0, // 10Hz
   SAMPLING_FREQUENCY_SLOW_RING_ON_HZ,      // 0.1Hz
   SAMPLING_FREQUENCY_DSD_IDLE,             // every 60s to check for stability, when DSD is in IDLE state
   SAMPLING_FREQUENCY_MAX
} SAMPLING_FREQUENCY;

typedef enum
{
   WEIGHT_SAMPLING_STATE_RING_PRESENT = 0,
   WEIGHT_SAMPLING_STATE_RING_ABSENT,
   WEIGHT_SAMPLING_STATE_RING_REPLACED,
   WEIGHT_SAMPLING_STATE_DSD_IDLE,
   WEIGHT_SAMPLING_STATE_MAX
} WEIGHT_SAMPLING_STATE;

/**
 * @brief Weight sensor calibration state
 */
typedef enum
{
   WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED = 0, /**< Initial state. No zero offset or calibration factor set */
   WEIGHT_SENSOR_CALIBRATION_STATE_ZEROING,          /**< Tare in progress */
   WEIGHT_SENSOR_CALIBRATION_STATE_ZEROED, /**< Zero offset has been set, but calibration factor has not been set */
   WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATING, /**< Calibration in progress */
   WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED,  /**< Both zero offset and calibration factor have been set */

   WEIGHT_SENSOR_CALIBRATION_STATE_MAX /**< Sentinel value */
} WEIGHT_SENSOR_CALIBRATION_STATE;

struct weight_sensor; // Forward declaration
typedef struct weight_sensor_interface weight_sensor_interface_t;

typedef struct weight_sensor_interface
{
   struct weight_sensor *parent; // Reference to the containing instance.

   /**
    * @brief Reset the scale
    *
    * @param interface Pointer to this interface
    * @return result_t Result of the operation
    */
   result_t (*reset_scale)(const weight_sensor_interface_t *interface);

   /**
    * @brief Set the zero offset
    *
    * @param interface Pointer to this interface
    * @param offset Offset to set
    * @return result_t Result of the operation
    */
   result_t (*set_zero_offset)(const weight_sensor_interface_t *interface, int32_t offset);

   /**
    * @brief Get the zero offset
    *
    * @param interface Pointer to this interface
    * @param out_offset Pointer to store zero offset
    * @return result_t Result of the operation
    */
   result_t (*get_zero_offset)(const weight_sensor_interface_t *interface, int32_t *out_offset);

   /**
    * @brief Set the calibration factor
    *
    * @param interface Pointer to this interface
    * @param factor Factor to set
    * @return result_t Result of the operation
    */
   result_t (*set_calibration_factor)(const weight_sensor_interface_t *interface, int32_t factor);

   /**
    * @brief Get the calibration factor
    *
    * @param interface Pointer to this interface
    * @param out_factor Pointer to store calibration factor
    * @return result_t Result of the operation
    */
   result_t (*get_calibration_factor)(const weight_sensor_interface_t *interface, int32_t *out_factor);

   /**
    * @brief Get the current calibration state of the weight sensor
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] state_out Pointer to store the current calibration state of the weight sensor
    *
    * @return result_t Result of the operation
    */
   result_t (*get_calibration_state)(const weight_sensor_interface_t *interface,
                                     WEIGHT_SENSOR_CALIBRATION_STATE *state_out);

   /**
    * @brief Get the current state of the weight sensor
    *
    * @param interface Pointer to this interface
    * @param sampling_state Pointer to store sampling state
    * @param sampling_frequency Pointer to store sampling frequency
    * @return result_t Result of the operation
    */
   result_t (*get_state)(const weight_sensor_interface_t *interface,
                         WEIGHT_SAMPLING_STATE *sampling_state,
                         SAMPLING_FREQUENCY *sampling_frequency);

   /**
    * @brief Try to tare the weight sensor
    *
    * Use this function during the calibration procedure to attempt to tare the weight sensor. This function will
    * monitor the weight readings and update the zero offset if it detects that the weight readings are stable while the
    * ring is absent. It has an internal timeout to ensure it does not wait indefinitely for stable readings.
    *
    * @param[in] interface Pointer to the interface instance
    * @param[in] is_ring_present Whether the ring is currently present. Must be false to perform taring
    * @param[out] is_tare_successful Pointer to store whether the tare was successful. This will be set to true if the
    * zero offset was updated during this function call
    *
    * @retval `RESULT_OK` if the function executed successfully, regardless of whether the tare was successful
    * @retval `WEIGHT_SENSOR_ERROR_NULL_POINTER` if interface or is_tare_successful is null
    * @retval `WEIGHT_SENSOR_ERROR_NOT_INITIALIZED` if the weight sensor instance has not been initialized
    * @retval `WEIGHT_SENSOR_ERROR_TIMEOUT` if the taring process timed out
    */
   result_t (*try_tare_if_stable)(const weight_sensor_interface_t *interface,
                                  bool is_ring_present,
                                  bool *is_tare_successful);

   /**
    * @brief Try to calibrate the weight sensor with a known weight
    *
    * Use this function during the calibration procedure to attempt to calibrate the weight sensor with a known weight.
    * The weight sensor must be tared (zero offset set) before calling this function. This function will monitor the
    * weight readings and update the calibration factor if it detects that the weight readings are stable while a weight
    * is present. It has an internal timeout to ensure it does not wait indefinitely for stable readings.
    *
    * @param[in] interface Pointer to the interface instance
    * @param[in] known_weight_mg The known weight in milligrams used for calibration
    * @param[in] is_ring_present Boolean indicating whether the ring is present
    * @param[out] is_weight_detected Pointer to store whether a weight is detected
    * @param[out] is_calibration_successful Pointer to store whether the calibration was successful
    *
    * @retval `RESULT_OK` if the function executed successfully, regardless of whether the calibration was successful
    * @retval `WEIGHT_SENSOR_ERROR_NULL_POINTER` if interface or is_calibration_successful is null
    * @retval `WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT` if known_weight_mg is 0 or leads to a zero calibration factor
    * @retval `WEIGHT_SENSOR_ERROR_NOT_INITIALIZED` if the weight sensor instance has not been initialized
    * @retval `WEIGHT_SENSOR_ERROR_ZERO_NOT_SET` if the zero offset has not been set before calling this function
    * @retval `WEIGHT_SENSOR_ERROR_TIMEOUT` if the calibration process timed out
    */
   result_t (*try_calibrate_if_stable)(const weight_sensor_interface_t *interface,
                                       uint32_t known_weight_mg,
                                       bool is_ring_present,
                                       bool *is_weight_detected,
                                       bool *is_calibration_successful);

   /**
    * @brief Retrieve the latest weight measurement data
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] weight_data_out Pointer to store the latest weight measurement data
    * @param[out] is_stale Pointer to store whether the weight data is stale (new sample being processed)
    *
    * @return result_t Result of the operation
    */
   result_t (*read_weight_data)(const weight_sensor_interface_t *interface,
                                weight_data_t *weight_data_out,
                                bool *is_stale);

   /**
    * @brief Check if the weight readings are currently stable
    *
    * Checks if the last few weight readings are stable based on the standard deviation of the weight measurements.
    * This can be used during calibration to determine if the weight readings have stabilized before attempting to tare
    * or calibrate.
    */
   result_t (*get_is_stable)(const weight_sensor_interface_t *interface, bool *is_stable);

   /**
    * @brief Process and update the latest weight measurement data.
    *
    * This function collects ADC samples, computes the average weight, standard deviation,
    * and updates the output structure with the latest measurement, timestamp, and ring presence status.
    * Note: The ADC average and standard deviation are computed at a frequency based on ring presence to optimize power
    * consumption.
    *
    * @param interface Pointer to this interface
    * @param is_ring_present Whether the ring is present
    * @return result_t Result of the operation
    */
   result_t (*process)(const weight_sensor_interface_t *interface, bool is_ring_present);
} weight_sensor_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // WEIGHT_SENSOR_INTERFACE_H