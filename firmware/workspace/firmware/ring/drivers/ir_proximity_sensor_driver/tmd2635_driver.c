/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tmd2635_driver.c
 * @ingroup ring/drivers/ir_proximity_sensor_driver
 * @brief Source file for the TMD2635 IR proximity sensor driver.
 *
 * Datasheet: https://download.mikroe.com/documents/datasheets/TMD2635_Datasheet.pdf
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "tmd2635_driver.h"

#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"

#include <nrf_delay.h>
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
#   if TMD2635_SAMPLING_PERIOD_MS_MAX < PROX_SAMPLING_PERIOD_MS_MAX
#      error "PROX_SAMPLING_PERIOD_MS_MAX exceeds supported maximum sampling period for TMD2635 driver."
#   endif
#else
#   define PROX_SAMPLING_PERIOD_MS_MAX TMD2635_SAMPLING_PERIOD_MS_MAX
#endif

#define TMD2635_SAMPLING_PERIOD_MS_DEFAULT (100u) /**< Default sampling period in milliseconds */

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

static result_t set_sleep(const ir_proximity_driver_interface_t *const interface);
static result_t set_wake(const ir_proximity_driver_interface_t *const interface);
static result_t get_proximity_data(const ir_proximity_driver_interface_t *const interface, uint16_t *data_out);
static result_t get_status_value(const ir_proximity_driver_interface_t *const interface, uint8_t *status_val);
static result_t set_sampling_rate(const ir_proximity_driver_interface_t *const interface, uint16_t sampling_period_ms);

// Internal (non-interface) functions

result_t
   update_register(const tmd2635_driver_t *p_self, uint8_t register_address, uint8_t clear_mask, uint8_t set_mask);
static result_t write_register(const tmd2635_driver_t *p_self, uint8_t register_address, uint8_t data_in);
static result_t read_register(const tmd2635_driver_t *p_self, uint8_t register_address, uint8_t *p_data_out);

static result_t validate_config(const tmd2635_config_t *p_cfg);
static result_t set_config(tmd2635_driver_t *p_self, const tmd2635_config_t *p_cfg);

static result_t set_mode(const ir_proximity_driver_interface_t *const interface, TMD2635_POWER_MODE mode);

static result_t ensure_present_and_responding(const tmd2635_driver_t *p_self);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t set_sleep(const ir_proximity_driver_interface_t *const interface)
{
   return set_mode(interface, TMD2635_POWER_MODE_SLEEP);
}

static result_t set_wake(const ir_proximity_driver_interface_t *const interface)
{
   return set_mode(interface, TMD2635_POWER_MODE_ACTIVE);
}

static result_t get_proximity_data(const ir_proximity_driver_interface_t *const interface, uint16_t *data_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, PROX_DRV_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_NULL(data_out, PROX_DRV_ERROR_PTR_NULL);

   tmd2635_driver_t *p_self = interface->parent;

   uint8_t pdatal = 0u;
   uint8_t pdatah = 0u;

   result_t result = read_register(p_self, TMD2635_REG_PDATAL, &pdatal);
   IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PDATAH, &pdatah));

   if(IS_OK(result))
   {
      uint16_t pdata = TMD2635_PDATA14_MAKE(pdatah, pdatal);

      if(pdata <= TMD2635_PDATA14_MASK)
      {
         *data_out = pdata;
      }
      else
      {
         SET_ERR(result, PROX_DRV_ERROR_INVALID_DATA);
      }
   }

   return result;
}

static result_t get_status_value(const ir_proximity_driver_interface_t *const interface, uint8_t *status_val)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, PROX_DRV_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_NULL(status_val, PROX_DRV_ERROR_PTR_NULL);

   tmd2635_driver_t *p_self = interface->parent;

   result_t result = read_register(p_self, TMD2635_REG_STATUS, status_val);
   UPDATE_ERR(result, PROX_DRV_ERROR_I2C_READ_FAIL);

   return result;
}

