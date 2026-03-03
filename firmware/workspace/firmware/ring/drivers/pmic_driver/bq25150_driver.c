/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file bq25150_driver.c
 * @ingroup pmic_driver
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include "nrf_delay.h"

// Custom includes
#include "bq25150.h"
#include "bq25150_driver.h"
#include "debug.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BMS_BQ25150_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define I2C_TIMEOUT_US  (100u)
#define I2C_BUFFER_SIZE (10u)
#define I2C_INTER_WRITE_DELAY_MS                                                                                       \
   (1u) // Delay between consecutive I2C writes during startup only. Determined empirically.

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t get_pmic_status(const bq25150_driver_interface_t *const interface, pmic_status_t *status);
static result_t get_measurements(const bq25150_driver_interface_t *const interface,
                                 pmic_battery_measurements_t *measurements);
static result_t set_shipmode(const bq25150_driver_interface_t *const interface);
static result_t set_ldo_switch_status(const bq25150_driver_interface_t *const interface, bool switch_on);

static result_t get_ldo_switch_status(const bq25150_driver_interface_t *const interface, bool *p_enabled);
static result_t set_charge_enable(const bq25150_driver_interface_t *const interface, bool enable_charge);
static result_t set_low_power_mode(const bq25150_driver_interface_t *const interface, bool enable_lp);

// Non-interface functions
static result_t read_adc_16bit(const bq25150_driver_t *self, uint8_t msb_reg, uint16_t *p_raw);
/**
 * @brief Initializes the driver using the provided interface and buffer.
 *
 * @param interface A pointer to the bq25150_driver_interface_t structure containing the function pointers.
 * @param number_of_rows The number of rows in the buffer.
 * @param buffer A 2D array of uint16_t pointers, where each row contains two uint16_t values representing the register
 * address and data.
 *
 * @return A result_t value indicating the success or failure of the initialization.
 */
static result_t init_driver_helper_function(const bq25150_driver_t *self,
                                            size_t number_of_rows,
                                            const uint8_t buffer[number_of_rows][2]);

/**
 * @brief Writes data to a specific register in the BQ25150 PMIC.
 *
 * @param self Pointer to the BQ25150 driver instance.
 * @param register_address The address of the register to write to.
 * @param data Pointer to the data to write.
 * @param data_length The number of bytes to write.
 *
 * @return Result of the operation.
 */
static result_t
   write_register(const bq25150_driver_t *self, uint8_t register_address, const uint8_t *data, uint8_t data_length);

/**
 * @brief Reads data from a specified register in the BQ25150 PMIC.
 *
 * @param self Pointer to the BQ25150 driver instance.
 * @param register_address The address of the register to read from.
 * @param rx_data Pointer to the buffer where the read data will be stored.
 * @param data_length The number of bytes to read from the register.
 *
 * @return Result of the operation.
 */
static result_t
   read_register(const bq25150_driver_t *self, uint8_t register_address, uint8_t *rx_data, uint8_t data_length);

static result_t check_faults(const bq25150_driver_t *self, const uint8_t *flags, const uint8_t *stats);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static result_t init_driver_helper_function(const bq25150_driver_t *self,
                                            size_t number_of_rows,
                                            const uint8_t buffer[number_of_rows][2])
{
   RETURN_ERR_IF_NULL(self, BQ25150_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   uint8_t tx_data = 0;
   uint8_t rx_data = 0;
   for(size_t row = 0; row < number_of_rows; row++)
   {
      tx_data = (uint8_t)(buffer[row][1]); // NOSONAR: Overflow guaranteed not to happen since the buffer is defined as
                                           // a constant in bq25150.h (assuming the array was defined correctly)
      result = write_register(self, buffer[row][0], &tx_data, 1u);
      BREAK_ON_ERR(result);
      nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS); // Wait for some time to allow the registers to settle

      // Check if the value was successfully written
      result = read_register(self, buffer[row][0], &rx_data, 1);
      nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS); // Wait for some time to allow the registers to settle
      BREAK_ON_ERR(result);
      if(rx_data != tx_data)
      {
         SET_ERR(result, BQ25150_DRV_ERROR_INIT);
         break;
      }
   }
   return result;
}

