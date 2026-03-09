
/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dock_data_manager.c
 * @ingroup dock_data_manager_module
 * @brief Implementation of the dock data manager module.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <string.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "dock_data_manager.h"

#ifndef DOCK_DATA_MANAGER_DISABLE_FDS_FLASH_WRITES
#   define DOCK_DATA_MANAGER_DISABLE_FDS_FLASH_WRITES (0)
#endif

#if((DOCK_DATA_MANAGER_DISABLE_FDS_FLASH_WRITES != 0) && (DOCK_DATA_MANAGER_DISABLE_FDS_FLASH_WRITES != 1))
#   error "DOCK_DATA_MANAGER_DISABLE_FDS_FLASH_WRITES must be 0 or 1."
#endif

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOCK_DATA_MANAGER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// RAM queue lengths
#define DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_LEN (100u)
#define DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_STORAGE_BYTES                                                               \
   (DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_LEN * sizeof(dose_event_t)) // 1800 bytes of RAM

#define DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_LEN (200u)
#define DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_STORAGE_BYTES                                                       \
   (DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_LEN * sizeof(dock_charge_status_t)) // 1800 bytes of RAM

#define DOCK_DATA_MANAGER_RING_DOCKED_STATUS_QUEUE_LEN (100u)
#define DOCK_DATA_MANAGER_RING_DOCKED_STATUS_QUEUE_STORAGE_BYTES                                                       \
   (DOCK_DATA_MANAGER_RING_DOCKED_STATUS_QUEUE_LEN * sizeof(ring_docked_status_t)) // 2900 bytes of RAM

#define DOCK_DATA_MANAGER_BLUETOOTH_STATUS_QUEUE_LEN (200u)
#define DOCK_DATA_MANAGER_BLUETOOTH_STATUS_QUEUE_STORAGE_BYTES                                                         \
   (DOCK_DATA_MANAGER_BLUETOOTH_STATUS_QUEUE_LEN * sizeof(bluetooth_status_t)) // 1800 bytes of RAM

#define DOCK_DATA_MANAGER_DOCK_BATTERY_LEVEL_QUEUE_LEN (450u)
#define DOCK_DATA_MANAGER_DOCK_BATTERY_LEVEL_QUEUE_STORAGE_BYTES                                                       \
   (DOCK_DATA_MANAGER_DOCK_BATTERY_LEVEL_QUEUE_LEN * sizeof(battery_level_t)) // 4050 bytes of RAM

#define DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_LEN (450u)
#define DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_STORAGE_BYTES                                                       \
   (DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_LEN * sizeof(battery_level_t)) // 4050 bytes of RAM

#define DOCK_DATA_MANAGER_DOCK_DEBUG_LOG_QUEUE_LEN (200u)
#define DOCK_DATA_MANAGER_DOCK_DEBUG_LOG_QUEUE_STORAGE_BYTES                                                           \
   (DOCK_DATA_MANAGER_DOCK_DEBUG_LOG_QUEUE_LEN * sizeof(raw_debug_log_t)) // 6400 bytes of RAM

#define DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_LEN (200u)
#define DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_STORAGE_BYTES                                                           \
   (DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_LEN * sizeof(raw_debug_log_t)) // 6400 bytes of RAM

// @todo: Return to original queue length of 3500 for V1.1
#define DOCK_DATA_MANAGER_DOSE_DATA_QUEUE_LEN           (35u) // Data gen @10Hz for max of 300s per dose event
#define DOCK_DATA_MANAGER_DOSE_DATA_QUEUE_STORAGE_BYTES (DOCK_DATA_MANAGER_DOSE_DATA_QUEUE_LEN * sizeof(dose_data_t))

#define DOCK_DATA_MANAGER_TEMP_LOG_QUEUE_LEN (720u) // Data gen @ 0.1Hz - so this catches 2 hours.
#define DOCK_DATA_MANAGER_TEMP_LOG_QUEUE_STORAGE_BYTES                                                                 \
   (DOCK_DATA_MANAGER_TEMP_LOG_QUEUE_LEN * sizeof(temperature_log_t))

// @todo: Return to original queue length of 3500 for V1.1
#define DOCK_DATA_MANAGER_WEIGHT_MEAS_LOG_QUEUE_LEN (35u) // Data gen @10Hz for max of 300s per dose event
#define DOCK_DATA_MANAGER_WEIGHT_MEAS_LOG_QUEUE_STORAGE_BYTES                                                          \
   (DOCK_DATA_MANAGER_WEIGHT_MEAS_LOG_QUEUE_LEN * sizeof(dock_weight_measurement_t))

