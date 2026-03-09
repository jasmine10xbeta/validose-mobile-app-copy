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
#include "ring_data_manager.h"
#include "common.h"
#include "debug.h"
#include "nrf.h"
#include "queue.h"
#include "ring_data_manager_interface.h"
#include "version.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_RING_DATA_MANAGER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define ERROR_DATA_LENGTH       (128u) /**< Length of error data entries */
#define NUM_FOR_MS_FROM_MILLIHZ (COMMON_1K_CST * COMMON_1K_CST)
#define SECONDS_IN_A_DAY        (86400u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
static result_t process(const ring_data_manager_interface_t *const ifc);
static result_t update_battery_sample_rate(const ring_data_manager_interface_t *const ifc,
                                           uint16_t sample_rate_millihz);
static result_t update_prox_data(const ring_data_manager_interface_t *const ifc, prox_data_t prox_data);

static result_t get_status(const ring_data_manager_interface_t *const ifc, ring_status_t *const status);
static result_t update_time(const ring_data_manager_interface_t *const ifc, uint64_t time_ms);

static result_t battery_logging_handler(const ring_data_manager_interface_t *const ifc,
                                        uint64_t current_time_ms,
                                        uint64_t current_systick_ms);

static result_t dose_event_handler(const ring_data_manager_interface_t *const ifc, uint32_t unix_time_s);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static result_t battery_logging_handler(const ring_data_manager_interface_t *const ifc,
                                        uint64_t current_time_ms,
                                        uint64_t current_systick_ms)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_OK_IF_TRUE(ifc->parent->_battery_log_trigger_ms > current_systick_ms);
   RETURN_OK_IF_TRUE(0u == ifc->parent->_current_status.battery_sample_frequency_millihz); // Disable battery logging

   ring_data_manager_t *self = ifc->parent;
   ring_status_t *status = &self->_current_status;
   uint8_t *watermark = &status->battery_fifo_used_percent_watermark;

   // Reset trigger to prevent a later error causing repeated logging
   self->_battery_log_trigger_ms
      = current_systick_ms + NUM_FOR_MS_FROM_MILLIHZ / status->battery_sample_frequency_millihz;

   // Get battery status - Placed here to rate limit I2C calls from the battery manager
   battery_status_t battery = {0};
   result_t result = self->_battery_manager_ifc->get_battery_status(self->_battery_manager_ifc, &battery);

   // Log battery data
   ring_battery_data_t data = {0};
   data.timestamp_unix_s = (uint32_t)(current_time_ms / 1000u);
   data.charge_percent = battery.battery_level;

   queue_status_t queue_status = {0};
   IF_OK_RUN_AND_UPDATE(result, self->_battery_queue_ifc->put(self->_battery_queue_ifc, &data, 1u, &queue_status));

   if(GET_ERR_CODE(result) == QUEUE_FULL)
   {
      // Queue is full.
      // Clear error as all modules behaved correctly. The Queue watermark will provide the error indication
      result = RESULT_OK;
   }
   else if(IS_OK(result))
   {
      status->battery_fifo_used_percent = queue_status.percent_full;
   }
   *watermark = *watermark > status->battery_fifo_used_percent ? *watermark : status->battery_fifo_used_percent;

   return result;
}

