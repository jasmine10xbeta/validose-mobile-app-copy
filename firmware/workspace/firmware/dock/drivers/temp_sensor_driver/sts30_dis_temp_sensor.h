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
#define CRC_BUFFER_SIZE (2u) // CRC is calculated over 2 bytes of data for temperature and status reads
#define MAX_NUM_TEMP_READ_RETRIES                                                                                      \
   (10u) // Max number of retries for reading temperature if ANACK is received before giving up and logging an error
#define MAX_NUM_COMMAND_RETRIES                                                                                        \
   (10u) // Max number of retries for sending a command if ANACK is received before giving up and logging an error
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
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
   uint8_t _read_temp_num_retries;
   uint8_t _command_num_retries;
   bool _temp_meas_timer_elapsed;

   // For timer handling and context
   // delay enforcement
   temp_timer_t _command_delay_timer;
   temp_timer_t _reset_delay_timer;
   temp_timer_t _temp_tmeas_delay_timer;
   // Single shot mode
   bool _is_busy_measuring; // indicates if a measurement command has been sent and we're waiting for it to complete
   bool _is_busy_command;   // indicates if we're waiting for a command gap to elapse before allowing another command
   bool _is_first_read;     // indicates if we've done the first read yet (to handle initial conditions on startup)
   int16_t _ss_current_temp_decidegC;
   bool _ss_is_temp_measurement_stale;
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