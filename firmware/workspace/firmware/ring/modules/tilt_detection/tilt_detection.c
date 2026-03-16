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

#include "debug.h"
#include <nrf_delay.h>

#include <math.h>
#include <stdlib.h> // For abs

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_TILT_DETECTION_MODULE;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* Pi fallback when platform math.h does not provide M_PI */
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

static result_t auto_detect_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS *axis);
static result_t set_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS axis);
static result_t clear_tilts(tilt_detection_interface_t *interface);
static result_t get_tilts(tilt_detection_interface_t *interface, uint8_t *new_tilt_count, tilt_data_t *tilts);
static result_t set_module_active(tilt_detection_interface_t *interface);
static result_t set_module_dormant(tilt_detection_interface_t *interface);

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

static result_t auto_detect_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS *axis)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(axis, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   const imu_interface_t *p_imu_ifc = p_self->_imu_interface;

   imu_data_entry_t imu_data[AUTO_DETECT_AVG_SAMPLE_COUNT] = {0};
   int32_t sum_x = 0, sum_y = 0, sum_z = 0;
   uint16_t count = 0u;

   // Ensure the IMU is active if
   result_t result = p_imu_ifc->set_state(p_imu_ifc, IMU_STATE_ACTIVE);
   UPDATE_ERR(result, TILT_DETECTION_ERROR_SET_IMU_ACTIVE);

   // Clear the old IMU data
   if(IS_OK(result))
   {
      result = p_imu_ifc->clear_imu_data(p_imu_ifc);
      UPDATE_ERR(result, TILT_DETECTION_ERROR_CLEAR_IMU_DATA);
   }

   // Delay to hopefully allow some samples to be collected
   if(IS_OK(result))
   {
      nrf_delay_ms(IMU_MS_PER_SAMPLE);
   }

   // Get the IMU data entry count
   if(IS_OK(result))
   {
      result = p_imu_ifc->get_imu_data_entry_count(p_imu_ifc, &count);
      UPDATE_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT);
   }

   // Make sure we have data and avoid overflow
   if(IS_OK(result))
   {
      if(0u == count)
      {
         SET_ERR(result, TILT_DETECTION_ERROR_NO_IMU_DATA);
      }
      else if(count > AUTO_DETECT_AVG_SAMPLE_COUNT)
      {
         count = AUTO_DETECT_AVG_SAMPLE_COUNT;
      }
   }

   // Get the IMU data
   if(IS_OK(result))
   {
      result = p_imu_ifc->get_imu_data_entries(p_imu_ifc, count, imu_data);
      UPDATE_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRIES);
   }

   if(IS_OK(result))
   {
      // Get the average of the IMU data
      for(uint16_t idx = 0u; idx < count; ++idx)
      {
         sum_x += imu_data[idx].acc_x_axis;
         sum_y += imu_data[idx].acc_y_axis;
         sum_z += imu_data[idx].acc_z_axis;
      }

      int32_t avg_x = sum_x / count;
      int32_t avg_y = sum_y / count;
      int32_t avg_z = sum_z / count;

      // Determine and set the axis
      int32_t abs_x = abs(avg_x);
      int32_t abs_y = abs(avg_y);
      int32_t abs_z = abs(avg_z);

      if(abs_x > abs_y && abs_x > abs_z)
      {
         *axis = avg_x < 0 ? TILT_DETECTION_AXIS_X_INVERTED : TILT_DETECTION_AXIS_X;
      }
      else if(abs_y > abs_x && abs_y > abs_z)
      {
         *axis = avg_y < 0 ? TILT_DETECTION_AXIS_Y_INVERTED : TILT_DETECTION_AXIS_Y;
      }
      else
      {
         *axis = avg_z < 0 ? TILT_DETECTION_AXIS_Z_INVERTED : TILT_DETECTION_AXIS_Z;
      }
   }

   // Set the IMU to dormant
   // Note: Only update the result if no prior errors occurred
   result_t set_dormant_result = p_imu_ifc->set_state(p_imu_ifc, IMU_STATE_DORMANT);
   if(IS_OK(result))
   {
      result = set_dormant_result;
      UPDATE_ERR(result, TILT_DETECTION_ERROR_SET_IMU_DORMANT);
   }

   return result;
}

