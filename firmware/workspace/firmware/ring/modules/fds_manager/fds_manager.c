/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file fds_manager.c
 * @ingroup fds_manager_module
 * @brief Implementation of the FDS Manager module.
 *
 * @note This module provides an interface for storing and retrieving records in non-volatile memory (NVM)
 *
 * This module will not be called frequently, so blocking calls and delays are acceptable.
 *
 * @todo Remove delays
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "fds_manager.h"
#include "debug.h"
#include "fds.h"
#include "fds_manager_interface.h"
#include <nrf_delay.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_FDS_MANAGER_MODULE;

// Timeout for FDS operations in milliseconds. 1ms was determined to be sufficient in testing.
// This was increased by a factor of 5 due to the infrequent use of this module and to add extra margin
#define FDS_TIMEOUT_MS      (5u)
#define DEVICE_DATA_FILE_ID (0x0001)
#define BYTES_IN_WORD       (4u)
#define MAX_INIT_MS         (1000u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface function declarations
static result_t store_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t data);
static result_t retrieve_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t *data);

// Non-interface function declarations
static void fds_event_handler(fds_evt_t const *p_evt);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static bool m_fds_init = false;

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static void fds_event_handler(fds_evt_t const *p_evt)
{
   if(NRF_SUCCESS == p_evt->result)
   {
      DEBUG_TRACE("Event: %d received (NRF_SUCCESS)", p_evt->id);
   }
   else
   {
      DEBUG_TRACE("Event: %d received (%d)", p_evt->id, p_evt->result);
   }

   switch(p_evt->id)
   {
      case FDS_EVT_INIT:
         if(NRF_SUCCESS == p_evt->result)
         {
            m_fds_init = true;
         }
         break;

      case FDS_EVT_WRITE:
      {
         if(NRF_SUCCESS == p_evt->result)
         {
            DEBUG_TRACE("Record ID:\t0x%04x", p_evt->write.record_id);
            DEBUG_TRACE("File ID:\t0x%04x", p_evt->write.file_id);
            DEBUG_TRACE("Record key:\t0x%04x", p_evt->write.record_key);
         }
      }
      break;
      case FDS_EVT_UPDATE:
         // Fall through
      case FDS_EVT_DEL_RECORD:
      // Fall through
      case FDS_EVT_DEL_FILE:
      // Fall through
      case FDS_EVT_GC:
      // Fall through
      default:
         break;
   }
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t store_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t data)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, FDS_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(record_id >= RECORD_ID_MAX, FDS_MANAGER_ERROR_INVALID_RECORD_ID);

   DEBUG_INFO("Storing record. ID: %d Value: %u", record_id, (uint32_t)data);

   // Check if the record already exists with the same value. If so, skip writing to preserve flash endurance.
   uint64_t existing_data = 0;
   result_t result = retrieve_uint64_t(ifc, record_id, &existing_data);

   if(existing_data != data)
   {
      ret_code_t nrf_ret;
      uint64_t value = data;

      fds_record_t record = {0};
      record.file_id = DEVICE_DATA_FILE_ID;
      record.key = RECORD_INFOS[record_id].key;
      record.data.p_data = &value;
      record.data.length_words = sizeof(value) / BYTES_IN_WORD; // Convert bytes to words

      fds_record_desc_t desc = {0}; // Find the record first
      fds_find_token_t tok = {0};

      nrf_ret = fds_record_find(DEVICE_DATA_FILE_ID, RECORD_INFOS[record_id].key, &desc, &tok);
      nrf_delay_ms(FDS_TIMEOUT_MS);

      UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("Error finding record ID %d. nrf_ret: %d", record_id, nrf_ret);
      }

      if(IS_OK(result))
      {
         // Record found successfully - update it
         nrf_ret = fds_record_update(&desc, &record); // Update the record
         nrf_delay_ms(FDS_TIMEOUT_MS);

         UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
         if(IS_ERR(result))
         {
            DEBUG_ERROR("Error updating record! nrf_ret: %d", nrf_ret);
         }
         fds_gc(); // Run the garbage collector
      }
   }

   return result;
}