static result_t dose_event_handler(const ring_data_manager_interface_t *const ifc, uint32_t unix_time_s)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;
   ring_data_manager_t *self = ifc->parent;
   ring_status_t *status = &self->_current_status;
   uint8_t *watermark = &status->dose_fifo_used_percent_watermark;

   // Run process dose detection module and check for a new dose event
   bool new_event_available = false;
   dose_detection_event_t event = {0};
   result = self->_dose_detection_ifc->process(self->_dose_detection_ifc);

   IF_OK_RUN_AND_UPDATE(
      result, self->_dose_detection_ifc->try_get_dose_event(self->_dose_detection_ifc, &event, &new_event_available));

   static uint16_t days_since_epoch = 0u;
   static uint8_t event_ctr = 0u;

   // Reset dose event counter and update days since epoch if a day has passed
   if(days_since_epoch != unix_time_s / SECONDS_IN_A_DAY)
   {
      days_since_epoch = (uint16_t)(unix_time_s / SECONDS_IN_A_DAY);
      event_ctr = 0u;
   }

   // If a new dose event is available, package and enqueue it
   while(IS_OK(result) && new_event_available)
   {
      event_ctr++;

      dose_event_t dose_event = {0};
      dose_event.event_id.days_since_epoch = days_since_epoch;
      dose_event.event_id.event_ctr = event_ctr;
      dose_event.start_timestamp_unix_s = (uint32_t)(event.dose_start_time_ms / COMMON_1K_CST);
      dose_event.duration_s = (uint16_t)((event.dose_end_time_ms - event.dose_start_time_ms) / COMMON_1K_CST);
      dose_event.tilt_count = event.tilt_count;
      dose_event.dose_completed_in_time = (uint8_t)(dose_event.duration_s < DOSE_DETECTION_MAX_DOSE_DURATION_S);

      // Enqueue dose event
      nfc_queue_status_t queue_status = {0};
      result = self->_dose_queue_ifc->nfc_put(self->_dose_queue_ifc, &dose_event, 1u, &queue_status);

      SEGGER_RTT_SetTerminal(6);
      SEGGER_RTT_printf(
         0,
         "Enqueued dose event. \nStart: %u\nDuration: %u\nTilt Count: %u\nCompleted in time: %u\n Event ID: %u-%u\n",
         dose_event.start_timestamp_unix_s,
         dose_event.duration_s,
         dose_event.tilt_count,
         dose_event.dose_completed_in_time,
         dose_event.event_id.days_since_epoch,
         dose_event.event_id.event_ctr);
      SEGGER_RTT_SetTerminal(0);

      if(IS_OK(result))
      {
         // Queue is full.
         // Clear error as all modules behaved correctly. The Queue watermark will provide the error indication
         status->dose_fifo_used_percent = queue_status.percent_full;
      }
      *watermark = *watermark > status->dose_fifo_used_percent ? *watermark : status->dose_fifo_used_percent;

      IF_OK_RUN_AND_UPDATE(
         result,
         self->_dose_detection_ifc->try_get_dose_event(self->_dose_detection_ifc, &event, &new_event_available));
   }

   // All dose events extracted. Clear the events from the dose detection module to free up space for new events
   IF_OK_RUN_AND_UPDATE(result, self->_dose_detection_ifc->clear_dose_events(self->_dose_detection_ifc));

   return result;
}
/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t process(const ring_data_manager_interface_t *const ifc)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;
   ring_data_manager_t *self = ifc->parent;

   uint64_t current_systick_ms = 0u; // Time tracking
   uint64_t current_time_ms = 0u;

   // **************************************************************************
   // Retrieve current system time
   IF_OK_RUN_AND_UPDATE(result, self->_systime_ifc->get_time_ms(self->_systime_ifc, &current_time_ms));
   IF_OK_RUN_AND_UPDATE(result, self->_systick_ifc->get_time_ms(self->_systick_ifc, &current_systick_ms));
   // **************************************************************************

   // **************************************************************************
   // Perform operations based on the retrieved statuses
   // Run dose update logic
   // Placed as early as possible in the process loop to avoid missing a dose because of an an error elsewhere
   IF_OK_RUN_AND_UPDATE(result, dose_event_handler(ifc, (uint32_t)(current_time_ms / COMMON_1K_CST)));
   IF_OK_RUN_AND_UPDATE(result, battery_logging_handler(ifc, current_time_ms, current_systick_ms));
   // **************************************************************************

   return result;
}

