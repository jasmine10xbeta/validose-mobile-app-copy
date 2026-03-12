/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_scheduler.c
 * @ingroup dose_scheduler_module
 * @brief Implementation file for the dose scheduler module
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "dose_scheduler.h"

#include "common.h"
#include "rtc_system_time_interface.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOSE_SCHEDULER_DOCK;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define UNIX_EPOCH_DOW_OFFSET (3u) /**< Offset to calculate day of week from Unix epoch (1970-01-01 was a Thursday) */

#define DOSE_DAYS_BITFIELD_MODE_MASK      (1u << 7u) /**< Mask to check if skip days mode is enabled */
#define DOSE_DAYS_BITFIELD_SKIP_DAYS_MASK (~DOSE_DAYS_BITFIELD_MODE_MASK) /**< Mask to extract skip days value */

#define IS_DOSE_WINDOW_IN_BOUNDS(_schedule_)                                                                           \
   (((_schedule_)->dose_window_count > 0u) && ((_schedule_)->dose_window_count <= DOSE_SCHEDULE_MAX_DOSES_PER_DAY))

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions

static result_t load_schedule(const dose_scheduler_interface_t *const interface,
                              const dose_schedule_t *const dose_schedule,
                              uint32_t start_time_unix_seconds);
static result_t check_dose_window(const dose_scheduler_interface_t *const interface, bool *const window_open);
static result_t close_dose_window(const dose_scheduler_interface_t *const interface);

// Non-interface functions

static result_t is_minute_in_dose_window(uint16_t minute_of_day,
                                         const uint16_t *dose_window_start_times_minutes,
                                         uint8_t dose_window_count,
                                         uint8_t dose_window_duration_minutes,
                                         bool today_only,
                                         bool *in_dose_window);
static result_t check_dose_window_skip_mode(const dose_schedule_t *const p_schedule,
                                            uint32_t start_date_unix_secs,
                                            uint32_t now_unix_secs,
                                            bool *in_dose_window);
static result_t
   check_dose_window_dow_mode(const dose_schedule_t *const p_schedule, uint32_t now_unix_secs, bool *in_dose_window);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Determines if a given minute of the day is within any of the defined dose windows
 *
 * @param[in] minute_of_day Minute of the day (0-1439)
 * @param[in] dose_window_start_times_minutes Array of dose window start times in minutes from midnight
 * @param[in] dose_window_count Number of dose windows defined in the schedule
 * @param[in] dose_window_duration_minutes Duration of each dose window in minutes
 * @param[in] today_only If true, only checks for windows starting today; if false, checks for windows crossing midnight
 * @param[out] in_dose_window Pointer to boolean that will be set to true if in dose window, false otherwise
 *
 * @return Result of the operation
 */
