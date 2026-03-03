/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tmd2635_driver.c
 * @ingroup ring/mock/ir_proximity_sensor_driver
 * @brief Source file for the TMD2635 IR proximity sensor mock driver.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "tmd2635_driver.h"

#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"

#include <stdint.h>

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_PROX_SENSOR_TMD2635_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define I2C_TIMEOUT_US (100u) // I2C operation timeout in microseconds

/*
 * `T_sample = (PWTIME + 1) * 2.78ms + (PRATE + 1) * 0.088 ms`
 * - PRATE is set to its reset value
 * - PWTIME maximum value is its mask value
 *
 * `T_sample_max = 714 ms`
 */
#define TMD2635_SAMPLING_PERIOD_MS_MAX                                                                                 \
   ((((TMD2635_PWTIME_MASK + 1) * TMD2635_PWTIME_TICK_US) + ((TMD2635_PRATE_RESET + 1) * TMD2635_PRATE_TICK_US))       \
    / COMMON_1K_FACTOR)

#if defined(PROX_SAMPLING_PERIOD_MS_MAX) && (PROX_SAMPLING_PERIOD_MS_MAX > 0u)
#   if (PROX_SAMPLING_PERIOD_MS_MAX > TMD2635_SAMPLING_PERIOD_MS_MAX)
#      error "PROX_SAMPLING_PERIOD_MS_MAX exceeds supported maximum sampling period for TMD2635 driver."
#   endif
#else
#   define PROX_SAMPLING_PERIOD_MS_MAX TMD2635_SAMPLING_PERIOD_MS_MAX
#endif

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

static result_t mock_set_sleep(const ir_proximity_driver_interface_t *const interface);
static result_t mock_set_wake(const ir_proximity_driver_interface_t *const interface);
static result_t mock_get_proximity_data(const ir_proximity_driver_interface_t *const interface, uint16_t *data_out);
static result_t mock_get_status_value(const ir_proximity_driver_interface_t *const interface, uint8_t *status_val);
static result_t mock_set_sampling_rate(const ir_proximity_driver_interface_t *const interface,
                                       uint16_t sampling_period_ms);

// Internal (non-interface) functions

result_t mock_update_register(const tmd2635_driver_t *p_self);
static result_t mock_write_register(const tmd2635_driver_t *self);
static result_t mock_read_register(const tmd2635_driver_t *self);

static result_t mock_set_mode(const ir_proximity_driver_interface_t *const interface, TMD2635_POWER_MODE mode);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t mock_set_sleep(const ir_proximity_driver_interface_t *const interface)
{
   return mock_set_mode(interface, TMD2635_POWER_MODE_SLEEP);
}

static result_t mock_set_wake(const ir_proximity_driver_interface_t *const interface)
{
   return mock_set_mode(interface, TMD2635_POWER_MODE_ACTIVE);
}

static result_t mock_get_proximity_data(const ir_proximity_driver_interface_t *const interface, uint16_t *data_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data_out, PROX_DRV_ERROR_PTR_NULL);

   tmd2635_driver_t *p_self = interface->parent;

   result_t result = mock_read_register(p_self);

   if(IS_OK(result) && (p_self->_mock_proximity_data > TMD2635_PDATA14_MASK))
   {
      SET_ERR(result, PROX_DRV_ERROR_INVALID_DATA);
   }

   if(IS_OK(result))
   {
      *data_out = p_self->_mock_proximity_data;
   }

   return result;
}

static result_t mock_get_status_value(const ir_proximity_driver_interface_t *const interface, uint8_t *status_val)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(status_val, PROX_DRV_ERROR_PTR_NULL);

   tmd2635_driver_t *self = interface->parent;

   result_t result = mock_read_register(self);

   if(IS_OK(result))
   {
      *status_val = self->_mock_status_value;
   }

   return result;
}

static result_t mock_set_sampling_rate(const ir_proximity_driver_interface_t *const interface,
                                       uint16_t sampling_period_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(((0u == sampling_period_ms) || (PROX_SAMPLING_PERIOD_MS_MAX < sampling_period_ms)),
                      PROX_DRV_ERROR_INVALID_PARAM);

   tmd2635_driver_t *p_self = interface->parent;

   result_t result = mock_update_register(p_self);

   if(IS_OK(result))
   {
      p_self->_mock_sampling_rate = sampling_period_ms;
   }

   return result;
}

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Mock a register update operation
 *
 * @param[in] p_self Pointer to the driver instance
 *
 * @return Status code indicating the result of the operation
 */
