/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "weight_sensor.h"

#include <string.h>

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_WEIGHT_SENSOR;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

// Interface functions
static result_t mock_process(const weight_sensor_interface_t *interface, bool is_ring_present);
static result_t mock_reset_scale(const weight_sensor_interface_t *interface);

static result_t mock_set_zero_offset(const weight_sensor_interface_t *interface, int32_t offset);
static result_t mock_get_zero_offset(const weight_sensor_interface_t *interface, int32_t *out_offset);

static result_t mock_set_calibration_factor(const weight_sensor_interface_t *interface, int32_t factor);
static result_t mock_get_calibration_factor(const weight_sensor_interface_t *interface, int32_t *out_factor);
static result_t mock_get_calibration_state(const weight_sensor_interface_t *interface,
                                           WEIGHT_SENSOR_CALIBRATION_STATE *state_out);

static result_t
   mock_try_tare_if_stable(const weight_sensor_interface_t *interface, bool is_ring_present, bool *is_tare_successful);
static result_t mock_try_calibrate_if_stable(const weight_sensor_interface_t *interface,
                                             uint32_t known_weight_mg,
                                             bool is_ring_present,
                                             bool *is_weight_detected,
                                             bool *is_calibration_successful);

static result_t
   mock_read_weight_data(const weight_sensor_interface_t *interface, weight_data_t *weight_data_out, bool *is_stale);
static result_t mock_get_is_stable(const weight_sensor_interface_t *interface, bool *is_stable);
static result_t mock_get_state(const weight_sensor_interface_t *interface,
                               WEIGHT_SAMPLING_STATE *sampling_state,
                               SAMPLING_FREQUENCY *sampling_frequency);

/***********************************************************************************************************************
 * Static helpers
 **********************************************************************************************************************/

static result_t get_forced_failure(bool should_fail)
{
   result_t result = RESULT_OK;

   if(should_fail)
   {
      SET_ERR(result, MOCK_WEIGHT_SENSOR_ERROR_FORCED_FAILURE);
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t mock_process(const weight_sensor_interface_t *interface, bool is_ring_present)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_process);

   if(IS_OK(result))
   {
      if(is_ring_present)
      {
         self->_mock_sampling_state = WEIGHT_SAMPLING_STATE_RING_PRESENT;
         self->_mock_sampling_frequency = SAMPLING_FREQUENCY_SLOW_RING_ON_HZ;
      }
      else
      {
         self->_mock_sampling_state = WEIGHT_SAMPLING_STATE_RING_ABSENT;
         self->_mock_sampling_frequency = SAMPLING_FREQUENCY_FAST_RING_OFF_HZ;
      }
   }

   return result;
}

static result_t mock_reset_scale(const weight_sensor_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_reset_scale);

   if(IS_OK(result))
   {
      self->_mock_zero_offset = 0;
      self->_calibration_factor = 1;
      self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;
   }

   return result;
}

static result_t mock_set_zero_offset(const weight_sensor_interface_t *interface, int32_t offset)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_set_zero_offset);

   if(IS_OK(result))
   {
      self->_mock_zero_offset = offset;

      if(WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED == self->_calibration_state)
      {
         self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_ZEROED;
      }
   }

   return result;
}

static result_t mock_get_zero_offset(const weight_sensor_interface_t *interface, int32_t *out_offset)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_offset, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_get_zero_offset);

   if(IS_OK(result))
   {
      *out_offset = self->_mock_zero_offset;
   }

   return result;
}

static result_t mock_set_calibration_factor(const weight_sensor_interface_t *interface, int32_t factor)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(0 == factor, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_set_calibration_factor);

   if(IS_OK(result))
   {
      RETURN_ERR_IF_TRUE(WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED == self->_calibration_state,
                         WEIGHT_SENSOR_ERROR_INVALID);

      self->_calibration_factor = factor;
      self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED;
   }

   return result;
}

static result_t mock_get_calibration_factor(const weight_sensor_interface_t *interface, int32_t *out_factor)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_factor, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_get_calibration_factor);

   if(IS_OK(result))
   {
      *out_factor = self->_calibration_factor;
   }

   return result;
}

static result_t mock_get_calibration_state(const weight_sensor_interface_t *interface,
                                           WEIGHT_SENSOR_CALIBRATION_STATE *state_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(state_out, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_get_calibration_state);

   if(IS_OK(result))
   {
      *state_out = self->_calibration_state;
   }

   return result;
}

static result_t
   mock_try_tare_if_stable(const weight_sensor_interface_t *interface, bool is_ring_present, bool *is_tare_successful)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_tare_successful, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_try_tare_if_stable);

   *is_tare_successful = false;

   if(IS_OK(result) && !is_ring_present && self->_mock_is_stable && self->_mock_try_tare_successful)
   {
      self->_mock_zero_offset = self->_latest_weight_data.weight_mg;
      if(WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED == self->_calibration_state)
      {
         self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_ZEROED;
      }
      *is_tare_successful = true;
   }

   return result;
}

