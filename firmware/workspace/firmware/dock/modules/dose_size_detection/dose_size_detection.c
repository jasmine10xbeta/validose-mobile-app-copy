/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_size_detection.c
 * @brief Implemtation of the dose size detection module
 *
 * ## Overview of best sample window:
 *
 * The best sample window is used to track the best sample over time so that it is possible to limit the age of the
 * sample used for dose size calculation.
 *
 * Over each 1-second time span, the best sample gathered in that time is tracked as a single entry to the window.
 * "Best" is defined as the valid sample with the lowest standard deviation, with timestamp as a tie-breaker.
 * If the best sample over the whole window falls out of the configured window duration, it is invalidated and a new
 * best sample is selected from the remaining entries in the window.
 *
 * ## Overview of stability tracking:
 *
 * Stability tracking is used to determine when weight measurements can be considered stable enough to be used for dose
 * size calculation. The stability tracker monitors the stability of weight measurements based on their standard
 * deviation. The initial threshold for accepting samples as stable is defined by DSD_SIGMA_INITIAL_THRESHOLD_MG. Once a
 * sample is accepted, its standard deviation is set as the new threshold for subsequent samples to be considered.
 *
 * This ensures that the most stable sample (in the allowed window) is preferred rather than the latest sample and
 * helps mitigate the impact of noise and drift on dose size calculation.
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

/** Convert milliseconds to seconds */
#define MS_TO_S(time_ms) ((uint32_t)((time_ms) / COMMON_1K_FACTOR))

/** Get sampling time in ms for a given sampling frequency */
#define HZ_TO_MS(freq_hz) ((uint32_t)(COMMON_1K_FACTOR / (freq_hz)))

/** Determine if the weight sample can be used for dose size detection */
#define CAN_GET_WEIGHT(self) ((self)->_stability_tracker.is_settled && (self)->_stability_tracker.sample_taken)

/** Time value used to indicate the rolling best-sample window has not been initialized yet. */
#define DSD_BEST_SAMPLE_WINDOW_SECOND_UNINITIALIZED (UINT32_MAX)

/** Whether the rolling best-sample window has been initialized. */
#define IS_BEST_SAMPLE_WINDOW_INITIALIZED(self)                                                                        \
   ((self)->_best_sample_window_latest_second_s != DSD_BEST_SAMPLE_WINDOW_SECOND_UNINITIALIZED)

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
static result_t init_dose_size_data(dose_size_detection_t *self);
static result_t init_dose_data(dsd_sample_t *dose_data);
static result_t
   read_and_ingest_weight_sample(dose_size_detection_t *const self, int32_t *weight_mg_get, uint16_t *stddev_mg_get);
static uint32_t int_sqrt(uint64_t value);
static result_t update_stability_tracker(stability_tracker_t *stability_tracker,
                                         bool is_stable,
                                         bool is_stable_first,
                                         uint64_t current_time_ms);
static result_t is_wait_time_elapsed(dose_size_detection_t *const self, uint64_t now_ms, bool *is_elapsed_out);

static result_t reset_best_sample_window(dose_size_detection_t *self);
static result_t prune_expired_best_window_entries(dose_size_detection_t *self, uint32_t now_s);
static result_t
   is_better_best_sample(const dsd_sample_t *candidate, const dsd_sample_t *current_best, bool *is_better_out);
static result_t recompute_best_sample_idx(dose_size_detection_t *self);
static result_t update_best_window_with_sample(dose_size_detection_t *self,
                                               uint64_t sample_time_ms,
                                               int32_t sample_weight_mg,
                                               uint16_t sample_sigma_mg);
static result_t
   check_is_dsd_blocked(const dose_size_detection_t *const p_self, bool *is_blocked_out, result_t *blocking_error_out);
static result_t reset_dsd_after_interruption(dose_size_detection_t *const p_self);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Reset stability tracking data
 *
 * Resets all stability-related tracking data in the dose size detection instance. This is typically called when
 * entering a new state to ensure stability tracking starts fresh.
 *
 * @param[in] self Pointer to the instance
 *
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t reset_stability_tracking_data(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   result_t result = init_stability_tracker(&self->_stability_tracker);
   IF_OK_RUN_AND_UPDATE(result, reset_best_sample_window(self));

   return result;
}

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
   tracker->is_not_moving = false;
   tracker->sample_taken = false;
   tracker->max_time_start_time_s = 0u;

   return RESULT_OK;
}

