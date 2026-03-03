/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tmd2635_driver.h
 * @ingroup ring/drivers/ir_proximity_sensor_driver
 * @brief Header file for the TMD2635 IR proximity sensor driver.
 *
 * Datasheet: https://download.mikroe.com/documents/datasheets/TMD2635_Datasheet.pdf
 */

#ifndef MOCK_TMD2635_DRIVER_H_
#define MOCK_TMD2635_DRIVER_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"
#include "i2c_driver_interface.h"
#include "ir_proximity_driver_interface.h"

#include <stdbool.h>
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define TMD2635_FULL_MASK 0xFFu // Full register clear mask for 8-bit registers

/* PRATE (0x82) */
#define TMD2635_REG_PRATE     0x82u             /**< Duration of a proximity sample. T_sample = (PRATE + 1) * 88us */
#define TMD2635_PRATE_MASK    TMD2635_FULL_MASK /**< Mask for PRATE register */
#define TMD2635_PRATE_RESET   0x1Fu             /**< Reset value for PRATE register (T_sample = ~2.8 ms) */
#define TMD2635_PRATE_TICK_US 88u               /**< Proximity sample time tick in microseconds */

/* PDATA (0x9C/0x9D) */
#define TMD2635_PDATA14_MASK 0x3FFFu /**< Mask for 14-bit proximity data */

/* PWTIME (0xA9) */
#define TMD2635_REG_PWTIME     0xA9u             /**< Proximity wait time */
#define TMD2635_PWTIME_MASK    TMD2635_FULL_MASK /**< Mask for PWTIME register */
#define TMD2635_PWTIME_RESET   0x00u             /**< Reset value for PWTIME register */
#define TMD2635_PWTIME_TICK_US 2780u             /**< Proximity wait time unit in microseconds (2.78 ms) */

/* STATUS (0x9B) */
#define TMD2635_REG_STATUS   0x9Bu             /**< Device status */
#define TMD2635_STATUS_MASK  TMD2635_FULL_MASK /**< Mask for STATUS register */
#define TMD2635_STATUS_RESET 0x00u             /**< Reset value for STATUS register */
#define TMD2635_STATUS_PHIGH                                                                                           \
   (1u << 7) /**< Set when PINT is set and PDATA > high threshold (after persistence). Cleared when PINT is cleared.   \
              */
#define TMD2635_STATUS_PLOW                                                                                            \
   (1u << 6) /**< Set when PINT is set and PDATA < low threshold (after persistence). Cleared when PINT is cleared. */
#define TMD2635_STATUS_PSAT                                                                                            \
   (1u << 5) /**< Proximity saturation flag indicates that an ambient or reflective-saturation event occurred during a \
                previous proximity cycle. */
#define TMD2635_STATUS_PINT                                                                                            \
   (1u << 4) /**< Proximity interrupt flag indicates that proximity results have exceeded thresholds and persistence   \
                settings. */
#define TMD2635_STATUS_CINT (1u << 3) /**< Calibration interrupt flag indicates that calibration has completed. */
#define TMD2635_STATUS_ZINT                                                                                            \
   (1u << 2) /**< Zero detection interrupt flag indicates that a zero value in PDATA has caused the proximity offset   \
                to be decremented (if AUTO_OFFSET_ADJ = 1). */
#define TMD2635_STATUS_PSAT_REFLECTIVE                                                                                 \
   (1u << 1) /**< The Reflective Proximity Saturation Interrupt flag signals that the AFE has saturated during the IR  \
                VCSEL active portion of proximity integration. */
#define TMD2635_STATUS_PSAT_AMBIENT                                                                                    \
   (1u << 0) /**< The Ambient Proximity Saturation Interrupt flag signals that the AFE has saturated during the IR     \
                VCSEL inactive portion of proximity integration. */
#define TMD2635_STATUS_CLEAR_ALL 0xFFu /**< Mask to clear all status bits by reading the STATUS register */

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief TMD2635 power mode options
 */
