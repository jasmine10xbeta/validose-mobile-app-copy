/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "math.h"
#include <stdio.h>
#include <string.h>
// Custom includes
#include "dose_size_detection.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOSE_SIZE_DETECTION;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @brief Check whether the baseline has been set for dose size detection
 *
 * @param _p_self_ Pointer to the dose size detection instance
 *
 * @return True if the calibration is set, false otherwise
 */
#define IS_DSD_BASELINED(_p_self_) ((_p_self_)->_full_assembly_weight_mg != 0u)

// Convert milliseconds to seconds
#define MS_TO_S(time_ms) ((uint32_t)((time_ms) / COMMON_1K_FACTOR))
// Get sampling time in ms for a given sampling frequency
#define HZ_TO_MS(freq_hz) ((uint32_t)(COMMON_1K_FACTOR / (freq_hz)))
// Check if weight is within bounds based on ring presence
#define IS_IN_BOUNDS(weight_mg, ring_present)                                                                          \
   (((ring_present) ? ((weight_mg) >= DOCK_RING_EMPTY_LOWER_W_MG && (weight_mg) <= DOCK_RING_FULL_UPPER_W_MG) :        \
                      ((weight_mg) >= DOCK_MAX_DRIFT_NEG_MG && (weight_mg) <= DOCK_MAX_DRIFT_MG)))
// Determine if weight is stable or not based on stddev thresholds and ring presence
#define IS_WEIGHT_STABLE(stddev, ring_present)                                                                         \
   ((stddev) <= ((ring_present) ? SIGMA_LOW_RING_PRESENT_MG : SIGMA_LOW_RING_ABSENT_MG))

// Determine if the weight sample can be used for dose size detection
#define CAN_GET_WEIGHT(self) ((self)->_stability_tracker.is_settled && (self)->_stability_tracker.sample_taken)

/* Ensure that `circ_buf_init` won't cause out-of-bounds access on the provided buffer */
#define MAX_BUFFER_CAPACITY STDDEV_LONG_BUFFER_CAP
#if STDDEV_LONG_BUFFER_REPLACED_CAP > MAX_BUFFER_CAPACITY
#   define MAX_BUFFER_CAPACITY STDDEV_LONG_BUFFER_REPLACED_CAP
#endif
STATIC_ASSERT(ARRAY_LEN(((dose_size_detection_t *)0)->_stddev_long_buffer) >= MAX_BUFFER_CAPACITY,
              "Stddev long buffer size is too small to accomodate expected samples");

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions

static result_t get_dose_size_data_state(const dose_size_detection_interface_t *const interface,
                                         DSD_DATA_STATE *data_state_out);
static result_t get_dose_size_data_bad_reason(const dose_size_detection_interface_t *const interface,
                                              DSD_DATA_BAD_REASON *bad_reason_out);
static result_t process(const dose_size_detection_interface_t *const interface, bool is_ring_present);
static result_t try_baseline_if_stable(const dose_size_detection_interface_t *const interface,
                                       bool is_ring_present,
                                       bool *baselining_successful);
static result_t get_full_assembly_weight(const dose_size_detection_interface_t *const interface,
                                         uint32_t *full_assembly_weight_mg_out);

// Non-interface functions

static result_t handle_ring_present_state(dose_size_detection_t *const self);
static result_t handle_ring_absent_state(dose_size_detection_t *const self);
static result_t handle_ring_replaced_state(dose_size_detection_t *const self);
static result_t handle_ring_replaced_settled_state(dose_size_detection_t *const self);
static result_t handle_ring_idle_state(dose_size_detection_t *const self);

static result_t switch_state(dose_size_detection_t *const self, uint64_t now_ms, bool sample_now);
static uint64_t get_sample_time_ms(DSD_FREQ sampling_frequency);

static result_t init_stability_tracker(stability_tracker_t *tracker);
static result_t init_dose_size_data(dose_size_data_t *dose_size_data);
static result_t init_drb_data(drb_data_t *drb_data);
static result_t circ_buffer_init(circ_buf_dsd_t *buffer, uint16_t capacity, int32_t *buf);

static result_t rolling_push(circ_buf_dsd_t *window, int32_t new_value);
static result_t rolling_stddev(circ_buf_dsd_t *window, uint16_t *out_stddev);
static result_t execute_weight_short_stddev(dose_size_detection_t *const self,
                                            int32_t *weight_mg_get,
                                            uint16_t *stddev_short_mg_get);
static result_t
   execute_weight_long_stddev(dose_size_detection_t *const self, int32_t *weight_mg_get, uint16_t *stddev_mg_get);
static result_t update_stability_tracker(stability_tracker_t *stability_tracker,
                                         bool is_weight_in_bounds,
                                         bool is_stable,
                                         bool is_stable_first,
                                         uint64_t current_time_ms);
static result_t is_wait_time_elapsed(dose_size_detection_t *const self, uint64_t now_ms, bool *is_elapsed_out);
static uint32_t int_sqrt(uint64_t value);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
/**
 * @brief Get the sample time in milliseconds for the given sampling frequency
 *
 * @param[in] sampling_frequency The desired sampling frequency enum value
 *
 * @return The sample time in milliseconds or 0u if the sampling frequency is invalid
 */
static uint64_t get_sample_time_ms(DSD_FREQ sampling_frequency)
{
   uint64_t sample_time_ms_out = 0u;

   switch(sampling_frequency)
   {
      case DSD_FREQ_FAST:
         sample_time_ms_out = HZ_TO_MS(FS_FAST_HZ);
         break;
      case DSD_FREQ_SLOW:
         sample_time_ms_out = HZ_TO_MS(FS_SLOW_HZ);
         break;
      case DSD_FREQ_IDLE:
         sample_time_ms_out = T_IDLE_SAMPLE_MS;
         break;
      case DSD_FREQ_MAX:
      // Fallthrough
      default:
         DEBUG_WARNING("Dose size detection state machine provided invalid sampling frequency: %u", sampling_frequency);
         break;
   }

   return sample_time_ms_out;
}