static result_t reset_best_sample_window(dose_size_detection_t *self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   memset(self->_best_sample_window, 0, sizeof(self->_best_sample_window));
   self->_best_sample_window_head = 0u;
   self->_best_sample_window_tail = 0u;
   self->_best_sample_idx = DSD_BEST_SAMPLE_IDX_INVALID;
   self->_best_sample_window_latest_second_s = DSD_BEST_SAMPLE_WINDOW_SECOND_UNINITIALIZED;

   return RESULT_OK;
}

/**
 * @brief Prune expired entries from the best sample tracking window based on the current time
 *
 * On each second boundary, the window is advanced and any entries that fall out of the window duration are invalidated.
 * This function updates the tracking variables accordingly.
 *
 * @param[in] self Pointer to the instance
 * @param[in] now_s Current time in seconds used to determine which entries are expired
 *
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t prune_expired_best_window_entries(dose_size_detection_t *self, uint32_t now_s)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   bool is_current_best_pruned = false;

   if(IS_BEST_SAMPLE_WINDOW_INITIALIZED(self) && (now_s < self->_best_sample_window_latest_second_s))
   {
      // Time moved backwards relative to the current window anchor, so reset the window.
      result = reset_best_sample_window(self);
   }
   else if(IS_BEST_SAMPLE_WINDOW_INITIALIZED(self) && (now_s > self->_best_sample_window_latest_second_s))
   {
      uint32_t elapsed_s = now_s - self->_best_sample_window_latest_second_s;
      uint32_t advance_steps = elapsed_s;
      if(advance_steps > DSD_BEST_SAMPLE_WINDOW_LEN)
      {
         advance_steps = DSD_BEST_SAMPLE_WINDOW_LEN;
      }

      for(uint32_t step = 0u; step < advance_steps; step++)
      {
         uint8_t next_head = (uint8_t)((self->_best_sample_window_head + 1u) % DSD_BEST_SAMPLE_WINDOW_LEN);

         if(next_head == self->_best_sample_window_tail)
         {
            if(self->_best_sample_idx == self->_best_sample_window_tail)
            {
               is_current_best_pruned = true;
            }

            self->_best_sample_window_tail
               = (uint8_t)((self->_best_sample_window_tail + 1u) % DSD_BEST_SAMPLE_WINDOW_LEN);
         }

         self->_best_sample_window_head = next_head;
         memset(&self->_best_sample_window[self->_best_sample_window_head], 0, sizeof(dsd_sample_t));
      }

      self->_best_sample_window_latest_second_s = now_s;
   }

   if(IS_OK(result) && is_current_best_pruned)
   {
      result = recompute_best_sample_idx(self);
   }

   return result;
}

static result_t
   is_better_best_sample(const dsd_sample_t *candidate, const dsd_sample_t *current_best, bool *is_better_out)
{
   RETURN_ERR_IF_NULL(candidate, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(current_best, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_better_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   *is_better_out = false;

   if(candidate->is_valid)
   {
      if(!current_best->is_valid)
      {
         *is_better_out = true;
      }
      else if(candidate->sigma_mg < current_best->sigma_mg)
      {
         *is_better_out = true;
      }
      else if((candidate->sigma_mg == current_best->sigma_mg) && (candidate->time_ms > current_best->time_ms))
      {
         *is_better_out = true;
      }
   }

   return RESULT_OK;
}

/**
 * @brief Recompute the index of the best sample in the best sample window
 *
 * This function scans active entries in the best sample window and picks the best valid one:
 * lowest sigma first, and newest timestamp as the tie-breaker.
 *
 * @param[in] self Pointer to the instance
 *
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t recompute_best_sample_idx(dose_size_detection_t *self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   uint8_t best_sample_idx = DSD_BEST_SAMPLE_IDX_INVALID;
   dsd_sample_t *p_best_sample = NULL;

   if(IS_BEST_SAMPLE_WINDOW_INITIALIZED(self))
   {
      uint8_t sample_idx = self->_best_sample_window_tail;
      uint8_t head_idx = self->_best_sample_window_head;

      while(IS_OK(result))
      {
         dsd_sample_t *p_entry = &self->_best_sample_window[sample_idx];

         if(p_entry->is_valid)
         {
            bool is_better = (NULL == p_best_sample);

            if(!is_better)
            {
               result = is_better_best_sample(p_entry, p_best_sample, &is_better);
            }

            if(IS_OK(result) && is_better)
            {
               p_best_sample = p_entry;
               best_sample_idx = sample_idx;
            }
         }

         if(sample_idx == head_idx)
         {
            break;
         }
         sample_idx = (uint8_t)((sample_idx + 1u) % DSD_BEST_SAMPLE_WINDOW_LEN);
      }
   }

   if(IS_OK(result))
   {
      self->_best_sample_idx = best_sample_idx;
   }

   return result;
}

static result_t update_best_window_with_sample(dose_size_detection_t *self,
                                               uint64_t sample_time_ms,
                                               int32_t sample_weight_mg,
                                               uint16_t sample_sigma_mg)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   uint32_t sample_second_s = MS_TO_S(sample_time_ms);
   bool is_sample_valid = (sample_sigma_mg <= DSD_SIGMA_INITIAL_THRESHOLD_MG);

   result_t result = prune_expired_best_window_entries(self, sample_second_s);

   if(IS_OK(result) && !IS_BEST_SAMPLE_WINDOW_INITIALIZED(self))
   {
      self->_best_sample_window_head = 0u;
      self->_best_sample_window_tail = 0u;
      self->_best_sample_window_latest_second_s = sample_second_s;
      memset(&self->_best_sample_window[self->_best_sample_window_head], 0, sizeof(dsd_sample_t));
   }

   if(IS_OK(result) && is_sample_valid)
   {
      uint8_t head_idx = self->_best_sample_window_head;
      dsd_sample_t *p_head_entry = &self->_best_sample_window[head_idx];
      dsd_sample_t candidate
         = {.time_ms = sample_time_ms, .weight_mg = sample_weight_mg, .sigma_mg = sample_sigma_mg, .is_valid = true};
      bool is_better_than_head = false;
      bool should_update_best_idx
         = (self->_best_sample_idx == DSD_BEST_SAMPLE_IDX_INVALID) || (self->_best_sample_idx == head_idx);

      // Head points at the current one-second slot. Only stable samples are eligible for that slot.
      IF_OK_RUN_AND_UPDATE(result, is_better_best_sample(&candidate, p_head_entry, &is_better_than_head));
      if(IS_OK(result) && is_better_than_head)
      {
         *p_head_entry = candidate;

         if(!should_update_best_idx)
         {
            bool is_better_than_window_best = false;
            IF_OK_RUN_AND_UPDATE(result,
                                 is_better_best_sample(&candidate,
                                                       &self->_best_sample_window[self->_best_sample_idx],
                                                       &is_better_than_window_best));
            if(IS_OK(result) && is_better_than_window_best)
            {
               should_update_best_idx = true;
            }
         }

         if(IS_OK(result) && should_update_best_idx)
         {
            self->_best_sample_idx = head_idx;
         }
      }
   }

   return result;
}

/**
 * @brief Check whether DSD is currently blocked
 *
 * Checks the prerequisites for DSD to operate, namely:
 * - Whether the weight sensor is calibrated
 * - Whether baselining is not currently in progress
 * - Whether baselining has been completed (i.e. we have a valid full assembly weight)
 *
 * @param[in] p_self Pointer to the dose size detection instance
 * @param[out] is_blocked_out Pointer to store whether DSD is blocked
 * @param[out] blocking_error_out Pointer to store the specific error code if DSD is blocked
 *
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t
   check_is_dsd_blocked(const dose_size_detection_t *const p_self, bool *is_blocked_out, result_t *blocking_error_out)
{
   RETURN_ERR_IF_NULL(p_self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_blocked_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(blocking_error_out, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   const weight_sensor_interface_t *p_weight_sensor_ifc = p_self->_weight_sensor_interface;

   *is_blocked_out = false;
   *blocking_error_out = RESULT_OK;

   WEIGHT_SENSOR_CALIBRATION_STATE calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;
   result_t result = p_weight_sensor_ifc->get_calibration_state(p_weight_sensor_ifc, &calibration_state);

   if(IS_OK(result))
   {
      bool is_weight_sensor_calibrated = (WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED == calibration_state);
      bool is_baselining_in_progress = p_self->_is_baselining_in_progress;
      bool is_dsd_baselined = (p_self->_full_assembly_weight_mg != 0u);

      if(false == is_weight_sensor_calibrated)
      {
         *is_blocked_out = true;
         SET_ERR(*blocking_error_out, DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED);
      }
      else if(is_baselining_in_progress || (false == is_dsd_baselined))
      {
         *is_blocked_out = true;
         SET_ERR(*blocking_error_out, DOSE_SIZE_DETECTION_ERROR_NOT_BASELINED);
      }
   }

   return result;
}

static result_t reset_dsd_after_interruption(dose_size_detection_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   result_t result = dsd_fsm_init(&p_self->_statemachine);
   // Keep _dose_size_data_state/_dose_size_data_bad_reason unchanged to preserve unread BAD data.
   IF_OK_RUN_AND_UPDATE(result, init_dose_size_data(p_self));
   IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&p_self->_stability_tracker));
   IF_OK_RUN_AND_UPDATE(result, reset_best_sample_window(p_self));

   p_self->_is_latest_sample_valid = false;

   if(IS_OK(result))
   {
      p_self->_sampling_frequency = DSD_FREQ_SLOW;
      p_self->_wait_time_ms = STABLE_WAIT_TIME_S * COMMON_1K_FACTOR;
   }

   return result;
}

/**
 * @brief Initialize the dose size data structure.
 *
 * Sets all dose-size related fields on the module instance to their default initial values.
 *
 * @param self Pointer to the dose_size_detection_t instance to initialize
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t init_dose_size_data(dose_size_detection_t *self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   self->_dose_size_mg = 0;
   self->_sigma_total_mg = 0u;

   result_t result = init_dose_data(&self->_pre_dose_data);
   IF_OK_RUN_AND_UPDATE(result, init_dose_data(&self->_drift_data));
   IF_OK_RUN_AND_UPDATE(result, init_dose_data(&self->_post_dose_data));

   return result;
}

/**
 * @brief Initialize a dose phase data structure.
 *
 * Sets all fields of the dsd_sample_t structure to their default initial values.
 *
 * @param dose_data Pointer to the dsd_sample_t instance to initialize
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t init_dose_data(dsd_sample_t *dose_data)
{
   RETURN_ERR_IF_NULL(dose_data, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   dose_data->weight_mg = 0;
   dose_data->sigma_mg = 0u;
   dose_data->is_valid = false;
   dose_data->time_ms = 0u;

   return RESULT_OK;
}

/**
 * @brief Execute weight measurement and stability evaluation using sensor-reported standard deviation.
 *
 * Reads the current weight from the weight sensor interface and uses the reported standard deviation.
 * Updates the stability tracker based on whether the measured weight is stable.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @param weight_mg_get Pointer to store the measured weight in milligrams
 * @param stddev_mg_get Pointer to store the standard deviation in milligrams
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t
   read_and_ingest_weight_sample(dose_size_detection_t *const self, int32_t *weight_mg_get, uint16_t *stddev_mg_get)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(weight_mg_get, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(stddev_mg_get, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);

   const weight_sensor_interface_t *p_weight_sensor_ifc = self->_weight_sensor_interface;
   const system_time_interface_t *p_systick_ifc = self->_system_time_interface;

   weight_data_t weight_data = {0};
   bool is_data_stale = false;
   uint64_t now_ms = 0u;

   result_t result = p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale);

   if(IS_OK(result) && !is_data_stale)
   {
      result = p_systick_ifc->get_time_ms(p_systick_ifc, &now_ms);
      IF_OK_RUN_AND_UPDATE(
         result,
         update_best_window_with_sample(self, weight_data.time_ms, weight_data.weight_mg, weight_data.stddev_mg));

      if(IS_OK(result))
      {
         const dsd_sample_t *best_sample = NULL;

         if(self->_best_sample_idx != DSD_BEST_SAMPLE_IDX_INVALID)
         {
            const dsd_sample_t *candidate_sample = &self->_best_sample_window[self->_best_sample_idx];
            if(candidate_sample->is_valid)
            {
               best_sample = candidate_sample;
            }
         }

         if(NULL != best_sample)
         {
            *weight_mg_get = best_sample->weight_mg;
            *stddev_mg_get = best_sample->sigma_mg;
         }
      }

      if(IS_OK(result))
      {
         bool stable_first = (0u == self->_stability_tracker.last_stable_time_s);
         bool is_weight_measurement_stable = false;

         if(self->_best_sample_idx != DSD_BEST_SAMPLE_IDX_INVALID)
         {
            is_weight_measurement_stable = self->_best_sample_window[self->_best_sample_idx].is_valid;
         }

         IF_OK_RUN_AND_UPDATE(
            result,
            update_stability_tracker(&self->_stability_tracker, is_weight_measurement_stable, stable_first, now_ms));
      }
   }
   else if(IS_OK(result) && is_data_stale)
   {
      // Skip stale samples for dose-size stability decisions.
      self->_stability_tracker.sample_taken = false;
   }

   return result;
}

/**
 * @brief Update the stability tracker based on weight and stability conditions.
 *
 * Updates the stability tracker structure with the current stability status, determining if the
 * system is stable or settled based on the provided stability conditions and timestamps.
 *
 * @param tracker Pointer to the stability_tracker_t instance to update
 * @param is_stable True if the current weight measurement is considered stable
 * @param stable_first True if this is the first stable measurement
 * @param now_ms Current timestamp in milliseconds
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t
   update_stability_tracker(stability_tracker_t *tracker, bool is_stable, bool stable_first, uint64_t now_ms)
{
   RETURN_ERR_IF_NULL(tracker, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == now_ms, DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT);

   bool first_sample = (is_stable && stable_first);
   bool subsequent_stable_sample = (is_stable && !(stable_first));
   bool subsequent_unstable_sample = (!is_stable);

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
 * pre-dose data and stability tracker accordingly.
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

   const system_time_interface_t *p_systick_ifc = self->_system_time_interface;

   // On first run in this state, reset tracking data
   if(self->_sm_outputs.changed)
   {
      result = reset_stability_tracking_data(self);
      ON_ERR_DEBUG_ERROR(result, "[dsd] Failed to reset stability tracking data");

      dsd_sample_t *p_pre_dose_data = &self->_pre_dose_data;
      IF_OK_RUN_AND_UPDATE(result, init_dose_data(p_pre_dose_data));

      self->_is_latest_sample_valid = false;

      if(DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED == self->_previous_state)
      {
         self->_sm_inputs.is_dose_size_computed = false;
      }
   }

   IF_OK_RUN_AND_UPDATE(result, p_systick_ifc->get_time_ms(p_systick_ifc, &now_ms));

   IF_OK_RUN_AND_UPDATE(result, read_and_ingest_weight_sample(self, &weight_mg, &stddev_short_mg));

   bool is_new_sample_valid = CAN_GET_WEIGHT(self);
   bool first_invalid_after_valid = self->_is_latest_sample_valid && !is_new_sample_valid;

   if(IS_OK(result))
   {
      if(is_new_sample_valid)
      {
         bool pre_dose_sample_changed = (!self->_pre_dose_data.is_valid)
                                        || (self->_pre_dose_data.weight_mg != weight_mg)
                                        || (self->_pre_dose_data.sigma_mg != stddev_short_mg);

         self->_pre_dose_data.time_ms = now_ms;
         self->_pre_dose_data.weight_mg = weight_mg;
         self->_pre_dose_data.sigma_mg = stddev_short_mg;
         self->_pre_dose_data.is_valid = true;

         if(pre_dose_sample_changed)
         {
            DEBUG_DEBUG("[dsd] Pre-dose: %d mg [sigma %u mg]", weight_mg, stddev_short_mg);
         }

         self->_got_valid_weight_sample = true;
         self->_stability_tracker.max_time_start_time_s = 0u;

         self->_is_latest_sample_valid = true;
      }
      else if(first_invalid_after_valid)
      {
         // Start max wait timer only when a previously valid stream turns invalid.
         self->_stability_tracker.max_time_start_time_s = MS_TO_S(now_ms);
         self->_is_latest_sample_valid = false;
      }
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
 * It uses the weight sensor's reported sigma values to assess stability.
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

   // On first run in this state
   if(self->_sm_outputs.changed)
   {
      // Reset tracking data
      result = reset_stability_tracking_data(self);
      ON_ERR_DEBUG_ERROR(result, "[dsd] Failed to reset stability tracking data");

      dsd_sample_t *p_drift_end_data = &self->_drift_data;
      IF_OK_RUN_AND_UPDATE(result, init_dose_data(p_drift_end_data));

      self->_is_latest_sample_valid = false;

      // Entering BUSY marks a new sequence and supersedes any unread interrupted BAD state.
      self->_dose_size_data_state = DSD_DATA_STATE_BUSY;
      self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;
   }

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   IF_OK_RUN_AND_UPDATE(result, read_and_ingest_weight_sample(self, &weight_mg_get, &stddev_mg_get));

   bool is_new_sample_valid = CAN_GET_WEIGHT(self);
   bool first_invalid_after_valid = self->_is_latest_sample_valid && !is_new_sample_valid;

   if(IS_OK(result))
   {
      if(is_new_sample_valid)
      {
         bool drift_sample_changed
            = (self->_drift_data.weight_mg != weight_mg_get) || (self->_drift_data.sigma_mg != stddev_mg_get);

         if(drift_sample_changed)
         {
            DEBUG_DEBUG("[dsd] drift: %d mg [sigma: %d mg]", weight_mg_get, stddev_mg_get);
         }

         self->_drift_data.weight_mg = weight_mg_get;
         self->_drift_data.sigma_mg = stddev_mg_get;
         self->_drift_data.is_valid = true;
         self->_drift_data.time_ms = now_ms;

         // Update state machine inputs once a valid drift sample is available.
         self->_got_valid_weight_sample = true;

         self->_stability_tracker.max_time_start_time_s = 0u;
         self->_is_latest_sample_valid = true;
      }
      else if(first_invalid_after_valid)
      {
         // Start max wait timer only when a previously valid stream turns invalid.
         self->_stability_tracker.max_time_start_time_s = MS_TO_S(now_ms);
         self->_is_latest_sample_valid = false;
      }
   }

   return result;
}

/**
 * @brief Handle the RING_REPLACED state of the dose size detection state machine.
 *
 * In this state, the system monitors weight measurements to determine the (stable) weight (mg) of the Dock, Ring and
 * Bottle (collectively) after a dose is taken, for the dose size calculation. It updates the dose-related baseline
 * post-dose data and stability tracker accordingly.
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

   // On first run in this state
   if(self->_sm_outputs.changed)
   {
      // Reset tracking data
      result = reset_stability_tracking_data(self);
      ON_ERR_DEBUG_ERROR(result, "[dsd] Failed to reset stability tracking data");

      dsd_sample_t *p_post_dose_data = &self->_post_dose_data;
      IF_OK_RUN_AND_UPDATE(result, init_dose_data(p_post_dose_data));

      // Start max wait timer in case we don't get valid samples after ring is replaced.
      self->_stability_tracker.max_time_start_time_s = MS_TO_S(now_ms);
   }

   IF_OK_RUN_AND_UPDATE(result, read_and_ingest_weight_sample(self, &weight_mg_get, &stddev_mg_get));

   if((IS_OK(result)) && (CAN_GET_WEIGHT(self)))
   {
      bool post_dose_sample_changed
         = (self->_post_dose_data.weight_mg != weight_mg_get) || (self->_post_dose_data.sigma_mg != stddev_mg_get);

      if(post_dose_sample_changed)
      {
         DEBUG_DEBUG("[dsd] Post-dose: %d mg [sigma: %d mg]", weight_mg_get, stddev_mg_get);
      }

      self->_post_dose_data.weight_mg = weight_mg_get;
      self->_post_dose_data.sigma_mg = stddev_mg_get;
      self->_post_dose_data.is_valid = true;
      self->_post_dose_data.time_ms = now_ms;

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
 * Dose size is calculated from total dispensed weight and drift correction while the ring was absent.
 *
 * @param self Pointer to the dose_size_detection_t instance
 * @return result_t RESULT_OK on success, error code otherwise
 */