static result_t set_upright_axis(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS axis)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   bool is_valid = is_valid_tilt_axis(axis);
   RETURN_ERR_IF_TRUE(!is_valid, TILT_DETECTION_ERROR_INVALID_AXIS);

   interface->parent->_upright_axis = axis;

   return RESULT_OK;
}

static result_t clear_tilts(tilt_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   const imu_interface_t *p_imu_ifc = p_self->_imu_interface;

   // Clear the IMU data
   result_t result = p_imu_ifc->clear_imu_data(p_imu_ifc);
   UPDATE_ERR(result, TILT_DETECTION_ERROR_CLEAR_IMU_DATA);

   // Reset tilt detection state
   if(IS_OK(result))
   {
      p_self->_tilt_active = false;
      p_self->_tilt_sample_count = 0u;
   }

   return result;
}

static result_t get_tilts(tilt_detection_interface_t *interface, uint8_t *new_tilt_count, tilt_data_t *tilts)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(new_tilt_count, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(tilts, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   const imu_interface_t *p_imu_ifc = p_self->_imu_interface;
   const system_time_interface_t *p_system_time_ifc = p_self->_system_time_interface;

   bool is_valid_axis = is_valid_tilt_axis(p_self->_upright_axis);
   RETURN_ERR_IF_TRUE(!is_valid_axis, TILT_DETECTION_ERROR_INVALID_AXIS);

   imu_data_entry_t imu_data[IMU_MAX_DATA_ENTRIES] = {0};
   float val = 0.0f;
   uint16_t count = 0u;
   *new_tilt_count = 0u;

   // Ensure the module is active
   result_t result = p_imu_ifc->set_state(p_imu_ifc, IMU_STATE_ACTIVE);
   UPDATE_ERR(result, TILT_DETECTION_ERROR_SET_IMU_ACTIVE);

   // Get the IMU data entry count
   if(IS_OK(result))
   {
      result = p_imu_ifc->get_imu_data_entry_count(p_imu_ifc, &count);
      UPDATE_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT);
   }

   // Clamp count to maximum allowed entries
   if(IS_OK(result) && (count > IMU_MAX_DATA_ENTRIES))
   {
      DEBUG_WARNING("IMU data entry count (%u) exceeds maximum (%u). Clamping.", count, IMU_MAX_DATA_ENTRIES);
      count = IMU_MAX_DATA_ENTRIES;
   }

   if(IS_OK(result) && (0u != count))
   {
      // Get the IMU data
      result = p_imu_ifc->get_imu_data_entries(p_imu_ifc, count, imu_data);
      UPDATE_ERR(result, TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRIES);

      // Process the IMU data
      for(uint16_t sample = 0u; (IS_OK(result) && (sample < count)); sample++)
      {
         if(p_self->_tilt_active)
         {
            p_self->_tilt_sample_count++;
         }

         switch(p_self->_upright_axis)
         {
            case TILT_DETECTION_AXIS_X:
            // Fallthrough
            case TILT_DETECTION_AXIS_X_INVERTED:
               val = (float)imu_data[sample].acc_x_axis;
               break;

            case TILT_DETECTION_AXIS_Y:
            // Fallthrough
            case TILT_DETECTION_AXIS_Y_INVERTED:
               val = (float)imu_data[sample].acc_y_axis;
               break;

            case TILT_DETECTION_AXIS_Z:
            // Fallthrough
            case TILT_DETECTION_AXIS_Z_INVERTED:
               val = (float)imu_data[sample].acc_z_axis;
               break;

            case TILT_DETECTION_AXIS_UNKNOWN:
            // Fallthrough
            case TILT_DETECTION_AXIS_MAX:
            // Fallthrough
            default:
               SET_ERR(result, TILT_DETECTION_ERROR_INVALID_AXIS);
         }

         // Invert if necessary
         if(IS_OK(result)
            && (p_self->_upright_axis >= TILT_DETECTION_AXIS_X_INVERTED
                && p_self->_upright_axis <= TILT_DETECTION_AXIS_Z_INVERTED))
         {
            val = -val;
         }

         // Normalize
         if(IS_OK(result))
         {
            float ax = (float)imu_data[sample].acc_x_axis;
            float ay = (float)imu_data[sample].acc_y_axis;
            float az = (float)imu_data[sample].acc_z_axis;

            float norm = sqrtf(ax * ax + ay * ay + az * az);
            if(norm < 1e-6f) // Prevent divide by zero
            {
               norm = 1.0f;
            }

            val /= norm;

            // Clamp value to valid range for acosf
            // Note: val cannot be NaN before this point, so only need to check bounds
            if(val > 1.0f)
            {
               val = 1.0f;
            }
            else if(val < -1.0f)
            {
               val = -1.0f;
            }
         }

         // Handle tilt detection
         if(IS_OK(result))
         {
            float angle = acosf(val);

            if(angle > DEG_TO_RAD(TILT_DETECTION_UPPER_LIMIT_DEGREES) && !p_self->_tilt_active)
            {
               DEBUG_DEBUG("Tilt Active");
               p_self->_tilt_active = true;
            }
            else if(angle < DEG_TO_RAD(TILT_DETECTION_LOWER_LIMIT_DEGREES) && p_self->_tilt_active)
            {
               DEBUG_DEBUG("Tilt Over");
               p_self->_tilt_active = false;
               uint32_t tilt_duration_ms = (uint32_t)(p_self->_tilt_sample_count * IMU_MS_PER_SAMPLE);
               p_self->_tilt_sample_count = 0u;

               if((tilt_duration_ms >= TILT_DETECTION_DURATION_THRESHOLD_MS)
                  && (*new_tilt_count < TILT_DETECTION_MAX_TILTS))
               {
                  uint64_t current_time_ms = 0u;
                  result = p_system_time_ifc->get_time_ms(p_system_time_ifc, &current_time_ms);

                  if(IS_OK(result))
                  {
                     tilts[*new_tilt_count].detected_at_time_ms = current_time_ms;
                     tilts[*new_tilt_count].duration_ms = tilt_duration_ms;
                     (*new_tilt_count)++;
                  }
                  else
                  {
                     DEBUG_ERROR("Failed to get system time. Tilt not recorded. (result: 0x%04X)", result);
                  }
               }
               else
               {
                  DEBUG_DEBUG("Tilt discarded. Duration %lu ms", tilt_duration_ms);
               }
            }
         }
      }
   }

   return result;
}

