/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tilt_detection.h
 * @brief Header file for the tilt detection module
 */

#ifndef TILT_DETECTION_H
#define TILT_DETECTION_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "imu_interface.h"
#include "system_time_interface.h"
#include "tilt_detection_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Tilt detection module instance
 */
typedef struct tilt_detection
{
   // Interface
   tilt_detection_interface_t interface;

   // Dependencies
   const imu_interface_t *_imu_interface;
   const system_time_interface_t *_system_time_interface;

   // Private data
   TILT_DETECTION_AXIS _upright_axis; /**< The axis that is considered upright */

   bool _tilt_active;           /**< Whether a tilt is currently active */
   uint32_t _tilt_sample_count; /**< The number of samples recorded during the current tilt */

   bool _is_initialized; /**< Whether this instance has been initialized */
} tilt_detection_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes an instance of the tilt detection module
 *
 * @param[in,out] self Pointer to the tilt detection instance to initialize
 * @param[in] system_time Pointer to the system time interface to use
 * @param[in] imu_interface Pointer to the IMU interface to use
 *
 * @return A status code indicating result of the operation
 */
result_t tilt_detection_init(tilt_detection_t *const self,
                             const system_time_interface_t *const system_time,
                             const imu_interface_t *const imu_interface);

#endif // TILT_DETECTION_H