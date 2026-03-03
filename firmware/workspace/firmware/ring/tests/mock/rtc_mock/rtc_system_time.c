/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "result.h"
#include "rtc_system_time.h"
#include "rtc_system_time_interface.h"

static const uint8_t THIS_UNIT_ID = SW_UNIT_ID_RTC_PCF85363A_DRIVER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface function
static result_t set_time_systime(const rtc_system_time_interface_t *const interface, const sys_time_t *time);
static result_t set_time_unix(const rtc_system_time_interface_t *const interface, const uint32_t unix_timestamp);
static result_t get_time_systime(const rtc_system_time_interface_t *const interface, sys_time_t *time);
static result_t get_time_unix(const rtc_system_time_interface_t *const interface, uint32_t *unix_time);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t set_time_systime(const rtc_system_time_interface_t *const interface, const sys_time_t *time)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   return RESULT_OK;
}

static result_t set_time_unix(const rtc_system_time_interface_t *const interface, const uint32_t unix_timestamp)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   interface->parent->_epoch = unix_timestamp;

   return RESULT_OK;
}

static result_t get_time_systime(const rtc_system_time_interface_t *const interface, sys_time_t *time)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   return RESULT_OK;
}

static result_t get_time_unix(const rtc_system_time_interface_t *const interface, uint32_t *unix_time)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(unix_time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   *unix_time = interface->parent->_epoch;

   return RESULT_OK;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t rtc_system_time_init(rtc_system_time_t *const self)
{
   result_t result = RESULT_OK;

   RETURN_ERR_IF_NULL(self, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   self->_initialized = false;
   self->interface.parent = self;
   self->interface.set_time_systime = set_time_systime;
   self->interface.get_time_systime = get_time_systime;
   self->interface.get_time_unix = get_time_unix;
   self->interface.set_time_unix = set_time_unix;

   return RESULT_OK;
}