static result_t
   read_register(const bq25150_driver_t *self, uint8_t register_address, uint8_t *rx_data, uint8_t data_length)
{
   RETURN_ERR_IF_NULL(self, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(rx_data, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(rx_data, BQ25150_DRV_ERROR_PTR_NULL);

   uint8_t tx_buffer[sizeof(register_address)] = {register_address};

   result_t result = self->_i2c_interface->transmit(
      self->_i2c_interface, self->_device_address, tx_buffer, sizeof(register_address), I2C_TIMEOUT_US);
   ON_ERR_DEBUG_ERROR(result, "PMIC driver: write to register failed.");

   IF_OK_RUN_AND_UPDATE(
      result,
      self->_i2c_interface->receive(self->_i2c_interface, self->_device_address, rx_data, data_length, I2C_TIMEOUT_US));
   ON_ERR_DEBUG_ERROR(result, "PMIC driver: read from register failed.");

   if(IS_ERR(result))
   {
      SET_ERR(result, BQ25150_DRV_ERROR_FAILED_I2C);
   }

   return result;
}

static result_t
   write_register(const bq25150_driver_t *self, uint8_t register_address, const uint8_t *data, uint8_t data_length)
{
   RETURN_ERR_IF_NULL(self, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, BQ25150_DRV_ERROR_PTR_NULL);

   uint8_t tx_buffer[I2C_BUFFER_SIZE] = {0};

   if(data_length > (I2C_BUFFER_SIZE - sizeof(register_address)))
   {
      RETURN_ERR(BQ25150_DRV_ERROR_BUFFER_OVERFLOW);
   }

   tx_buffer[0] = register_address;

   memcpy(&tx_buffer[1], data, data_length);

   result_t result = self->_i2c_interface->transmit(
      self->_i2c_interface, self->_device_address, tx_buffer, data_length + sizeof(register_address), I2C_TIMEOUT_US);

   if(IS_ERR(result))
   {
      DEBUG_ERROR(
         "PMIC driver: write to register failed. Unit: %d, Code:%d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      SET_ERR(result, BQ25150_DRV_ERROR_FAILED_I2C);
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t set_charge_enable(const bq25150_driver_interface_t *const interface, bool enable_charge)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   const bq25150_driver_t *self = interface->parent;
   result_t result = RESULT_OK;
   uint8_t reg_val = 0;

   if(enable_charge)
   {
      nrf_gpio_pin_clear(self->_ce_pin); // Pin is inverted
      result = read_register(self, REG_CHG_CTRL1, &reg_val, sizeof(reg_val));

      if(IS_OK(result) && (reg_val != 0x10))
      {
         reg_val = 0x10;
         result = write_register(self, REG_CHG_CTRL1, &reg_val, sizeof(reg_val));
      }

      IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_IC_CTRL2, &reg_val, sizeof(reg_val)));

      if(IS_OK(result) && (reg_val != 0x00)) // 0x00 = Enable charge
      {
         reg_val = 0x00;
         result = write_register(self, REG_IC_CTRL2, &reg_val, sizeof(reg_val));
      }
   }
   else
   {
      // Turn charging off
      nrf_gpio_pin_set(self->_ce_pin);
      result = read_register(self, REG_CHG_CTRL1, &reg_val, sizeof(reg_val));

      if(IS_OK(result) && (reg_val != 0x58))
      {
         reg_val = 0x58;
         result = write_register(self, REG_CHG_CTRL1, &reg_val, sizeof(reg_val));
      }

      IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_IC_CTRL2, &reg_val, sizeof(reg_val)));

      if(IS_OK(result) && (reg_val != 0x01)) // 0x01 = Disable charge
      {
         reg_val = 0x01;
         result = write_register(self, REG_IC_CTRL2, &reg_val, sizeof(reg_val));
      }
   }
   return result;
}

static result_t set_low_power_mode(const bq25150_driver_interface_t *const interface, bool enable_lp)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;
   const bq25150_driver_t *self = interface->parent;

   if(enable_lp)
   {
      nrf_gpio_pin_clear(self->_lp_pin); // Pin is inverted
   }
   else
   {
      nrf_gpio_pin_set(self->_lp_pin);
   }
   return result;
}

