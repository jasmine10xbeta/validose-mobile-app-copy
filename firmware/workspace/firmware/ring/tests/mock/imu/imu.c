/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file imu.c
 * @ingroup mock/drivers/imu
 * @brief Source file for the mock IMU driver used for unit testing
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "imu.h"

#include "common.h"

static uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_IMU_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define G_RANGE               ((int16_t)(2u))                         // +/- 2g range
#define LSB_PER_G             ((int16_t)((INT16_MAX + 1u) / G_RANGE)) // LSBs per g
#define GRAV_ACC_MM_PER_S_SQR (int16_t)(9807)                         // g = 9807 mm/s^2

#define SCALE_FACTOR_LSB_PER_G  (int16_t)(16384)
#define GRAVITY_IN_MM_PER_S_SQR (int16_t)(9810)

#define TO_DECI_DEGREES_PER_S (int16_t)(10)
#define LSB_DECI_DEGREE_PER_S (int16_t)(2644)

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

static result_t mock_get_temperature_celsius(const imu_interface_t *const interface, int16_t *temperature_celsius);
static result_t mock_set_state(const imu_interface_t *const interface, IMU_STATE state);
static result_t mock_get_state(const imu_interface_t *const interface, IMU_STATE *state);
static result_t mock_get_interrupts(const imu_interface_t *const interface, imu_interrupt_map_t *map);
static result_t mock_get_imu_data_entry_count(const imu_interface_t *const interface, uint16_t *count);
static result_t mock_clear_imu_data(const imu_interface_t *const interface);
static result_t
   mock_get_imu_data_entries(const imu_interface_t *const interface, uint16_t count, imu_data_entry_t *entries);
static result_t mock_get_imu_data_entry(const imu_interface_t *const interface, imu_data_entry_t *entry);

// Non-interface functions

static result_t mock_imu_reg_read(imu_t *const p_self);
static result_t mock_imu_reg_write(imu_t *const p_self);
static result_t mock_imu_entry_read(imu_t *const p_self, imu_data_entry_t *entry);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t mock_get_imu_data_entry_count(const imu_interface_t *const interface, uint16_t *count)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(count, IMU_ERROR_PTR_NULL);

   imu_t *p_self = interface->parent;

   result_t result = mock_imu_reg_read(p_self);

   if(IS_OK(result))
   {
      *count = p_self->_mock_imu_data_entry_count;
   }

   return result;
}

static result_t mock_clear_imu_data(const imu_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);

   imu_t *p_self = interface->parent;

   result_t result = mock_imu_reg_write(p_self);

   // Set the mock imu data entry count to zero to simulate clearing the fifo
   if(IS_OK(result))
   {
      p_self->_mock_imu_data_entry_count = 0u;
      p_self->_mock_imu_data_entry_read_index = 0u;
   }

   return result;
}

static result_t
   mock_get_imu_data_entries(const imu_interface_t *const interface, uint16_t count, imu_data_entry_t *entries)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(entries, IMU_ERROR_PTR_NULL);

   imu_t *p_self = interface->parent;

   uint16_t available_entries = 0u;

   // Check that we aren't reading more data entries than are available
   result_t result = mock_get_imu_data_entry_count(interface, &available_entries);

   if((count > available_entries) && IS_OK(result))
   {
      SET_ERR(result, IMU_ERROR_INVALID_ARG);
   }

   imu_data_entry_t rx_entry = {0};

   // Read count number of data entries from the fifo data register
   for(int index = 0; index < count; index++)
   {
      IF_OK_RUN_AND_UPDATE(result, mock_imu_entry_read(p_self, &rx_entry));
      BREAK_ON_ERR(result);

      rx_entry.acc_x_axis = (int16_t)(rx_entry.acc_x_axis * GRAVITY_IN_MM_PER_S_SQR / SCALE_FACTOR_LSB_PER_G);
      rx_entry.acc_y_axis = (int16_t)(rx_entry.acc_y_axis * GRAVITY_IN_MM_PER_S_SQR / SCALE_FACTOR_LSB_PER_G);
      rx_entry.acc_z_axis = (int16_t)(rx_entry.acc_z_axis * GRAVITY_IN_MM_PER_S_SQR / SCALE_FACTOR_LSB_PER_G);

      rx_entry.angular_rate_x_axis
         = (int16_t)(rx_entry.angular_rate_x_axis * TO_DECI_DEGREES_PER_S / LSB_DECI_DEGREE_PER_S);
      rx_entry.angular_rate_y_axis
         = (int16_t)(rx_entry.angular_rate_y_axis * TO_DECI_DEGREES_PER_S / LSB_DECI_DEGREE_PER_S);
      rx_entry.angular_rate_z_axis
         = (int16_t)(rx_entry.angular_rate_z_axis * TO_DECI_DEGREES_PER_S / LSB_DECI_DEGREE_PER_S);

      entries[index] = rx_entry;
   }

   IF_OK_RUN_AND_UPDATE(result, mock_imu_reg_read(p_self));

   return result;
}

