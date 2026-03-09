/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @brief This unit is a generic system time module that can be implemented with it's own source file or by RTC hardware
 * that does the time keeping. The RTC_SYSTEM_TIME_ERROR enum defines the possible errors that can occur during the
 * system time module's operation and should be adapted to match the time keeping implementation used.
 *
 * The rtc_system_time_init() function arguments should also be adapted to match the time keeping implementation used.
 */
#ifndef RTC_SYSTEM_TIME_H_
#define RTC_SYSTEM_TIME_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "common.h"
#include "i2c_driver_interface.h"
#include "rtc_system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define ERROR_RETURN_VALUE (UINT32_MAX)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct rtc_system_time
{
   rtc_system_time_interface_t interface;

   sys_time_t _system_time;

   uint8_t _ic_i2c_address;
   bool _initialized;
} rtc_system_time_t;
/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t rtc_system_time_init(rtc_system_time_t *const self,
                              uint8_t ic_i2c_address,
                              const i2c_driver_interface_t *i2c_interface,
                              sys_time_t system_time);

#endif // RTC_SYSTEM_TIME_H_