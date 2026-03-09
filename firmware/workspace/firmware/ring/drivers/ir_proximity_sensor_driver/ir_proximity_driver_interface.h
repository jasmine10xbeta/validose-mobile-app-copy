/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ir_proximity_driver_interface.h
 * @ingroup ring/drivers/ir_proximity_sensor_driver
 * @brief Interface file for the IR proximity sensor driver.
 */

#ifndef IR_PROX_DRIVER_INTERFACE_H_
#define IR_PROX_DRIVER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"

#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Maximum sampling period in milliseconds. Corresponds to implementation limits. */
#define PROX_SAMPLING_PERIOD_MS_MAX (714u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error codes
 */
typedef enum
{
   PROX_DRV_ERROR_NONE = 0, /**< No error */

   PROX_DRV_ERROR_PTR_NULL,                      /**< Unexpected NULL pointer */
   PROX_DRV_ERROR_INVALID_PARAM,                 /**< Invalid parameter */
   PROX_DRV_ERROR_INVALID_CONFIG,                /**< Invalid configuration parameter */
   PROX_DRV_ERROR_UNINITIALIZED,                 /**< Driver not initialized */
   PROX_DRV_ERROR_INVALID_DATA,                  /**< Invalid data value */
   PROX_DRV_ERROR_I2C_WRITE_FAIL,                /**< I2C write failure */
   PROX_DRV_ERROR_I2C_READ_FAIL,                 /**< I2C read failure */
   PROX_DRV_ERROR_REGISTER_UPDATE_FAIL,          /**< Register update failure */
   PROX_DRV_ERROR_DEVICE_NOT_FOUND,              /**< Proximity sensor driver not detected */
   PROX_DRV_ERROR_UNKNOWN_PROX_SATURATION,       /**< Unknown saturation error */
   PROX_DRV_ERROR_REFLECTIVE_SATURATION,         /**< Reflective saturation occurred */
   PROX_DRV_ERROR_AMBIENT_SATURATION,            /**< Ambient saturation occurred */
   PROX_DRV_ERROR_AMBIENT_REFLECTIVE_SATURATION, /**< Ambient & reflective saturation occurred */

   PROX_DRV_ERROR_MAX, /**< Sentinel value */
} PROX_DRV_ERROR;

struct tmd2635_driver; // Forward declaration

/**
 * @brief Interface for the IR proximity sensor driver.
 */
typedef struct ir_proximity_driver_interface ir_proximity_driver_interface_t;
struct ir_proximity_driver_interface
{
   struct tmd2635_driver *parent; /**< Reference to the containing instance. */

   /**
    * @brief Set the sampling rate for proximity readings
    *
    * Configures the relevant registers to adjust the sampling rate of the proximity sensor.
    *
    * @param[in] interface Pointer to the interface instance
    * @param[in] sampling_period_ms Desired sampling period in milliseconds. Maximum value defined by
    * `PROX_SAMPLING_PERIOD_MS_MAX` in the implementation.
    *
    * @return Status code indicating the result of the operation
    *
    * @note The actual sampling period will be set as close as possible to the requested value, according to sensor
    * capabilities.
    */
   result_t (*set_sampling_rate)(const ir_proximity_driver_interface_t *const interface, uint16_t sampling_period_ms);

   /**
    * @brief Wake the IR proximity sensor from sleep mode
    *
    * @param[in] interface Pointer to the interface instance
    *
    * @return Status code indicating the result of the operation
    */
   result_t (*set_wake)(const ir_proximity_driver_interface_t *const interface);

   /**
    * @brief Set the IR proximity sensor to sleep mode
    *
    * @param[in] interface Pointer to the interface instance
    *
    * @return Status code indicating the result of the operation
    */
   result_t (*set_sleep)(const ir_proximity_driver_interface_t *const interface);

   /**
    * @brief Get proximity data
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] data_out Pointer where the proximity data will be stored
    *
    * @return Status code indicating the result of the operation
    *
    * @note Proximity data might be unreliable immediately after waking up the sensor from sleep mode or
    * powering it on. It is recommended to wait for a short period or discard initial readings after such events. Read
    * the driver documentation for more details.
    */
   result_t (*get_proximity_data)(const ir_proximity_driver_interface_t *const interface, uint16_t *data_out);

   /**
    * @brief Get status register value
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] status_val Pointer where the status value will be stored
    *
    * @return Status code indicating the result of the operation
    *
    * @note Reading the status register also clears the status register interrupt flags.
    */
   result_t (*get_status_value)(const ir_proximity_driver_interface_t *const interface, uint8_t *status_val);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // IR_PROX_DRIVER_INTERFACE_H_