static result_t retrieve_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t *data)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, FDS_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(data, FDS_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_TRUE(record_id >= RECORD_ID_MAX, FDS_MANAGER_ERROR_INVALID_RECORD_ID);

   result_t result = RESULT_OK;
   ret_code_t nrf_ret;

   // Check if record exists, if not create it with default values
   fds_record_desc_t desc = {0};
   fds_find_token_t tok = {0};

   nrf_ret = fds_record_find(DEVICE_DATA_FILE_ID, RECORD_INFOS[record_id].key, &desc, &tok);
   nrf_delay_ms(FDS_TIMEOUT_MS);

   UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);

   if(IS_OK(result))
   {
      fds_flash_record_t flash_record = {0};

      nrf_ret = fds_record_open(&desc, &flash_record);
      nrf_delay_ms(FDS_TIMEOUT_MS);
      UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);

      if(IS_OK(result))
      {
         // Copy the record data to the data pointer
         memcpy(data, flash_record.p_data, sizeof(uint64_t));
         nrf_delay_ms(FDS_TIMEOUT_MS);

         nrf_ret = fds_record_close(&desc);
         nrf_delay_ms(FDS_TIMEOUT_MS);
         UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
      }
   }
   else
   {
      // Some other error occurred
      UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
   }
   return result;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t fds_manager_init(fds_manager_t *const self)
{
   RETURN_ERR_IF_NULL(self, FDS_MANAGER_ERROR_NULL_PTR);

   result_t result = RESULT_OK;
   ret_code_t nrf_ret;

   self->interface.parent = self;

   self->interface.store_uint64_t = store_uint64_t;
   self->interface.retrieve_uint64_t = retrieve_uint64_t;

   // Register the event handler
   (void)fds_register(fds_event_handler);

   // Initialize FDS and wait for completion
   nrf_ret = fds_init();
   UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);

   fds_stat_t fds_stats;
   fds_stat(&fds_stats);

   DEBUG_TRACE("FDS Stats - \n Open Records: %d\n Valid Records: %d\n Dirty Records: %d\n Words "
               "Reserved: %d\n Words Used: %d \nCorruption: %d",
               fds_stats.open_records,
               fds_stats.valid_records,
               fds_stats.dirty_records,
               fds_stats.words_reserved,
               fds_stats.words_used,
               fds_stats.corruption);

   // Wait up to one second for initialization to complete
   // If initialization not complete by then - return error
   for(uint16_t timeout = 0u; (timeout <= MAX_INIT_MS) && (!m_fds_init) && IS_OK(result); timeout += FDS_TIMEOUT_MS)
   {
      nrf_delay_ms(FDS_TIMEOUT_MS);

      if(timeout >= (MAX_INIT_MS - FDS_TIMEOUT_MS)) // Check for timeout
      {
         SET_ERR(result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
      }
   }

   // Initialize file system and records
   for(uint8_t idx = 0u; (idx < RECORD_ID_MAX) && IS_OK(result); idx++)
   {
      // Check if record exists, if not create it with default values
      fds_record_desc_t desc = {0};
      fds_find_token_t tok = {0};
      fds_record_t record = {0};

      nrf_ret = fds_record_find(DEVICE_DATA_FILE_ID, RECORD_INFOS[idx].key, &desc, &tok);
      nrf_delay_ms(FDS_TIMEOUT_MS);

      if(NRF_SUCCESS == nrf_ret)
      {
         // Record exists - nothing to do
         DEBUG_TRACE("==== Record ID: %d already exists", idx);
      }
      else if(FDS_ERR_NOT_FOUND == nrf_ret)
      {
         DEBUG_TRACE("==== Creating record ID: %d", idx);

         // Record not found - create it
         uint64_t null_data = 0u;

         record.file_id = DEVICE_DATA_FILE_ID;
         record.key = RECORD_INFOS[idx].key;
         record.data.p_data = &null_data;
         record.data.length_words = sizeof(uint64_t) / BYTES_IN_WORD; // Convert bytes to words, rounding up

         nrf_ret = fds_record_write(NULL, &record);
         nrf_delay_ms(FDS_TIMEOUT_MS);
         UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
      }
      else // Some other error occurred
      {
         UPDATE_IF_NRF_ERR(nrf_ret, result, FDS_MANAGER_ERROR_STORAGE_FAILURE);
      }
   }
   // ************************************************************

   if(IS_OK(result))
   {
      self->_initialization_status = INITIALIZED;
   }

   return result;
}