static result_t set_sampling_rate(const ir_proximity_driver_interface_t *const interface, uint16_t sampling_period_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, PROX_DRV_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(((0u == sampling_period_ms) || (PROX_SAMPLING_PERIOD_MS_MAX < sampling_period_ms)),
                      PROX_DRV_ERROR_INVALID_PARAM);

   tmd2635_driver_t *p_self = interface->parent;
   result_t result = RESULT_OK;

   // Only update if the sampling period is changing
   if(sampling_period_ms != p_self->_sampling_period_ms)
   {
      // Calculate number of ticks for the desired sampling period
      uint32_t sampling_period_us = (uint32_t)sampling_period_ms * COMMON_1K_FACTOR;
      uint32_t prate_contribution_us = (uint32_t)(TMD2635_PRATE_RESET + 1u) * TMD2635_PRATE_TICK_US;
      uint32_t remaining_us = (sampling_period_us > prate_contribution_us) ?
                                 (sampling_period_us - prate_contribution_us) :
                                 TMD2635_PWTIME_TICK_US; // Subtract PRATE contribution while avoiding overflow
      uint32_t pwtime_ticks
         = (remaining_us + (TMD2635_PWTIME_TICK_US - 1u)) / TMD2635_PWTIME_TICK_US; // Calculate ticks while rounding up
      uint8_t pwtime_val = (uint8_t)(pwtime_ticks - 1u);                            // Convert to register value

      uint32_t actual_sampling_period_us = (uint32_t)(pwtime_val + 1u) * TMD2635_PWTIME_TICK_US + prate_contribution_us;
      DEBUG_DEBUG("Set sampling period: %u us (PWTIME=0x%02X)", actual_sampling_period_us, pwtime_val);

      // Set proximity wait time register
      IF_OK_RUN_AND_UPDATE(result,
                           update_register(p_self, TMD2635_REG_PWTIME, TMD2635_PWTIME_MASK, (uint8_t)pwtime_val));

      if(IS_OK(result))
      {
         p_self->_sampling_period_ms = sampling_period_ms;
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Updates specific bits in a register of the TMD2635
 *
 * This function updates a register through a read-modify-write operation. It verifies the update by reading back the
 * register value.
 *
 * @param[in] p_self Pointer to the driver instance
 * @param[in] register_address The address of the register to update
 * @param[in] clear_mask A mask indicating which bits to clear
 * @param[in] set_mask A mask indicating which bits to set
 *
 * @return Status code indicating the result of the operation
 *
 */
result_t update_register(const tmd2635_driver_t *p_self, uint8_t register_address, uint8_t clear_mask, uint8_t set_mask)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   // Read
   uint8_t read_value = 0u;
   result_t result = read_register(p_self, register_address, &read_value);

   // Modify
   uint8_t new_value = (read_value & (uint8_t)~clear_mask) | set_mask;

   // Only write and verify if the value changes
   if(IS_OK(result) && (new_value != read_value))
   {
      // Write
      IF_OK_RUN_AND_UPDATE(result, write_register(p_self, register_address, new_value));

      // Verify
      uint8_t verify_value = 0u;
      IF_OK_RUN_AND_UPDATE(result, read_register(p_self, register_address, &verify_value));

      if(IS_OK(result) && (verify_value != new_value))
      {
         SET_ERR(result, PROX_DRV_ERROR_REGISTER_UPDATE_FAIL);
      }
   }

   return result;
}

/**
 * @brief Writes data to a register of the TMD2635
 *
 * @param[in] p_self Pointer to the driver instance
 * @param[in] register_address The address of the register to write to.
 * @param[in] data_in The data byte to write to the register.
 *
 * @return Status code indicating the result of the operation.
 */
static result_t write_register(const tmd2635_driver_t *p_self, uint8_t register_address, uint8_t data_in)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   uint8_t tx_buffer[] = {register_address, data_in};

   result_t result = p_self->_p_i2c_ifc->transmit(
      p_self->_p_i2c_ifc, p_self->_cfg.i2c_addr, tx_buffer, sizeof(tx_buffer), I2C_TIMEOUT_US);
   UPDATE_ERR(result, PROX_DRV_ERROR_I2C_WRITE_FAIL);

   return result;
}

/**
 * @brief Reads data from a specified register in the TMD2635 driver.
 *
 * @param interface Pointer to the tmd2635 driver instance
 * @param register_address The address of the register to read from.
 * @param rx_data Pointer to the buffer where the read data will be stored.
 * @param data_length The number of bytes to read.
 *
 * @note This function assumes register addresses are 8-bit.
 *
 * @return A result indicating the success or failure of the operation.
 */
static result_t read_register(const tmd2635_driver_t *p_self, uint8_t register_address, uint8_t *p_data_out)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_data_out, PROX_DRV_ERROR_PTR_NULL);

   result_t result = p_self->_p_i2c_ifc->transmit(
      p_self->_p_i2c_ifc, p_self->_cfg.i2c_addr, &register_address, sizeof(uint8_t), I2C_TIMEOUT_US);
   UPDATE_ERR(result, PROX_DRV_ERROR_I2C_READ_FAIL);

   if(IS_OK(result))
   {
      result = p_self->_p_i2c_ifc->receive(
         p_self->_p_i2c_ifc, p_self->_cfg.i2c_addr, p_data_out, sizeof(uint8_t), I2C_TIMEOUT_US);
      UPDATE_ERR(result, PROX_DRV_ERROR_I2C_READ_FAIL);
   }

   return result;
}

