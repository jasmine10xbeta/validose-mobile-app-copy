/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "nrf_delay.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "rtc_mock.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_RTC_PCF85363A_DRIVER;

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
static result_t mock_set_time_systime(const rtc_system_time_interface_t *const interface, const sys_time_t *time);
static result_t mock_set_time_epoch(const rtc_system_time_interface_t *const interface, const uint32_t unix_timestamp);
static result_t mock_get_time_systime(const rtc_system_time_interface_t *const interface, sys_time_t *time);
static result_t mock_get_time_epoch(const rtc_system_time_interface_t *const interface, uint32_t *epoch_time);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t mock_set_time_systime(const rtc_system_time_interface_t *const interface, const sys_time_t *time)
{
   if(NULL == interface)
   {
      return RESULT_NO_ERROR;
   }

   if(NULL == time)
   {
      return RESULT_NO_ERROR;
   }
   interface->parent->_system_time = *time;
   return RESULT_OK;
}

static result_t mock_set_time_epoch(const rtc_system_time_interface_t *const interface, const uint32_t unix_timestamp)
{
   result_t result = RESULT_OK;

   if(NULL == interface)
   {
      return RESULT_NO_ERROR;
   }
   interface->parent->_epoch = unix_timestamp;
   return RESULT_OK;
}

static result_t mock_get_time_systime(const rtc_system_time_interface_t *const interface, sys_time_t *time)
{
   if(NULL == interface)
   {
      return RESULT_NO_ERROR;
   }

   if(NULL == time)
   {
      return RESULT_NO_ERROR;
   }
   *time = interface->parent->_system_time;
   return RESULT_OK;
}

static result_t mock_get_time_epoch(const rtc_system_time_interface_t *const interface, uint32_t *epoch_time)
{
   result_t result = RESULT_OK;
   if(NULL == interface)
   {
      return RESULT_NO_ERROR;
   }

   if(NULL == epoch_time)
   {
      return RESULT_NO_ERROR;
   }
   *epoch_time = interface->parent->_epoch;
   return result;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_rtc_system_time_init(rtc_system_time_t *self)
{
   RETURN_ERR_IF_NULL(self, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   // Assign interface
   self->interface.parent = self;
   self->interface.set_time_systime = mock_set_time_systime;
   self->interface.get_time_systime = mock_get_time_systime;
   self->interface.get_time_unix = mock_get_time_epoch;
   self->interface.set_time_unix = mock_set_time_epoch;

   // Initialize private data
   self->_epoch = 0u;

   self->_initialized = true;

   return RESULT_OK;
}