static result_t set_ldo_switch_status(const bq25150_driver_interface_t *const interface, bool switch_on)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   const bq25150_driver_t *self = interface->parent;

   uint8_t reg_val = 0;

   // Get the current status of the LDO
   result_t result = read_register(self, REG_LDO_CTRL, &reg_val, sizeof(reg_val));
   ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to read LDO control register.");

   if(IS_OK(result))
   {
      bool is_ldo_enabled = (reg_val & EN_LS_LDO_MASK) != 0u;

      // Update the status of the switch/LDO if necessary
      if(switch_on && !is_ldo_enabled)
      {
         reg_val = reg_val | EN_LS_LDO_MASK;
         result = write_register(self, REG_LDO_CTRL, &reg_val, sizeof(reg_val));
      }
      else if(!switch_on && is_ldo_enabled)
      {
         reg_val = (uint8_t)(reg_val & ~EN_LS_LDO_MASK);
         result = write_register(self, REG_LDO_CTRL, &reg_val, sizeof(reg_val));
      }

      ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to update LDO control register.");
   }

   return result;
}

static result_t set_shipmode(const bq25150_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   const bq25150_driver_t *self = interface->parent;

   uint8_t reg_val = 0;

   // Read current contents of IC_CTRL0 register
   result_t result = read_register(self, REG_IC_CTRL0, &reg_val, sizeof(reg_val));
   ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to read IC_CTRL0 register to update ship mode status.");

   if(IS_OK(result))
   {
      // Update contents of IC_CTRL0 register to enter ship mode
      reg_val |= EN_SHIP_MODE_MASK;
      result = write_register(self, REG_IC_CTRL0, &reg_val, sizeof(reg_val));

      ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to update IC_CTRL0 register to enter ship mode.");
   }

   return result;
}

static result_t get_ldo_switch_status(const bq25150_driver_interface_t *const interface, bool *p_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_enabled, BQ25150_DRV_ERROR_PTR_NULL);
   const bq25150_driver_t *self = interface->parent;

   uint8_t reg_val;

   result_t result = read_register(self, REG_LDO_CTRL, &reg_val, 1u);
   ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to read LDO control register.");

   if(IS_OK(result))
   {
      *p_enabled = (reg_val & EN_LS_LDO_MASK) != 0u;
   }

   return result;
}

static result_t read_adc_16bit(const bq25150_driver_t *self, uint8_t msb_reg, uint16_t *p_raw)
{
   RETURN_ERR_IF_NULL(self, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_raw, BQ25150_DRV_ERROR_PTR_NULL);

   uint8_t buff[2u] = {0};
   result_t result = read_register(self, msb_reg, buff, sizeof(buff));
   if(IS_ERR(result))
   {
      *p_raw = (uint16_t)(((uint16_t)buff[0] << 7u) | buff[1]);
   }

   *p_raw = (uint16_t)(((uint16_t)buff[0] << 8u) | buff[1]);

   return RESULT_OK;
}

static result_t get_measurements(const bq25150_driver_interface_t *const interface, pmic_battery_measurements_t *p_meas)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_meas, BQ25150_DRV_ERROR_PTR_NULL);
   const bq25150_driver_t *self = interface->parent;

   uint16_t raw = 0;
   uint8_t reg_val = 0;

   // VIN (input voltage)
   result_t result = read_adc_16bit(self, REG_ADC_VIN_MSB, &raw);

   if(IS_OK(result))
   {
      p_meas->vin_mv = (uint16_t)((((uint32_t)raw * 6000u) / (UINT16_MAX + 1u))); // 6000mV from datasheet Table 9-2.
      // VBAT (battery voltage)
      result = read_adc_16bit(self, REG_ADC_VBAT_MSB, &raw);
   }

   if(IS_OK(result))
   {
      p_meas->vbat_mv = (uint16_t)((((uint32_t)raw * 6000u) / (UINT16_MAX + 1u))); // 6000mV from datasheet Table 9-2.
      // IIN (input current)
      // Read input current limit to determine which ADC conversion equation to use from the datasheet.
      result = read_register(self, REG_ILIM_CTRL, &reg_val, sizeof(reg_val));
   }

   if(IS_OK(result))
   {
      reg_val = reg_val & 0x07; // Mask off the upper 4 bits to get the set current limit setting.
      result = read_adc_16bit(self, REG_ADC_IIN_MSB, &raw);
   }

   if(IS_OK(result))
   {
      if(reg_val <= ILIM_150MA) // See Table 9-2 ADC Measurement tables in datasheet.
      {
         p_meas->iin_ma = (uint16_t)((((uint32_t)raw * 375u) / (UINT16_MAX + 1u))); // 375 mA from datasheet Table 9-2.
      }
      else
      {
         p_meas->iin_ma = (uint16_t)((((uint32_t)raw * 750u) / (UINT16_MAX + 1u))); // 750 mA from datasheet Table 9-2.
      }
   }

   return result;
}

