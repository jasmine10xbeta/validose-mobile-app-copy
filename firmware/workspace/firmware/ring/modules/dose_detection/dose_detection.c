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

// Custom includes
#include "dose_detection.h"

#include "common.h"
#include "debug.h"
#include "dose_detection_fsm.h"
#include "queue.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOSE_DETECTION_MODULE;

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
static result_t clear_dose_events(dose_detection_interface_t *interface);
static result_t get_current_state(dose_detection_interface_t *interface, DOSE_DETECTION_STATE *state_out);
static result_t is_dose_event_available(dose_detection_interface_t *interface, bool *is_available);
static result_t
   try_get_dose_event(dose_detection_interface_t *interface, dose_detection_event_t *event_out, bool *event_retrieved);
static result_t process(dose_detection_interface_t *interface);

// Non-interface functions

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t clear_dose_events(dose_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_DETECTION_ERROR_NULL_PTR);

   dose_detection_t *p_self = interface->parent;
   queue_interface_t *p_queue = &p_self->_dose_event_queue.interface;

   RETURN_ERR_IF_TRUE(!p_self->_is_initialized, DOSE_DETECTION_ERROR_NOT_INITIALIZED);

   result_t result = p_queue->flush(p_queue);
   if(IS_ERR(result))
   {
      DEBUG_ERROR("Error flushing dose event queue [result=0x%04X]", result);
   }

   return result;
}

static result_t get_current_state(dose_detection_interface_t *interface, DOSE_DETECTION_STATE *state_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(state_out, DOSE_DETECTION_ERROR_NULL_PTR);

   dose_detection_t *p_self = interface->parent;
   statemachine_t *p_fsm = &p_self->_fsm;

   RETURN_ERR_IF_TRUE(!p_self->_is_initialized, DOSE_DETECTION_ERROR_NOT_INITIALIZED);

   *state_out = (DOSE_DETECTION_STATE)(p_fsm->current_state);

   return RESULT_OK;
}

static result_t is_dose_event_available(dose_detection_interface_t *interface, bool *is_available)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(is_available, DOSE_DETECTION_ERROR_NULL_PTR);

   dose_detection_t *p_self = interface->parent;
   queue_interface_t *p_queue = &p_self->_dose_event_queue.interface;

   RETURN_ERR_IF_TRUE(!p_self->_is_initialized, DOSE_DETECTION_ERROR_NOT_INITIALIZED);

   size_t event_count = 0u;
   result_t result = p_queue->get_count(p_queue, &event_count);
   if(IS_OK(result))
   {
      *is_available = (event_count > 0u);
   }
   else
   {
      DEBUG_ERROR("Error getting dose event count [result=0x%04X]", result);
   }

   return result;
}

result_t
   try_get_dose_event(dose_detection_interface_t *interface, dose_detection_event_t *event_out, bool *event_retrieved)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(event_out, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(event_retrieved, DOSE_DETECTION_ERROR_NULL_PTR);

   dose_detection_t *p_self = interface->parent;
   queue_interface_t *p_queue = &p_self->_dose_event_queue.interface;

   RETURN_ERR_IF_TRUE(!p_self->_is_initialized, DOSE_DETECTION_ERROR_NOT_INITIALIZED);

   *event_retrieved = false;

   size_t event_count = 0u;
   result_t result = p_queue->get_count(p_queue, &event_count);
   if(IS_OK(result) && (event_count > 0u))
   {
      result = p_queue->dequeue(p_queue, event_out);
      if(IS_OK(result))
      {
         *event_retrieved = true;
      }
      else
      {
         DEBUG_ERROR("Error dequeuing dose event [result=0x%04X]", result);
      }
   }

   return result;
}