static result_t update_battery_sample_rate(const ring_data_manager_interface_t *const ifc, uint16_t sample_rate_millihz)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;
   ring_data_manager_t *self = ifc->parent;

   result = self->_fds_manager_ifc->store_uint64_t(
      self->_fds_manager_ifc, RECORD_ID_BATTERY_SAMPLE_FREQUENCY, (uint64_t)sample_rate_millihz);

   // Only update local status if the NVM operation was succesful
   if(IS_OK(result))
   {
      ifc->parent->_current_status.battery_sample_frequency_millihz = sample_rate_millihz;
   }
   return result;
}

static result_t update_prox_data(const ring_data_manager_interface_t *const ifc, prox_data_t prox_data)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;
   ring_data_manager_t *self = ifc->parent;

   // Store proximity data in FDS
   result = self->_fds_manager_ifc->store_uint64_t(
      self->_fds_manager_ifc, RECORD_ID_PROXIMITY_CAP_ON, (uint64_t)prox_data.cap_on);
   IF_OK_RUN_AND_UPDATE(result,
                        self->_fds_manager_ifc->store_uint64_t(
                           self->_fds_manager_ifc, RECORD_ID_PROXIMITY_CAP_OFF, (uint64_t)prox_data.cap_off));

   // Update the values in the proximity sensor
   proximity_thresholds_t thresholds = {0};
   thresholds.high = prox_data.cap_on;
   thresholds.low = prox_data.cap_off;

   self->_cap_detection_ifc->set_prox_sensor_thresholds(self->_cap_detection_ifc, thresholds);

   return result;
}

static result_t get_status(const ring_data_manager_interface_t *const ifc, ring_status_t *const status)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(status, RING_DATA_MANAGER_ERROR_NULL_PTR);

   result_t result = RESULT_OK;
   ring_data_manager_t *self = ifc->parent;
   ring_battery_manager_interface_t *battery_ifc = self->_battery_manager_ifc;

   // Update the timestamp on the status structure
   uint64_t current_time_ms = 0u;
   uint64_t uptime_ms = 0u;
   int16_t temperature_c = 0u;
   battery_status_t battery_status = {0};
   proximity_thresholds_t prox = {0};
   uint16_t current_prox = 0u;
   CAP_STATE current_cap_state = CAP_STATE_UNKNOWN;

   IMU_STATE initial_imu_state = IMU_STATE_DORMANT;
   IF_OK_RUN_AND_UPDATE(result, self->_imu_ifc->get_state(self->_imu_ifc, &initial_imu_state));

   IF_OK_RUN_AND_UPDATE(result, self->_systime_ifc->get_time_ms(self->_systime_ifc, &current_time_ms));
   IF_OK_RUN_AND_UPDATE(result, self->_systick_ifc->get_time_ms(self->_systick_ifc, &uptime_ms));
   IF_OK_RUN_AND_UPDATE(result, self->_imu_ifc->set_state(self->_imu_ifc, IMU_STATE_ACTIVE));
   IF_OK_RUN_AND_UPDATE(result, self->_imu_ifc->get_temperature_celsius(self->_imu_ifc, &temperature_c));
   IF_OK_RUN_AND_UPDATE(result, battery_ifc->get_battery_status(battery_ifc, &battery_status));

   IF_OK_RUN_AND_UPDATE(result, self->_cap_detection_ifc->get_prox_sensor_thresholds(self->_cap_detection_ifc, &prox));
   IF_OK_RUN_AND_UPDATE(
      result, self->_cap_detection_ifc->get_cap_status(self->_cap_detection_ifc, &current_cap_state, &current_prox));

   // Update all relevant fields in the status structure
   if(IS_OK(result))
   {
      self->_current_status.uptime_s = (uint32_t)(uptime_ms / COMMON_1K_CST);
      self->_current_status.timestamp_unix_s = (uint32_t)(current_time_ms / COMMON_1K_CST);
      self->_current_status.battery_charge_status = (uint8_t)battery_status.battery_state;
      self->_current_status.temperature_celsius = temperature_c;

      self->_current_status.cap_on = (uint16_t)prox.high;
      self->_current_status.cap_off = (uint16_t)prox.low;
      self->_current_status.current_prox = current_prox;

      // Hardware and firmware versions are set at initialization and do not need to be updated here

      *status = ifc->parent->_current_status;
   }

   // Set the IMU back to the initial state.
   self->_imu_ifc->set_state(self->_imu_ifc, initial_imu_state);
   return result;
}