static result_t set_module_active(tilt_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   const imu_interface_t *p_imu_ifc = p_self->_imu_interface;

   result_t result = p_imu_ifc->set_state(p_imu_ifc, IMU_STATE_ACTIVE);
   UPDATE_ERR(result, TILT_DETECTION_ERROR_SET_IMU_ACTIVE);

   // Reset tilt detection state
   if(IS_OK(result))
   {
      p_self->_tilt_active = false;
      p_self->_tilt_sample_count = 0u;
   }

   return result;
}

static result_t set_module_dormant(tilt_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, TILT_DETECTION_ERROR_PTR_NULL);

   tilt_detection_t *p_self = interface->parent;
   const imu_interface_t *p_imu_ifc = p_self->_imu_interface;

   result_t result = p_imu_ifc->set_state(p_imu_ifc, IMU_STATE_DORMANT);
   UPDATE_ERR(result, TILT_DETECTION_ERROR_SET_IMU_DORMANT);

   // Reset tilt detection state
   if(IS_OK(result))
   {
      p_self->_tilt_active = false;
      p_self->_tilt_sample_count = 0u;
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t tilt_detection_init(tilt_detection_t *const self,
                             const system_time_interface_t *const system_time,
                             const imu_interface_t *const imu_interface)
{
   RETURN_ERR_IF_NULL(self, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(system_time, TILT_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(imu_interface, TILT_DETECTION_ERROR_PTR_NULL);

   // Assign interface
   self->interface.parent = self;
   self->interface.auto_detect_upright_axis = auto_detect_upright_axis;
   self->interface.set_upright_axis = set_upright_axis;
   self->interface.clear_tilts = clear_tilts;
   self->interface.get_tilts = get_tilts;
   self->interface.set_module_active = set_module_active;
   self->interface.set_module_dormant = set_module_dormant;

   // Assign dependencies
   self->_system_time_interface = system_time;
   self->_imu_interface = imu_interface;

   // Initialize private data
   self->_upright_axis = TILT_DETECTION_DEFAULT_UPRIGHT_AXIS;
   self->_tilt_active = false;
   self->_tilt_sample_count = 0u;

   self->_is_initialized = true;

   return RESULT_OK;
}
