/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file system_time.c
 * @ingroup mock/modules/system_time_module
 * @brief Mock implementation of the system time module for unit testing
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "system_time.h"

#include <stdint.h>

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_SYSTEM_TIME;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t mock_inc_time_by_1_ms(const system_time_interface_t *const interface);
static result_t mock_inc_time_by_10_ms(const system_time_interface_t *const interface);
static result_t mock_inc_time_by_100_ms(const system_time_interface_t *const interface);
static result_t mock_inc_time_by_1000_ms(const system_time_interface_t *const interface);
static result_t mock_inc_time_by_set_val_ms(const system_time_interface_t *const interface, uint16_t value_ms);
static result_t mock_set_time_ms(const system_time_interface_t *const interface, uint64_t value_ms);
static result_t mock_get_time_ms(const system_time_interface_t *const interface, uint64_t *current_time_ms);

// Non-interface functions
static result_t check_for_overflow(uint64_t current_time_ms, uint64_t value_before_increment);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t mock_inc_time_by_1_ms(const system_time_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);

   system_time_t *self = interface->parent;
   uint64_t value_before_inc = self->_current_time_ms;
   self->_current_time_ms += 1u;

   return check_for_overflow(self->_current_time_ms, value_before_inc);
}

static result_t mock_inc_time_by_10_ms(const system_time_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);

   system_time_t *self = interface->parent;

   uint64_t value_before_inc = self->_current_time_ms;
   self->_current_time_ms += 10u;
   return check_for_overflow(self->_current_time_ms, value_before_inc);
}

static result_t mock_inc_time_by_100_ms(const system_time_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);

   system_time_t *self = interface->parent;

   uint64_t value_before_inc = self->_current_time_ms;
   self->_current_time_ms += 100u;
   return check_for_overflow(self->_current_time_ms, value_before_inc);
}

static result_t mock_inc_time_by_1000_ms(const system_time_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);

   system_time_t *self = interface->parent;

   uint64_t value_before_inc = self->_current_time_ms;
   self->_current_time_ms += 1000u;
   return check_for_overflow(self->_current_time_ms, value_before_inc);
}

static result_t mock_inc_time_by_set_val_ms(const system_time_interface_t *const interface, uint16_t value_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);

   system_time_t *self = interface->parent;

   uint64_t value_before_inc = self->_current_time_ms;
   self->_current_time_ms += value_ms;
   return check_for_overflow(self->_current_time_ms, value_before_inc);
}

static result_t mock_set_time_ms(const system_time_interface_t *const interface, uint64_t value_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);

   system_time_t *self = interface->parent;

   self->_current_time_ms = value_ms;
   return RESULT_OK;
}

static result_t mock_get_time_ms(const system_time_interface_t *const interface, uint64_t *current_time_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(current_time_ms, SYSTEM_TIME_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   const system_time_t *self = interface->parent;

   *current_time_ms = self->_current_time_ms;
   return result;
}

/***********************************************************************************************************************
 * Static local function definitions
 **********************************************************************************************************************/

static result_t check_for_overflow(uint64_t current_time_ms, uint64_t value_before_increment)
{
   RETURN_ERR_IF_TRUE(current_time_ms < value_before_increment, SYSTEM_TIME_ERROR_OVERFLOW);
   result_t result = RESULT_OK;

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t system_time_init(system_time_t *const self)
{
   RETURN_ERR_IF_NULL(self, MOCK_SYSTEM_TIME_ERROR_NULL);

   // Assign interface
   self->interface.parent = self;
   self->interface.inc_time_by_1_ms = mock_inc_time_by_1_ms;
   self->interface.inc_time_by_10_ms = mock_inc_time_by_10_ms;
   self->interface.inc_time_by_100_ms = mock_inc_time_by_100_ms;
   self->interface.inc_time_by_1000_ms = mock_inc_time_by_1000_ms;
   self->interface.inc_time_by_set_val_ms = mock_inc_time_by_set_val_ms;
   self->interface.set_time_ms = mock_set_time_ms;
   self->interface.get_time_ms = mock_get_time_ms;

   // Initialize the current time
   self->_current_time_ms = 0u;

   return RESULT_OK;
}