#define DOCK_DATA_MANAGER_RING_STATUS_QUEUE_LEN (50u) // Max queue length among status queues
#define DOCK_DATA_MANAGER_RING_STATUS_QUEUE_STORAGE_BYTES                                                              \
   (DOCK_DATA_MANAGER_RING_STATUS_QUEUE_LEN * sizeof(ring_status_t)) // 2150 bytes of RAM

#define DOCK_DATA_MANAGER_DOCK_STATUS_QUEUE_LEN (50u) // Max queue length among status queues
#define DOCK_DATA_MANAGER_DOCK_STATUS_QUEUE_STORAGE_BYTES                                                              \
   (DOCK_DATA_MANAGER_DOCK_STATUS_QUEUE_LEN * sizeof(dock_status_t)) // 1400 bytes of RAM

#define DOCK_DATA_MANAGER_TOTAL_QUEUE_SIZE_BYTES                                                                       \
   (DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_STORAGE_BYTES + DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_STORAGE_BYTES        \
    + DOCK_DATA_MANAGER_RING_DOCKED_STATUS_QUEUE_STORAGE_BYTES                                                         \
    + DOCK_DATA_MANAGER_BLUETOOTH_STATUS_QUEUE_STORAGE_BYTES                                                           \
    + DOCK_DATA_MANAGER_DOCK_BATTERY_LEVEL_QUEUE_STORAGE_BYTES                                                         \
    + DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_STORAGE_BYTES + DOCK_DATA_MANAGER_DOCK_DEBUG_LOG_QUEUE_STORAGE_BYTES  \
    + DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_STORAGE_BYTES + DOCK_DATA_MANAGER_DOSE_DATA_QUEUE_STORAGE_BYTES           \
    + DOCK_DATA_MANAGER_TEMP_LOG_QUEUE_STORAGE_BYTES + DOCK_DATA_MANAGER_WEIGHT_MEAS_LOG_QUEUE_STORAGE_BYTES           \
    + DOCK_DATA_MANAGER_RING_STATUS_QUEUE_STORAGE_BYTES + DOCK_DATA_MANAGER_DOCK_STATUS_QUEUE_STORAGE_BYTES)

#define DOCK_DATA_MANAGER_TOTAL_RAM_QUEUE_ASSIGNMENT_BYTES (163840u)
STATIC_ASSERT(DOCK_DATA_MANAGER_TOTAL_RAM_QUEUE_ASSIGNMENT_BYTES >= DOCK_DATA_MANAGER_TOTAL_QUEUE_SIZE_BYTES,
              "Dock data manager RAM queue assignments exceed defined limits.");

// Non-volatile storage (FDS)
#define DOCK_DATA_MANAGER_DEVICE_DATA_FILE_ID           (0x0001)
#define DOCK_DATA_MANAGER_DOSE_SCHEDULE_RECORD_KEY      (0x0001)
#define DOCK_DATA_MANAGER_WEIGHT_CALIBRATION_RECORD_KEY (0x0003)
#define DOCK_DATA_MANAGER_TOTAL_WEIGHT_RECORD_KEY       (0x0004)
#define DOCK_DATA_MANAGER_SHIP_MODE_RECORD_KEY          (0x0005)
#define DOCK_DATA_MANAGER_MED_NFC_UID_RECORD_KEY        (0x0006)

#define DOCK_DATA_MANAGER_FDS_DELAY_MS (1u)
static inline size_t dock_data_manager_fds_words_from_bytes(size_t bytes)
{
   return (bytes + (sizeof(uint32_t) - 1u)) / sizeof(uint32_t);
}

#define DOCK_DATA_MANAGER_WEIGHT_CAL_RECORD_WORDS                                                                      \
   ((sizeof(weight_stack_calibration_record_t) + (sizeof(uint32_t) - 1u)) / sizeof(uint32_t))
#define DOCK_DATA_MANAGER_DOSE_SCHEDULE_RECORD_WORDS                                                                   \
   ((sizeof(dose_schedule_record_t) + (sizeof(uint32_t) - 1u)) / sizeof(uint32_t))