static result_t mock_get_imu_data_entry(const imu_interface_t *const interface, imu_data_entry_t *entry)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(entry, IMU_ERROR_PTR_NULL);

   imu_t *p_self = interface->parent;

   uint16_t available_entries = 0u;

   result_t result = mock_get_imu_data_entry_count(interface, &available_entries);

   if((available_entries == 0u) && IS_OK(result))
   {
      SET_ERR(result, IMU_ERROR_NO_ENTRIES);
   }

   if(IS_OK(result))
   {
      memset(entry, 0u, sizeof(imu_data_entry_t));
   }

   // Read the latest data entry and discard the rest
   for(uint16_t index = 0; index < available_entries; index++)
   {
      IF_OK_RUN_AND_UPDATE(result, mock_imu_entry_read(p_self, entry));
      BREAK_ON_ERR(result);

      // Ignore values that we don't care to return
      if(index != available_entries - 1u)
      {
         continue;
      }

      // Convert to mm/s^2
      entry->acc_x_axis = (int16_t)(entry->acc_x_axis * GRAVITY_IN_MM_PER_S_SQR / SCALE_FACTOR_LSB_PER_G);
      entry->acc_y_axis = (int16_t)(entry->acc_y_axis * GRAVITY_IN_MM_PER_S_SQR / SCALE_FACTOR_LSB_PER_G);
      entry->acc_z_axis = (int16_t)(entry->acc_z_axis * GRAVITY_IN_MM_PER_S_SQR / SCALE_FACTOR_LSB_PER_G);

      entry->angular_rate_x_axis
         = (int16_t)(entry->angular_rate_x_axis * TO_DECI_DEGREES_PER_S / LSB_DECI_DEGREE_PER_S);
      entry->angular_rate_y_axis
         = (int16_t)(entry->angular_rate_y_axis * TO_DECI_DEGREES_PER_S / LSB_DECI_DEGREE_PER_S);
      entry->angular_rate_z_axis
         = (int16_t)(entry->angular_rate_z_axis * TO_DECI_DEGREES_PER_S / LSB_DECI_DEGREE_PER_S);
   }

   IF_OK_RUN_AND_UPDATE(result, mock_imu_reg_read(p_self));

   return result;
}

static result_t mock_get_state(const imu_interface_t *const interface, IMU_STATE *state)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(state, IMU_ERROR_PTR_NULL);

   *state = interface->parent->_state;

   return RESULT_OK;
}

static result_t mock_set_state(const imu_interface_t *const interface, IMU_STATE state)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE((IMU_STATE_MAX <= state), IMU_ERROR_INVALID_ARG);

   imu_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   if(p_self->_state != state)
   {
      result = mock_imu_reg_write(p_self); // Mock all state changes as register writes

      if(IS_OK(result))
      {
         p_self->_state = state;
      }
   }

   return result;
}

static result_t mock_get_temperature_celsius(const imu_interface_t *const interface, int16_t *temperature_celsius)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(temperature_celsius, IMU_ERROR_PTR_NULL);

   imu_t *p_self = interface->parent;

   RETURN_ERR_IF_TRUE(IMU_STATE_ACTIVE != p_self->_state, IMU_ERROR_INVALID_STATE);

   result_t result = mock_imu_reg_read(p_self);

   if(IS_OK(result))
   {
      *temperature_celsius = p_self->_mock_temperature_degc;
   }

   return result;
}

static result_t mock_get_interrupts(const imu_interface_t *const interface, imu_interrupt_map_t *map)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, IMU_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(map, IMU_ERROR_PTR_NULL);

   imu_t *p_self = interface->parent;

   RETURN_ERR_IF_TRUE(IMU_STATE_ACTIVE != p_self->_state, IMU_ERROR_INVALID_STATE);

   result_t result = mock_imu_reg_read(p_self);

   if(IS_OK(result))
   {
      *map = p_self->_mock_interrupt_map;
   }

   return result;
}

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Mock IMU register read function
 *
 * @param[in] p_self Pointer to the mock IMU instance
 *
 * @return Status code indicating result of the operation
 */
