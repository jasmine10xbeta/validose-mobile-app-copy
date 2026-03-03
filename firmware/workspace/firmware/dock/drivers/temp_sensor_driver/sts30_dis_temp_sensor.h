/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef STS30_DIS_TEMP_SENSOR_H
#define STS30_DIS_TEMP_SENSOR_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_timer.h"
#include "nrf_drv_spi.h"

// Custom includes
#include "common.h"
#include "i2c_driver.h"
#include "sts30_dis_temp_sensor_interface.h"
#include "system_time.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define CRC_BUFFER_SIZE (2u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Status register bits for the STS30 temperature sensor.
 *
 * @enum STS30_STATUS_REG_BITS
 * @var STS30_STATUS_REG_BITS_CHECKSUM_FAILED Checksum failed status bit.
 * @var STS30_STATUS_REG_BITS_CMD_NOT_PROCESSED Command not processed status bit.
 * @var STS30_STATUS_REG_BITS_RESET_DETECTED Reset detected status bit.
 * @var STS30_STATUS_REG_BITS_ALERT_TRACKING Alert tracking status bit.
 * @var STS30_STATUS_REG_BITS_HEATER_ACTIVE Heater active status bit.
 * @var STS30_STATUS_REG_BITS_ALERT_PENDING Alert pending status bit.
 */
typedef enum
{
   STS30_STATUS_REG_BITS_CHECKSUM_FAILED = 0,
   STS30_STATUS_REG_BITS_CMD_NOT_PROCESSED = 1,
   STS30_STATUS_REG_BITS_RESET_DETECTED = 4,
   STS30_STATUS_REG_BITS_ALERT_TRACKING = 10,
   STS30_STATUS_REG_BITS_HEATER_ACTIVE = 13,
   STS30_STATUS_REG_BITS_ALERT_PENDING = 15
} STS30_STATUS_REG_BITS;

/**
 * @brief Temperature measurement modes for the STS30 sensor.
 *
 * @enum GET_TEMP_MODE
 * @var GET_TEMP_MODE_SINGLE_SHOT Single shot temperature measurement mode.
 * @var GET_TEMP_MODE_PERIODIC Periodic temperature measurement mode.
 */
typedef enum
{
   GET_TEMP_MODE_SINGLE_SHOT = 0,
   GET_TEMP_MODE_PERIODIC
} GET_TEMP_MODE;

/**
 * @brief Command types for the STS30 sensor driver.
 *
 * @enum COMMAND_TYPE
 * @var COMMAND_TYPE_GET_TEMP Command to get temperature.
 * @var COMMAND_TYPE_GET_STATUS Command to get status.
 * @var COMMAND_TYPE_OTHER Other command type.
 */
typedef enum
{
   COMMAND_TYPE_GET_TEMP = 0,
   COMMAND_TYPE_GET_STATUS,
   COMMAND_TYPE_OTHER
} COMMAND_TYPE;

// Forward declarations of the structs since they're interdependent.
typedef struct timer_context timer_context_t;
typedef struct temp_timer temp_timer_t;
typedef struct sts30_dis_temp_sensor_driver sts30_dis_temp_sensor_driver_t;

/**
 * @brief Timer context structure for temperature sensor driver timers
 *
 * @param driver_instance Pointer to the driver instance
 */
struct timer_context
{
   sts30_dis_temp_sensor_driver_t *driver_instance;
};

/**
 * @brief Temperature timer structure for managing app timers
 *
 * @param timer_storage Storage for the app timer
 * @param timer_id App timer ID
 * @param temp_timer_context Context for the timer
 */
struct temp_timer
{
   app_timer_t timer_storage;
   app_timer_id_t timer_id;
   timer_context_t temp_timer_context;
};

/**
 * @brief STS30 DIS Temperature Sensor Driver Struct
 *
 * @param interface STS30 DIS Temperature Sensor Interface
 * @param _initialized Indicates if the driver has been initialized
 * @param _raw_temp_value Raw Temperature Value
 * @param _i2c_address I2C Address of the sensor
 * @param _last_temp_read_time_ms Timestamp of the last temperature read in milliseconds
 * @param _reset_timer_elapsed Indicates if the reset timer has elapsed
 * @param _command_timer_elapsed Indicates if the command timer has elapsed
 * @param _temp_meas_timer_elapsed Indicates if the temperature measurement timer has elapsed
 * @param _command_delay_timer Command Delay Timer
 * @param _reset_delay_timer Reset Delay Timer
 * @param _temp_tmeas_delay_timer Temperature Measurement Delay Timer
 * @param _ss_current_temp_decidegC Single-Shot Current Temperature in deci-degrees Celsius
 * @param _ss_is_temp_measurement_stale Single-Shot Temperature Measurement Stale Flag
 * @param _p_current_temp_decidegC Periodic Current Temperature in deci-degrees Celsius
 * @param _p_is_temp_measurement_stale Periodic Temperature Measurement Stale Flag
 * @param _current_status Current Status Value
 * @param _current_mode Current Temperature Measurement Mode
 * @param _last_command_type Last Command Type Issued
 * @param _i2c_interface I2C Driver Interface
 * @param _system_time_interface System Time Interface
 */
struct sts30_dis_temp_sensor_driver
{
   sts30_dis_temp_sensor_interface_t interface;
   const i2c_driver_interface_t *_i2c_interface;
   const system_time_interface_t *_system_time_interface;

   bool _initialized;
   uint8_t _i2c_address;
   uint64_t _last_temp_read_time_ms;

   bool _reset_timer_elapsed;
   bool _command_timer_elapsed;
   bool _temp_meas_timer_elapsed;

   // For timer handling and context
   // delay enforcement
   temp_timer_t _command_delay_timer;
   temp_timer_t _reset_delay_timer;
   temp_timer_t _temp_tmeas_delay_timer;
   // Single shot mode
   int16_t _ss_current_temp_decidegC;
   bool _ss_is_temp_measurement_stale;
   // Periodic mode
   int16_t _p_current_temp_decidegC;
   bool _p_is_temp_measurement_stale;
   // Get status mode
   status_t _current_status;
   // Mode
   GET_TEMP_MODE _current_mode;
   COMMAND_TYPE _last_command_type;
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initialize the STS30 DIS temperature sensor driver.
 *
 * @param self Pointer to the driver instance to initialize.
 * @param i2c_interface Pointer to the I2C driver interface.
 * @param system_time_interface Pointer to the system time interface.
 * @param i2c_address I2C address of the sensor.
 * @return result_t Result of the initialization.
 */
result_t sts30_dis_temp_sensor_init(sts30_dis_temp_sensor_driver_t *const self,
                                    const i2c_driver_interface_t *i2c_interface,
                                    const system_time_interface_t *system_time_interface,
                                    uint8_t i2c_address);

#endif // STS30_DIS_TEMP_SENSOR_H