static result_t update_time(const ring_data_manager_interface_t *const ifc, uint64_t time_ms)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;
   ring_data_manager_t *self = ifc->parent;

   result = self->_systime_ifc->set_time_ms(self->_systime_ifc, time_ms);

   // Update ship mode exit date with the first time update to come through
   if(IS_OK(result) && (0u == ifc->parent->_current_status.ship_mode_exit_timestamp_unix))
   {
      // Subtract the current uptime from the provided time to get the ship mode exit time.
      // This is done to handle the case where the ring has been running for a while without a time update
      uint64_t uptime_ms = 0u;
      result = self->_systick_ifc->get_time_ms(self->_systick_ifc, &uptime_ms);

      uint64_t ship_mode_exit_ms = time_ms > uptime_ms ? (time_ms - uptime_ms) : time_ms;

      IF_OK_RUN_AND_UPDATE(result,
                           ifc->parent->_fds_manager_ifc->store_uint64_t(
                              ifc->parent->_fds_manager_ifc, RECORD_ID_SHIP_MODE_EXIT_TIME, ship_mode_exit_ms));

      if(IS_OK(result))
      {
         ifc->parent->_current_status.ship_mode_exit_timestamp_unix = (uint32_t)(ship_mode_exit_ms / COMMON_1K_CST);
      }
   }

   return result;
}

