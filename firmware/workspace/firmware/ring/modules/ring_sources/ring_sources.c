/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_sources.c
 * @ingroup ring_sources
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "ring_sources.h"
#include "debug.h"
#include "result.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_RING_SOURCE_MANAGER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

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

// Non-interface functions

/**
 * @brief Get the number of elements available in the source with the given ID.
 *
 * @param id The source ID to query.
 * @param element_count Pointer to store the number of available elements.
 * @return result_t indicating the success or failure of the function.
 */
static result_t get_element_count(const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t *element_count);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static result_t get_element_count(const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t *element_count)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, SOURCE_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_battery_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_data_manager_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_dose_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_error_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(element_count, SOURCE_MANAGER_ERROR_INVALID_ARG_PTR);
   RETURN_ERR_IF_TRUE(id >= SOURCE_ID_MAX, SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID);

   result_t result = RESULT_OK;
   uint8_t dose_elements = 0;
   size_t queue_elements = 0;

   switch(id)
   {
      case SOURCE_ID_BATTERY:
         result = ifc->parent->_battery_queue_ifc->get_count(ifc->parent->_battery_queue_ifc, &queue_elements);
         *element_count = (uint16_t)queue_elements;
         break;

      case SOURCE_ID_ERROR:
         result = ifc->parent->_error_queue_ifc->get_count(ifc->parent->_error_queue_ifc, &queue_elements);
         *element_count = (uint16_t)queue_elements;
         break;

      case SOURCE_ID_DOSE:
         result = ifc->parent->_dose_queue_ifc->nfc_get_element_count(ifc->parent->_dose_queue_ifc, &dose_elements);
         *element_count = (uint16_t)dose_elements;
         break;

      case SOURCE_ID_STATUS:
         *element_count = 1u; // Status is always a single element
         break;

      case SOURCE_ID_CAP_DETECTION_CFG:
         *element_count = 1u; // Cap detection config is always a single element
         break;

      case SOURCE_ID_MAX:
      default:
         DEBUG_ERROR("Invalid source ID");
         SET_ERR(result, SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID);
         break;
   }
   return result;
}
/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t clear_bytes(const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t byte_count)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, SOURCE_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_battery_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_data_manager_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_dose_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_error_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_TRUE(id >= SOURCE_ID_MAX, SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID);
   RETURN_ERR_IF_TRUE(0u == byte_count, SOURCE_MANAGER_ERROR_INVALID_BYTE_COUNT);
   RETURN_ERR_IF_TRUE(byte_count % ifc->parent->_source_element_size[id] != 0, SOURCE_MANAGER_ERROR_INVALID_BYTE_COUNT);

   result_t result = RESULT_OK;
   uint16_t element_count = byte_count / ifc->parent->_source_element_size[id];

   switch(id)
   {
      case SOURCE_ID_BATTERY:
         result = ifc->parent->_battery_queue_ifc->pop_multiple(ifc->parent->_battery_queue_ifc, (size_t)element_count);
         break;

      case SOURCE_ID_ERROR:
         result = ifc->parent->_error_queue_ifc->pop_multiple(ifc->parent->_error_queue_ifc, (size_t)element_count);
         break;

      case SOURCE_ID_DOSE:
         UPDATE_ERR_IF_TRUE(result, element_count > UINT8_MAX, SOURCE_MANAGER_ERROR_INVALID_BYTE_COUNT);
         ON_ERR_DEBUG_ERROR(result, "Dose queue pop count exceeds uint8_t: %d", (unsigned int)element_count);
         IF_OK_RUN_AND_UPDATE(
            result,
            ifc->parent->_dose_queue_ifc->nfc_pop_multiple(ifc->parent->_dose_queue_ifc, (uint8_t)element_count));
         break;

      case SOURCE_ID_STATUS:
         // Do nothing - Status source has no queue to clear
         break;

      case SOURCE_ID_CAP_DETECTION_CFG:
         // Do nothing - Cap detection config source has no queue to clear
         break;

      case SOURCE_ID_MAX:
      default:
         DEBUG_ERROR("Invalid source ID");
         SET_ERR(result, SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID);
         break;
   }

   return result;
}

