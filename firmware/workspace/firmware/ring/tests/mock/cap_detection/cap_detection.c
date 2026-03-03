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

// Internal (non-interface) functions

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t
   get_cap_status(const cap_detection_interface_t *const interface, CAP_STATE *cap_state, uint16_t *prox_val)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, CAP_MODULE_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_NULL(cap_state, CAP_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(prox_val, CAP_MODULE_ERROR_PTR_NULL);

   cap_detection_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   if(p_self->_fail_imu_read_proximity)
   {
      SET_ERR(result, CAP_MODULE_ERROR_GET_PROXIMITY_FAIL);
   }

   if(IS_OK(result))
   {
      *prox_val = p_self->_mock_proximity_data;
   }

   if(IS_OK(result) && p_self->_fail_imu_get_status)
   {
      SET_ERR(result, CAP_MODULE_ERROR_GET_STATUS_FAIL);
   }

   if(IS_OK(result))
   {
      *cap_state = p_self->_mock_cap_state;
   }

   if(IS_OK(result) && p_self->_mock_status_error != PROX_DRV_ERROR_NONE)
   {
      SET_ERR(result, p_self->_mock_status_error);
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_cap_detection_init(cap_detection_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, CAP_MODULE_ERROR_PTR_NULL);

   // Assign interface
   p_self->interface.parent = p_self;
   p_self->interface.get_cap_status = get_cap_status;

   // Initialize private data
   p_self->_mock_cap_state = CAP_STATE_UNKNOWN;
   p_self->_mock_proximity_data = 0u;

   p_self->_fail_imu_read_proximity = false;
   p_self->_fail_imu_get_status = false;
   p_self->_mock_status_error = PROX_DRV_ERROR_NONE;

   p_self->_is_initialized = true;

   return RESULT_OK;
}