#define DOCK_DATA_MANAGER_TOTAL_WEIGHT_RECORD_WORDS ((sizeof(int32_t) + (sizeof(uint32_t) - 1u)) / sizeof(uint32_t))
#define DOCK_DATA_MANAGER_SHIP_MODE_RECORD_WORDS    ((sizeof(ship_mode_t) + (sizeof(uint32_t) - 1u)) / sizeof(uint32_t))
#define DOCK_DATA_MANAGER_MED_NFC_UID_RECORD_WORDS  ((NFC_TAG_MAX_UID_SIZE + (sizeof(uint32_t) - 1u)) / sizeof(uint32_t))

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct
{
   queue_interface_t *queue_ifc;
   size_t element_size;
} queue_info_t;

// Store each record both as a typed struct/value and as a 32-bit word array for FDS read/write.
typedef union
{
   weight_stack_calibration_record_t record;
   uint32_t words[DOCK_DATA_MANAGER_WEIGHT_CAL_RECORD_WORDS];
} weight_calibration_record_storage_t;

// Typed access for code, word-array access for FDS.
typedef union
{
   dose_schedule_record_t schedule_record;
   uint32_t words[DOCK_DATA_MANAGER_DOSE_SCHEDULE_RECORD_WORDS];
} dose_schedule_record_storage_t;

// Typed access for code, word-array access for FDS.
typedef union
{
   uint32_t value;
   uint32_t words[DOCK_DATA_MANAGER_TOTAL_WEIGHT_RECORD_WORDS];
} total_weight_record_storage_t;

// Typed access for code, word-array access for FDS.
typedef union
{
   ship_mode_t value;
   uint32_t words[DOCK_DATA_MANAGER_SHIP_MODE_RECORD_WORDS];
} ship_mode_record_storage_t;

// Typed access for code, word-array access for FDS.
typedef union
{
   uint8_t uid[NFC_TAG_MAX_UID_SIZE];
   uint32_t words[DOCK_DATA_MANAGER_MED_NFC_UID_RECORD_WORDS];
} med_nfc_uid_record_storage_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t enqueue(const dock_data_manager_interface_t *const interface,
                        DATA_ID data_id,
                        void *data,
                        size_t element_size,
                        size_t element_count);
static result_t dequeue(const dock_data_manager_interface_t *const interface,
                        DATA_ID data_id,
                        void *data,
                        size_t element_size,
                        size_t element_count);
static result_t peek(const dock_data_manager_interface_t *const interface,
                     DATA_ID data_id,
                     void *data,
                     size_t element_size,
                     size_t element_count);
static result_t pop(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t element_count);

static result_t get_free_element_count(const dock_data_manager_interface_t *const interface,
                                       DATA_ID data_id,
                                       size_t *free_element_count);

static result_t
   get_element_size(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t *element_size);
static result_t
   get_element_count(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t *element_count);

static result_t set_weight_calibration_record(const dock_data_manager_interface_t *const interface,
                                              weight_stack_calibration_record_t *calibration_record);
static result_t get_weight_calibration_record(const dock_data_manager_interface_t *const interface,
                                              weight_stack_calibration_record_t *calibration_record);

static result_t get_dose_schedule_record(const dock_data_manager_interface_t *const interface,
                                         dose_schedule_record_t *dose_schedule_record_out);
static result_t set_dose_schedule_record(const dock_data_manager_interface_t *const interface,
                                         dose_schedule_record_t *dose_schedule_record_in);

static result_t get_total_weight_dispensed_mg(const dock_data_manager_interface_t *const interface,
                                              uint32_t *total_weight_dispensed_mg);
static result_t set_total_weight_dispensed_mg(const dock_data_manager_interface_t *const interface,
                                              uint32_t total_weight_dispensed_mg);

static result_t get_ship_mode_data(const dock_data_manager_interface_t *const interface, ship_mode_t *ship_mode_data);
static result_t set_ship_mode_data(const dock_data_manager_interface_t *const interface, ship_mode_t *ship_mode_data);

static result_t
   get_med_nfc_uid(const dock_data_manager_interface_t *const interface, uint8_t *med_nfc_uid, uint8_t uid_buffer_size);
static result_t
   set_med_nfc_uid(const dock_data_manager_interface_t *const interface, uint8_t *med_nfc_uid, uint8_t uid_buffer_size);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

// Dose Event File and Queue
static queue_t m_dose_event_queue = {0};
static uint8_t m_dose_event_queue_storage[DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_STORAGE_BYTES] = {0};

// Dock Charge Status File and Queue
static queue_t m_dock_charge_status_queue = {0};
static uint8_t m_dock_charge_status_queue_storage[DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_STORAGE_BYTES] = {0};