static result_t mock_imu_reg_read(imu_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);

   result_t result = RESULT_OK;

   // Simulate a failure if the flag is set
   if(p_self->_fail_imu_reg_read)
   {
      SET_ERR(result, IMU_ERROR_I2C_ERROR);
   }

   return result;
}

/**
 * @brief Mock IMU register write function
 *
 * @param[in] p_self Pointer to the mock IMU instance
 *
 * @return Status code indicating result of the operation
 */
static result_t mock_imu_reg_write(imu_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);

   result_t result = RESULT_OK;

   if(p_self->_fail_imu_reg_write)
   {
      SET_ERR(result, IMU_ERROR_I2C_ERROR);
   }

   return result;
}

/**
 * @brief Mock IMU entry read function
 *
 * @param[in] p_self Pointer to the mock IMU instance
 * @param[out] entry Pointer to the imu data entry to populate
 *
 * @return Status code indicating result of the operation
 */
static result_t mock_imu_entry_read(imu_t *const p_self, imu_data_entry_t *entry)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);
   RETURN_ERR_IF_NULL(entry, MOCK_IMU_ERROR_NULL);

   result_t result = RESULT_OK;

   // Simulate a failure if the flag is set
   if(p_self->_fail_imu_reg_read)
   {
      SET_ERR(result, IMU_ERROR_I2C_ERROR);
   }

   if(IS_OK(result) && (p_self->_mock_imu_data_entry_read_index >= p_self->_mock_imu_data_entry_count))
   {
      SET_ERR(result, IMU_ERROR_I2C_ERROR);
   }

   if(IS_OK(result))
   {
      *entry = p_self->_mock_raw_imu_data_entries[p_self->_mock_imu_data_entry_read_index];
      p_self->_mock_imu_data_entry_read_index++;
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t mock_imu_init(imu_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);

   // Assign interface
   p_self->interface.parent = p_self;
   p_self->interface.get_temperature_celsius = mock_get_temperature_celsius;
   p_self->interface.get_state = mock_get_state;
   p_self->interface.set_state = mock_set_state;
   p_self->interface.get_interrupts = mock_get_interrupts;
   p_self->interface.get_imu_data_entry_count = mock_get_imu_data_entry_count;
   p_self->interface.clear_imu_data = mock_clear_imu_data;
   p_self->interface.get_imu_data_entries = mock_get_imu_data_entries;
   p_self->interface.get_imu_data_entry = mock_get_imu_data_entry;

   // Initialize internal state
   p_self->_state = IMU_STATE_DORMANT;

   p_self->_mock_imu_data_entry_count = 0u;
   memset(&p_self->_mock_raw_imu_data_entries, 0, sizeof(p_self->_mock_raw_imu_data_entries));

   p_self->_fail_imu_reg_read = false;
   p_self->_fail_imu_reg_write = false;

   p_self->_is_initialized = true;

   return RESULT_OK;
}

result_t
   mock_imu_set_data_entries(imu_t *const p_self, const imu_data_entry_t *const p_entries, const uint16_t entry_count)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);
   RETURN_ERR_IF_NULL(p_entries, MOCK_IMU_ERROR_NULL);
   RETURN_ERR_IF_TRUE(entry_count > IMU_MAX_DATA_ENTRIES, MOCK_IMU_ERROR_INVALID_ARG);

   memcpy(p_self->_mock_raw_imu_data_entries, p_entries, entry_count * sizeof(imu_data_entry_t));
   p_self->_mock_imu_data_entry_count = entry_count;
   p_self->_mock_imu_data_entry_read_index = 0u;

   return RESULT_OK;
}

result_t mock_imu_set_temperature(imu_t *const p_self, const int16_t temperature_degc)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);

   p_self->_mock_temperature_degc = temperature_degc;
   return RESULT_OK;
}

result_t mock_imu_set_interrupt_map(imu_t *const p_self, const imu_interrupt_map_t *const p_map)
{
   RETURN_ERR_IF_NULL(p_self, MOCK_IMU_ERROR_NULL);
   RETURN_ERR_IF_NULL(p_map, MOCK_IMU_ERROR_NULL);

   p_self->_mock_interrupt_map = *p_map;

   return RESULT_OK;
}
