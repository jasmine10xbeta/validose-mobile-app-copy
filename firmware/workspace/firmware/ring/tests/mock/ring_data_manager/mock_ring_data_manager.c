/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_data_manager.c
 * @ingroup ring data manager
 * @brief Implementation of the ring data manager module
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "result.h"
#include "ring_data_manager.h"
#include "ring_data_manager_interface.h"

#include <stdio.h>
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
static result_t update_time(const ring_data_manager_interface_t *const interface, uint64_t time_ms);
static result_t get_status(const ring_data_manager_interface_t *const interface, ring_status_t *const status);
static result_t update_battery_sample_rate(const ring_data_manager_interface_t *const interface,
                                           uint16_t sample_rate_millihz);
static result_t update_imu_sample_rate(const ring_data_manager_interface_t *const interface,
                                       uint16_t sample_rate_millihz);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t update_time(const ring_data_manager_interface_t *interface, uint64_t time_ms)
{
   interface->parent->_current_time_ms = time_ms;
   return interface->parent->_result;
}

static result_t get_status(const ring_data_manager_interface_t *interface, ring_status_t *const status)
{
   *status = interface->parent->_status;
   return interface->parent->_result;
}

static result_t update_battery_sample_rate(const ring_data_manager_interface_t *interface, uint16_t sample_rate_millihz)
{
   interface->parent->_battery_sample_rate_millihz = sample_rate_millihz;
   return interface->parent->_result;
}

static result_t update_imu_sample_rate(const ring_data_manager_interface_t *interface, uint16_t sample_rate_millihz)
{
   interface->parent->_imu_sample_rate_millihz = sample_rate_millihz;
   return interface->parent->_result;
}

static result_t update_dose_schedule(const ring_data_manager_interface_t *interface, dose_schedule_t dose_schedule)
{
   interface->parent->_dose_schedule = dose_schedule;
   return interface->parent->_result;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_data_manager_init(ring_data_manager_t *const self)
{
   // Assign parent
   self->interface.parent = self;

   // Map function pointers
   self->interface.update_time = update_time;
   self->interface.get_status = get_status;
   self->interface.update_battery_sample_rate = update_battery_sample_rate;
   self->_dose_schedule = (dose_schedule_t){0};

   return RESULT_OK;
}