// Ring Docked Status File and Queue
static queue_t m_ring_docked_status_queue = {0};
static uint8_t m_ring_docked_status_queue_storage[DOCK_DATA_MANAGER_RING_DOCKED_STATUS_QUEUE_STORAGE_BYTES] = {0};

// Bluetooth Status File and Queue
static queue_t m_bluetooth_status_queue = {0};
static uint8_t m_bluetooth_status_queue_storage[DOCK_DATA_MANAGER_BLUETOOTH_STATUS_QUEUE_STORAGE_BYTES] = {0};

// Dock Battery Level File and Queue
static queue_t m_dock_battery_level_queue = {0};
static uint8_t m_dock_battery_level_queue_storage[DOCK_DATA_MANAGER_DOCK_BATTERY_LEVEL_QUEUE_STORAGE_BYTES] = {0};

// Ring Battery Level File and Queue
static queue_t m_ring_battery_level_queue = {0};
static uint8_t m_ring_battery_level_queue_storage[DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_STORAGE_BYTES] = {0};

// Dock Debug Log File and Queue
static queue_t m_dock_debug_log_queue = {0};
static uint8_t m_dock_debug_log_queue_storage[DOCK_DATA_MANAGER_DOCK_DEBUG_LOG_QUEUE_STORAGE_BYTES] = {0};

// Ring Debug Log File and Queue
static queue_t m_ring_debug_log_queue = {0};
static uint8_t m_ring_debug_log_queue_storage[DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_STORAGE_BYTES] = {0};

// Dose Data File and Queue
static queue_t m_dose_data_queue = {0};
static uint8_t m_dose_data_queue_storage[DOCK_DATA_MANAGER_DOSE_DATA_QUEUE_STORAGE_BYTES] = {0};

// Temperature Log File and Queue
static queue_t m_temperature_log_queue = {0};
static uint8_t m_temperature_log_queue_storage[DOCK_DATA_MANAGER_TEMP_LOG_QUEUE_STORAGE_BYTES] = {0};

// Weight Measurement Log File and Queue
static queue_t m_weight_measurement_log_queue = {0};
static uint8_t m_weight_measurement_log_queue_storage[DOCK_DATA_MANAGER_WEIGHT_MEAS_LOG_QUEUE_STORAGE_BYTES] = {0};

// Ring Status File and Queue
static queue_t m_ring_status_queue = {0};
static uint8_t m_ring_status_queue_storage[DOCK_DATA_MANAGER_RING_STATUS_QUEUE_STORAGE_BYTES] = {0};

// Dock Status File and Queue
static queue_t m_dock_status_queue = {0};
static uint8_t m_dock_status_queue_storage[DOCK_DATA_MANAGER_DOCK_STATUS_QUEUE_STORAGE_BYTES] = {0};

// Queue interfaces and info for each data ID
static queue_info_t m_queue_data[DATA_ID_MAX] = {0};

// FDS storage
static volatile bool m_is_fds_initialized = false;
static bool m_fds_init_started = false;
static bool m_fds_records_loaded = false;
static weight_calibration_record_storage_t m_weight_calibration_record = {0};
static dose_schedule_record_storage_t m_dose_schedule_record = {0};
static total_weight_record_storage_t m_total_weight_record = {0};
static ship_mode_record_storage_t m_ship_mode_record = {0};
static med_nfc_uid_record_storage_t m_med_nfc_uid_record = {0};

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t
   get_element_size(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t *element_size)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(element_size, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   *element_size = m_queue_data[data_id].element_size;
   return RESULT_OK;
}

