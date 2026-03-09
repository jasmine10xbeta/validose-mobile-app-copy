/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup bmi323_imu_driver IMU
 * @ingroup drivers
 * @brief Inertial Measurement Unit (IMU).
 * @details Design specifications:
 * 1. Power management modes -- IMU can be configured in ACTIVE or DORMANT modes
 * 2. Configure interrupts via on-chip INT pins:
 * - SIG MOTION mapped to interrupt 1
 * - TAP mapped to interrupt 2
 * 3. Enable 6 axis IMU data capturing at some defined sampling rate. The data should be stored in the internal FIFO
 * buffer and allow periodic retrieval of all data in the buffer.
 * 4. Enable configuration of the on chip low pass filtering for the IMU data.
 * 5. Enable temperature measurement through the module
 * 6. Enable gesture detection for single, double, and triple tap.
 * 7. Enable significant motion detection and configuration.
 *
 * @file imu.h
 * @ingroup imu_driver
 * @brief
 */

#ifndef IMU_H_
#   define IMU_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#   include "nrf_drv_twi.h"
#   include <stdbool.h>

// Custom includes
#   include "../../common/common.h"
#   include "../../common/result.h"
#   include "../i2c/i2c_driver_interface.h"
#   include "imu_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @def IMU_ACTIVE_SAMPLING_RATE_HZ
 * @brief Sampling rate of the IMU when active
 *
 * Allowed sampling rates:
 *    - 12.5 Hz (use IMU_ACTIVE_SAMPLING_RATE_HZ = 12 to represent 12.5 Hz)
 *    - 25 Hz
 *    - 50 Hz
 *    - 100 Hz
 *
 * Defaults to 100 Hz if not defined elsewhere
 */
#   ifndef IMU_ACTIVE_SAMPLING_RATE_HZ
#      define IMU_ACTIVE_SAMPLING_RATE_HZ (100u)
#   endif

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct imu
{
   imu_interface_t interface;

   // Additional internal state variables
   IMU_STATE _state;
} imu_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes the IMU
 *
 * This function initializes the IMU.
 *
 * @param self Pointer to the imu instance.
 *
 * @return RESULT_OK if the initialization is successful, otherwise an appropriate error notification.
 */
result_t imu_x_init(imu_t *const self, i2c_driver_interface_t *i2c_interface);

#endif // IMU_H_

/** @} */