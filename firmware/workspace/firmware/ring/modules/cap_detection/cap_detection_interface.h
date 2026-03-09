/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file cap_detection_interface.h (driver for miniature proximity sensor module tmd2635)
 * @ingroup modules/cap_detection
 * @brief
 */

#ifndef CAP_DETECTION_MODULE_INTERFACE_H_
#define CAP_DETECTION_MODULE_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <app_util.h> // for STATIC_ASSERT
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "ir_proximity_driver_interface.h" // For PROX_SAMPLING_PERIOD_MS_MAX

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Upper threshold for proximity sensor readings when the cap is considered ON */
#define CAP_ON_PROX_THRESHOLD (5500u)

/** Lower threshold for proximity sensor readings when the cap is considered OFF */
#define CAP_OFF_PROX_THRESHOLD (5499u)

#define CAP_ON_SAMPLING_PERIOD_MS (500u) /**< Sampling period when the cap is ON in milliseconds */

#define CAP_OFF_SAMPLING_PERIOD_MS (100u) /**< Sampling period when the cap is OFF in milliseconds */

/***********************************************************************************************************************
 * Assertions
 **********************************************************************************************************************/

STATIC_ASSERT(CAP_OFF_PROX_THRESHOLD <= CAP_ON_PROX_THRESHOLD,
              "CAP_OFF_PROX_THRESHOLD must be less than or equal to CAP_ON_PROX_THRESHOLD.");

STATIC_ASSERT(CAP_ON_SAMPLING_PERIOD_MS <= PROX_SAMPLING_PERIOD_MS_MAX,
              "CAP_ON_SAMPLING_PERIOD_MS exceeds maximum supported sampling period.");

STATIC_ASSERT(CAP_OFF_SAMPLING_PERIOD_MS <= PROX_SAMPLING_PERIOD_MS_MAX,
              "CAP_OFF_SAMPLING_PERIOD_MS exceeds maximum supported sampling period.");

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Definition of proximity sensor driver errors.
 */
typedef enum
{
   CAP_MODULE_ERROR_NONE = 0,             // No error
   CAP_MODULE_ERROR_PTR_NULL,             // Null pointer error
   CAP_MODULE_ERROR_INIT_FAILURE,         // Failure to initialize driver
   CAP_MODULE_ERROR_NRF_ERR_CHECK,        // Generic Nordic SDK error
   CAP_MODULE_ERROR_GET_STATUS_FAIL,      // Fail to get status register values from driver
   CAP_MODULE_ERROR_GET_PROXIMITY_FAIL,   // Fail to get proximity reading from driver register
   CAP_MODULE_ERROR_UPDATE_DRV_REGISTERS, // Fail to update driver registers
   CAP_MODULE_ERROR_INVALID_PARAM,        // Invalid function parameter
   CAP_MODULE_ERROR_UNINITIALIZED,        // Driver not initialized
   CAP_MODULE_ERROR_SET_DRV_ACTIVE,       // Failed to activate driver
   CAP_MODULE_ERROR_GET_RESET_INT,        // Failed to get/ reset INT value
   CAP_MODULE_ERROR_UNKNOWN               // Unknown error
} CAP_MODULE_ERROR;

typedef struct
{
   uint16_t high; // Cap on threshold
   uint16_t low;  // Cap off threshold
} proximity_thresholds_t;

struct cap_detection; // Forward declaration

typedef struct cap_detection_interface cap_detection_interface_t;

typedef enum
{
   CAP_STATE_UNKNOWN = 0,
   CAP_STATE_OPEN = 1,
   CAP_STATE_CLOSED = 2
} CAP_STATE;

struct cap_detection_interface
{
   struct cap_detection *parent; // Reference to the containing instance.

   /**
    * @brief Get cap status from driver (cap ON/ cap OFF)
    *
    * @param interface The cap_detection_interface_t interface handles.
    * @param cap_state pointer to cap state
    */
   result_t (*get_cap_status)(const cap_detection_interface_t *const interface,
                              CAP_STATE *cap_state,
                              uint16_t *prox_val);

   result_t (*get_prox_sensor_thresholds)(const cap_detection_interface_t *const interface,
                                          proximity_thresholds_t *thresholds);

   result_t (*set_prox_sensor_thresholds)(const cap_detection_interface_t *const interface,
                                          proximity_thresholds_t thresholds);
   // todo: might add is_reliable flag -> saturation
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // CAP_DETECTION_MODULE_INTERFACE_H_