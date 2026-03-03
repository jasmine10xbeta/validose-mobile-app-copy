/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tilt_detection.c
 * @brief Source file of the tilt detection module
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "tilt_detection.h"

#include "imu_interface.h"

#include <nrf_delay.h>

#include <math.h>
#include <stdlib.h> // For abs

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_TILT_DETECTION_MODULE;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#ifndef M_PI
#   define M_PI 3.14159265358979323846f /* Pi */
#endif

#define DEG_TO_RAD(_deg_) ((_deg_) * (M_PI / 180.0f))
#define RAD_TO_DEG(_rad_) ((_rad_) * (180.0f / M_PI))

/** Maximum number of data entries that can be read from the IMU */
#define IMU_MAX_DATA_ENTRIES (IMU_MAX_FIFO_FRAMES)

/** Milliseconds between IMU samples */
#define IMU_MS_PER_SAMPLE (1000u / IMU_ACTIVE_SAMPLING_RATE_HZ)

/** Number of samples to average during auto-detection */
#define AUTO_DETECT_AVG_SAMPLE_COUNT (10u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

// Interface functions

static result_t mock_auto_detect_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS *axis);
static result_t mock_set_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS axis);
static result_t mock_clear_tilts(tilt_detection_interface_t *interface);
static result_t mock_get_tilts(tilt_detection_interface_t *interface, uint8_t *new_tilt_count, tilt_data_t *tilts);
static result_t mock_set_module_active(tilt_detection_interface_t *interface);
static result_t mock_set_module_dormant(tilt_detection_interface_t *interface);

// Non-interface functions

static bool is_valid_tilt_axis(TILT_DETECTION_AXIS axis);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Checks if the tilt axis is valid
 *
 * @param axis The tilt axis to check
 */
static bool is_valid_tilt_axis(TILT_DETECTION_AXIS axis)
{
   return ((TILT_DETECTION_AXIS_UNKNOWN != axis) && (axis < TILT_DETECTION_AXIS_MAX));
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t mock_auto_detect_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS *axis)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(axis, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   if(p_self->_fail_imu_set_state)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_SET_IMU_ACTIVE);
   }
   else if(p_self->_fail_imu_clear_data)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_CLEAR_IMU_DATA);
   }
   else if(p_self->_fail_imu_get_data_entry_count)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT);
   }
   else if(p_self->_fail_no_imu_data)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_NO_IMU_DATA);
   }
   else if(p_self->_fail_imu_get_data_entries)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRIES);
   }

   if(IS_OK(result))
   {
      *axis = p_self->_mock_detected_axis;
   }

   if(IS_OK(result) && p_self->_fail_imu_set_state)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_SET_IMU_DORMANT);
   }

   return result;
}

static result_t mock_set_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS axis)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   bool is_valid = is_valid_tilt_axis(axis);
   RETURN_ERR_IF_TRUE(!is_valid, TILT_DETECTION_ERROR_INVALID_AXIS);

   interface->parent->_upright_axis = axis;

   return RESULT_OK;
}

static result_t mock_clear_tilts(tilt_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   if(p_self->_fail_imu_clear_data)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_CLEAR_IMU_DATA);
   }

   return result;
}

static result_t mock_get_tilts(tilt_detection_interface_t *interface, uint8_t *new_tilt_count, tilt_data_t *tilts)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(new_tilt_count, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(tilts, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;

   bool is_valid_axis = is_valid_tilt_axis(p_self->_upright_axis);
   RETURN_ERR_IF_TRUE(!is_valid_axis, TILT_DETECTION_ERROR_INVALID_AXIS);

   *new_tilt_count = 0u;

   result_t result = RESULT_OK;

   if(p_self->_fail_imu_set_state)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_SET_IMU_ACTIVE);
   }
   else if(p_self->_fail_imu_get_data_entry_count)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT);
   }

   if(IS_OK(result))
   {
      if(p_self->_fail_imu_get_data_entries)
      {
         SET_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRIES);
      }

      if(IS_OK(result))
      {
         *new_tilt_count = p_self->_mock_tilt_count;
         for(uint8_t idx = 0u; idx < p_self->_mock_tilt_count; idx++)
         {
            tilts[idx] = p_self->_mock_tilts[idx];
         }
      }
   }

   return result;
}

static result_t mock_set_module_active(tilt_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   if(p_self->_fail_imu_set_state)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_SET_IMU_ACTIVE);
   }

   return result;
}

static result_t mock_set_module_dormant(tilt_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   if(p_self->_fail_imu_set_state)
   {
      SET_ERR(result, TILT_DETECTION_ERROR_SET_IMU_DORMANT);
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_tilt_detection_init(tilt_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, TILT_DETECTION_ERROR_PTR_NULL);

   // Assign interface
   self->interface.parent = self;
   self->interface.auto_detect_upright_axis = mock_auto_detect_upright_axis;
   self->interface.set_upright_axis = mock_set_upright_axis;
   self->interface.clear_tilts = mock_clear_tilts;
   self->interface.get_tilts = mock_get_tilts;
   self->interface.set_module_active = mock_set_module_active;
   self->interface.set_module_dormant = mock_set_module_dormant;

   // Initialize private data
   self->_upright_axis = TILT_DETECTION_DEFAULT_UPRIGHT_AXIS;
   self->_mock_detected_axis = TILT_DETECTION_AXIS_Z;
   self->_mock_tilt_count = 0u;
   self->_fail_imu_set_state = false;
   self->_fail_imu_clear_data = false;
   self->_fail_imu_get_data_entry_count = false;
   self->_fail_no_imu_data = false;
   self->_fail_imu_get_data_entries = false;

   self->_is_initialized = true;

   return RESULT_OK;
}