static result_t dequeue(const dock_data_manager_interface_t *const interface,
                        DATA_ID data_id,
                        void *data,
                        size_t element_size,
                        size_t element_count)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(data, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(0u == element_count, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(element_size != m_queue_data[data_id].element_size, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   return m_queue_data[data_id].queue_ifc->dequeue_multiple(m_queue_data[data_id].queue_ifc, data, element_count);
}

static result_t peek(const dock_data_manager_interface_t *const interface,
                     DATA_ID data_id,
                     void *data,
                     size_t element_size,
                     size_t element_count)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(data, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(0u == element_count, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(element_size != m_queue_data[data_id].element_size, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   return m_queue_data[data_id].queue_ifc->peek_multiple(m_queue_data[data_id].queue_ifc, data, element_count);
}

static result_t pop(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t element_count)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(0u == element_count, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   return m_queue_data[data_id].queue_ifc->pop_multiple(m_queue_data[data_id].queue_ifc, element_count);
}

static result_t get_free_element_count(const dock_data_manager_interface_t *const interface,
                                       DATA_ID data_id,
                                       size_t *free_element_count)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(free_element_count, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   return m_queue_data[data_id].queue_ifc->get_available_space(m_queue_data[data_id].queue_ifc, free_element_count);
}

static result_t enqueue(const dock_data_manager_interface_t *const interface,
                        DATA_ID data_id,
                        void *data,
                        size_t element_size,
                        size_t element_count)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(data, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(0u == element_count, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE(element_size != m_queue_data[data_id].element_size, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   size_t free_element_count = 0u;
   result_t result = get_free_element_count(interface, data_id, &free_element_count);

   UPDATE_ERR_IF_TRUE(result, element_count > free_element_count, DOCK_DATA_MANAGER_QUEUE_FULL);
   IF_OK_RUN_AND_UPDATE(
      result, m_queue_data[data_id].queue_ifc->enqueue_multiple(m_queue_data[data_id].queue_ifc, data, element_count));

   return result;
}

static result_t
   get_element_count(const dock_data_manager_interface_t *const interface, DATA_ID data_id, size_t *element_count)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(element_count, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(data_id >= DATA_ID_MAX, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   return m_queue_data[data_id].queue_ifc->get_count(m_queue_data[data_id].queue_ifc, element_count);
}

static result_t set_weight_calibration_record(const dock_data_manager_interface_t *const interface,
                                              weight_stack_calibration_record_t *calibration_record)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(calibration_record, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      bool is_changed
         = (0
            != memcmp(
               &m_weight_calibration_record.record, calibration_record, sizeof(weight_stack_calibration_record_t)));

      if(is_changed)
      {
         weight_calibration_record_storage_t updated_record = {0};
         memcpy(&updated_record.record, calibration_record, sizeof(weight_stack_calibration_record_t));

         // result = fds_write_or_update_record(DOCK_DATA_MANAGER_DEVICE_DATA_FILE_ID,
         //                                     DOCK_DATA_MANAGER_WEIGHT_CALIBRATION_RECORD_KEY,
         //                                     updated_record.words,
         //                                     DOCK_DATA_MANAGER_WEIGHT_CAL_RECORD_WORDS);

         if(IS_OK(result))
         {
            m_weight_calibration_record = updated_record;
         }
      }
   }

   return result;
}

static result_t get_weight_calibration_record(const dock_data_manager_interface_t *const interface,
                                              weight_stack_calibration_record_t *calibration_record)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(calibration_record, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      memcpy(calibration_record, &m_weight_calibration_record.record, sizeof(weight_stack_calibration_record_t));
   }

   return result;
}

static result_t get_dose_schedule_record(const dock_data_manager_interface_t *const interface,
                                         dose_schedule_record_t *dose_schedule_record_out)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(dose_schedule_record_out, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      memcpy(dose_schedule_record_out, &m_dose_schedule_record.schedule_record, sizeof(dose_schedule_record_t));
   }

   return result;
}
static result_t set_dose_schedule_record(const dock_data_manager_interface_t *const interface,
                                         dose_schedule_record_t *dose_schedule_record_in)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(dose_schedule_record_in, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      int memcpy_result
         = memcmp(&m_dose_schedule_record.schedule_record, dose_schedule_record_in, sizeof(dose_schedule_record_t));
      bool is_changed = (0 != memcpy_result);

      if(is_changed)
      {
         dose_schedule_record_storage_t updated_record = {0};
         memcpy(&updated_record.schedule_record, dose_schedule_record_in, sizeof(dose_schedule_record_t));

         // result = fds_write_or_update_record(DOCK_DATA_MANAGER_DEVICE_DATA_FILE_ID,
         //                                     DOCK_DATA_MANAGER_DOSE_SCHEDULE_RECORD_KEY,
         //                                     updated_record.words,
         //                                     DOCK_DATA_MANAGER_DOSE_SCHEDULE_RECORD_WORDS);

         if(IS_OK(result))
         {
            m_dose_schedule_record = updated_record;
         }
      }
   }

   return result;
}

static result_t get_total_weight_dispensed_mg(const dock_data_manager_interface_t *const interface,
                                              uint32_t *total_weight_dispensed_mg)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(total_weight_dispensed_mg, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      *total_weight_dispensed_mg = m_total_weight_record.value;
   }

   return result;
}

static result_t set_total_weight_dispensed_mg(const dock_data_manager_interface_t *const interface,
                                              uint32_t total_weight_dispensed_mg)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      bool is_changed = (m_total_weight_record.value != total_weight_dispensed_mg);

      if(is_changed)
      {
         total_weight_record_storage_t updated_record = {.value = total_weight_dispensed_mg};

         // result = fds_write_or_update_record(DOCK_DATA_MANAGER_DEVICE_DATA_FILE_ID,
         //                                     DOCK_DATA_MANAGER_TOTAL_WEIGHT_RECORD_KEY,
         //                                     updated_record.words,
         //                                     DOCK_DATA_MANAGER_TOTAL_WEIGHT_RECORD_WORDS);

         if(IS_OK(result))
         {
            m_total_weight_record = updated_record;
         }
      }
   }

   return result;
}