static result_t mock_try_calibrate_if_stable(const weight_sensor_interface_t *interface,
                                             uint32_t known_weight_mg,
                                             bool is_ring_present,
                                             bool *is_weight_detected,
                                             bool *is_calibration_successful)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_weight_detected, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_calibration_successful, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(0u == known_weight_mg, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_try_calibrate_if_stable);

   *is_weight_detected = false;
   *is_calibration_successful = false;

   if(IS_OK(result) && !is_ring_present && self->_mock_is_stable && self->_mock_try_calibrate_successful)
   {
      RETURN_ERR_IF_TRUE(WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED == self->_calibration_state,
                         WEIGHT_SENSOR_ERROR_INVALID);

      int64_t net_weight_mg = (int64_t)self->_latest_weight_data.weight_mg - (int64_t)self->_mock_zero_offset;
      if(0 == net_weight_mg)
      {
         SET_ERR(result, WEIGHT_SENSOR_ERROR_INVALID);
      }
      else
      {
         *is_weight_detected = true;

         int64_t numerator = net_weight_mg * (int64_t)COMMON_1K_FACTOR;
         int64_t denominator = (int64_t)known_weight_mg;
         int64_t calibration_factor = (numerator >= 0) ? ((numerator + (denominator / 2)) / denominator)
                                                        : ((numerator - (denominator / 2)) / denominator);

         if(0 == calibration_factor)
         {
            SET_ERR(result, WEIGHT_SENSOR_ERROR_INVALID);
         }
         else
         {
            if(calibration_factor > INT32_MAX)
            {
               calibration_factor = INT32_MAX;
            }
            else if(calibration_factor < INT32_MIN)
            {
               calibration_factor = INT32_MIN;
            }

            self->_calibration_factor = (int32_t)calibration_factor;
            self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED;
            *is_calibration_successful = true;
         }
      }
   }

   return result;
}

static result_t
   mock_read_weight_data(const weight_sensor_interface_t *interface, weight_data_t *weight_data_out, bool *is_stale)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(weight_data_out, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_stale, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_read_weight_data);

   if(IS_OK(result))
   {
      *weight_data_out = self->_latest_weight_data;
      *is_stale = self->_is_weight_data_stale;
   }

   return result;
}

static result_t mock_get_is_stable(const weight_sensor_interface_t *interface, bool *is_stable)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_stable, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_get_is_stable);

   if(IS_OK(result))
   {
      *is_stable = self->_mock_is_stable;
   }

   return result;
}

static result_t mock_get_state(const weight_sensor_interface_t *interface,
                               WEIGHT_SAMPLING_STATE *sampling_state,
                               SAMPLING_FREQUENCY *sampling_frequency)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(sampling_state, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(sampling_frequency, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = get_forced_failure(self->_fail_get_state);

   if(IS_OK(result))
   {
      *sampling_state = self->_mock_sampling_state;
      *sampling_frequency = self->_mock_sampling_frequency;
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_weight_sensor_set_calibration_state(weight_sensor_t *const self, WEIGHT_SENSOR_CALIBRATION_STATE state)
{
   RETURN_ERR_IF_NULL(self, MOCK_WEIGHT_SENSOR_ERROR_NULL);

   self->_calibration_state = state;
   return RESULT_OK;
}

result_t mock_weight_sensor_set_weight_data(weight_sensor_t *const self,
                                            int32_t weight_mg,
                                            uint16_t stddev_mg,
                                            uint64_t time_ms,
                                            bool is_ring_present,
                                            int16_t temp_deciC)
{
   RETURN_ERR_IF_NULL(self, MOCK_WEIGHT_SENSOR_ERROR_NULL);

   self->_latest_weight_data.weight_mg = weight_mg;
   self->_latest_weight_data.stddev_mg = stddev_mg;
   self->_latest_weight_data.time_ms = time_ms;
   self->_latest_weight_data.is_ring_present = is_ring_present;
   self->_latest_weight_data.temp_deciC = temp_deciC;

   return RESULT_OK;
}

result_t mock_weight_sensor_set_data_stale(weight_sensor_t *const self, bool is_stale)
{
   RETURN_ERR_IF_NULL(self, MOCK_WEIGHT_SENSOR_ERROR_NULL);

   self->_is_weight_data_stale = is_stale;
   return RESULT_OK;
}

result_t mock_weight_sensor_set_is_stable(weight_sensor_t *const self, bool is_stable)
{
   RETURN_ERR_IF_NULL(self, MOCK_WEIGHT_SENSOR_ERROR_NULL);

   self->_mock_is_stable = is_stable;
   return RESULT_OK;
}

result_t mock_weight_sensor_init(weight_sensor_t *const self)
{
   RETURN_ERR_IF_NULL(self, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   memset(self, 0, sizeof(*self));

   self->interface.parent = self;

   self->interface.reset_scale = mock_reset_scale;
   self->interface.set_zero_offset = mock_set_zero_offset;
   self->interface.get_zero_offset = mock_get_zero_offset;
   self->interface.set_calibration_factor = mock_set_calibration_factor;
   self->interface.get_calibration_factor = mock_get_calibration_factor;
   self->interface.get_calibration_state = mock_get_calibration_state;
   self->interface.get_state = mock_get_state;
   self->interface.try_tare_if_stable = mock_try_tare_if_stable;
   self->interface.try_calibrate_if_stable = mock_try_calibrate_if_stable;
   self->interface.read_weight_data = mock_read_weight_data;
   self->interface.get_is_stable = mock_get_is_stable;
   self->interface.process = mock_process;

   self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;
   self->_mock_zero_offset = 0;
   self->_calibration_factor = 1;

   self->_mock_sampling_state = WEIGHT_SAMPLING_STATE_RING_PRESENT;
   self->_mock_sampling_frequency = SAMPLING_FREQUENCY_SLOW_RING_ON_HZ;

   self->_latest_weight_data.weight_mg = 0;
   self->_latest_weight_data.stddev_mg = 0u;
   self->_latest_weight_data.temp_deciC = 0;
   self->_latest_weight_data.is_ring_present = true;
   self->_latest_weight_data.time_ms = 0u;
   self->_is_weight_data_stale = false;

   self->_mock_is_stable = true;
   self->_mock_try_tare_successful = true;
   self->_mock_try_calibrate_successful = true;

   self->_initialized = true;

   return RESULT_OK;
}