/**
 * @brief Validate a driver configuration struct
 *
 * @param[in] p_cfg Pointer to the configuration struct to validate
 *
 * @return Status code indicating the result of the validation
 */
static result_t validate_config(const tmd2635_config_t *p_cfg)
{
   RETURN_ERR_IF_NULL(p_cfg, PROX_DRV_ERROR_PTR_NULL);

   RETURN_ERR_IF_TRUE(((p_cfg->i2c_addr != TMD2635_I2C_ADDR_SCL_SDA) && (p_cfg->i2c_addr != TMD2635_I2C_ADDR_SDA_SCL)),
                      PROX_DRV_ERROR_INVALID_CONFIG);

   RETURN_ERR_IF_TRUE((p_cfg->photodiode >= TMD2635_PD_MAX), PROX_DRV_ERROR_INVALID_CONFIG);
   RETURN_ERR_IF_TRUE((p_cfg->gain >= TMD2635_GAIN_MAX), PROX_DRV_ERROR_INVALID_CONFIG);
   RETURN_ERR_IF_TRUE(((p_cfg->max_pulses < TMD2635_MIN_PULSES) || (p_cfg->max_pulses > TMD2635_MAX_PULSES)),
                      PROX_DRV_ERROR_INVALID_CONFIG);
   RETURN_ERR_IF_TRUE((p_cfg->pulse_len >= TMD2635_PULSE_LEN_MAX), PROX_DRV_ERROR_INVALID_CONFIG);
   RETURN_ERR_IF_TRUE((p_cfg->drive_current >= TMD2635_DRIVE_MAX), PROX_DRV_ERROR_INVALID_CONFIG);
   RETURN_ERR_IF_TRUE((p_cfg->hw_avg >= TMD2635_HWAVG_MAX), PROX_DRV_ERROR_INVALID_CONFIG);
   RETURN_ERR_IF_TRUE((p_cfg->moving_avg >= TMD2635_MAVG_MAX), PROX_DRV_ERROR_INVALID_CONFIG);

   return RESULT_OK;
}

/**
 * @brief Configure the TMD2635 proximity sensor with specified settings.
 *
 * @param p_self Pointer to the driver instance
 * @param proximity_sensor_config Struct containing all of the configuration values.
 *
 * @return A result indicating the success or failure of the operation.
 *
 * @note This function also sets the device to idle mode.
 */
