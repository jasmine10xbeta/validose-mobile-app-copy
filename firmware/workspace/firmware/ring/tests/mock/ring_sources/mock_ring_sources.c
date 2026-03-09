/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file mock_ring_sources.c
 * @ingroup ring_sources
 * @brief Mock implementation of ring sources for testing purposes.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "debug.h"
#include "queue.h"
#include "result.h"
#include "ring_sources.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_LEN (100u)
#define DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_STORAGE_BYTES                                                               \
   (DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_LEN * sizeof(dose_event_t)) // 1800 bytes of RAM

#define DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_LEN (200u)
#define DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_STORAGE_BYTES                                                       \
   (DOCK_DATA_MANAGER_DOCK_CHARGE_STATUS_QUEUE_LEN * sizeof(dock_charge_status_t)) // 1800 bytes of RAM

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

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t clear_bytes(const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t byte_count);
static result_t copy_bytes(
   const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t max_bytes, uint8_t *dest, uint16_t *copied_bytes);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
// Dose Event File and Queue
static queue_t m_dose_event_queue = {0};
static uint8_t m_dose_event_queue_storage[DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_STORAGE_BYTES] = {0};
static queue_t m_ring_battery_level_queue = {0};
static uint8_t m_ring_battery_level_queue_storage[DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_STORAGE_BYTES] = {0};
static queue_t m_ring_debug_log_queue = {0};
static uint8_t m_ring_debug_log_queue_storage[DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_STORAGE_BYTES] = {0};

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t clear_bytes(const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t byte_count)
{
   switch(id)
   {
      case SOURCE_ID_STATUS:
         return RESULT_OK;
         break;

      case SOURCE_ID_DOSE:
         return ifc->parent->_dose_event_queue_ifc->pop_multiple(ifc->parent->_dose_event_queue_ifc,
                                                                 byte_count / sizeof(dose_event_t));
         break;

      case SOURCE_ID_BATTERY:
         return ifc->parent->_ring_battery_level_queue_ifc->pop_multiple(ifc->parent->_ring_battery_level_queue_ifc,
                                                                         byte_count / sizeof(battery_level_t));
         break;

      case SOURCE_ID_ERROR:
         return ifc->parent->_ring_debug_log_queue_ifc->pop_multiple(ifc->parent->_ring_debug_log_queue_ifc,
                                                                     byte_count / sizeof(raw_debug_log_t));
         break;

      default:
         return SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID;
   }

   return RESULT_OK;
}

static result_t copy_bytes(
   const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t max_bytes, uint8_t *dest, uint16_t *copied_bytes)
{
   size_t available_elements = 0u;
   size_t dequeue_element_count = 0u;

   result_t result = RESULT_OK;

   switch(id)
   {
      case SOURCE_ID_STATUS:

         *copied_bytes = sizeof(ring_status_t) < max_bytes ? sizeof(ring_status_t) : max_bytes;

         ifc->parent->_dose_event_queue_ifc->get_available_space(ifc->parent->_dose_event_queue_ifc,
                                                                 &available_elements);
         ifc->parent->_status.dose_fifo_used_percent
            = 100 - ((available_elements * 100) / DOCK_DATA_MANAGER_DOSE_EVENT_QUEUE_LEN);
         ifc->parent->_ring_battery_level_queue_ifc->get_available_space(ifc->parent->_ring_battery_level_queue_ifc,
                                                                         &available_elements);
         ifc->parent->_status.battery_fifo_used_percent
            = 100 - ((available_elements * 100) / DOCK_DATA_MANAGER_RING_BATTERY_LEVEL_QUEUE_LEN);
         ifc->parent->_ring_debug_log_queue_ifc->get_available_space(ifc->parent->_ring_debug_log_queue_ifc,
                                                                     &available_elements);
         ifc->parent->_status.error_fifo_used_percent
            = 100 - ((available_elements * 100) / DOCK_DATA_MANAGER_RING_DEBUG_LOG_QUEUE_LEN);

         memcpy(dest, &ifc->parent->_status, *copied_bytes);

         return RESULT_OK;
         break;

      case SOURCE_ID_DOSE:
         ifc->parent->_dose_event_queue_ifc->get_count(ifc->parent->_dose_event_queue_ifc, &available_elements);

         dequeue_element_count = max_bytes / sizeof(dose_event_t) < available_elements ?
                                    max_bytes / sizeof(dose_event_t) :
                                    available_elements;

         result = ifc->parent->_dose_event_queue_ifc->peek_multiple(
            ifc->parent->_dose_event_queue_ifc, dest, dequeue_element_count);
         *copied_bytes = dequeue_element_count * sizeof(dose_event_t);

         if(IS_ERR(result))
         {
            printf("[MOCK_RING_SOURCES] Error copying dose events: %d\n", GET_ERR_CODE(result));
         }
         break;

      case SOURCE_ID_BATTERY:

         ifc->parent->_ring_battery_level_queue_ifc->get_count(ifc->parent->_ring_battery_level_queue_ifc,
                                                               &available_elements);

         dequeue_element_count = max_bytes / sizeof(battery_level_t) < available_elements ?
                                    max_bytes / sizeof(battery_level_t) :
                                    available_elements;

         result = ifc->parent->_ring_battery_level_queue_ifc->peek_multiple(
            ifc->parent->_ring_battery_level_queue_ifc, dest, dequeue_element_count);

         *copied_bytes = dequeue_element_count * sizeof(battery_level_t);

         break;

      case SOURCE_ID_ERROR:

         ifc->parent->_ring_debug_log_queue_ifc->get_count(ifc->parent->_ring_debug_log_queue_ifc, &available_elements);
         dequeue_element_count = max_bytes / sizeof(raw_debug_log_t) < available_elements ?
                                    max_bytes / sizeof(raw_debug_log_t) :
                                    available_elements;

         result = ifc->parent->_ring_debug_log_queue_ifc->peek_multiple(
            ifc->parent->_ring_debug_log_queue_ifc, dest, dequeue_element_count);

         *copied_bytes = dequeue_element_count * sizeof(raw_debug_log_t);
         break;

      default:
         return SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID;
   }

   return result;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_sources_init(ring_sources_t *const self)
{
   result_t result = RESULT_OK;

   self->interface.parent = self;

   // Map function pointers
   self->interface.clear_bytes = clear_bytes;
   self->interface.copy_bytes = copy_bytes;

   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ring_battery_level_queue,
                                   m_ring_battery_level_queue_storage,
                                   sizeof(m_ring_battery_level_queue_storage),
                                   sizeof(battery_level_t)));
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ring_debug_log_queue,
                                   m_ring_debug_log_queue_storage,
                                   sizeof(m_ring_debug_log_queue_storage),
                                   sizeof(raw_debug_log_t)));

   IF_OK_RUN_AND_UPDATE(
      result,
      queue_init(
         &m_dose_event_queue, m_dose_event_queue_storage, sizeof(m_dose_event_queue_storage), sizeof(dose_event_t)));

   self->_dose_event_queue_ifc = &m_dose_event_queue.interface;
   self->_ring_battery_level_queue_ifc = &m_ring_battery_level_queue.interface;
   self->_ring_debug_log_queue_ifc = &m_ring_debug_log_queue.interface;

   self->_status = (ring_status_t){0};

   return result;
}