static result_t get_pmic_status(const bq25150_driver_interface_t *const interface, pmic_status_t *p_status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_status, BQ25150_DRV_ERROR_PTR_NULL);
   const bq25150_driver_t *self = interface->parent;

   uint8_t stat[2] = {0};        // The index corresponds to the stat register numbers.
   static uint8_t flag[4] = {0}; // The index corresponds to the flag register numbers.

   // Read STAT0 (0x00) to STAT1 (0x01) (STAT3 not implemented) in a single I2C burst
   result_t result = read_register(self, REG_STAT0, stat, sizeof(stat));
   ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to read STAT registers.");

   if(IS_OK(result))
   {
      /**
       * @note THIS IS THE ONLY PLACE WHERE THE FLAG REGISTERS SHOULD BE READ. Given that the registers are cleared upon
       * reading, it's easier to read and manage the flags from a single place than risk reading them from multiple
       * places and missing an important flag.
       */
      // Read FLAG registers 0 to 3 in in a single I2C burst
      result
         = read_register(self, REG_FLAG0, flag, sizeof(flag)); // This clears the flags in the registers upon reading.

      ON_ERR_DEBUG_ERROR(result, "PMIC driver: Failed to read FLAG registers.");
   }
   /**
    * @note THIS IS THE ONLY PLACE WHERE THE FLAG REGISTERS SHOULD BE READ. Given that the registers are cleared upon
    * reading, it's easier to read and manage the flags from a single place than risk reading them from multiple
    * places and missing an important flag.
    */
   // Read FLAG registers 0 to 3 in in a single I2C burst
   result = read_register(self, REG_FLAG0, flag, sizeof(flag)); // This clears the flags in the registers upon reading.
   if(IS_OK(result))
   {
      for(uint8_t idx = 0; idx < 4; idx++)
      {
         p_status->flag_reg[idx] = flag[idx];
      }

      p_status->status_reg[0] = stat[0];
      p_status->status_reg[1] = stat[1];

      // Charger status
      p_status->charger_connected = (stat[0] & VIN_PGOOD_STAT_MASK) != 0u;
      p_status->charger_good = p_status->charger_connected && ((0 == (stat[1] & VIN_OVP_FAULT_STAT_MASK)));
  
      // Battery detected when not in BAT_UVLO fault
      p_status->battery_detected = (0 == (stat[1] & BAT_UVLO_FAULT_STAT_MASK));

      // Battery detected when not in BAT_UVLO fault
      p_status->battery_detected = (0 == (stat[1] & BAT_UVLO_FAULT_STAT_MASK));

      p_status->charging_completed = (stat[0] & CHARGE_DONE_STAT_MASK) != 0u;
      p_status->charging_stopped_die_temp = (stat[0] & THERMREG_ACTIVE_STAT_MASK) != 0u;

      // Infer charging completion status
      if(p_status->charger_good && p_status->battery_detected && !p_status->charging_completed)
      {
         p_status->charging = true;
      }
      else
      {
         p_status->charging = false;
      }

      // Check if ADC measurements are ready
      p_status->vbat_measurement_ready = ((flag[2] & ADC_READY_FLAG_MASK) != 0);

      p_status->is_reset_detected = false; // Not implemented.

      /* JEITA / TS region mapping ------------------------------------------ */
      if((stat[1] & TS_COLD_STAT_MASK) != 0u)
      {
         p_status->ntc_status = PMIC_NTC_STATUS_COLD;
      }
      else if((stat[1] & TS_COOL_STAT_MASK) != 0u)
      {
         p_status->ntc_status = PMIC_NTC_STATUS_COOL;
      }
      else if((stat[1] & TS_WARM_STAT_MASK) != 0u)
      {
         p_status->ntc_status = PMIC_NTC_STATUS_WARM;
      }
      else if((stat[1] & TS_HOT_STAT_MASK) != 0u)
      {
         p_status->ntc_status = PMIC_NTC_STATUS_HOT;
      }
      else
      {
         p_status->ntc_status = PMIC_NTC_STATUS_UNKNOWN; /* no TS bit set */
      }

      result = check_faults(self, flag, stat);
   }

   return result;
}