static result_t is_minute_in_dose_window(uint16_t minute_of_day,
                                         const uint16_t *dose_window_start_times_minutes,
                                         uint8_t dose_window_count,
                                         uint8_t dose_window_duration_minutes,
                                         bool today_only,
                                         bool *in_dose_window)
{
   RETURN_ERR_IF_NULL(dose_window_start_times_minutes, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_NULL(in_dose_window, DOSE_SCHEDULER_ERROR_NULL);

   *in_dose_window = false;

   for(uint8_t idx = 0u; idx < dose_window_count; idx++)
   {
      uint16_t window_start = dose_window_start_times_minutes[idx];
      uint16_t window_end = (uint16_t)((window_start + dose_window_duration_minutes) % MINUTES_PER_DAY);

      if(window_end > window_start) // Normal case: window does not cross midnight
      {
         *in_dose_window = ((minute_of_day >= window_start) && (minute_of_day < window_end));
      }
      else // Window crosses midnight
      {
         if(today_only)
         {
            *in_dose_window = (minute_of_day >= window_start);
         }
         else
         {
            *in_dose_window = (minute_of_day >= window_start) || (minute_of_day < window_end);
         }
      }

      if(*in_dose_window)
      {
         break;
      }
   }

   return RESULT_OK;
}

/**
 * @brief Checks if the current time is within a dose window, considering skip days mode
 *
 * @param[in] p_schedule Pointer to the dose schedule
 * @param[in] start_date_unix_secs Start date of the dose schedule in Unix timestamp format in seconds. Must be stripped
 * of time portion.
 * @param[in] now_unix_secs Current time in Unix timestamp format in seconds
 * @param[out] in_dose_window Pointer to boolean that will be set to true if in dose window, false otherwise
 *
 * @return Result of the operation
 */
static result_t check_dose_window_skip_mode(const dose_schedule_t *const p_schedule,
                                            uint32_t start_date_unix_secs,
                                            uint32_t now_unix_secs,
                                            bool *in_dose_window)
{
   RETURN_ERR_IF_NULL(p_schedule, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_NULL(in_dose_window, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_TRUE((start_date_unix_secs > STRIP_TIME_UNIX32(now_unix_secs)),
                      DOSE_SCHEDULER_ERROR_INVALID_TIMESTAMP);
   RETURN_ERR_IF_TRUE((false == IS_DOSE_WINDOW_IN_BOUNDS(p_schedule)), DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT);

   result_t result = RESULT_OK;
   uint16_t minutes_since_midnight = (uint16_t)((now_unix_secs % SECONDS_PER_DAY) / SECONDS_PER_MINUTE);
   uint8_t skip_days = (p_schedule->dose_days_bitfield & (uint8_t)DOSE_DAYS_BITFIELD_SKIP_DAYS_MASK);

   // Note: Creating local copy of window start times to avoid unaligned memory access
   uint16_t window_start_times[DOSE_SCHEDULE_MAX_DOSES_PER_DAY] = {0};
   memcpy(window_start_times,
          p_schedule->dose_window_start_times_minutes,
          p_schedule->dose_window_count * sizeof(window_start_times[0]));

   *in_dose_window = false;

   if(0u == skip_days)
   {
      result = is_minute_in_dose_window(minutes_since_midnight,
                                        window_start_times,
                                        p_schedule->dose_window_count,
                                        p_schedule->dose_window_duration_minutes,
                                        false,
                                        in_dose_window);
   }
   else
   {
      uint32_t today_unix_seconds = STRIP_TIME_UNIX32(now_unix_secs);
      uint32_t days_since_start = (today_unix_seconds - start_date_unix_secs) / SECONDS_PER_DAY;
      uint8_t days_since_last_dose_day = (uint8_t)(days_since_start % (skip_days + 1u));

      // Today is a dose day
      if(0u == days_since_last_dose_day)
      {
         result = is_minute_in_dose_window(minutes_since_midnight,
                                           window_start_times,
                                           p_schedule->dose_window_count,
                                           p_schedule->dose_window_duration_minutes,
                                           true, // Yesterday was not a dose window. Only check today's windows
                                           in_dose_window);
      }
      // Yesterday was a dose day
      else if(1u == days_since_last_dose_day)
      {
         // Check if the last window crosses midnight and is still open
         uint16_t last_window_start_minutes = window_start_times[p_schedule->dose_window_count - 1u];
         uint16_t last_window_end_minutes
            = (uint16_t)((last_window_start_minutes + p_schedule->dose_window_duration_minutes) % MINUTES_PER_DAY);

         *in_dose_window = (last_window_end_minutes < last_window_start_minutes)
                           && (minutes_since_midnight < last_window_end_minutes);
      }
   }

   return result;
}

/**
 * @brief Checks if the current time is within a dose window, considering day of week mode
 *
 * @param[in] p_schedule Pointer to the dose schedule
 * @param[in] now_unix_secs Current time in Unix timestamp format in seconds
 * @param[out] in_dose_window Pointer to boolean that will be set to true if in dose window, false otherwise
 *
 * @return Result of the operation
 */
static result_t
   check_dose_window_dow_mode(const dose_schedule_t *const p_schedule, uint32_t now_unix_secs, bool *in_dose_window)
{
   RETURN_ERR_IF_NULL(p_schedule, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_NULL(in_dose_window, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_TRUE((false == IS_DOSE_WINDOW_IN_BOUNDS(p_schedule)), DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT);

   result_t result = RESULT_OK;
   uint16_t minutes_since_midnight = (uint16_t)((now_unix_secs % SECONDS_PER_DAY) / SECONDS_PER_MINUTE);

   uint32_t today_unix_seconds = STRIP_TIME_UNIX32(now_unix_secs);
   uint32_t days_since_epoch = (today_unix_seconds / SECONDS_PER_DAY);

   uint8_t dow = (uint8_t)((days_since_epoch + UNIX_EPOCH_DOW_OFFSET) % DAYS_PER_WEEK); // 0=Monday, 6=Sunday
   uint8_t dow_bit = (1u << dow);
   bool is_dose_day = (0u != (p_schedule->dose_days_bitfield & dow_bit));

   uint8_t yesterday_dow = (0u == dow) ? (uint8_t)(DAYS_PER_WEEK - 1u) : (uint8_t)(dow - 1u);
   uint8_t yesterday_dow_bit = (1u << yesterday_dow);
   bool was_yesterday_dose_day = (0u != (p_schedule->dose_days_bitfield & yesterday_dow_bit));

   // Note: Creating local copy of window start times to avoid unaligned memory access
   uint16_t window_start_times[DOSE_SCHEDULE_MAX_DOSES_PER_DAY] = {0};
   memcpy(window_start_times,
          p_schedule->dose_window_start_times_minutes,
          p_schedule->dose_window_count * sizeof(window_start_times[0]));

   *in_dose_window = false;

   // Today is a dose day
   if(is_dose_day)
   {
      // Check all windows for today only if yesterday was not a dose day.
      bool today_only = (false == was_yesterday_dose_day);

      result = is_minute_in_dose_window(minutes_since_midnight,
                                        window_start_times,
                                        p_schedule->dose_window_count,
                                        p_schedule->dose_window_duration_minutes,
                                        today_only,
                                        in_dose_window);
   }
   else if(was_yesterday_dose_day)
   {
      // Check if the last window crosses midnight and is still open
      uint16_t last_window_start_minutes = window_start_times[p_schedule->dose_window_count - 1u];
      uint16_t last_window_end_minutes
         = (uint16_t)((last_window_start_minutes + p_schedule->dose_window_duration_minutes) % MINUTES_PER_DAY);

      *in_dose_window
         = (last_window_end_minutes < last_window_start_minutes) && (minutes_since_midnight < last_window_end_minutes);
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t load_schedule(const dose_scheduler_interface_t *const interface,
                              const dose_schedule_t *const dose_schedule,
                              uint32_t start_time_unix_seconds)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_NULL(dose_schedule, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_TRUE((false == interface->parent->_is_initialized), DOSE_SCHEDULER_ERROR_UNINITIALIZED);

   dose_scheduler_t *const p_self = interface->parent;
   const rtc_system_time_interface_t *const p_rtc_ifc = p_self->_rtc_interface;

   uint32_t now_unix_seconds = 0u;
   uint32_t start_date_unix_seconds = 0u;

   // Get current date
   result_t result = p_rtc_ifc->get_time_unix(p_rtc_ifc, &now_unix_seconds);
   UPDATE_ERR(result, DOSE_SCHEDULER_ERROR_INTERNAL);

   if(IS_OK(result))
   {
      // Strip time portion from timestamps to compare dates only
      uint32_t today_unix_seconds = STRIP_TIME_UNIX32(now_unix_seconds);
      start_date_unix_seconds = STRIP_TIME_UNIX32(start_time_unix_seconds);

      // Check that the start date is not in the future
      if(start_date_unix_seconds > today_unix_seconds)
      {
         SET_ERR(result, DOSE_SCHEDULER_ERROR_INVALID_TIMESTAMP);

         // Clear previously loaded schedule on invalid timestamp
         memset(&p_self->_schedule, 0, sizeof(dose_schedule_t));
         p_self->_start_date_unix_seconds = 0u;
         p_self->_dose_schedule_loaded = false;
      }
   }

   if(IS_OK(result))
   {
      // Validate the new schedule
      result = validate_dose_schedule(dose_schedule);

      if(IS_OK(result))
      {
         // Load the new schedule
         memcpy(&p_self->_schedule, dose_schedule, sizeof(dose_schedule_t));
         p_self->_start_date_unix_seconds = start_date_unix_seconds;
         p_self->_dose_schedule_loaded = true;

         // Reset dose pending flag to ensure dose windows are properly detected with the new schedule
         p_self->_dose_pending_for_window = true;
      }
      else
      {
         // Clear previously loaded schedule on validation failure
         memset(&p_self->_schedule, 0, sizeof(dose_schedule_t));
         p_self->_start_date_unix_seconds = 0u;
         p_self->_dose_schedule_loaded = false;
      }
   }

   return result;
}

static result_t check_dose_window(const dose_scheduler_interface_t *const interface, bool *const window_open)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_NULL(window_open, DOSE_SCHEDULER_ERROR_NULL);

   dose_scheduler_t *const p_self = interface->parent;
   const rtc_system_time_interface_t *const p_rtc_ifc = p_self->_rtc_interface;
   const dose_schedule_t *const p_schedule = &p_self->_schedule;

   RETURN_ERR_IF_TRUE((false == p_self->_is_initialized), DOSE_SCHEDULER_ERROR_UNINITIALIZED);

   result_t result = RESULT_OK;

   if(false == p_self->_dose_schedule_loaded)
   {
      *window_open = false;
   }
   else
   {
      bool in_dose_window = false;

      // Get current date
      uint32_t now_unix_seconds = 0u;
      result = p_rtc_ifc->get_time_unix(p_rtc_ifc, &now_unix_seconds);
      UPDATE_ERR(result, DOSE_SCHEDULER_ERROR_INTERNAL);

      if(IS_OK(result))
      {
         // Check schedule mode
         bool is_skip_days_mode = (0u != (p_schedule->dose_days_bitfield & DOSE_DAYS_BITFIELD_MODE_MASK));

         if(is_skip_days_mode) // Skip days mode
         {
            result = check_dose_window_skip_mode(
               p_schedule, p_self->_start_date_unix_seconds, now_unix_seconds, &in_dose_window);
         }
         else // Day of week mode
         {
            result = check_dose_window_dow_mode(p_schedule, now_unix_seconds, &in_dose_window);
         }
      }

      // Set output
      if(IS_OK(result))
      {
         *window_open = in_dose_window && p_self->_dose_pending_for_window;
      }

      // If not in a dose window, reset dose pending flag
      if(IS_OK(result) && (false == in_dose_window))
      {
         p_self->_dose_pending_for_window = true;
      }
   }

   return result;
}

static result_t close_dose_window(const dose_scheduler_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_TRUE((false == interface->parent->_is_initialized), DOSE_SCHEDULER_ERROR_UNINITIALIZED);

   interface->parent->_dose_pending_for_window = false;

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t validate_dose_schedule(const dose_schedule_t *const p_schedule)
{
   RETURN_ERR_IF_NULL(p_schedule, DOSE_SCHEDULER_ERROR_NULL);

   // Check the medication type
   /** @todo Reserved for future use. Implement checks once medication type is implemented. */

   // Check the dosage amount is within range
   RETURN_ERR_IF_TRUE((0u == p_schedule->dosage_mg), DOSE_SCHEDULER_ERROR_INVALID_DOSAGE);
   RETURN_ERR_IF_TRUE((p_schedule->dosage_mg > DOSE_SCHEDULE_MAX_DOSAGE_MG), DOSE_SCHEDULER_ERROR_INVALID_DOSAGE);

   // Check temperature limits
   RETURN_ERR_IF_TRUE((DOSE_SCHEDULE_MAX_TEMP_THRESHOLD_DEG_C < p_schedule->temp_upper_limit_deg_c),
                      DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD);
   RETURN_ERR_IF_TRUE((DOSE_SCHEDULE_MIN_TEMP_THRESHOLD_DEG_C > p_schedule->temp_lower_limit_deg_c),
                      DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD);
   RETURN_ERR_IF_TRUE((p_schedule->temp_lower_limit_deg_c >= p_schedule->temp_upper_limit_deg_c),
                      DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD);

   // Check temperature averaging window
   RETURN_ERR_IF_TRUE((DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC < p_schedule->temp_avg_window_duration_sec),
                      DOSE_SCHEDULER_ERROR_INVALID_TEMP_AVG_WINDOW);

   // Check dose window duration
   RETURN_ERR_IF_TRUE((0u == p_schedule->dose_window_duration_minutes), DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION);
   RETURN_ERR_IF_TRUE((DOSE_SCHEDULE_MAX_DOSE_WINDOW_MINUTES < p_schedule->dose_window_duration_minutes),
                      DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION);

   // Check the dose count is within range
   RETURN_ERR_IF_TRUE((0u == p_schedule->dose_window_count), DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT);
   RETURN_ERR_IF_TRUE((DOSE_SCHEDULE_MAX_DOSES_PER_DAY < p_schedule->dose_window_count),
                      DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT);

   // Check dose windows
   for(uint8_t idx = 0u; idx < p_schedule->dose_window_count; idx++)
   {
      uint16_t window_start = p_schedule->dose_window_start_times_minutes[idx];

      // Check dose window start time
      RETURN_ERR_IF_TRUE((window_start >= MINUTES_PER_DAY), DOSE_SCHEDULER_ERROR_INVALID_WINDOW_START);

      // Only check overlapping/adjacent windows if there is more than one window
      if(p_schedule->dose_window_count > 1u)
      {
         uint16_t window_end = (uint16_t)((window_start + p_schedule->dose_window_duration_minutes) % MINUTES_PER_DAY);
         uint16_t next_window_start = 0u;

         if((idx + 1u) < p_schedule->dose_window_count) // Not the last window
         {
            next_window_start = p_schedule->dose_window_start_times_minutes[idx + 1u];
         }
         else if(window_end < window_start) // Last window that wraps around midnight
         {
            next_window_start = p_schedule->dose_window_start_times_minutes[0];
         }
         else // Last window
         {
            next_window_start = UINT16_MAX;
         }

         // Check the window does not overlap with the next one
         RETURN_ERR_IF_TRUE((window_end > next_window_start), DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS);

         // Check that the window is not adjacent to the next window
         RETURN_ERR_IF_TRUE((window_end == next_window_start), DOSE_SCHEDULER_ERROR_ADJACENT_WINDOWS);
      }
   }

   return RESULT_OK;
}

result_t dose_scheduler_init(dose_scheduler_t *const p_self, rtc_system_time_interface_t *const rtc_interface)
{
   RETURN_ERR_IF_NULL(p_self, DOSE_SCHEDULER_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(rtc_interface, DOSE_SCHEDULER_ERROR_NULL);

   // Assign interface
   p_self->interface.parent = p_self;
   p_self->interface.load_schedule = load_schedule;
   p_self->interface.check_dose_window = check_dose_window;
   p_self->interface.close_dose_window = close_dose_window;

   // Assign dependencies
   p_self->_rtc_interface = rtc_interface;

   // Initialize private data
   p_self->_schedule = (dose_schedule_t){0};
   p_self->_start_date_unix_seconds = 0u;
   p_self->_dose_schedule_loaded = false;
   p_self->_dose_pending_for_window = true;

   p_self->_is_initialized = true;

   return RESULT_OK;
}
