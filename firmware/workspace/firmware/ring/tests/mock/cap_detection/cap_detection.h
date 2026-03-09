/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file cap_detection.h
 * @brief Header file for the mock cap detection module.
 */

#ifndef MOCK_CAP_DETECTION_MODULE_H_
#define MOCK_CAP_DETECTION_MODULE_H_

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
// Standard includes
#include <stdbool.h>

// Custom includes
#include "cap_detection_interface.h"
#include "common.h"
#include "ir_proximity_driver_interface.h"

/**********************************************************************************************************************
 * Definitions
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Types
 *********************************************************************************************************************/

/**
 * @brief Cap detection module instance
 */
typedef struct cap_detection cap_detection_t;
struct cap_detection
{
   // Interface
   cap_detection_interface_t interface;

   // Private data
   CAP_STATE _mock_cap_state;     /** Mock cap state */
   uint16_t _mock_proximity_data; /** Mock proximity data value */

   bool _fail_imu_read_proximity; /** Whether to simulate a failure when reading IMU proximity data */
   bool _fail_imu_get_status;     /** Whether to simulate a failure when getting IMU status */
   uint8_t _mock_status_error;    /** Mock status error to simulate different error conditions */

   bool _is_initialized; /** Whether the instance is initialized */
};

/**********************************************************************************************************************
 * Variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Global functions
 *********************************************************************************************************************/

/**
 * @brief Initialize an instance of the mock cap detection module
 *
 * @param[in,out] p_self Pointer to the cap detection instance to initialize
 *
 * @return Status code indicating the result of the operation
 */
result_t mock_cap_detection_init(cap_detection_t *const p_self);

#endif /* MOCK_CAP_DETECTION_MODULE_H_ */
