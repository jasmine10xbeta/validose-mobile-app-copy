/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RTC_SYSTEM_TIME_INTERFACE_H_
#define RTC_SYSTEM_TIME_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define SECONDS_PER_MINUTE (60u)
#define SECONDS_PER_HOUR   (3600u)
#define SECONDS_PER_DAY    (86400u)

#define MINUTES_PER_HOUR (60u)
#define MINUTES_PER_DAY  (1440u)

#define DAYS_PER_WEEK  (7u)
#define HOURS_PER_DAY  (24u)
#define MAX_NUM_MONTHS (12u)

/**
 * @brief Strips the time portion from a 32-bit Unix timestamp, leaving only the date portion.
 *
 * @param _unix_ The Unix timestamp to be stripped.
 *
 * @return The Unix timestamp with the time portion stripped.
 */
#define STRIP_TIME_UNIX32(_unix_) ((uint32_t)(_unix_) - ((uint32_t)(_unix_) % (uint32_t)SECONDS_PER_DAY))

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for the RTC system time module
 */
typedef enum
{
   RTC_SYSTEM_TIME_ERROR_NONE = 0,
   RTC_SYSTEM_TIME_ERROR_INIT_GENERAL,
   RTC_SYSTEM_TIME_ERROR_PTR_NULL,
   RTC_SYSTEM_TIME_ERROR_I2C,
   RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE,
   RTC_SYSTEM_TIME_ERROR_OSC_INACTIVE,
   RTC_SYSTEM_TIME_ERROR_VERIFY_FAILED,
   RTC_SYSTEM_TIME_ERROR_MAX,
} RTC_SYSTEM_TIME_ERROR;

typedef struct __attribute__((packed, aligned(1)))
{
   uint8_t seconds;
   uint8_t minutes;
   uint8_t hours;
   uint8_t day;
   uint8_t weekday;
   uint8_t month;
   uint8_t year; // since 2000
} sys_time_t;

typedef enum
{
   SYS_TIME_WEEKDAY_SUNDAY = 0,
   SYS_TIME_WEEKDAY_MONDAY,
   SYS_TIME_WEEKDAY_TUESDAY,
   SYS_TIME_WEEKDAY_WEDNESDAY,
   SYS_TIME_WEEKDAY_THURSDAY,
   SYS_TIME_WEEKDAY_FRIDAY,
   SYS_TIME_WEEKDAY_SATURDAY,
   SYS_TIME_WEEKDAY_MAX
} SYS_TIME_WEEKDAY;

struct rtc_system_time; // Forward declaration

typedef struct rtc_system_time_interface rtc_system_time_interface_t; // Forward declaration

typedef struct rtc_system_time_interface
{
   struct rtc_system_time *parent; // Reference to the containing instance.

   /**
    * @brief Sets the current time with sys_time_t formatted input.
    *
    * @param interface Pointer to the interface structure.
    * @param time Pointer to a structure containing the time to be set.
    *
    * @return Result of the operation.
    */
   result_t (*set_time_systime)(const rtc_system_time_interface_t *const interface, const sys_time_t *time);

   /**
    * @brief Sets the current time with epoch (UNIX) formatted input.
    *
    * @param interface Pointer to the interface structure.
    * @param time Pointer to a variable containing the epoch time to be set.
    *
    * @return Result of the operation.
    */
   result_t (*set_time_unix)(const rtc_system_time_interface_t *const interface, const uint32_t epoch_time);

   /**
    * @brief Retrieves the current time.
    *
    * @param interface Pointer to the interface structure.
    * @param time Pointer to a structure where the current time will be stored.
    *
    * @return Result of the operation.
    */
   result_t (*get_time_systime)(const rtc_system_time_interface_t *const interface, sys_time_t *time);

   /**
    * @brief Retrieves the current Unix time.
    *
    * @note Unix time here is unsigned 32 bit integer instead of the usual signed 32 bit integer.
    *
    * @param interface Pointer to the interface structure.
    * @param epoch_time Pointer to a variable where the current time in epoch format will be stored.
    *
    * @return Result of the operation.
    */
   result_t (*get_time_unix)(const rtc_system_time_interface_t *const interface, uint32_t *epoch_time);

   /**
    * @brief Converts sys_time_t formatted time to epoch (UNIX) time in seconds.
    *
    * @param time The sys_time_t formatted time to convert.
    *
    * @return The converted epoch time in seconds.
    */
   uint32_t (*sys_time_to_unix)(const sys_time_t *p_time);

   /**
    * @brief Converts epoch (UNIX) time in seconds to sys_time_t format.
    *
    * @param unix_timestamp The epoch time in seconds to convert.
    * @param system_time Pointer to a structure where the converted time will be stored.
    *
    * @return The converted sys_time_t formatted time.
    */
   result_t (*unix_to_sys_time)(const uint32_t unix_timestamp, sys_time_t *system_time);

} rtc_system_time_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // RTC_SYSTEM_TIME_INTERFACE_H_