static result_t enqueue_error(const ring_data_manager_interface_t *const ifc, raw_debug_log_t *debug_log)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, RING_DATA_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(debug_log, RING_DATA_MANAGER_ERROR_NULL_PTR);

   ring_data_manager_t *self = ifc->parent;
   ring_status_t *status = &self->_current_status;
   uint8_t *watermark = &status->error_fifo_used_percent_watermark;

   queue_status_t queue_status = {0};

   result_t result = self->_error_queue_ifc->put(self->_error_queue_ifc, debug_log, 1u, &queue_status);

   if(GET_ERR_CODE(result) == QUEUE_FULL)
   {
      // Queue is full.
      // Clear error as all modules behaved correctly. The Queue watermark will provide the error indication
      result = RESULT_OK;
   }
   else if(IS_OK(result))
   {
      status->error_fifo_used_percent = queue_status.percent_full;
   }
   *watermark = *watermark > status->error_fifo_used_percent ? *watermark : status->error_fifo_used_percent;

   return result;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t init_ring_data_manager(ring_data_manager_t *const self,
                                system_time_interface_t *system_time_ifc,
                                system_time_interface_t *systick_time_ifc,
                                fds_manager_interface_t *fds_manager_ifc,
                                imu_interface_t *imu_ifc,
                                ring_battery_manager_interface_t *battery_manager_ifc,
                                dose_detection_interface_t *dose_detection_ifc,
                                nfc_tag_eeprom_queue_interface_t *dose_data_queue_ifc,
                                queue_interface_t *battery_data_queue_ifc,
                                queue_interface_t *error_data_queue_ifc,
                                cap_detection_interface_t *cap_detection_ifc)
{
   RETURN_ERR_IF_NULL(self, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(system_time_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(systick_time_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(fds_manager_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(imu_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(battery_manager_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(dose_detection_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(dose_data_queue_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(battery_data_queue_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(error_data_queue_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(cap_detection_ifc, RING_DATA_MANAGER_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   self->interface.parent = self;

   self->interface.process = process;
   self->interface.update_battery_sample_rate = update_battery_sample_rate;
   self->interface.get_status = get_status;
   self->interface.update_time = update_time;
   self->interface.enqueue_error = enqueue_error;
   self->interface.update_prox_data = update_prox_data;

   self->_systime_ifc = system_time_ifc;
   self->_systick_ifc = systick_time_ifc;
   self->_fds_manager_ifc = fds_manager_ifc;
   self->_battery_manager_ifc = battery_manager_ifc;
   self->_imu_ifc = imu_ifc;
   self->_dose_detection_ifc = dose_detection_ifc;
   self->_cap_detection_ifc = cap_detection_ifc;

   self->_dose_queue_ifc = dose_data_queue_ifc;
   self->_battery_queue_ifc = battery_data_queue_ifc;
   self->_error_queue_ifc = error_data_queue_ifc;

   // Values that will be used to initialize the status structure
   semantic_version_t hw_version = {1, 0, 0};
   semantic_version_t fw_version = {0, 1, 9};

   uint32_t dev_id0 = NRF_FICR->DEVICEID[0];
   uint32_t dev_id1 = NRF_FICR->DEVICEID[1];
   uint64_t dev_id = ((uint64_t)dev_id1 << 32) | dev_id0;

   uint64_t retrieved_uint64_data = 0u;
   uint64_t ship_mode_exit_time_ms = 0u;
   uint16_t battery_sample_frequency_millihz = 0u;
   uint8_t dose_fifo_fill_percent = 0u;
   battery_status_t battery_status = {0};

   // Retrieve values
   result = fds_manager_ifc->retrieve_uint64_t(fds_manager_ifc, RECORD_ID_SHIP_MODE_EXIT_TIME, &ship_mode_exit_time_ms);

   IF_OK_RUN_AND_UPDATE(
      result,
      fds_manager_ifc->retrieve_uint64_t(fds_manager_ifc, RECORD_ID_BATTERY_SAMPLE_FREQUENCY, &retrieved_uint64_data));
   battery_sample_frequency_millihz = (uint16_t)retrieved_uint64_data;

   IF_OK_RUN_AND_UPDATE(result,
                        self->_dose_queue_ifc->nfc_get_percent_full(self->_dose_queue_ifc, &dose_fifo_fill_percent));

   IF_OK_RUN_AND_UPDATE(result,
                        self->_battery_manager_ifc->get_battery_status(self->_battery_manager_ifc, &battery_status));

   if(IS_OK(result))
   {
      // Initialize the status structure
      self->_current_status.hardware_version = hw_version;
      self->_current_status.firmware_version = fw_version;
      self->_current_status.mac[0] = (uint8_t)(dev_id >> 0);
      self->_current_status.mac[1] = (uint8_t)(dev_id >> 8);
      self->_current_status.mac[2] = (uint8_t)(dev_id >> 16);
      self->_current_status.mac[3] = (uint8_t)(dev_id >> 24);
      self->_current_status.mac[4] = (uint8_t)(dev_id >> 32);
      self->_current_status.mac[5] = (uint8_t)(dev_id >> 40);

      self->_current_status.uptime_s = 0u;              // Updated when requested
      self->_current_status.timestamp_unix_s = 0u;      // Updated when requested
      self->_current_status.battery_charge_status = 0u; // Updated when requested
      self->_current_status.temperature_celsius = 0u;   // Updated when requested

      self->_current_status.dose_fifo_used_percent = dose_fifo_fill_percent;
      self->_current_status.dose_fifo_used_percent_watermark = dose_fifo_fill_percent;
      self->_current_status.battery_fifo_used_percent = 0u;
      self->_current_status.battery_fifo_used_percent_watermark = 0u;
      self->_current_status.imu_fifo_used_percent = 0u;
      self->_current_status.imu_fifo_used_percent_watermark = 0u;
      self->_current_status.error_fifo_used_percent = 0u;
      self->_current_status.error_fifo_used_percent_watermark = 0u;

      self->_current_status.ship_mode_exit_timestamp_unix = (uint32_t)(ship_mode_exit_time_ms / COMMON_1K_CST);
      self->_current_status.battery_sample_frequency_millihz = battery_sample_frequency_millihz;

      // Set local tracking variables
      self->_is_docked = battery_status.charger_connected;

      // Set the initialized flag only if all operations were successful
      self->_initialization_status = INITIALIZED;
   }

   return result;
}