/**
 * @brief Initialize the stability tracker structure.
 *
 * Sets all fields of the stability_tracker_t structure to their default initial values.
 *
 * @param tracker Pointer to the stability_tracker_t instance to initialize
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t init_stability_tracker(stability_tracker_t *tracker)
{
   RETURN_ERR_IF_NULL(tracker, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   tracker->_last_sample_time_ms = 0u;
   tracker->is_stable = false;
   tracker->is_settled = false;
   tracker->last_stable_time_s = 0u;
   tracker->is_weight_in_bounds = false;
   tracker->is_not_moving = false;
   tracker->sample_taken = false;
   tracker->max_time_start_time_s = 0u;

   return RESULT_OK;
}

/**
 * @brief Initialize the dose size data structure.
 *
 * Sets all fields of the dose_size_data_t structure to their default initial values.
 *
 * @param dose_size_data Pointer to the dose_size_data_t instance to initialize
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t init_dose_size_data(dose_size_data_t *dose_size_data)
{
   RETURN_ERR_IF_NULL(dose_size_data, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_data->_dose_size_op2_mg = 0;
   dose_size_data->_dose_size_op1_mg = 0;
   dose_size_data->sigma_total_mg = 0u;
   dose_size_data->_drb_initial_mg = 0;
   dose_size_data->_drift_initial_mg = 0;

   result_t result = init_drb_data(&dose_size_data->drb_pre);
   IF_OK_RUN_AND_UPDATE(result, init_drb_data(&dose_size_data->drift_d1));
   IF_OK_RUN_AND_UPDATE(result, init_drb_data(&dose_size_data->drift_d2));
   IF_OK_RUN_AND_UPDATE(result, init_drb_data(&dose_size_data->drb_post));

   return result;
}

/**
 * @brief Initialize the dose-related baseline (DRB) data structure.
 *
 * Sets all fields of the drb_data_t structure to their default initial values.
 *
 * @param drb_data Pointer to the drb_data_t instance to initialize
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t init_drb_data(drb_data_t *drb_data)
{
   RETURN_ERR_IF_NULL(drb_data, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   drb_data->weight_mg = 0;
   drb_data->sigma_mg = 0u;
   drb_data->is_valid = false;
   drb_data->last_updated_ms = 0u;

   return RESULT_OK;
}

/**
 * @brief Initialize a circular buffer for rolling statistics.
 *
 * Sets up the circular buffer with the provided capacity and buffer array, initializing all fields to default values.
 *
 * @param buffer Pointer to the circ_buf_dsd_t instance to initialize
 * @param capacity The maximum number of elements the buffer can hold
 * @param buf Pointer to the integer array to use as the buffer storage
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t circ_buffer_init(circ_buf_dsd_t *buffer, uint16_t capacity, int32_t *buf)
{
   RETURN_ERR_IF_NULL(buffer, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(buf, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == capacity, DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT);

   buffer->buf = buf;
   buffer->cap = capacity;
   buffer->head = 0u;
   buffer->count = 0u;
   buffer->sum = 0;
   buffer->sum2 = 0u;

   for(uint16_t count = 0u; count < buffer->cap; count++)
   {
      buffer->buf[count] = 0;
   }

   return RESULT_OK;
}

/**
 * @brief Push a new value into the rolling window buffer and update statistics.
 *
 * Adds a new value to the rolling window buffer, updating the buffer and internal counters. Does not calculate or
 * return standard deviation.
 *
 * @param window Pointer to the rolling_window_t structure.
 * @param new_value The value to push into the buffer.
 * @return result_t RESULT_OK on success, DOSE_SIZE_DETECTION_ERROR_PTR_NULL if window is NULL.
 */