static result_t process(dose_detection_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, DOSE_DETECTION_ERROR_NULL_PTR);

   dose_detection_t *p_self = interface->parent;
   statemachine_t *p_fsm = &p_self->_fsm;
   cap_detection_interface_t *p_cap = p_self->_cap_interface;
   tilt_detection_interface_t *p_tilt = p_self->_tilt_interface;
   system_time_interface_t *p_time = p_self->_system_time_interface;

   RETURN_ERR_IF_TRUE(!p_self->_is_initialized, DOSE_DETECTION_ERROR_NOT_INITIALIZED);

   CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   uint8_t tilt_count = 0u;
   uint64_t current_time_ms = 0u;

   // Get cap state
   uint16_t prox_value = 0u;
   result_t result = p_cap->get_cap_status(p_cap, &cap_state, &prox_value);
   if(IS_ERR(result))
   {
      DEBUG_ERROR("Error getting cap status [result=0x%04X]", result);
   }

   // Get tilt state if not in IDLE state
   if(IS_OK(result) && (DOSE_DETECTION_STATE_IDLE != p_fsm->current_state))
   {
      tilt_data_t tilts[TILT_DETECTION_MAX_TILTS] = {0};
      result = p_tilt->get_tilts(p_tilt, &tilt_count, tilts);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("Error getting tilt data [result=0x%04X]", result);
      }
   }

   // Get system time
   if(IS_OK(result))
   {
      result = p_time->get_time_ms(p_time, &current_time_ms);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("Error getting system time [result=0x%04X]", result);
      }
   }

   if(IS_OK(result))
   {
      // Update state machine
      dose_detection_fsm_inputs_t fsm_inputs = {.cap_on = (CAP_STATE_OPEN != cap_state), // Default to cap ON if unknown
                                                .tilt_detected = (tilt_count > 0u),
                                                .dose_start_time_ms = p_self->_dose_start_time,
                                                .current_time_ms = current_time_ms,
                                                .valid_dose_threshold_ms = DOSE_DETECTION_VALID_DOSE_DURATION_MS};
      dose_detection_fsm_outputs_t fsm_outputs = {0};
      p_fsm->update_statemachine(p_fsm, &fsm_inputs, &fsm_outputs);

      if(fsm_outputs.state_changed && (DOSE_DETECTION_STATE_MAX > p_fsm->current_state) && (0 <= p_fsm->current_state))
      {
         DEBUG_DEBUG("Dose detection FSM state changed to %s", dose_detection_fsm_state_names[p_fsm->current_state]);
      }

      // Handle state changes and outputs
      switch(p_fsm->current_state)
      {
         case DOSE_DETECTION_STATE_IDLE:
         {
            if(fsm_outputs.state_changed)
            {
               // Clear dose event data
               p_self->_dose_start_time = 0u;
               p_self->_tilt_count = 0u;

               // Disable tilt detection
               result = p_tilt->set_module_dormant(p_tilt);
               if(IS_OK(result))
               {
                  DEBUG_DEBUG("Tilt detection deactivated");
               }
               else
               {
                  DEBUG_ERROR("Error setting tilt module dormant [result=0x%04X]", result);
               }
            }
         }
         break;

         case DOSE_DETECTION_STATE_PRIMED:
         {
            if(fsm_outputs.state_changed)
            {
               // Set dose start time
               p_self->_dose_start_time = current_time_ms;

               // Enable tilt detection
               result = p_tilt->set_module_active(p_tilt);
               if(IS_OK(result))
               {
                  DEBUG_DEBUG("Tilt detection activated");
               }
               else
               {
                  DEBUG_ERROR("Error setting tilt module active [result=0x%04X]", result);
               }
            }
         }
         break;

         case DOSE_DETECTION_STATE_EVALUATING:
         {
            // Collect tilt count
            p_self->_tilt_count += tilt_count;
         }
         break;

         case DOSE_DETECTION_STATE_DOSE_DETECTED:
         {
            if(fsm_outputs.state_changed)
            {
               // Collect final tilts that might have occurred when exiting EVALUATING state
               p_self->_tilt_count += tilt_count;

               // Record dose event
               dose_detection_event_t event = {.dose_start_time_ms = p_self->_dose_start_time,
                                               .dose_end_time_ms = current_time_ms,
                                               .tilt_count = p_self->_tilt_count,
                                               .state = DOSE_EVENT_STATE_CAP_CLOSED_WITH_TILT};

               queue_interface_t *p_queue = &p_self->_dose_event_queue.interface;

               result = p_queue->enqueue(p_queue, &event);
               if(IS_OK(result))
               {
                  DEBUG_DEBUG("Dose event recorded: start_time=%llu, end_time=%llu, tilt_count=%u",
                              (unsigned long long)event.dose_start_time_ms,
                              (unsigned long long)event.dose_end_time_ms,
                              event.tilt_count);
               }
               else
               {
                  DEBUG_ERROR("Error enqueuing dose event [result=0x%04X]", result);
               }
            }
         }
         break;

         default:
            DEBUG_ERROR("Unknown dose detection state: %d", p_self->_fsm.current_state);
            break;
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t dose_detection_init(dose_detection_t *const p_self,
                             system_time_interface_t *const p_system_time,
                             cap_detection_interface_t *const p_cap_interface,
                             tilt_detection_interface_t *const p_tilt_interface)
{
   RETURN_ERR_IF_NULL(p_self, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(p_system_time, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(p_cap_interface, DOSE_DETECTION_ERROR_NULL_PTR);
   RETURN_ERR_IF_INTERFACE_NULL(p_tilt_interface, DOSE_DETECTION_ERROR_NULL_PTR);

   p_self->_is_initialized = false;
   // Assign interface
   p_self->interface.parent = p_self;
   p_self->interface.clear_dose_events = clear_dose_events;
   p_self->interface.get_current_state = get_current_state;
   p_self->interface.is_dose_event_available = is_dose_event_available;
   p_self->interface.try_get_dose_event = try_get_dose_event;
   p_self->interface.process = process;

   // Assign dependencies
   p_self->_system_time_interface = p_system_time;
   p_self->_cap_interface = p_cap_interface;
   p_self->_tilt_interface = p_tilt_interface;

   // Initialize private data
   p_self->_dose_start_time = 0u;
   p_self->_tilt_count = 0u;

   // Initialize state machine
   result_t result = dose_detection_fsm_init(&p_self->_fsm);

   // Initialize dose event queue
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&p_self->_dose_event_queue,
                                   p_self->_dose_event_queue_buffer,
                                   DOSE_EVENT_QUEUE_BUFFER_SIZE,
                                   sizeof(dose_detection_event_t)));

   if(IS_OK(result))
   {
      p_self->_is_initialized = true;
   }

   return result;
}