static result_t copy_bytes(
   const ring_sources_interface_t *ifc, SOURCE_ID id, uint16_t max_bytes, uint8_t *dest, uint16_t *copied_bytes)
{
   RETURN_ERR_IF_INTERFACE_NULL(ifc, SOURCE_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_battery_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_data_manager_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_dose_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(ifc->parent->_error_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(dest, SOURCE_MANAGER_ERROR_INVALID_DEST_PTR);
   RETURN_ERR_IF_NULL(copied_bytes, SOURCE_MANAGER_ERROR_INVALID_ARG_PTR);
   RETURN_ERR_IF_TRUE(id >= SOURCE_ID_MAX, SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID);
   RETURN_ERR_IF_TRUE(0u == max_bytes, SOURCE_MANAGER_ERROR_INVALID_DEST_SIZE);
   RETURN_ERR_IF_TRUE((0u == max_bytes / ifc->parent->_source_element_size[id]),
                      SOURCE_MANAGER_ERROR_INVALID_DEST_SIZE);

   result_t result = RESULT_OK;
   ring_sources_t *self = ifc->parent;

   // Set the element size for the requested source ID
   uint16_t max_elements = max_bytes / self->_source_element_size[id];

   // Ensure that we do not copy more elements than are available or request 0 elements from a source
   uint16_t available_elements = 0;
   uint16_t copied_elements = 0;
   result = get_element_count(ifc, id, &available_elements);

   if(IS_OK(result) && (available_elements > 0))
   {
      // Clamp copied_elements to the maximum that can fit in the provided buffer
      copied_elements = max_elements > available_elements ? available_elements : max_elements;
      *copied_bytes = copied_elements * self->_source_element_size[id];

      switch(id)
      {
         case SOURCE_ID_BATTERY:
            result = self->_battery_queue_ifc->peek_multiple(self->_battery_queue_ifc, dest, (size_t)copied_elements);
            break;

         case SOURCE_ID_ERROR:
            result = self->_error_queue_ifc->peek_multiple(self->_error_queue_ifc, dest, (size_t)copied_elements);
            break;

         case SOURCE_ID_DOSE:
            UPDATE_ERR_IF_TRUE(result, copied_elements > UINT8_MAX, SOURCE_MANAGER_ERROR_INVALID_BYTE_COUNT);
            ON_ERR_DEBUG_ERROR(result, "Dose queue peek count exceeds uint8_t: %d", (unsigned int)copied_elements);
            IF_OK_RUN_AND_UPDATE(
               result, self->_dose_queue_ifc->nfc_peek_multiple(self->_dose_queue_ifc, dest, (uint8_t)copied_elements));
            break;

         case SOURCE_ID_STATUS:
            result = self->_data_manager_ifc->get_status(self->_data_manager_ifc, (void *)dest);
            break;

         case SOURCE_ID_CAP_DETECTION_CFG:
            result = self->_data_manager_ifc->get_cap_detection_status(self->_data_manager_ifc, (void *)dest);
            break;

         case SOURCE_ID_MAX:
         default:
            DEBUG_ERROR("Invalid source ID");
            SET_ERR(result, SOURCE_MANAGER_ERROR_INVALID_SOURCE_ID);
            break;
      }
   }
   return result;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_sources_init(ring_sources_t *const self,
                           ring_data_manager_interface_t *data_manager_ifc,
                           nfc_tag_eeprom_queue_interface_t *dose_queue_ifc,
                           queue_interface_t *error_queue_ifc,
                           queue_interface_t *battery_queue_ifc)
{
   RETURN_ERR_IF_NULL(self, SOURCE_MANAGER_ERROR_NULL_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(data_manager_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(dose_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(error_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);
   RETURN_ERR_IF_NULL(battery_queue_ifc, SOURCE_MANAGER_ERROR_NULL_SOURCE_INTERFACE_PTR);

   // Assign parent
   self->interface.parent = self;

   // Map function pointers
   self->interface.clear_bytes = clear_bytes;
   self->interface.copy_bytes = copy_bytes;

   // Assign source interfaces
   self->_data_manager_ifc = data_manager_ifc;
   self->_dose_queue_ifc = dose_queue_ifc;
   self->_battery_queue_ifc = battery_queue_ifc;
   self->_error_queue_ifc = error_queue_ifc;

   // Keep a local copy of element sizes for each source ID
   // This is to avoid repeated calls to get_element_size functions and the associated error handling complexity
   size_t dose_source_element_size = 0;
   size_t error_source_element_size = 0;
   size_t battery_source_element_size = 0;

   result_t result = dose_queue_ifc->nfc_get_element_size(dose_queue_ifc, &dose_source_element_size);
   IF_OK_RUN_AND_UPDATE(result, error_queue_ifc->get_element_size(error_queue_ifc, &error_source_element_size));
   IF_OK_RUN_AND_UPDATE(result, battery_queue_ifc->get_element_size(battery_queue_ifc, &battery_source_element_size));

   self->_source_element_size[SOURCE_ID_BATTERY] = (uint16_t)battery_source_element_size;
   self->_source_element_size[SOURCE_ID_ERROR] = (uint16_t)error_source_element_size;
   self->_source_element_size[SOURCE_ID_DOSE] = (uint16_t)dose_source_element_size;
   self->_source_element_size[SOURCE_ID_STATUS] = (uint16_t)sizeof(ring_status_t);
   self->_source_element_size[SOURCE_ID_CAP_DETECTION_CFG] = (uint16_t)sizeof(cap_detection_status_t);

   // Unmap interfaces if there was an error during initialization
   // This way interface checks will double as initialization checks
   if(IS_ERR(result))
   {
      self->interface.parent = NULL;  // This will result in interface pointer checks failing if use is attempted
      self->_data_manager_ifc = NULL; // after a failure during initialization
      self->_dose_queue_ifc = NULL;
      self->_battery_queue_ifc = NULL;
      self->_error_queue_ifc = NULL;
   }

   return result;
}