static result_t set_config(tmd2635_driver_t *p_self, const tmd2635_config_t *p_cfg)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_cfg, PROX_DRV_ERROR_PTR_NULL);

   // Set device to idle mode before configuring
   result_t result
      = update_register(p_self, TMD2635_REG_ENABLE, TMD2635_ENABLE_PEN, TMD2635_ENABLE_PON | TMD2635_ENABLE_PWEN);

   if(IS_OK(result))
   {
      p_self->_power_mode = TMD2635_POWER_MODE_IDLE;
   }

   // Set interrupt threshold registers
   // Note: Unused in current implementation but set to default reset values
   // Note: Threshold low and high bytes need to be written immediately after each other to ensure proper latching.
   if(IS_OK(result))
   {
      uint8_t piltl_new_value = TMD2635_PILTL_RESET;
      uint8_t pilth_new_value = TMD2635_PILTH_RESET;
      uint8_t piltl_read_value = 0u;
      uint8_t pilth_read_value = 0u;

      // Read current values
      IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PILTL, &piltl_read_value));
      IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PILTH, &pilth_read_value));

      // Update only if necessary
      if(IS_OK(result) && ((piltl_read_value != piltl_new_value) || (pilth_read_value != pilth_new_value)))
      {
         // Write low then high
         IF_OK_RUN_AND_UPDATE(result, write_register(p_self, TMD2635_REG_PILTL, piltl_new_value));
         IF_OK_RUN_AND_UPDATE(result, write_register(p_self, TMD2635_REG_PILTH, pilth_new_value));

         // Verify
         uint8_t piltl_verify_value = 0u;
         uint8_t pilth_verify_value = 0u;
         IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PILTL, &piltl_verify_value));
         IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PILTH, &pilth_verify_value));

         if(IS_OK(result) && ((piltl_verify_value != piltl_new_value) || (pilth_verify_value != pilth_new_value)))
         {
            SET_ERR(result, PROX_DRV_ERROR_REGISTER_UPDATE_FAIL);
         }
      }
   }

   if(IS_OK(result))
   {
      uint8_t pihtl_new_value = TMD2635_PIHTL_RESET;
      uint8_t pihth_new_value = TMD2635_PIHTH_RESET;
      uint8_t pihtl_read_value = 0u;
      uint8_t pihth_read_value = 0u;

      // Read current values
      IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PIHTL, &pihtl_read_value));
      IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PIHTH, &pihth_read_value));

      // Update only if necessary
      if(IS_OK(result) && ((pihtl_read_value != pihtl_new_value) || (pihth_read_value != pihth_new_value)))
      {
         // Write low then high
         IF_OK_RUN_AND_UPDATE(result, write_register(p_self, TMD2635_REG_PIHTL, pihtl_new_value));
         IF_OK_RUN_AND_UPDATE(result, write_register(p_self, TMD2635_REG_PIHTH, pihth_new_value));

         // Verify
         uint8_t pihtl_verify_value = 0u;
         uint8_t pihth_verify_value = 0u;
         IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PIHTL, &pihtl_verify_value));
         IF_OK_RUN_AND_UPDATE(result, read_register(p_self, TMD2635_REG_PIHTH, &pihth_verify_value));

         if(IS_OK(result) && ((pihtl_verify_value != pihtl_new_value) || (pihth_verify_value != pihth_new_value)))
         {
            SET_ERR(result, PROX_DRV_ERROR_REGISTER_UPDATE_FAIL);
         }
      }
   }

   // Configure PCFG0 register: proximity gain and maximum VCSEL pulses
   uint8_t pcfg0_val = 0u;
   switch(p_cfg->gain)
   {
      case TMD2635_GAIN_1X:
         pcfg0_val |= TMD2635_PCFG0_PGAIN_1X;
         break;
      case TMD2635_GAIN_2X:
         pcfg0_val |= TMD2635_PCFG0_PGAIN_2X;
         break;
      case TMD2635_GAIN_4X:
         pcfg0_val |= TMD2635_PCFG0_PGAIN_4X;
         break;
      case TMD2635_GAIN_8X:
         pcfg0_val |= TMD2635_PCFG0_PGAIN_8X;
         break;
      case TMD2635_GAIN_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
   }

   pcfg0_val |= TMD2635_PCFG0_PPULSE(p_cfg->max_pulses);

   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_PCFG0, TMD2635_PCFG0_MASK, pcfg0_val));

   // Configure PCFG1 register: pulse length and drive current
   uint8_t pcfg1_val = 0u;
   switch(p_cfg->pulse_len)
   {
      case TMD2635_PULSE_LEN_1US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_1US;
         break;
      case TMD2635_PULSE_LEN_2US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_2US;
         break;
      case TMD2635_PULSE_LEN_4US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_4US;
         break;
      case TMD2635_PULSE_LEN_8US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_8US;
         break;
      case TMD2635_PULSE_LEN_12US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_12US;
         break;
      case TMD2635_PULSE_LEN_16US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_16US;
         break;
      case TMD2635_PULSE_LEN_24US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_24US;
         break;
      case TMD2635_PULSE_LEN_32US:
         pcfg1_val |= TMD2635_PCFG1_PPULSE_LEN_32US;
         break;
      case TMD2635_PULSE_LEN_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
   }

   switch(p_cfg->drive_current)
   {
      case TMD2635_DRIVE_7MA:
         pcfg1_val |= TMD2635_PCFG1_PLDRIVE_7MA;
         break;
      case TMD2635_DRIVE_8MA:
         pcfg1_val |= TMD2635_PCFG1_PLDRIVE_8MA;
         break;
      case TMD2635_DRIVE_9MA:
         pcfg1_val |= TMD2635_PCFG1_PLDRIVE_9MA;
         break;
      case TMD2635_DRIVE_10MA:
         pcfg1_val |= TMD2635_PCFG1_PLDRIVE_10MA;
         break;
      case TMD2635_DRIVE_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
   }

   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_PCFG1, TMD2635_PCFG1_MASK, pcfg1_val));

   // Configure POFFSET registers: proximity offset compensation
   // Note: Unused in current implementation but set to default reset values
   IF_OK_RUN_AND_UPDATE(result,
                        update_register(p_self, TMD2635_REG_POFFSETL, TMD2635_POFFSETL_MASK, TMD2635_POFFSETL_RESET));
   IF_OK_RUN_AND_UPDATE(result,
                        update_register(p_self, TMD2635_REG_POFFSETH, TMD2635_POFFSETH_MASK, TMD2635_POFFSETH_RESET));

   // Configure CFG0 register: PWLONG bit based on desired wait time
   // Note: Unused in current implementation but set to default reset value
   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_CFG0, TMD2635_CFG0_MASK, TMD2635_CFG0_RESET));

   // Configure PERS register: interrupt persistence
   // Note: Unused in current implementation but set to default reset value
   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_PERS, TMD2635_PERS_MASK, TMD2635_PERS_RESET));

   // Configure PWTIME register: proximity wait time
   // Part of runtime configuration, so set to default value here
   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_PWTIME, TMD2635_PWTIME_MASK, TMD2635_PWTIME_RESET));

   // Configure PRATE register: proximity measurement rate
   // Note: Not configured in current implementation but set to default reset value
   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_PRATE, TMD2635_PRATE_MASK, TMD2635_PRATE_RESET));

   // Configure INTENAB register: enable/disable proximity interrupts
   // Note: Unused in current implementation but set to default reset value
   IF_OK_RUN_AND_UPDATE(result,
                        update_register(p_self, TMD2635_REG_INTENAB, TMD2635_INTENAB_MASK, TMD2635_INTENAB_RESET));

   // Enable and configure hardware averaging
   // Configure CALIBCFG register: hardware averaging and auto-offset compensation
   // Note: Auto-offset compensation unused in current implementation
   uint8_t calibcfg_hw_avg_val = 0u;
   switch(p_cfg->hw_avg)
   {
      case TMD2635_HWAVG_OFF:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_0;
         break;
      case TMD2635_HWAVG_2:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_2;
         break;
      case TMD2635_HWAVG_4:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_4;
         break;
      case TMD2635_HWAVG_8:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_8;
         break;
      case TMD2635_HWAVG_16:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_16;
         break;
      case TMD2635_HWAVG_32:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_32;
         break;
      case TMD2635_HWAVG_64:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_64;
         break;
      case TMD2635_HWAVG_128:
         calibcfg_hw_avg_val |= TMD2635_CALIBCFG_PROX_AVG_128;
         break;
      case TMD2635_HWAVG_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
         break;
   }

   // Set PROX_AVG bits while resetting other bits to default
   uint8_t calibcfg_set_mask = (TMD2635_CALIBCFG_RESET & ~TMD2635_CALIBCFG_PROX_AVG_MASK) | calibcfg_hw_avg_val;

   IF_OK_RUN_AND_UPDATE(result,
                        update_register(p_self, TMD2635_REG_CALIBCFG, TMD2635_CALIBCFG_MASK, calibcfg_set_mask));

   // Configure CFG8 register: photodiode selection
   uint8_t cfg8_val = 0u;
   switch(p_cfg->photodiode)
   {
      case TMD2635_PD_FAR:
         cfg8_val |= TMD2635_CFG8_PDSELECT_FAR;
         break;
      case TMD2635_PD_NEAR:
         cfg8_val |= TMD2635_CFG8_PDSELECT_NEAR;
         break;
      case TMD2635_PD_BOTH:
         cfg8_val |= TMD2635_CFG8_PDSELECT_BOTH;
         break;
      case TMD2635_PD_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
         break;
   }

   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_CFG8, TMD2635_CFG8_MASK, cfg8_val));

   // Configure CFG3 register: Interrupt clear mode and sleep-after-interrupt
   // Note: Not configured in current implementation but set to clear interrupts on read and no sleep after interrupt
   IF_OK_RUN_AND_UPDATE(result,
                        update_register(p_self, TMD2635_REG_CFG3, TMD2635_CFG3_SAI, TMD2635_CFG3_INT_READ_CLEAR));

   // Configure CFG6 register: automatic proximity compensation
   // Note: Not configured in current implementation but set to default (enabled)
   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_CFG6, TMD2635_CFG6_MASK, TMD2635_CFG6_RESET));

   // Configure PFILTER register: moving average filter
   uint8_t pfilter_val = 0u;
   switch(p_cfg->moving_avg)
   {
      case TMD2635_MAVG_OFF:
         pfilter_val |= TMD2635_MAVG_OFF;
         break;
      case TMD2635_MAVG_2:
         pfilter_val |= TMD2635_MAVG_2;
         break;
      case TMD2635_MAVG_4:
         pfilter_val |= TMD2635_MAVG_4;
         break;
      case TMD2635_MAVG_8:
         pfilter_val |= TMD2635_MAVG_8;
         break;
      case TMD2635_MAVG_MAX:
         // Fallthrough
      default:
         SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
   }

   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_PFILTER, TMD2635_PFILTER_MASK, pfilter_val));

   // Set compulsory test9 register to reserved value
   IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_TEST9, TMD2635_TEST9_MASK, TMD2635_TEST9_REQUIRED));

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
static result_t set_mode(const ir_proximity_driver_interface_t *const interface, TMD2635_POWER_MODE mode)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(false == interface->parent->_is_initialized, PROX_DRV_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_TRUE((TMD2635_POWER_MODE_MAX <= mode), PROX_DRV_ERROR_INVALID_PARAM);

   tmd2635_driver_t *p_self = interface->parent;
   TMD2635_POWER_MODE prev_mode = p_self->_power_mode;
   result_t result = RESULT_OK;

   if(mode != prev_mode)
   {
      uint8_t clear_mask = 0u;
      uint8_t set_mask = 0u;

      switch(mode)
      {
         case TMD2635_POWER_MODE_SLEEP:
            clear_mask = TMD2635_ENABLE_PON | TMD2635_ENABLE_PEN;
            break;
         case TMD2635_POWER_MODE_IDLE:
            clear_mask = TMD2635_ENABLE_PEN;
            set_mask = TMD2635_ENABLE_PON;
            break;
         case TMD2635_POWER_MODE_ACTIVE:
            set_mask = TMD2635_ENABLE_PON | TMD2635_ENABLE_PEN;
            break;
         case TMD2635_POWER_MODE_MAX:
            // Fallthrough
         default:
            SET_ERR(result, PROX_DRV_ERROR_INVALID_PARAM);
            break;
      }

      IF_OK_RUN_AND_UPDATE(result, update_register(p_self, TMD2635_REG_ENABLE, clear_mask, set_mask));

      if(IS_OK(result))
      {
         p_self->_power_mode = mode;
      }
   }

   return result;
}