static result_t handle_ring_replaced_settled_state(dose_size_detection_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOSE_SIZE_DETECTION_ERROR_PTR_NULL);
   result_t result = RESULT_OK;
   int32_t total_dispensed_mg_raw = 0;
   uint32_t total_dispensed_mg_running = 0u;

   bool data_valid = true;

   if(!self->_pre_dose_data.is_valid)
   {
      DEBUG_WARNING("Pre-dose data invalid");
      data_valid = false;
   }
   if(!self->_post_dose_data.is_valid)
   {
      DEBUG_WARNING("Post-dose data invalid");
      data_valid = false;
   }
   if(!self->_drift_data.is_valid)
   {
      DEBUG_WARNING("Drift data invalid");
      data_valid = false;
   }

   if(false == data_valid)
   {
      self->_dose_size_data_state = DSD_DATA_STATE_BAD;
      self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_INVALID_DOSE_DATA;
   }
   else
   {
      DSD_DATA_BAD_REASON bad_reason = DSD_DATA_BAD_REASON_NONE;

      // Dose size calculation
      total_dispensed_mg_raw
         = ((int32_t)self->_full_assembly_weight_mg - self->_post_dose_data.weight_mg) - self->_drift_data.weight_mg;

      // Total sigma calculation
      // Assuming independent samples, thus neglecting covariance terms
      uint64_t sigma_drift = (uint64_t)self->_drift_data.sigma_mg;
      uint64_t sigma_post = (uint64_t)self->_post_dose_data.sigma_mg;
      uint64_t variance = sigma_drift * sigma_drift + sigma_post * sigma_post;
      uint32_t sigma_total = int_sqrt(variance);
      self->_sigma_total_mg = (sigma_total > UINT16_MAX) ? UINT16_MAX : (uint16_t)sigma_total;

      self->_dose_size_mg = total_dispensed_mg_raw - (int32_t)self->_last_total_dispensed_weight_mg;

      if(total_dispensed_mg_raw < 0)
      {
         DEBUG_WARNING("Total dispensed (mg) negative. Marking data as bad and using zero for running total.");
         total_dispensed_mg_running = 0u;
         bad_reason = DSD_DATA_BAD_REASON_NEGATIVE_TOTAL_DISPENSED;
      }
      else
      {
         total_dispensed_mg_running = (uint32_t)total_dispensed_mg_raw;
      }

      if((self->_dose_size_mg < 0) && (DSD_DATA_BAD_REASON_NONE == bad_reason))
      {
         DEBUG_WARNING("Dose size (mg) negative. Marking data as bad while preserving computed value.");
         bad_reason = DSD_DATA_BAD_REASON_NEGATIVE_DOSE_SIZE;
      }

      // Keep running total aligned with latest computed total (clamped to zero when computed total is negative).
      self->_last_total_dispensed_weight_mg = total_dispensed_mg_running;

      if(bad_reason != DSD_DATA_BAD_REASON_NONE)
      {
         self->_dose_size_data_state = DSD_DATA_STATE_BAD;
         self->_dose_size_data_bad_reason = bad_reason;
      }
      else
      {
         self->_dose_size_data_state = DSD_DATA_STATE_READY;
         self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;
      }
   }

   // Update state machine inputs (regardless of result of computation)
   self->_sm_inputs.is_dose_size_computed = true;

   // Log data for debugging
   DEBUG_DEBUG("[dsd] dose size: %d mg, total: %d mg, sigma total: %u mg",
               self->_dose_size_mg,
               total_dispensed_mg_raw,
               self->_sigma_total_mg);
   DEBUG_DEBUG("[dsd] Pre-dose: %d mg (sigma %u mg) last updated %u ms",
               self->_pre_dose_data.weight_mg,
               self->_pre_dose_data.sigma_mg,
               self->_pre_dose_data.time_ms);
   DEBUG_DEBUG("[dsd] Post-dose: %d mg (sigma %u mg) last updated %u ms",
               self->_post_dose_data.weight_mg,
               self->_post_dose_data.sigma_mg,
               self->_post_dose_data.time_ms);
   DEBUG_DEBUG("[dsd] Drift: %d mg (sigma %u mg) last updated %u ms",
               self->_drift_data.weight_mg,
               self->_drift_data.sigma_mg,
               self->_drift_data.time_ms);

   return result;
}