typedef enum
{
   TMD2635_POWER_MODE_SLEEP = 0, /**< Low power sleep mode. Will not respond to I2C until in IDLE or ACTIVE mode */
   TMD2635_POWER_MODE_IDLE,      /**< Idle mode. Not measuring proximity data */
   TMD2635_POWER_MODE_ACTIVE,    /**< Active mode. Actively measuring proximity data */

   TMD2635_POWER_MODE_MAX /**< Sentinel value */
} TMD2635_POWER_MODE;

/**
 * @brief Additional error definitions for testing
 */
typedef enum
{
   MOCK_PROX_DRV_ERROR_NONE = PROX_DRV_ERROR_MAX, /**< No error. Starts after the real errors */

   MOCK_PROX_DRV_ERROR_NULL,          /**< Unexpected null reference */
   MOCK_PROX_DRV_ERROR_INVALID_PARAM, /**< Invalid parameter */

   MOCK_TILT_DETECTION_ERROR_MAX, /**< Sentinel value */
} MOCK_TILT_DETECTION_ERROR;

/**
 * @brief Behavioral options for testing
 *
 * Defines a set of boolean failure modes which can be toggled to simulate different failure scenarios during testing.
 */
typedef enum
{
   MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_READ = 0, /**< Simulate failure of I2C read operations */
   MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_WRITE,    /**< Simulate failure of I2C write operations */
   MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_REG_UPDATE,   /**< Simulate failure of register update operations */

   MOCK_TMD2635_BEHAVIOR_OPTION_MAX /**< Sentinel value */
} MOCK_TMD2635_BEHAVIOR_OPTIONS;

/**
 * @brief TMD2635 proximity sensor driver mock instance
 */
typedef struct tmd2635_driver tmd2635_driver_t;
struct tmd2635_driver
{
   // Interface
   ir_proximity_driver_interface_t interface; /**< Instance of proximity driver interface */

   // Private data
   TMD2635_POWER_MODE _power_mode; /**< Current power mode of the proximity sensor */

   uint16_t _mock_proximity_data; /**< Mock proximity data to return on read */
   uint8_t _mock_status_value;    /**< Mock status register value to return on read */
   uint16_t _mock_sampling_rate;  /**< Mock sampling rate set */

   bool _fail_i2c_read;   /**< Whether I2C read operations should fail */
   bool _fail_i2c_write;  /**< Whether I2C write operations should fail */
   bool _fail_reg_update; /**< Whether register update should fail (at the update verification operation) */

   bool _is_initialized; /**< Whether the instance is initialized */
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes a TMD2635 proximity sensor driver mock instance
 *
 * @param[in,out] p_self Pointer to the driver mock instance to initialize
 *
 * @return Status code indicating the result of the operation
 */
result_t mock_tmd2635_driver_init(tmd2635_driver_t *const p_self);

/**
 * @brief Sets the mock proximity data to be returned on read operations
 *
 * @param[in,out] p_self Pointer to the driver mock instance
 * @param[in] data_in The mock proximity data to set
 *
 * @return Status code indicating the result of the operation
 */
result_t mock_tmd2635_set_proximity_data(tmd2635_driver_t *const p_self, uint16_t data_in);

/**
 * @brief Sets the mock status register value to be returned on read operations
 */
result_t mock_tmd2635_set_status_value(tmd2635_driver_t *const p_self, uint8_t status_in);

/**
 * @brief Configures a behavior option for the mock driver
 *
 * @param[in,out] p_self Pointer to the driver mock instance
 * @param[in] option The behavior option to configure
 * @param[in] enable Whether to enable (true) the specified option
 *
 * @return Status code indicating the result of the operation
 */
result_t
   mock_tmd2635_set_behavior_option(tmd2635_driver_t *const p_self, MOCK_TMD2635_BEHAVIOR_OPTIONS option, bool enable);

#endif // MOCK_TMD2635_DRIVER_H_