static result_t check_faults(const bq25150_driver_t *self, const uint8_t *flags, const uint8_t *stats)
{
   RETURN_ERR_IF_NULL(self, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(flags, BQ25150_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(stats, BQ25150_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   bool is_fault_set = false;

   // Note: The order of the following checks is important as the higher-priority faults should be checked first.
   if((stats[1] & VIN_OVP_FAULT_STAT_MASK) != 0u)
   {
      DEBUG_ERROR("PMIC driver: VIN_OVP fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_VIN_OVP); // Highest-priority fault -> first return if err.
   }
   else if(((stats[1] & BAT_OCP_FAULT_STAT_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: BAT_OCP fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_BAT_OCP);
   }
   else if(((flags[3] & LDO_OCP_FAULT_FLAG_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: LDO_OCP fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_LDO_OCP);
   }
   else if(((flags[1] & BAT_UVLO_FAULT_STAT_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: BAT_UVLO fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_BAT_UVLO);
   }
   else if(((flags[3] & SAFETY_TMR_FAULT_FLAG_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: SAFETY_TMR fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_SAFETY_TIMER);
   }
   else if(((flags[3] & WD_FAULT_FLAG_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: WD fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_WATCHDOG);
   }
   else if(((flags[3] & IMAX_FAULT_FLAG_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: IMAX_OPEN fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_IMAX_OPEN);
   }
   else if(((flags[2] & TS_OPEN_FAULT_MASK) != 0u) && (!is_fault_set))
   {
      DEBUG_ERROR("PMIC driver: TS_OPEN fault detected.");
      is_fault_set = true;
      SET_ERR(result, BQ25150_DRV_ERROR_TS_OPEN);
   }

   return result;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t bq25150_driver_init(bq25150_driver_t *const self,
                             const i2c_driver_interface_t *i2c_interface,
                             uint8_t device_address,
                             uint32_t lp_pin,
                             uint32_t ce_pin)
{
   RETURN_ERR_IF_NULL(self, BQ25150_DRV_ERROR_INIT);
   RETURN_ERR_IF_INTERFACE_NULL(i2c_interface, BQ25150_DRV_ERROR_INIT);

   result_t result = RESULT_OK;

   self->_initialized = false;
   self->interface.parent = self;
   self->_lp_pin = lp_pin;
   self->_ce_pin = ce_pin;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.get_pmic_status = get_pmic_status;
   self->interface.get_measurements = get_measurements;
   self->interface.enter_ship_mode = set_shipmode;
   self->interface.set_ldo_status = set_ldo_switch_status;
   self->interface.get_ldo_status = get_ldo_switch_status;
   self->interface.set_low_power_mode = set_low_power_mode;
   self->interface.set_charge_enable = set_charge_enable;

   if(device_address > I2C_ADDRESS_MAX)
   {
      DEBUG_ERROR("PMIC driver: Invalid I2C device address.");
      SET_ERR(result, BQ25150_DRV_ERROR_INVALID_ADDRESS);
   }
   else
   {
      self->_i2c_interface = i2c_interface;
      self->_device_address = device_address;

      // Explicitly reset all the registers on startup
      uint8_t tx_data = 0x01;
      result = write_register(self, REG_IC_CTRL0, &tx_data, sizeof(tx_data));

      // Init the PMIC
      IF_OK_RUN_AND_UPDATE(result, set_low_power_mode(&self->interface, false));

      if(IS_OK(result))
      {
         size_t num_rows = 0;

         // Explicitly initialize all writable registers
         num_rows = (sizeof(bq25150_register_configs) / sizeof(bq25150_register_configs[0]));
         result = init_driver_helper_function(self, num_rows, bq25150_register_configs);
      }

      IF_OK_RUN_AND_UPDATE(result, set_charge_enable(&self->interface, true));

      if(IS_OK(result))
      {
         self->_initialized = true;
      }
   }
   return result;
}