result_t mock_update_register(const tmd2635_driver_t *p_self)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   result_t result = mock_read_register(p_self);
   IF_OK_RUN_AND_UPDATE(result, mock_write_register(p_self));
   // Second read (for verification doesn't need to be mocked)

   // Simulate register update verification failure if configured to do so
   if(IS_OK(result) && p_self->_fail_reg_update)
   {
      SET_ERR(result, PROX_DRV_ERROR_REGISTER_UPDATE_FAIL);
   }

   return result;
}

/**
 * @brief Mock a register write operation
 *
 * @param[in] self Pointer to the driver instance
 *
 * @return Status code indicating the result of the operation.
 */
static result_t mock_write_register(const tmd2635_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, PROX_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   // Simulate I2C write failure if configured to do so
   if(self->_fail_i2c_write)
   {
      SET_ERR(result, PROX_DRV_ERROR_I2C_WRITE_FAIL);
   }

   return result;
}

/**
 * @brief Mock a register read operation
 *
 * @param interface Pointer to the tmd2635 driver instance
 *
 * @return A result indicating the success or failure of the operation.
 */
static result_t mock_read_register(const tmd2635_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, PROX_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   // Simulate I2C read failure if configured to do so
   if(self->_fail_i2c_read)
   {
      SET_ERR(result, PROX_DRV_ERROR_I2C_READ_FAIL);
   }

   return result;
}

/**
 * @brief Sets the power mode of the TMD2635 sensor
 *
 * @param[in] interface Pointer to the interface instance
 * @param[in] mode The power mode to set
 *
 * @return Status code indicating the result of the operation
 */
static result_t mock_set_mode(const ir_proximity_driver_interface_t *const interface, TMD2635_POWER_MODE mode)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE((TMD2635_POWER_MODE_MAX <= mode), PROX_DRV_ERROR_INVALID_PARAM);

   tmd2635_driver_t *p_self = interface->parent;

   result_t result = mock_update_register(p_self);
   if(IS_OK(result))
   {
      p_self->_power_mode = mode;
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_tmd2635_driver_init(tmd2635_driver_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   // Assign interface
   p_self->interface.parent = p_self;
   p_self->interface.set_sleep = mock_set_sleep;
   p_self->interface.set_wake = mock_set_wake;
   p_self->interface.get_proximity_data = mock_get_proximity_data;
   p_self->interface.get_status_value = mock_get_status_value;
   p_self->interface.set_sampling_rate = mock_set_sampling_rate;

   // Initialize private variables
   p_self->_power_mode = TMD2635_POWER_MODE_IDLE;
   p_self->_mock_proximity_data = 0u;
   p_self->_mock_status_value = 0u;
   p_self->_mock_sampling_rate = PROX_SAMPLING_PERIOD_MS_MAX;

   p_self->_fail_i2c_read = false;
   p_self->_fail_i2c_write = false;
   p_self->_fail_reg_update = false;

   p_self->_is_initialized = true;

   return RESULT_OK;
}

result_t mock_tmd2635_set_proximity_data(tmd2635_driver_t *const p_self, uint16_t data_in)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   p_self->_mock_proximity_data = data_in;

   return RESULT_OK;
}

result_t mock_tmd2635_set_status_value(tmd2635_driver_t *const p_self, uint8_t status_in)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   p_self->_mock_status_value = status_in;

   return RESULT_OK;
}

result_t
   mock_tmd2635_set_behavior_option(tmd2635_driver_t *const p_self, MOCK_TMD2635_BEHAVIOR_OPTIONS option, bool enable)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   switch(option)
   {
      case MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_READ:
         p_self->_fail_i2c_read = enable;
         break;
      case MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_I2C_WRITE:
         p_self->_fail_i2c_write = enable;
         break;
      case MOCK_TMD2635_BEHAVIOR_OPTION_FAIL_REG_UPDATE:
         p_self->_fail_reg_update = enable;
         break;
      case MOCK_TMD2635_BEHAVIOR_OPTION_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_PARAM);
   }

   return result;
}