static result_t rolling_push(circ_buf_dsd_t *buffer, int32_t new_value)
{
   RETURN_ERR_IF_NULL(buffer, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   buffer->buf[buffer->head++] = new_value;

   if(buffer->head >= buffer->cap)
   {
      buffer->head = 0u;
   }

   if(buffer->count < buffer->cap)
   {
      buffer->count++;
   }

   return RESULT_OK;
}

/**
 * @brief Calculates the standard deviation of the values in the rolling window buffer.
 *
 * Computes the sample standard deviation using all values currently in the rolling window buffer. If there are not
 * enough samples, the output is set to 0. Uses a numerically stable integer algorithm and returns the result as an
 * integer.
 *
 * @param window Pointer to the rolling_window_t structure containing the buffer and statistics.
 * @param out_stddev Pointer to a uint16_t where the computed standard deviation will be stored.
 * @return result_t RESULT_OK on success, DOSE_SIZE_DETECTION_ERROR_PTR_NULL if window or out_stddev is NULL.
 */
static result_t rolling_stddev(circ_buf_dsd_t *buffer, uint16_t *out_stddev)
{
   RETURN_ERR_IF_NULL(buffer, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(out_stddev, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   int64_t sum = 0;
   int64_t sum2 = 0;

   // ---- Compute sum and sum of squares ----
   for(uint16_t count = 0u; count < buffer->count; count++)
   {
      int32_t val = buffer->buf[count];
      sum += val;
      sum2 += (int64_t)val * (int64_t)val;
   }

   buffer->sum = sum;
   buffer->sum2 = (sum2 < 0) ? 0u : (uint64_t)sum2;

   if(buffer->count < 2u)
   {
      // Not enough samples yet
      *out_stddev = 0u;
   }
   else
   {
      // ---- Variance (sample) using stable integer form with overflow and rounding ----
      const uint32_t n32 = buffer->count;
      const uint64_t n = (uint64_t)n32;
      int64_t sum_i = buffer->sum;
      uint64_t sum2_i = buffer->sum2;

      // Try to compute sum*sum in 64-bit with overflow check
      bool overflow = false;
      uint64_t sum_abs = (sum_i >= 0) ? (uint64_t)sum_i : (uint64_t)(-(sum_i + 1)) + 1u;
      uint64_t sum_sq = 0u;
      if(sum_i > 0 && sum_abs > (UINT64_MAX / sum_abs))
      {
         overflow = true;
      }
      else
      {
         sum_sq = sum_abs * sum_abs;
      }

      uint64_t n_sum2 = 0u;
      if(sum2_i > 0u && n > (UINT64_MAX / sum2_i))
      {
         overflow = true;
      }
      else
      {
         n_sum2 = n * sum2_i;
      }

      uint64_t var_mg2 = 0u;
      if(overflow || n <= 1u)
      {
         var_mg2 = 0u;
      }
      else
      {
         uint64_t num64 = (n_sum2 >= sum_sq) ? (n_sum2 - sum_sq) : 0u;
         uint64_t den64 = n * (n - 1u);
         var_mg2 = den64 ? (num64 + (den64 >> 1u)) / den64 : 0u; // rounding division
      }

      uint32_t stddev = int_sqrt(var_mg2);
      *out_stddev = (stddev > UINT16_MAX) ? UINT16_MAX : (uint16_t)stddev;
   }

   return RESULT_OK;
}

/**
 * @brief Execute weight measurement and stability evaluation using short-term standard deviation.
 *
 * Reads the current weight and its short-term standard deviation from the weight sensor interface. Updates the
 * stability tracker based on whether the weight is within bounds and stable.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @param weight_mg_get Pointer to store the measured weight in milligrams
 * @param stddev_short_mg_get Pointer to store the short-term standard deviation in milligrams
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t
   execute_weight_short_stddev(dose_size_detection_t *const self, int32_t *weight_mg_get, uint16_t *stddev_short_mg_get)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(weight_mg_get, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(stddev_short_mg_get, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   const weight_sensor_interface_t *p_weight_sensor_ifc = self->_weight_sensor_interface;
   const system_time_interface_t *p_systick_ifc = self->_system_time_interface;

   weight_data_t weight_data = {0};
   bool is_data_stale = false; // Unused
   uint64_t now_ms = 0u;
   bool is_weight_measurement_stable = false;

   result_t result = p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale);

   if(IS_OK(result))
   {
      self->_stability_tracker.is_weight_in_bounds
         = IS_IN_BOUNDS(weight_data.weight_mg, self->_sm_inputs.is_ring_present);
      is_weight_measurement_stable = IS_WEIGHT_STABLE(weight_data.stddev_mg, self->_sm_inputs.is_ring_present);
      *weight_mg_get = weight_data.weight_mg;
      *stddev_short_mg_get = weight_data.stddev_mg;
   }

   IF_OK_RUN_AND_UPDATE(result, p_systick_ifc->get_time_ms(p_systick_ifc, &now_ms));

   if(IS_OK(result))
   {
      bool stable_first = (0u == self->_stability_tracker.last_stable_time_s);

      result = update_stability_tracker(&self->_stability_tracker,
                                        self->_stability_tracker.is_weight_in_bounds,
                                        is_weight_measurement_stable,
                                        stable_first,
                                        now_ms);
   }

   return result;
}

/**
 * @brief Execute weight measurement and stability evaluation using long-term standard deviation.
 *
 * Reads the current weight from the weight sensor interface and updates the long-term standard deviation buffer.
 * Updates the stability tracker based on whether the weight is within bounds and stable.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @param weight_mg_get Pointer to store the measured weight in milligrams
 * @param stddev_mg_get Pointer to store the standard deviation in milligrams
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t
   execute_weight_long_stddev(dose_size_detection_t *const self, int32_t *weight_mg_get, uint16_t *stddev_mg_get)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(weight_mg_get, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(stddev_mg_get, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   const weight_sensor_interface_t *p_weight_sensor_ifc = self->_weight_sensor_interface;

   weight_data_t weight_data = {0};
   bool is_data_stale = false; // Unused
   bool is_weight_measurement_stable = false;
   uint64_t now_ms = 0u;

   result_t result = p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale);

   if(IS_OK(result))
   {
      self->_stability_tracker.is_weight_in_bounds
         = IS_IN_BOUNDS(weight_data.weight_mg, self->_sm_inputs.is_ring_present);
      *weight_mg_get = weight_data.weight_mg;
      *stddev_mg_get = weight_data.stddev_mg;
   }

   // Update sigma with new sample
   if(self->_stability_tracker.is_weight_in_bounds)
   {
      IF_OK_RUN_AND_UPDATE(result, rolling_push(&self->_stddev_long, weight_data.weight_mg));
      IF_OK_RUN_AND_UPDATE(result, rolling_stddev(&self->_stddev_long, &self->_sigma_long_mg));

      if(IS_OK(result))
      {
         is_weight_measurement_stable = IS_WEIGHT_STABLE(self->_sigma_long_mg, self->_sm_inputs.is_ring_present);
      }
   }

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   if(IS_OK(result))
   {
      bool stable_first = (0u == self->_stability_tracker.last_stable_time_s);

      result = update_stability_tracker(&self->_stability_tracker,
                                        self->_stability_tracker.is_weight_in_bounds,
                                        is_weight_measurement_stable,
                                        stable_first,
                                        now_ms);
   }

   return result;
}

/**
 * @brief Update the stability tracker based on weight and stability conditions.
 *
 * Updates the stability tracker structure with the current weight bounds and stability status, determining if the
 * system is stable or settled based on the provided conditions and timestamps.
 *
 * @param tracker Pointer to the stability_tracker_t instance to update
 * @param is_weight_in_bounds True if the current weight is within acceptable bounds
 * @param is_stable True if the current weight measurement is considered stable
 * @param stable_first True if this is the first stable measurement
 * @param now_ms Current timestamp in milliseconds
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t update_stability_tracker(
   stability_tracker_t *tracker, bool is_weight_in_bounds, bool is_stable, bool stable_first, uint64_t now_ms)
{
   RETURN_ERR_IF_NULL(tracker, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == now_ms, DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT);

   bool first_sample = (is_stable && is_weight_in_bounds && stable_first);
   bool subsequent_stable_sample = (is_stable && is_weight_in_bounds && !(stable_first));
   bool subsequent_unstable_sample = (!(is_stable) || !(is_weight_in_bounds));

   tracker->sample_taken = first_sample || subsequent_stable_sample || subsequent_unstable_sample;

   if(first_sample)
   {
      tracker->last_stable_time_s = MS_TO_S(now_ms);
      tracker->is_stable = true;
      tracker->is_settled = false;
   }
   else if(subsequent_stable_sample)
   {
      uint32_t elapsed_s = MS_TO_S(now_ms) - tracker->last_stable_time_s;
      if(elapsed_s >= SETTLED_WEIGHT_DURATION_SEC)
      {
         tracker->last_stable_time_s = MS_TO_S(now_ms);
         tracker->is_stable = true;
         tracker->is_settled = true;
      }
   }
   else if(subsequent_unstable_sample)
   {
      tracker->is_stable = false;
      tracker->last_stable_time_s = 0u;
      tracker->is_settled = false;
   }

   return RESULT_OK;
}

/**
 * @brief Handle the RING_PRESENT state of the dose size detection state machine.
 *
 * In this state, the system monitors weight measurements to determine the (stable) weight (mg) of the Dock, Ring and
 * Bottle (collectively) before a dose is taken, for the dose size calculation. It updates the dose-related baseline
 * (DRB) data and stability tracker accordingly.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t handle_ring_present_state(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   int32_t weight_mg = 0;
   uint16_t stddev_short_mg = 0;
   uint64_t now_ms = 0u;

   // Check for state first time running
   if(self->_sm_outputs.changed)
   {
      self->_is_next_invalid_sample_the_first = true;
      IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));
      IF_OK_RUN_AND_UPDATE(result, init_drb_data(&self->_dose_size_data.drb_pre));

      if(DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED == self->_previous_state)
      {
         self->_sm_inputs.is_dose_size_computed = false;
      }
   }

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   IF_OK_RUN_AND_UPDATE(result, execute_weight_short_stddev(self, &weight_mg, &stddev_short_mg));

   if((IS_OK(result)) && (CAN_GET_WEIGHT(self)))
   {
      if(self->_dose_size_data.drb_pre.weight_mg != weight_mg
         || self->_dose_size_data.drb_pre.sigma_mg != stddev_short_mg)
      {
         DEBUG_DEBUG("[dsd] DRB pre: %d mg (sigma %u mg)", weight_mg, stddev_short_mg);
      }

      self->_dose_size_data.drb_pre.last_updated_ms = now_ms;
      self->_dose_size_data.drb_pre.weight_mg = weight_mg;
      self->_dose_size_data.drb_pre.sigma_mg = stddev_short_mg;
      self->_dose_size_data.drb_pre.is_valid = true;
      self->_got_valid_weight_sample = true;
      self->_stability_tracker.max_time_start_time_s = 0u;

      // Reset flag for invalid sample tracking
      self->_is_next_invalid_sample_the_first = true;
   }
   else if(IS_OK(result) && self->_is_next_invalid_sample_the_first)
   {
      // Reset max time timer on first invalid weight sample
      self->_stability_tracker.max_time_start_time_s = MS_TO_S(now_ms);
      self->_is_next_invalid_sample_the_first = false;
   }

   return result;
}

/**
 * @brief Handle the RING_ABSENT state of the dose size detection state machine.
 *
 * In this state, the system monitors weight measurements to determine the (stable) weight (mg) of the Dock while a dose
 * is taken, for the dose size calculation. It updates the drift data and stability tracker
 * accordingly.
 *
 * It determines the sigma (long-term standard deviation) of the weight measurements to assess stability.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t handle_ring_absent_state(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   result_t result = RESULT_OK;
   int32_t weight_mg_get = 0;
   uint16_t stddev_mg_get = 0;
   uint64_t now_ms = 0u;

   // Check for state first time running
   if(self->_sm_outputs.changed)
   {
      self->_dose_size_data_state = DSD_DATA_STATE_BUSY;
      self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;

      self->_sigma_long_mg = 0u;
      self->_is_next_invalid_sample_the_first = true;

      result = circ_buffer_init(&(self->_stddev_long), STDDEV_LONG_BUFFER_CAP, self->_stddev_long_buffer);
      IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));
      IF_OK_RUN_AND_UPDATE(result, init_drb_data(&self->_dose_size_data.drift_d1));
      IF_OK_RUN_AND_UPDATE(result, init_drb_data(&self->_dose_size_data.drift_d2));
   }

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   IF_OK_RUN_AND_UPDATE(result, execute_weight_long_stddev(self, &weight_mg_get, &stddev_mg_get));

   if(IS_OK(result) && CAN_GET_WEIGHT(self))
   {
      /* Update the first drift measurement as soon as the ring is removed
         Update the second drift measurement thereafter, until (just before) the ring is replaced
       */
      if(!self->_dose_size_data.drift_d1.is_valid)
      {
         DEBUG_DEBUG("[dsd] drift d1: %d mg", weight_mg_get);

         self->_dose_size_data.drift_d1.weight_mg = weight_mg_get;
         self->_dose_size_data.drift_d1.sigma_mg = stddev_mg_get;
         self->_dose_size_data.drift_d1.is_valid = true;
         self->_dose_size_data.drift_d1.last_updated_ms = now_ms;
      }
      else
      {
         if(self->_dose_size_data.drift_d2.weight_mg != weight_mg_get
            || self->_dose_size_data.drift_d2.sigma_mg != stddev_mg_get)
         {
            DEBUG_DEBUG("[dsd] drift d2: %d mg", weight_mg_get);
         }

         self->_dose_size_data.drift_d2.weight_mg = weight_mg_get;
         self->_dose_size_data.drift_d2.sigma_mg = stddev_mg_get;
         self->_dose_size_data.drift_d2.is_valid = true;
         self->_dose_size_data.drift_d2.last_updated_ms = now_ms;
         // Update state machine inputs only after second drift measurement
         self->_got_valid_weight_sample = true;
      }

      self->_stability_tracker.max_time_start_time_s = 0u;

      // Reset flag for invalid sample tracking
      self->_is_next_invalid_sample_the_first = true;
   }
   else if(IS_OK(result) && self->_is_next_invalid_sample_the_first)
   {
      // Reset max time timer on first invalid weight sample
      self->_stability_tracker.max_time_start_time_s = MS_TO_S(now_ms);
      self->_is_next_invalid_sample_the_first = false;
   }

   return result;
}

/**
 * @brief Handle the RING_REPLACED state of the dose size detection state machine.
 *
 * In this state, the system monitors weight measurements to determine the (stable) weight (mg) of the Dock, Ring and
 * Bottle (collectively) after a dose is taken, for the dose size calculation. It updates the dose-related baseline
 * (DRB) data and stability tracker accordingly.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t handle_ring_replaced_state(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   result_t result = RESULT_OK;
   int32_t weight_mg_get = 0;
   uint16_t stddev_mg_get = 0;
   uint64_t now_ms = 0u;

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   // Check for state first time running
   if(self->_sm_outputs.changed)
   {
      self->_sigma_long_mg = 0u;

      result = circ_buffer_init(&(self->_stddev_long), STDDEV_LONG_BUFFER_REPLACED_CAP, self->_stddev_long_buffer);
      IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));
      self->_stability_tracker.max_time_start_time_s = MS_TO_S(now_ms);
      IF_OK_RUN_AND_UPDATE(result, init_drb_data(&self->_dose_size_data.drb_post));
   }

   IF_OK_RUN_AND_UPDATE(result, execute_weight_long_stddev(self, &weight_mg_get, &stddev_mg_get));

   if((IS_OK(result)) && (CAN_GET_WEIGHT(self)))
   {
      if(self->_dose_size_data.drb_post.weight_mg != weight_mg_get
         || self->_dose_size_data.drb_post.sigma_mg != stddev_mg_get)
      {
         DEBUG_DEBUG("[dsd] DRB post: %d mg", weight_mg_get);
      }

      self->_dose_size_data.drb_post.weight_mg = weight_mg_get;
      self->_dose_size_data.drb_post.sigma_mg = stddev_mg_get;
      self->_dose_size_data.drb_post.is_valid = true;
      self->_dose_size_data.drb_post.last_updated_ms = now_ms;

      // Update state machine inputs
      self->_got_valid_weight_sample = true;
   }

   return result;
}

/**
 * @brief Handle the RING_REPLACED_SETTLED state of the dose size detection state machine.
 *
 * In this state, the system calculates the dose size based on the pre- and post-dose weight measurements and
 * drift corrections. It validates the calculated dose sizes and updates the dose size data accordingly.
 *
 * Two methods are used to calculate the dose size, for robustness:
 * 1. Option 1: Based on the difference between pre-dose and post-dose weights, adjusted for drift.
 * 2. Option 2: Based on the total dispensed weight minus the last valid post-dose weight, adjusted for drift.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t handle_ring_replaced_settled_state(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   result_t result = RESULT_OK;
   int32_t total_dispensed_mg = 0;

   bool data_valid = true;

   if(!self->_dose_size_data.drb_pre.is_valid)
   {
      DEBUG_WARNING("Pre-dose DRB data invalid");
      data_valid = false;
   }
   if(!self->_dose_size_data.drb_post.is_valid)
   {
      DEBUG_WARNING("Post-dose DRB data invalid");
      data_valid = false;
   }
   if(!self->_dose_size_data.drift_d1.is_valid)
   {
      DEBUG_WARNING("First drift DRB data invalid");
      data_valid = false;
   }
   if(!self->_dose_size_data.drift_d2.is_valid)
   {
      DEBUG_WARNING("Second drift DRB data invalid");
      data_valid = false;
   }

   if(false == data_valid)
   {
      self->_dose_size_data_state = DSD_DATA_STATE_BAD;
      self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_INVALID_DRB_DATA;
   }
   else
   {
      DSD_DATA_BAD_REASON bad_reason = DSD_DATA_BAD_REASON_NONE;

      // option 1 dose size calculation
      int32_t rb_pre_mg = self->_dose_size_data.drb_pre.weight_mg - self->_dose_size_data.drift_d1.weight_mg;
      int32_t rb_post_mg = self->_dose_size_data.drb_post.weight_mg - self->_dose_size_data.drift_d2.weight_mg;
      self->_dose_size_data._dose_size_op1_mg = rb_pre_mg - rb_post_mg;

      // option 2 dose size calculation
      total_dispensed_mg = ((int32_t)self->_full_assembly_weight_mg - self->_dose_size_data.drb_post.weight_mg)
                           - self->_dose_size_data.drift_d2.weight_mg;

      // Total sigma calculation
      // Assuming independent samples, thus neglecting covariance terms
      uint16_t sigma_drift = self->_dose_size_data.drift_d2.sigma_mg;
      uint16_t sigma_post = self->_dose_size_data.drb_post.sigma_mg;
      uint64_t variance = (uint64_t)sigma_drift * sigma_drift + (uint64_t)sigma_post * sigma_post;
      uint32_t sigma_total = int_sqrt(variance);
      self->_dose_size_data.sigma_total_mg = (sigma_total > UINT16_MAX) ? UINT16_MAX : (uint16_t)sigma_total;

      if(total_dispensed_mg < 0)
      {
         DEBUG_WARNING("Total dispensed (mg) negative, setting to zero. Invalid value detected.");
         total_dispensed_mg = 0;
         bad_reason = DSD_DATA_BAD_REASON_NEGATIVE_TOTAL_DISPENSED;
      }

      self->_dose_size_data._dose_size_op2_mg = total_dispensed_mg - (int32_t)self->_last_total_dispensed_weight_mg;

      if(self->_dose_size_data._dose_size_op2_mg < 0)
      {
         DEBUG_WARNING("Dose size (mg) negative, setting to zero. Invalid dose size detected.");
         self->_dose_size_data._dose_size_op2_mg = 0;

         bad_reason = DSD_DATA_BAD_REASON_NEGATIVE_DOSE_SIZE;
      }

      if(self->_last_total_dispensed_weight_mg > UINT32_MAX - (uint32_t)self->_dose_size_data._dose_size_op2_mg)
      {
         DEBUG_WARNING("Total dispensed weight overflow. Capping to UINT32_MAX");
         self->_last_total_dispensed_weight_mg = UINT32_MAX;

         bad_reason = DSD_DATA_BAD_REASON_TOTAL_DISPENSED_OVERFLOW;
      }
      else
      {
         self->_last_total_dispensed_weight_mg += (uint32_t)self->_dose_size_data._dose_size_op2_mg; // Will be >= 0
      }

      if(bad_reason != DSD_DATA_BAD_REASON_NONE)
      {
         self->_dose_size_data_state = DSD_DATA_STATE_BAD;
         self->_dose_size_data_bad_reason = bad_reason;
      }
      else
      {
         self->_dose_size_data_state = DSD_DATA_STATE_READY;
         self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;

         DEBUG_DEBUG("[dsd] dose size: %d mg, total: %d mg, sigma total: %u mg",
                     self->_dose_size_data._dose_size_op2_mg,
                     total_dispensed_mg,
                     self->_dose_size_data.sigma_total_mg);
      }
   }

   // Update state machine inputs (regardless of result of computation)
   self->_sm_inputs.is_dose_size_computed = true;

   // Log intermediate data for debugging
   DEBUG_DEBUG("[dsd] DRB pre: %d mg (sigma %u mg) last updated %u ms",
               self->_dose_size_data.drb_pre.weight_mg,
               self->_dose_size_data.drb_pre.sigma_mg,
               self->_dose_size_data.drb_pre.last_updated_ms);
   DEBUG_DEBUG("[dsd] DRB post: %d mg (sigma %u mg) last updated %u ms",
               self->_dose_size_data.drb_post.weight_mg,
               self->_dose_size_data.drb_post.sigma_mg,
               self->_dose_size_data.drb_post.last_updated_ms);
   DEBUG_DEBUG("[dsd] Drift D1: %d mg (sigma %u mg) last updated %u ms",
               self->_dose_size_data.drift_d1.weight_mg,
               self->_dose_size_data.drift_d1.sigma_mg,
               self->_dose_size_data.drift_d1.last_updated_ms);
   DEBUG_DEBUG("[dsd] Drift D2: %d mg (sigma %u mg) last updated %u ms",
               self->_dose_size_data.drift_d2.weight_mg,
               self->_dose_size_data.drift_d2.sigma_mg,
               self->_dose_size_data.drift_d2.last_updated_ms);

   return result;
}

/**
 * @brief Handle the IDLE state of the dose size detection state machine.
 *
 * In this state, the system monitors weight measurements at a much slower rate to save power while still preparing
 * for dose size detection. It updates the stability tracker accordingly.
 *
 * As soon as a stable weight measurement within bounds is detected, it sets the got_valid_sample input to
 * true, indicating readiness for dose size detection and causing the statemachine to transition.
 *
 * This is to ensure that if the Dock's orientation/ movement is unfit for dose size detection (and weight
 * measurements are therefore unstable) for a prolonged period, the system does not attempt dose size detection nor
 * does it sample at a high rate (to conserve power).
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t handle_ring_idle_state(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   const weight_sensor_interface_t *p_weight_sensor_ifc = self->_weight_sensor_interface;

   weight_data_t weight_data = {0};
   bool is_data_stale = false; // Unused
   uint64_t now_ms = 0u;
   bool is_weight_measurement_stable = false;

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   if(self->_sm_outputs.changed)
   {
      DEBUG_WARNING("Entered IDLE state - weight measurements will be taken at a slower rate and dose size detection "
                    "will not be resumed until a stable weight measurement within bounds is detected");
      IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));
      self->_time_enter_idle_state_ms = now_ms;

      IF_OK_RUN_AND_UPDATE(result,
                           p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale));

      // Re-zero weight sensor if drift is low and weight is out of bounds, and measurement was unstable for one
      // minute.
      if(!(self->_sm_inputs.is_ring_present))
      {
         if(weight_data.stddev_mg < DRIFT_SIGMA_MG
            && (weight_data.weight_mg > RE_ZERO_MAX_DRIFT_MG || weight_data.weight_mg < RE_ZERO_MAX_DRIFT_NEG_MG))
         {
            result = p_weight_sensor_ifc->set_zero_offset(p_weight_sensor_ifc, weight_data.weight_mg);
            if(IS_OK(result))
            {
               DEBUG_WARNING("Re-zeroed weight sensor with offset %d mg to compensate for detected drift",
                             weight_data.weight_mg);
            }
            else
            {
               DEBUG_WARNING("Failed to re-zero weight sensor, error code: %d", result);
            }
         }
      }
   }

   if((MS_TO_S(now_ms) - MS_TO_S(self->_time_enter_idle_state_ms)) >= IDLE_STATE_WAIT_MAX_S)
   {
      DEBUG_WARNING("Maximum wait time in IDLE state elapsed - cannot determine dose size, put Dock on a stable "
                    "surface and replace the ring to attempt dose size detection again");
      self->_dose_size_data_state = DSD_DATA_STATE_BAD;
      self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_TIMEOUT;
      self->_seq_interrupted_or_timer_expired = true;
   }

   IF_OK_RUN_AND_UPDATE(result,
                        p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale));

   if(IS_OK(result))
   {
      self->_stability_tracker.is_weight_in_bounds
         = IS_IN_BOUNDS(weight_data.weight_mg, self->_sm_inputs.is_ring_present);
      is_weight_measurement_stable = IS_WEIGHT_STABLE(weight_data.stddev_mg, self->_sm_inputs.is_ring_present);
   }

   if((IS_OK(result)) && (self->_stability_tracker.is_weight_in_bounds) && (is_weight_measurement_stable))
   {
      self->_got_valid_weight_sample = true;
   }

   return result;
}

/**
 * @brief Switch the state of the dose size detection state machine based on current state and inputs.
 *
 * Calls the appropriate handler function for the current state of the state machine, passing in the dose size
 * detection instance. Handles transitions and updates based on the state machine outputs and whether a sample is
 * to be taken, based on the appropriate frequency.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @param now_ms Current timestamp in milliseconds
 * @param sample_now Boolean indicating if a sample is requested now
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t switch_state(dose_size_detection_t *const self, uint64_t now_ms, bool sample_now)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == now_ms, DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT);

   result_t result = RESULT_OK;

   switch(self->_statemachine.current_state)
   {
      case DSD_STATEMACHINE_STATE_RING_PRESENT:
         if((sample_now || self->_sm_outputs.changed) && (false == self->_sm_outputs.is_sequence_interrupted))
         {
            result = handle_ring_present_state(self);
         }
         break;

      case DSD_STATEMACHINE_STATE_RING_ABSENT:
         if(false == self->_sm_outputs.is_sequence_interrupted)
         {
            if((sample_now || self->_sm_outputs.changed))
            {
               result = handle_ring_absent_state(self);
            }
         }
         else
         {
            DEBUG_WARNING("Ring removed before dose size detection could be completed");
            self->_dose_size_data_state = DSD_DATA_STATE_BAD;
            self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_INTERRUPTED;
            self->_seq_interrupted_or_timer_expired = true;
         }
         break;

      case DSD_STATEMACHINE_STATE_RING_REPLACED:
         if((sample_now || self->_sm_outputs.changed) && (false == self->_sm_outputs.is_sequence_interrupted))
         {
            result = handle_ring_replaced_state(self);
         }
         break;

      case DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED:
         if(false == self->_sm_outputs.is_sequence_interrupted)
         {
            result = handle_ring_replaced_settled_state(self);
         }
         break;

      case DSD_STATEMACHINE_STATE_IDLE:
         if((sample_now || self->_sm_outputs.changed) && (false == self->_sm_outputs.is_sequence_interrupted))
         {
            result = handle_ring_idle_state(self);
         }
         break;

      default:
         SET_ERR(result, DSD_FSM_ERROR_INVALID_STATE);
         DEBUG_WARNING("Invalid state %d in dose size detection FSM", self->_statemachine.current_state);
         break;
   }

   return result;
}

/**
 * @brief Check if the wait time for the current state has elapsed.
 *
 * Compares the elapsed time since the start of the wait period with the configured wait time for the current state.
 * Updates the is_max_time_elapsed input accordingly.
 *
 * Firstly, this ensures that the system does not wait indefinitely for a stable weight measurement.
 * Secondly, it ensures that after the ring is replaced, the system keeps sampling at a high frequency for
 * T_SETTLE_RING_S to ensure that enough data is collected for dose size calculation an to ensure that the system is
 * stable after the ring has been replaced.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @param now_ms Current timestamp in milliseconds
 * @param[out] is_elapsed_out Pointer to store whether the wait time has elapsed
 *
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t is_wait_time_elapsed(dose_size_detection_t *const self, uint64_t now_ms, bool *is_elapsed_out)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == now_ms, DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT);
   RETURN_ERR_IF_NULL(is_elapsed_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   uint64_t elapsed_ms = 0u;
   DSD_STATEMACHINE_STATE current_state = self->_statemachine.current_state;

   if(self->_stability_tracker.max_time_start_time_s > 0u)
   {
      elapsed_ms = now_ms - ((uint64_t)self->_stability_tracker.max_time_start_time_s * COMMON_1K_FACTOR);
   }

   switch(current_state)
   {
      case DSD_STATEMACHINE_STATE_RING_PRESENT:
      // Fallthrough
      case DSD_STATEMACHINE_STATE_RING_ABSENT:
      // Fallthrough
      case DSD_STATEMACHINE_STATE_RING_REPLACED:
         *is_elapsed_out = (elapsed_ms >= self->_wait_time_ms);
         break;

      case DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED:
      // Fallthrough
      case DSD_STATEMACHINE_STATE_IDLE:
      // Fallthrough
      case DSD_STATEMACHINE_STATE_MAX:
      // Fallthrough
      default:
         *is_elapsed_out = false;
         break;
   }

   return RESULT_OK;
}

/**
 * @brief Computes the integer square root of a 64-bit unsigned integer using the binary method
 *
 * @param[in] value The 64-bit unsigned integer for which to compute the integer square root
 *
 * @return The integer square root of the input value
 */
static uint32_t int_sqrt(uint64_t value)
{
   uint64_t remainder = value;
   uint64_t out = 0u;
   uint64_t bit = 1ULL << 62u;

   while(bit > remainder)
   {
      bit >>= 2u;
   }

   while(bit != 0u)
   {
      if(remainder >= (out + bit))
      {
         remainder -= (out + bit);
         out = (out >> 1u) + bit;
      }
      else
      {
         out >>= 1u;
      }
      bit >>= 2u;
   }

   return (out > UINT32_MAX) ? UINT32_MAX : (uint32_t)out;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t get_dose_size_data_state(const dose_size_detection_interface_t *const interface,
                                         DSD_DATA_STATE *data_state_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data_state_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);

   *data_state_out = p_self->_dose_size_data_state;

   return RESULT_OK;
}

static result_t get_dose_size_data_bad_reason(const dose_size_detection_interface_t *const interface,
                                              DSD_DATA_BAD_REASON *bad_reason_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(bad_reason_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);

   *bad_reason_out = p_self->_dose_size_data_bad_reason;

   return RESULT_OK;
}

static result_t fetch_dose_size_data(const dose_size_detection_interface_t *const interface,
                                     dose_size_data_out_t *dose_size_data_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(dose_size_data_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;
   bool copy_data = false;

   if(p_self->_dose_size_data_state == DSD_DATA_STATE_BAD)
   {
      SET_ERR(result, DOSE_SIZE_DETECTION_ERROR_DATA_BAD);
      copy_data = true;
   }
   else if(p_self->_dose_size_data_state == DSD_DATA_STATE_NO_DATA)
   {
      SET_ERR(result, DOSE_SIZE_DETECTION_ERROR_NO_DATA);
   }
   else if(p_self->_dose_size_data_state == DSD_DATA_STATE_BUSY)
   {
      SET_ERR(result, DOSE_SIZE_DETECTION_ERROR_BUSY);
   }
   else
   {
      copy_data = true;
   }

   if(copy_data)
   {
      dose_size_data_out->dose_size_mg = (uint32_t)p_self->_dose_size_data._dose_size_op2_mg;     // Will be >= 0
      dose_size_data_out->total_dispensed_mg = (uint32_t)p_self->_last_total_dispensed_weight_mg; // Will be >= 0
      dose_size_data_out->sigma_total_mg = p_self->_dose_size_data.sigma_total_mg;

      p_self->_dose_size_data_state = DSD_DATA_STATE_NO_DATA;
      p_self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;
   }

   return result;
}

static result_t try_baseline_if_stable(const dose_size_detection_interface_t *const interface,
                                       bool is_ring_present,
                                       bool *baselining_successful)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(baselining_successful, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);

   const weight_sensor_interface_t *p_weight_sensor_ifc = p_self->_weight_sensor_interface;
   const system_time_interface_t *p_systick_ifc = p_self->_system_time_interface;

   result_t result = RESULT_OK;
   bool is_first_call = !p_self->_is_baselining_in_progress;
   bool can_baseline = true;

   *baselining_successful = false;

   // Initialize the baselining process on the first call
   if(is_first_call)
   {
      // Cache the baseline data to disable dose size detection calculations until the next baseline is established
      p_self->_full_assembly_weight_mg_before_baselining = p_self->_full_assembly_weight_mg;
      p_self->_last_total_dispensed_weight_mg_before_baselining = p_self->_last_total_dispensed_weight_mg;
      p_self->_full_assembly_weight_mg = 0u;
      p_self->_last_total_dispensed_weight_mg = 0u;

      result = p_systick_ifc->get_time_ms(p_systick_ifc, &p_self->_baselining_start_time_ms);
      if(IS_OK(result))
      {
         p_self->_last_baselining_sample_time_ms = 0u;
         p_self->_is_baselining_in_progress = true;
      }
   }

   // Check calibration state
   if(IS_OK(result))
   {
      WEIGHT_SENSOR_CALIBRATION_STATE calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;

      result = p_weight_sensor_ifc->get_calibration_state(p_weight_sensor_ifc, &calibration_state);
      if(IS_OK(result) && (WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED != calibration_state))
      {
         SET_ERR(result, DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED);
      }
   }

   // Check ring presence
   if(IS_OK(result) && !is_ring_present)
   {
      can_baseline = false;
   }

   // Check stability
   if(IS_OK(result) && can_baseline)
   {
      bool is_stable = false;
      result = p_weight_sensor_ifc->get_is_stable(p_weight_sensor_ifc, &is_stable);

      if(IS_OK(result) && !is_stable)
      {
         can_baseline = false;
      }
   }

   // Fetch the weight data
   if(IS_OK(result) && can_baseline)
   {
      weight_data_t weight_data = {0};
      bool is_data_stale = false;
      result = p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale);

      // Check that the data is new
      if(IS_OK(result) && !is_data_stale && (0u != weight_data.time_ms)
         && (weight_data.time_ms != p_self->_last_baselining_sample_time_ms))
      {
         p_self->_last_baselining_sample_time_ms = weight_data.time_ms;

         // Check that the weight exceeds the threshold to be considered a valid baseline
         if(weight_data.weight_mg > (int32_t)DSD_BASELINE_WEIGHT_THRESHOLD_MG)
         {
            p_self->_full_assembly_weight_mg = (uint32_t)weight_data.weight_mg;
            p_self->_last_total_dispensed_weight_mg = 0u;
            *baselining_successful = true;
            p_self->_is_baselining_in_progress = false;
         }
      }
   }

   // Check baselining timeout
   if(IS_OK(result) && !*baselining_successful)
   {
      uint64_t current_time_ms = 0u;
      result = p_systick_ifc->get_time_ms(p_systick_ifc, &current_time_ms);
      if(IS_OK(result) && ((current_time_ms - p_self->_baselining_start_time_ms) > DSD_BASELINING_TIMEOUT_MS))
      {
         can_baseline = false;
         SET_ERR(result, DOSE_SIZE_DETECTION_ERROR_TIMEOUT);
      }
   }

   // Revert baselining state if baselining was not successful
   if(IS_ERR(result))
   {
      p_self->_full_assembly_weight_mg = p_self->_full_assembly_weight_mg_before_baselining;
      p_self->_last_total_dispensed_weight_mg = p_self->_last_total_dispensed_weight_mg_before_baselining;
      p_self->_is_baselining_in_progress = false;
   }

   return result;
}