static result_t get_ship_mode_data(const dock_data_manager_interface_t *const interface, ship_mode_t *ship_mode_data)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(ship_mode_data, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      memcpy(ship_mode_data, &m_ship_mode_record.value, sizeof(ship_mode_t));
   }

   return result;
}
static result_t set_ship_mode_data(const dock_data_manager_interface_t *const interface, ship_mode_t *ship_mode_data)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(ship_mode_data, DOCK_DATA_MANAGER_ERROR_NULL);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      bool is_changed = (0 != memcmp(&m_ship_mode_record.value, ship_mode_data, sizeof(ship_mode_t)));

      if(is_changed)
      {
         ship_mode_record_storage_t updated_record = {0};
         memcpy(&updated_record.value, ship_mode_data, sizeof(ship_mode_t));

         // result = fds_write_or_update_record(DOCK_DATA_MANAGER_DEVICE_DATA_FILE_ID,
         //                                     DOCK_DATA_MANAGER_SHIP_MODE_RECORD_KEY,
         //                                     updated_record.words,
         //                                     DOCK_DATA_MANAGER_SHIP_MODE_RECORD_WORDS);

         if(IS_OK(result))
         {
            m_ship_mode_record = updated_record;
         }
      }
   }

   return result;
}

static result_t
   get_med_nfc_uid(const dock_data_manager_interface_t *const interface, uint8_t *med_nfc_uid, uint8_t uid_buffer_size)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(med_nfc_uid, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE((uid_buffer_size != NFC_TAG_MAX_UID_SIZE), DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      memcpy(med_nfc_uid, m_med_nfc_uid_record.uid, NFC_TAG_MAX_UID_SIZE);
   }

   return result;
}
static result_t
   set_med_nfc_uid(const dock_data_manager_interface_t *const interface, uint8_t *med_nfc_uid, uint8_t uid_buffer_size)
{
   RETURN_ERR_IF_NULL(interface, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_NULL(med_nfc_uid, DOCK_DATA_MANAGER_ERROR_NULL);
   RETURN_ERR_IF_TRUE(uid_buffer_size != NFC_TAG_MAX_UID_SIZE, DOCK_DATA_MANAGER_ERROR_INVALID_PARAM);

   result_t result = RESULT_OK;

   if(IS_OK(result))
   {
      bool is_changed = (0 != memcmp(m_med_nfc_uid_record.uid, med_nfc_uid, NFC_TAG_MAX_UID_SIZE));

      if(is_changed)
      {
         med_nfc_uid_record_storage_t updated_record = {0};
         memcpy(updated_record.uid, med_nfc_uid, NFC_TAG_MAX_UID_SIZE);

         // result = fds_write_or_update_record(DOCK_DATA_MANAGER_DEVICE_DATA_FILE_ID,
         //                                     DOCK_DATA_MANAGER_MED_NFC_UID_RECORD_KEY,
         //                                     updated_record.words,
         //                                     DOCK_DATA_MANAGER_MED_NFC_UID_RECORD_WORDS);

         if(IS_OK(result))
         {
            m_med_nfc_uid_record = updated_record;
         }
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t dock_data_manager_init(dock_data_manager_t *const self)
{
   RETURN_ERR_IF_NULL(self, DOCK_DATA_MANAGER_ERROR_NULL);

   self->_initialized = false;
   m_is_fds_initialized = false;
   m_fds_init_started = false;
   m_fds_records_loaded = false;
   memset(&m_weight_calibration_record, 0, sizeof(m_weight_calibration_record));
   memset(&m_dose_schedule_record, 0, sizeof(m_dose_schedule_record));
   memset(&m_total_weight_record, 0, sizeof(m_total_weight_record));
   memset(&m_ship_mode_record, 0, sizeof(m_ship_mode_record));
   memset(&m_med_nfc_uid_record, 0, sizeof(m_med_nfc_uid_record));

   self->interface.parent = self;

   self->interface.enqueue = enqueue;
   self->interface.dequeue = dequeue;
   self->interface.peek = peek;
   self->interface.pop = pop;
   self->interface.get_free_element_count = get_free_element_count;
   self->interface.get_element_size = get_element_size;
   self->interface.get_element_count = get_element_count;
   self->interface.set_weight_calibration_record = set_weight_calibration_record;
   self->interface.get_weight_calibration_record = get_weight_calibration_record;
   self->interface.get_dose_schedule_record = get_dose_schedule_record;
   self->interface.set_dose_schedule_record = set_dose_schedule_record;
   self->interface.get_total_weight_dispensed_mg = get_total_weight_dispensed_mg;
   self->interface.set_total_weight_dispensed_mg = set_total_weight_dispensed_mg;
   self->interface.get_ship_mode_data = get_ship_mode_data;
   self->interface.set_ship_mode_data = set_ship_mode_data;
   self->interface.get_med_nfc_uid = get_med_nfc_uid;
   self->interface.set_med_nfc_uid = set_med_nfc_uid;

   // Initialize the dose event queue
   result_t result = queue_init(
      &m_dose_event_queue, m_dose_event_queue_storage, sizeof(m_dose_event_queue_storage), sizeof(dose_event_t));
   IF_OK_RUN_AND_UPDATE(result,
                        m_dose_event_queue.interface.get_element_size(&m_dose_event_queue.interface,
                                                                      &m_queue_data[DATA_ID_DOSE_EVENT].element_size));
   m_queue_data[DATA_ID_DOSE_EVENT].queue_ifc = &m_dose_event_queue.interface;

   // Initialize the dose data queue
   IF_OK_RUN_AND_UPDATE(
      result,
      queue_init(
         &m_dose_data_queue, m_dose_data_queue_storage, sizeof(m_dose_data_queue_storage), sizeof(dose_data_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        m_dose_data_queue.interface.get_element_size(
                           &m_dose_data_queue.interface, &m_queue_data[DATA_ID_DOSE_DATAPOINT].element_size));
   m_queue_data[DATA_ID_DOSE_DATAPOINT].queue_ifc = &m_dose_data_queue.interface;

   // Initialize the temperature log queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_temperature_log_queue,
                                   m_temperature_log_queue_storage,
                                   sizeof(m_temperature_log_queue_storage),
                                   sizeof(temperature_log_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        m_temperature_log_queue.interface.get_element_size(
                           &m_temperature_log_queue.interface, &m_queue_data[DATA_ID_TEMPERATURE_LOG].element_size));
   m_queue_data[DATA_ID_TEMPERATURE_LOG].queue_ifc = &m_temperature_log_queue.interface;

   // Initialize the weight measurement queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_weight_measurement_log_queue,
                                   m_weight_measurement_log_queue_storage,
                                   sizeof(m_weight_measurement_log_queue_storage),
                                   sizeof(dock_weight_measurement_t)));
   IF_OK_RUN_AND_UPDATE(
      result,
      m_weight_measurement_log_queue.interface.get_element_size(
         &m_weight_measurement_log_queue.interface, &m_queue_data[DATA_ID_WEIGHT_MEASUREMENT].element_size));
   m_queue_data[DATA_ID_WEIGHT_MEASUREMENT].queue_ifc = &m_weight_measurement_log_queue.interface;

   // Initialize the dock charge status queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_dock_charge_status_queue,
                                   m_dock_charge_status_queue_storage,
                                   sizeof(m_dock_charge_status_queue_storage),
                                   sizeof(dock_charge_status_t)));
   IF_OK_RUN_AND_UPDATE(
      result,
      m_dock_charge_status_queue.interface.get_element_size(&m_dock_charge_status_queue.interface,
                                                            &m_queue_data[DATA_ID_DOCK_CHARGE_STATUS].element_size));
   m_queue_data[DATA_ID_DOCK_CHARGE_STATUS].queue_ifc = &m_dock_charge_status_queue.interface;

   // Initialize the ring docked status queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ring_docked_status_queue,
                                   m_ring_docked_status_queue_storage,
                                   sizeof(m_ring_docked_status_queue_storage),
                                   sizeof(ring_docked_status_t)));
   IF_OK_RUN_AND_UPDATE(
      result,
      m_ring_docked_status_queue.interface.get_element_size(&m_ring_docked_status_queue.interface,
                                                            &m_queue_data[DATA_ID_RING_DOCKED_STATUS].element_size));
   m_queue_data[DATA_ID_RING_DOCKED_STATUS].queue_ifc = &m_ring_docked_status_queue.interface;

   // Initialize the bluetooth status queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_bluetooth_status_queue,
                                   m_bluetooth_status_queue_storage,
                                   sizeof(m_bluetooth_status_queue_storage),
                                   sizeof(bluetooth_status_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        m_bluetooth_status_queue.interface.get_element_size(
                           &m_bluetooth_status_queue.interface, &m_queue_data[DATA_ID_BLUETOOTH_STATUS].element_size));
   m_queue_data[DATA_ID_BLUETOOTH_STATUS].queue_ifc = &m_bluetooth_status_queue.interface;

   // Initialize the dock battery level queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_dock_battery_level_queue,
                                   m_dock_battery_level_queue_storage,
                                   sizeof(m_dock_battery_level_queue_storage),
                                   sizeof(battery_level_t)));
   IF_OK_RUN_AND_UPDATE(
      result,
      m_dock_battery_level_queue.interface.get_element_size(&m_dock_battery_level_queue.interface,
                                                            &m_queue_data[DATA_ID_DOCK_BATTERY_LEVEL].element_size));
   m_queue_data[DATA_ID_DOCK_BATTERY_LEVEL].queue_ifc = &m_dock_battery_level_queue.interface;

   // Initialize the ring battery level queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ring_battery_level_queue,
                                   m_ring_battery_level_queue_storage,
                                   sizeof(m_ring_battery_level_queue_storage),
                                   sizeof(battery_level_t)));
   IF_OK_RUN_AND_UPDATE(
      result,
      m_ring_battery_level_queue.interface.get_element_size(&m_ring_battery_level_queue.interface,
                                                            &m_queue_data[DATA_ID_RING_BATTERY_LEVEL].element_size));
   m_queue_data[DATA_ID_RING_BATTERY_LEVEL].queue_ifc = &m_ring_battery_level_queue.interface;

   // Initialize the dock debug log queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_dock_debug_log_queue,
                                   m_dock_debug_log_queue_storage,
                                   sizeof(m_dock_debug_log_queue_storage),
                                   sizeof(raw_debug_log_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        m_dock_debug_log_queue.interface.get_element_size(
                           &m_dock_debug_log_queue.interface, &m_queue_data[DATA_ID_DOCK_DEBUG_LOG].element_size));
   m_queue_data[DATA_ID_DOCK_DEBUG_LOG].queue_ifc = &m_dock_debug_log_queue.interface;

   // Initialize the ring debug log queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ring_debug_log_queue,
                                   m_ring_debug_log_queue_storage,
                                   sizeof(m_ring_debug_log_queue_storage),
                                   sizeof(raw_debug_log_t)));

   IF_OK_RUN_AND_UPDATE(result,
                        m_ring_debug_log_queue.interface.get_element_size(
                           &m_ring_debug_log_queue.interface, &m_queue_data[DATA_ID_RING_DEBUG_LOG].element_size));
   m_queue_data[DATA_ID_RING_DEBUG_LOG].queue_ifc = &m_ring_debug_log_queue.interface;

   // Initialize the ring status queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ring_status_queue,
                                   m_ring_status_queue_storage,
                                   sizeof(m_ring_status_queue_storage),
                                   sizeof(ring_status_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        m_ring_status_queue.interface.get_element_size(
                           &m_ring_status_queue.interface, &m_queue_data[DATA_ID_RING_STATUS].element_size));
   m_queue_data[DATA_ID_RING_STATUS].queue_ifc = &m_ring_status_queue.interface;

   // Initialize the dock status queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_dock_status_queue,
                                   m_dock_status_queue_storage,
                                   sizeof(m_dock_status_queue_storage),
                                   sizeof(dock_status_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        m_dock_status_queue.interface.get_element_size(
                           &m_dock_status_queue.interface, &m_queue_data[DATA_ID_DOCK_STATUS].element_size));
   m_queue_data[DATA_ID_DOCK_STATUS].queue_ifc = &m_dock_status_queue.interface;

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
