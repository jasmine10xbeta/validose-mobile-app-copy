/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef STS30_DIS_TEMP_SENSOR_INTERFACE_H
#define STS30_DIS_TEMP_SENSOR_INTERFACE_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stddef.h>
#include <stdint.h>

// Custom includes
#include "result.h"
#include "spi_driver_interface.h"
#include "sts30_commands.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
// Error codes specific to the module.
/**
 * @brief Error values for STS30 temperature sensor.
 *
 * @enum STS30_DIS_TEMP_SENSOR_ERROR
 * @var STS30_DIS_TEMP_SENSOR_ERROR_NONE No error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER Null pointer error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED Not initialized error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_I2C_ERROR I2C communication error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_CRC_MISMATCH CRC mismatch error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT Invalid argument error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET Delay not met error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_TIMER Timer error.
 * @var STS30_DIS_TEMP_SENSOR_ERROR_MAX Maximum error value (sentinel).
 */
typedef enum
{
   STS30_DIS_TEMP_SENSOR_ERROR_NONE = 0,
   STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER,
   STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED,
   STS30_DIS_TEMP_SENSOR_ERROR_I2C_ERROR,
   STS30_DIS_TEMP_SENSOR_ERROR_CRC_MISMATCH,
   STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT,
   STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET,
   STS30_DIS_TEMP_SENSOR_ERROR_TIMER,
   STS30_DIS_TEMP_SENSOR_ERROR_MAX
} STS30_DIS_TEMP_SENSOR_ERROR;

struct sts30_dis_temp_sensor_driver; // Forward declaration
typedef struct sts30_dis_temp_sensor_interface sts30_dis_temp_sensor_interface_t;

/**
 * @brief STS30 DIS Temperature Sensor Interface abstraction to decouple driver from platform-specific implementation.
 */
typedef struct sts30_dis_temp_sensor_interface
{
   /**
    * @brief Reference to the containing driver instance.
    */
   struct sts30_dis_temp_sensor_driver *parent;

   /**
    * @brief Performs a sensor soft reset (I2C command).
    *
    * @param interface Pointer to the STS30 interface.
    * @return result_t indicating success or failure.
    */
   result_t (*soft_reset)(const sts30_dis_temp_sensor_interface_t *const interface);

   // NOTE: Hardware reset not supported on this HW version - pin tied down. Function removed.

   /**
    * @brief Reads the latest temperature measurement.
    *
    * @param interface Pointer to the STS30 interface.
    * @param temp_decidegc Output: temperature in deci-degrees Celsius.
    * @param is_stale Output: true if the value is stale, false if fresh.
    * @return result_t indicating success or failure.
    */
   result_t (*get_temperature)(const sts30_dis_temp_sensor_interface_t *const interface,
                               int16_t *temp_decidegc,
                               bool *is_stale);

   /**
    * @brief Enables or disables the on-chip heater (for plausibility checking only).
    *
    * @param interface Pointer to the STS30 interface.
    * @param enable True to enable, false to disable.
    * @return result_t indicating success or failure.
    */
   result_t (*set_heater)(const sts30_dis_temp_sensor_interface_t *const interface, bool enable);

   /**
    * @brief Processes function to be called periodically by general control, to update temperature
    * readings at a set frequency and process interrupt flags.
    *
    * NB: The temp_read_freq_ms can NOT be smaller than the measurement time for the current mode!
    * i.e. If in single-shot mode with low repeatability (t_meas max = 5ms), the temp_read_freq_ms
    * must be > 5ms. To account for differences in measurement times, it is recommended to use atleast tmeas_max*2 as a
    * safe value for the temp_read_freq_ms parameter.
    *
    * @param interface Pointer to the STS30 interface.
    * @param temp_read_freq_ms Frequency of temperature reads in milliseconds.
    * @return result_t indicating success or failure.
    */
   result_t (*temp_sensor_process)(const sts30_dis_temp_sensor_interface_t *const interfaces,
                                   uint64_t temp_read_freq_ms);

} sts30_dis_temp_sensor_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // STS30_DIS_TEMP_SENSOR_INTERFACE_H