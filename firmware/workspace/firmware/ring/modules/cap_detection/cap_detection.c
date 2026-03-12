/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file cap_detection.c
 * @ingroup modules/cap_detection
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "cap_detection.h"

// Standard includes
#include <app_util.h> // For STATIC_ASSERT
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "tmd2635_driver.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_CAP_DETECTION_MODULE;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define PSAT_BIT            (5u)
#define PSAT_REFLECTIVE_BIT (1u)
#define PSAT_AMBIENT_BIT    (0u)

#define MAX_PROXIMITY_VALUE (16384u) /**< Maximum valid proximity value = 2^14 */

/***********************************************************************************************************************
 * Assertions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t
   get_cap_status(const cap_detection_interface_t *const interface, CAP_STATE *cap_state, uint16_t *prox_val);

static result_t set_config(const cap_detection_interface_t *const interface, uint16_t threshhold, uint16_t hysteresis);
static result_t
   get_config(const cap_detection_interface_t *const interface, uint16_t *threshhold, uint16_t *hysteresis);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t set_config(const cap_detection_interface_t *const interface, uint16_t threshhold, uint16_t hysteresis)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, CAP_MODULE_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_TRUE((hysteresis > threshhold), CAP_MODULE_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE((threshhold == 0), CAP_MODULE_ERROR_INVALID_PARAM);

   interface->parent->_threshhold = threshhold;
   interface->parent->_hysteresis = hysteresis;

   return RESULT_OK;
}

static result_t get_config(const cap_detection_interface_t *const interface, uint16_t *threshhold, uint16_t *hysteresis)
{
   // For future implementation if we want to support dynamic configuration of cap detection parameters.
   RETURN_ERR_IF_INTERFACE_NULL(interface, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, CAP_MODULE_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_NULL(threshhold, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(hysteresis, CAP_MODULE_ERROR_PTR_NULL);

   *threshhold = interface->parent->_threshhold;
   *hysteresis = interface->parent->_hysteresis;

   return RESULT_OK;
}

static result_t
   get_cap_status(const cap_detection_interface_t *const interface, CAP_STATE *cap_state, uint16_t *prox_val)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, CAP_MODULE_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_NULL(cap_state, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(prox_val, CAP_MODULE_ERROR_PTR_NULL);

   cap_detection_t *self = interface->parent;
   const ir_proximity_driver_interface_t *p_prox_ifc = self->_p_prox_ifc;

   uint8_t status = 0u;
   uint16_t pdata = 0u;

   // Read proximity
   result_t result = p_prox_ifc->get_proximity_data(p_prox_ifc, &pdata);
   UPDATE_ERR(result, CAP_MODULE_ERROR_GET_PROXIMITY_FAIL);

   if(IS_OK(result))
   {
      *prox_val = pdata;
   }

   // Get status register flags
   if(IS_OK(result))
   {
      result = p_prox_ifc->get_status_value(p_prox_ifc, &status);
      UPDATE_ERR(result, CAP_MODULE_ERROR_GET_STATUS_FAIL);
   }

   // Determine cap state and adjust sampling rate if needed
   // Note: Result of setting the sampling rate shall not affect the overall result of this function
   if(IS_OK(result))
   {
      result_t sr_result = RESULT_OK;

      if((0u == pdata) && (CAP_STATE_UNKNOWN == self->_cap_state))
      {
         // Sensor data might not be valid yet after wake up, so we keep UNKNOWN state.
         *cap_state = CAP_STATE_UNKNOWN;
      }
      else if((pdata >= (self->_threshhold + (self->_hysteresis / 2u))) && (CAP_STATE_CLOSED != self->_cap_state))
      {
         // Cap is detected as ON. Adjust sampling rate for CAP ON state.
         *cap_state = CAP_STATE_CLOSED;
         self->_cap_state = *cap_state;
         sr_result = p_prox_ifc->set_sampling_rate(p_prox_ifc, CAP_ON_SAMPLING_PERIOD_MS);
         if(IS_ERR(sr_result))
         {
            DEBUG_WARNING("Failed to set proximity sampling rate for CAP ON state.");
         }
      }
      else if((pdata < (self->_threshhold - (self->_hysteresis / 2u))) && (CAP_STATE_OPEN != self->_cap_state))
      {
         // Cap is detected as OFF. Adjust sampling rate for CAP OFF state.
         // Note: zero proximity value treated as OPEN state as well after initialization.
         *cap_state = CAP_STATE_OPEN;
         self->_cap_state = *cap_state;
         result_t sr_result = p_prox_ifc->set_sampling_rate(p_prox_ifc, CAP_OFF_SAMPLING_PERIOD_MS);
         if(IS_ERR(sr_result))
         {
            DEBUG_WARNING("Failed to set proximity sampling rate for CAP OFF state.");
         }
      }
      else
      {
         // No cap state change or error occurred
         *cap_state = self->_cap_state;
      }
   }

   // Check for saturation
   if(IS_OK(result) && IS_SET(status, PSAT_BIT))
   {
      bool reflective_saturation = IS_SET(status, PSAT_REFLECTIVE_BIT);
      bool ambient_saturation = IS_SET(status, PSAT_AMBIENT_BIT);

      if(ambient_saturation && reflective_saturation)
      {
         SET_ERR(result, PROX_DRV_ERROR_AMBIENT_REFLECTIVE_SATURATION);
         DEBUG_INFO("Ambient and reflective saturation occurred.");
      }
      else if(reflective_saturation)
      {
         SET_ERR(result, PROX_DRV_ERROR_REFLECTIVE_SATURATION);
         DEBUG_INFO("Reflective saturation occurred.");
      }
      else if(ambient_saturation)
      {
         SET_ERR(result, PROX_DRV_ERROR_AMBIENT_SATURATION);
         DEBUG_INFO("Ambient saturation occurred.");
      }
      else
      {
         SET_ERR(result, PROX_DRV_ERROR_UNKNOWN_PROX_SATURATION);
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t cap_detection_init(cap_detection_t *const self,
                            const ir_proximity_driver_interface_t *const p_prox_ifc,
                            uint16_t init_threshhold,
                            uint16_t init_hysteresis)
{
   RETURN_ERR_IF_NULL(self, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(p_prox_ifc, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE((init_hysteresis > init_threshhold), CAP_MODULE_ERROR_INVALID_PARAM);
   RETURN_ERR_IF_TRUE((init_threshhold == 0), CAP_MODULE_ERROR_INVALID_PARAM);

   self->_is_initialized = false;

   // Assign interface
   self->interface.parent = self;
   self->interface.get_cap_status = get_cap_status;
   self->interface.set_config = set_config;
   self->interface.get_config = get_config;

   // Assign dependencies
   self->_p_prox_ifc = p_prox_ifc;

   // Initialize private data
   self->_cap_state = CAP_STATE_UNKNOWN;
   self->_threshhold = init_threshhold;
   self->_hysteresis = init_hysteresis;

   // Activate the proximity sensor with initial sampling rate
   result_t result = self->_p_prox_ifc->set_sampling_rate(self->_p_prox_ifc, CAP_ON_SAMPLING_PERIOD_MS);

   IF_OK_RUN_AND_UPDATE(result, self->_p_prox_ifc->set_wake(self->_p_prox_ifc));

   if(IS_OK(result))
   {
      self->_is_initialized = true;
   }

   UPDATE_ERR(result, CAP_MODULE_ERROR_INIT_FAILURE);

   return result;
}