/**
 * @brief Ensure the TMD2635 is present and responding over I2C
 *
 * Ensures the TMD2635 device is present on the I2C bus and responding correctly by reading its ID register.
 *
 * The TMD2635 requires a delay after power-up before it is ready to respond to I2C commands and a dummy I2C access
 * to initialize the device I2C address.
 *
 * @param[in] p_self Pointer to the driver instance
 *
 * @return Status code indicating the result of the operation
 */
static result_t ensure_present_and_responding(const tmd2635_driver_t *p_self)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   // Wait for device to be ready after power-up
   nrf_delay_us(TMD2635_ACTIVE_TIME_US);

   // Read ID twice. Once as required dummy I2C access which might succeed if I2C access already happened on the bus.
   // Again to confirm the device is present and responding correctly if the first read was unsuccessful (expected).
   for(uint8_t attempt = 0u; attempt < 2u; attempt++)
   {
      uint8_t id = 0u;
      result = read_register(p_self, TMD2635_REG_ID, &id);

      if(IS_OK(result))
      {
         if(TMD2635_ID_RESET == id)
         {
            break;
         }
         else
         {
            SET_ERR(result, PROX_DRV_ERROR_DEVICE_NOT_FOUND);
         }
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t tmd2635_driver_init(tmd2635_driver_t *const p_self,
                             const tmd2635_config_t *p_cfg,
                             const i2c_driver_interface_t *p_i2c_ifc)
{
   RETURN_ERR_IF_NULL(p_self, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_cfg, PROX_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(p_i2c_ifc, PROX_DRV_ERROR_PTR_NULL);

   // Assign interface
   p_self->interface.parent = p_self;
   p_self->interface.set_sleep = set_sleep;
   p_self->interface.set_wake = set_wake;
   p_self->interface.get_proximity_data = get_proximity_data;
   p_self->interface.get_status_value = get_status_value;
   p_self->interface.set_sampling_rate = set_sampling_rate;

   // Assign dependencies
   p_self->_p_i2c_ifc = p_i2c_ifc;

   // Initialize private data
   p_self->_power_mode = TMD2635_POWER_MODE_IDLE;
   p_self->_is_initialized = true;

   // Validate configuration
   result_t result = validate_config(p_cfg);
   if(IS_OK(result))
   {
      memcpy(&(p_self->_cfg), p_cfg, sizeof(tmd2635_config_t));
   }

   // Ensure device is present and responding
   if(IS_OK(result))
   {
      result = ensure_present_and_responding(p_self);
   }

   // Apply configuration
   IF_OK_RUN_AND_UPDATE(result, set_config(p_self, p_cfg));

   // Set default sampling period
   p_self->_sampling_period_ms = 0u;
   IF_OK_RUN_AND_UPDATE(result, set_sampling_rate(&p_self->interface, TMD2635_SAMPLING_PERIOD_MS_DEFAULT));
   if(p_self->_sampling_period_ms != TMD2635_SAMPLING_PERIOD_MS_DEFAULT)
   {
      SET_ERR(result, PROX_DRV_ERROR_INVALID_CONFIG);
   }

   // If any step failed, mark the driver as uninitialized
   if(IS_ERR(result))
   {
      p_self->_is_initialized = false;
   }

   return result;
}