/**
 * @brief Handle the IDLE state of the dose size detection state machine.
 *
 * In this state, the system monitors weight measurements at a much slower rate to save power while still preparing
 * for dose size detection. It updates the stability tracker accordingly.
 *
 * As soon as a stable weight measurement is detected, it sets the got_valid_sample input to
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
   bool is_data_stale = false;
   uint64_t now_ms = 0u;
   bool is_weight_measurement_stable = false;

   IF_OK_RUN_AND_UPDATE(result, self->_system_time_interface->get_time_ms(self->_system_time_interface, &now_ms));

   if(self->_sm_outputs.changed)
   {
      DEBUG_WARNING("Entered IDLE state - weight measurements will be taken at a slower rate and dose size detection "
                    "will not be resumed until a stable weight measurement is detected");
      IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));
      self->_time_enter_idle_state_ms = now_ms;
   }

   if((MS_TO_S(now_ms) - MS_TO_S(self->_time_enter_idle_state_ms)) >= IDLE_STATE_WAIT_MAX_S)
   {
      DEBUG_WARNING("Maximum wait time in IDLE state elapsed - cannot determine dose size, put Dock on a stable "
                    "surface and replace the ring to attempt dose size detection again");
      self->_time_enter_idle_state_ms = now_ms; // Reset timer to avoid spamming warnings
   }

   IF_OK_RUN_AND_UPDATE(result,
                        p_weight_sensor_ifc->read_weight_data(p_weight_sensor_ifc, &weight_data, &is_data_stale));

   if(IS_OK(result) && !is_data_stale)
   {
      is_weight_measurement_stable = (weight_data.stddev_mg <= DSD_SIGMA_INITIAL_THRESHOLD_MG);
   }

   if((IS_OK(result)) && !is_data_stale && is_weight_measurement_stable)
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
            IF_OK_RUN_AND_UPDATE(result, reset_dsd_after_interruption(self));
            ON_ERR_DEBUG_ERROR(result, "Failed to reset dose size detection state after interruption");
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
   *is_elapsed_out = false;

   if(self->_stability_tracker.max_time_start_time_s > 0u)
   {
      elapsed_ms = now_ms - ((uint64_t)self->_stability_tracker.max_time_start_time_s * COMMON_1K_FACTOR);
   }

   if(self->_wait_time_ms > 0u)
   {
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
      dose_size_data_out->dose_size_mg = p_self->_dose_size_mg;
      dose_size_data_out->total_dispensed_mg = p_self->_last_total_dispensed_weight_mg;
      dose_size_data_out->sigma_total_mg = p_self->_sigma_total_mg;

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

   const system_time_interface_t *p_systick_ifc = p_self->_system_time_interface;

   // Check whether DSD is blocked
   bool was_dsd_blocked = p_self->_is_blocked;
   result_t blocking_error = RESULT_OK;
   result_t result = check_is_dsd_blocked(p_self, &p_self->_is_blocked, &blocking_error);
   if(IS_OK(result))
   {
      if((false == was_dsd_blocked) && (true == p_self->_is_blocked))
      {
         DEBUG_WARNING("DSD blocked (result(%d,%d)). Resetting dose size detection state.",
                       GET_ERR_UNIT(blocking_error),
                       GET_ERR_CODE(blocking_error));

         result = reset_dsd_after_interruption(p_self);
         ON_ERR_DEBUG_ERROR(result, "Failed to reset dose size detection state after blocking");

         if(IS_OK(result))
         {
            p_self->_dose_size_data_state = DSD_DATA_STATE_NO_DATA;
            p_self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;
         }
      }
      else if((true == was_dsd_blocked) && (false == p_self->_is_blocked))
      {
         DEBUG_WARNING("DSD unblocked. Resuming from previously reset runtime state.");
      }
   }

   if(IS_OK(result) && p_self->_is_blocked)
   {
      result = blocking_error;
   }

   if(IS_OK(result))
   {
      uint64_t now_ms = 0u;
      uint64_t sample_time_ms = 0u;
      bool sample_now = false;
      bool is_max_time_elapsed = false;
      bool is_sigma_below_best_threshold = false;

      result = p_systick_ifc->get_time_ms(p_systick_ifc, &now_ms);
      IF_OK_RUN_AND_UPDATE(result, is_wait_time_elapsed(p_self, now_ms, &is_max_time_elapsed));

      p_self->_previous_state = (DSD_STATEMACHINE_STATE)p_self->_statemachine.current_state;

      // Gather state machine inputs
      const dsd_sample_t *best_sample = NULL;

      p_self->_sm_inputs.is_ring_present = is_ring_present;
      p_self->_sm_inputs.is_max_time_elapsed = is_max_time_elapsed;
      p_self->_sm_inputs.got_valid_weight_sample = p_self->_got_valid_weight_sample;

      if(p_self->_best_sample_idx != DSD_BEST_SAMPLE_IDX_INVALID)
      {
         const dsd_sample_t *candidate_sample = &p_self->_best_sample_window[p_self->_best_sample_idx];
         if(candidate_sample->is_valid)
         {
            best_sample = candidate_sample;
         }
      }
      if(NULL != best_sample)
      {
         is_sigma_below_best_threshold = (best_sample->sigma_mg <= DSD_SIGMA_BEST_THRESHOLD_MG);
      }

      p_self->_sm_inputs.is_sigma_below_best_threshold = is_sigma_below_best_threshold;
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
      IF_OK_RUN_AND_UPDATE(result, switch_state(p_self, now_ms, sample_now));
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

   // Initialize internal state.
   self->_last_total_dispensed_weight_mg = last_total_dispensed_weight_mg;
   self->_full_assembly_weight_mg = full_assembly_weight_mg;
   self->_full_assembly_weight_mg_before_baselining = full_assembly_weight_mg;
   self->_last_total_dispensed_weight_mg_before_baselining = last_total_dispensed_weight_mg;

   self->_dose_size_data_state = DSD_DATA_STATE_NO_DATA;
   self->_dose_size_data_bad_reason = DSD_DATA_BAD_REASON_NONE;
   self->_wait_time_ms = STABLE_WAIT_TIME_S * COMMON_1K_FACTOR;
   self->_sampling_frequency = DSD_FREQ_SLOW;
   self->_previous_state = DSD_STATEMACHINE_STATE_RING_PRESENT;
   self->_is_latest_sample_valid = false;
   self->_got_valid_weight_sample = false;
   self->_is_blocked = false;
   self->_is_baselining_in_progress = false;
   self->_baselining_start_time_ms = 0u;
   self->_last_baselining_sample_time_ms = 0u;
   self->_time_enter_idle_state_ms = 0u;

   // Initialize state machine
   result_t result = dsd_fsm_init(&self->_statemachine); // Initial state DSD_STATEMACHINE_STATE_RING_PRESENT
   IF_OK_RUN_AND_UPDATE(result, init_dose_size_data(self));
   IF_OK_RUN_AND_UPDATE(result, init_stability_tracker(&self->_stability_tracker));
   IF_OK_RUN_AND_UPDATE(result, reset_best_sample_window(self));

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