static result_t abort_baselining(const dose_size_detection_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);

   if(p_self->_is_baselining_in_progress)
   {
      p_self->_full_assembly_weight_mg = p_self->_full_assembly_weight_mg_before_baselining;
      p_self->_last_total_dispensed_weight_mg = p_self->_last_total_dispensed_weight_mg_before_baselining;
      p_self->_is_baselining_in_progress = false;
   }

   return RESULT_OK;
}

static result_t get_full_assembly_weight(const dose_size_detection_interface_t *const interface,
                                         uint32_t *full_assembly_weight_mg_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(full_assembly_weight_mg_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);

   *full_assembly_weight_mg_out = p_self->_full_assembly_weight_mg;

   return RESULT_OK;
}

static result_t process(const dose_size_detection_interface_t *const interface, bool is_ring_present)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_size_detection_t *p_self = interface->parent;

   RETURN_ERR_IF_TRUE(false == p_self->_initialized, DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(false == IS_DSD_BASELINED(p_self), DOSE_SIZE_DETECTION_ERROR_NOT_BASELINED);

   const weight_sensor_interface_t *p_weight_sensor_ifc = p_self->_weight_sensor_interface;
   const system_time_interface_t *p_systick_ifc = p_self->_system_time_interface;

   WEIGHT_SENSOR_CALIBRATION_STATE calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;
   result_t result = p_weight_sensor_ifc->get_calibration_state(p_weight_sensor_ifc, &calibration_state);
   RETURN_ON_ERR(result);
   RETURN_ERR_IF_TRUE(calibration_state != WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED,
                      DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED);

   uint64_t now_ms = 0u;
   uint64_t sample_time_ms = 0u;
   bool sample_now = false;
   bool is_max_time_elapsed = false;

   if(p_self->_seq_interrupted_or_timer_expired)
   {
      // Restart the dose size detection state machine
      DEBUG_WARNING("Restarting dose size detection state machine after interruption or timer expiry");
      p_self->_seq_interrupted_or_timer_expired = false;

      result = dsd_fsm_init(&p_self->_statemachine);
      IF_OK_RUN_AND_UPDATE(result, init_dose_size_data(&p_self->_dose_size_data));
      IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&p_self->_stability_tracker));

      if(IS_OK(result))
      {
         p_self->_sampling_frequency = DSD_FREQ_SLOW;
         p_self->_wait_time_ms = STABLE_WAIT_TIME_S * COMMON_1K_FACTOR;
      }
   }

   // Check if the wait time for the current state has elapsed
   IF_OK_RUN_AND_UPDATE(result, p_systick_ifc->get_time_ms(p_systick_ifc, &now_ms));

   if(IS_OK(result) && (p_self->_wait_time_ms > 0u))
   {
      result = is_wait_time_elapsed(p_self, now_ms, &is_max_time_elapsed);
   }

   p_self->_previous_state = (DSD_STATEMACHINE_STATE)p_self->_statemachine.current_state;

   // Gather state machine inputs
   p_self->_sm_inputs.is_ring_present = is_ring_present;
   p_self->_sm_inputs.is_max_time_elapsed = is_max_time_elapsed;
   p_self->_sm_inputs.got_valid_weight_sample = p_self->_got_valid_weight_sample;
   // Note that the is_dose_size_computed input is set in the handle_ring_replaced_settled_state function after dose
   // size computation and cleared in handle_ring_present_state function to ensure proper transition from
   // RING_REPLACED_SETTLED back to RING_PRESENT state if the data is fetched early.

   // Clear state machine outputs
   memset(&p_self->_sm_outputs, 0, sizeof(p_self->_sm_outputs));

   // Clear stale flags
   p_self->_got_valid_weight_sample = false;

   // Update state machine
   p_self->_statemachine.update_statemachine(&p_self->_statemachine, &p_self->_sm_inputs, &p_self->_sm_outputs);

   // Persist important state machine outputs
   if(p_self->_sm_outputs.changed)
   {
      p_self->_wait_time_ms = p_self->_sm_outputs.wait_time * COMMON_1K_FACTOR;
      p_self->_sampling_frequency = p_self->_sm_outputs.sampling_frequency;
   }

   // Get sample time based on sampling frequency for current state
   if(IS_OK(result))
   {
      sample_time_ms = get_sample_time_ms(p_self->_sampling_frequency);
      if(0u == sample_time_ms)
      {
         SET_ERR(result, DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT);
      }
   }

   // Determine if sample is to be taken now, according to sample time (ms) for current state
   if(IS_OK(result))
   {
      uint64_t sample_time_elapsed_ms = now_ms - p_self->_stability_tracker._last_sample_time_ms;

      if((sample_time_elapsed_ms >= sample_time_ms) || (0u == p_self->_stability_tracker._last_sample_time_ms))
      {
         sample_now = true;
         p_self->_stability_tracker._last_sample_time_ms = now_ms;
      }
   }

   // Handle current state
   if(IS_OK(result))
   {
      result = switch_state(p_self, now_ms, sample_now);
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t dose_size_detection_init(dose_size_detection_t *const self,
                                  const weight_sensor_interface_t *const weight_sensor_interface,
                                  const system_time_interface_t *const system_time_interface,
                                  uint32_t full_assembly_weight_mg,
                                  uint32_t last_total_dispensed_weight_mg)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(system_time_interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(weight_sensor_interface, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   // Zeroize instance
   memset(self, 0, sizeof(dose_size_detection_t));

   // Assign interface
   self->interface.parent = self;
   self->interface.process = process;
   self->interface.try_baseline_if_stable = try_baseline_if_stable;
   self->interface.abort_baselining = abort_baselining;
   self->interface.get_full_assembly_weight = get_full_assembly_weight;
   self->interface.get_dose_size_data_state = get_dose_size_data_state;
   self->interface.get_dose_size_data_bad_reason = get_dose_size_data_bad_reason;
   self->interface.fetch_dose_size_data = fetch_dose_size_data;

   // Assign dependencies
   self->_system_time_interface = system_time_interface;
   self->_weight_sensor_interface = weight_sensor_interface;

   // Initialize internal state
   self->_last_total_dispensed_weight_mg = last_total_dispensed_weight_mg;
   self->_full_assembly_weight_mg = full_assembly_weight_mg;

   self->_dose_size_data_state = DSD_DATA_STATE_NO_DATA;
   self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;

   self->_wait_time_ms = STABLE_WAIT_TIME_S * COMMON_1K_FACTOR;
   self->_sampling_frequency = DSD_FREQ_SLOW;

   self->_is_next_invalid_sample_the_first = true;
   self->_sigma_long_mg = 0u;
   self->_previous_state = DSD_STATEMACHINE_STATE_RING_PRESENT;
   self->_seq_interrupted_or_timer_expired = false;
   self->_is_baselining_in_progress = false;
   self->_baselining_start_time_ms = 0u;
   self->_last_baselining_sample_time_ms = 0u;
   self->_full_assembly_weight_mg_before_baselining = full_assembly_weight_mg;
   self->_last_total_dispensed_weight_mg_before_baselining = last_total_dispensed_weight_mg;
   self->_time_enter_idle_state_ms = 0u;

   // Initialize state machine
   result_t result = dsd_fsm_init(&self->_statemachine); // Initial state DSD_STATEMACHINE_STATE_RING_PRESENT
   IF_OK_RUN_AND_UPDATE(result, init_dose_size_data(&self->_dose_size_data));
   IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
