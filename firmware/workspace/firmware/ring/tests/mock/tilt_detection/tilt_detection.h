/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tilt_detection.h
 * @brief Header file for the mock tilt detection module
 */

#ifndef MOCK_TILT_DETECTION_H
#define MOCK_TILT_DETECTION_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "tilt_detection_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Additional error codes for testing purposes
 */
typedef enum
{
   MOCK_TILT_DETECTION_ERROR_NONE = TILT_DETECTION_ERROR_MAX, /**< No error. Starts after the real errors */

   MOCK_TILT_DETECTION_ERROR_NULL, /**< Unexpected NULL reference */

   MOCK_TILT_DETECTION_ERROR_MAX /**< Sentinel value */
} MOCK_TILT_DETECTION_ERROR;

/**
 * @brief Tilt detection module instance
 */
typedef struct tilt_detection
{
   // Interface
   tilt_detection_interface_t interface;

   // Private data
   TILT_DETECTION_AXIS _upright_axis;       /**< The axis that is considered upright */
   TILT_DETECTION_AXIS _mock_detected_axis; /**< The axis to return during auto-detection */

   tilt_data_t _mock_tilts[TILT_DETECTION_MAX_TILTS]; /**< The tilts to return during get_tilts calls */
   uint8_t _mock_tilt_count;                          /**< The number of tilts to return during get_tilts calls */

   bool _fail_imu_set_state;            /**< Whether to simulate a failure when setting IMU state */
   bool _fail_imu_clear_data;           /**< Whether to simulate a failure when clearing IMU data */
   bool _fail_imu_get_data_entry_count; /**< Whether to simulate a failure when getting IMU data entry count */
   bool _fail_no_imu_data;              /**< Whether to simulate no IMU data available */
   bool _fail_imu_get_data_entries;     /**< Whether to simulate a failure when getting IMU data entries */

   bool _is_initialized; /**< Whether this instance has been initialized */
} tilt_detection_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes an instance of the mock tilt detection module
 *
 * @param[in,out] self Pointer to the tilt detection instance to initialize
 *
 * @return A status code indicating result of the operation
 */
result_t mock_tilt_detection_init(tilt_detection_t *const self);

#endif // MOCK_TILT_DETECTION_H