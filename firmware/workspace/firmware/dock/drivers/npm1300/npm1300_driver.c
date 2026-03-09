/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file npm1300_driver.c
 * @ingroup pmic_driver
 * @brief
 *
 * @note Ship mode: To enter shipping mode, write 0x01 to REG_TASKENTERSHIPMODE. The host software must wait until
 * EVENTSVBUSIN0SET to ensure VBUS is disconnected and discharged before writing to the register. When VBUS
 * is not present, the device enters Ship mode immediately.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include "nrf_delay.h"

// Custom includes
#include "npm1300.h"
#include "npm1300_driver.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BMS_NPM1300_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define I2C_TIMEOUT_US  (100u)
#define I2C_BUFFER_SIZE (10u)

#define NPM1300_MEASUREMENT_DATA_SIZE (10u)
#define VBAT_ADC_BURST_SIZE           (4u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t get_pmic_status(const npm1300_driver_interface_t *const interface, pmic_status_t *status);
static result_t get_buck1_status(const npm1300_driver_interface_t *const interface, bool *is_enabled);
static result_t set_buck1_status(const npm1300_driver_interface_t *const interface, bool enable);
static result_t set_switch1(const npm1300_driver_interface_t *const interface, bool switch_on);
static result_t set_switch2(const npm1300_driver_interface_t *const interface, bool switch_on);
static result_t set_shipmode(const npm1300_driver_interface_t *const interface);
static result_t get_switch1_status(const npm1300_driver_interface_t *const interface, bool *switch_enabled);
static result_t get_switch2_status(const npm1300_driver_interface_t *const interface, bool *switch_enabled);
static result_t get_measurements(const npm1300_driver_interface_t *const interface,
                                 pmic_battery_measurements_t *measurements);
static result_t enable_auto_measurements(const npm1300_driver_interface_t *const interface);
static result_t disable_auto_measurements(const npm1300_driver_interface_t *const interface);

// Non-interface functions
/**
 * @brief Initializes the driver using the provided interface and buffer.
 *
 * @param interface A pointer to the npm1300_driver_interface_t structure containing the function pointers.
 * @param number_of_rows The number of rows in the buffer.
 * @param buffer A 2D array of uint16_t pointers, where each row contains two uint16_t values representing the register
 * address and data.
 *
 * @return A result_t value indicating the success or failure of the initialization.
 */
static result_t init_driver_helper_function(const npm1300_driver_t *self,
                                            size_t number_of_rows,
                                            const uint16_t buffer[number_of_rows][2]);

/**
 * @brief Writes data to a specified register in the NPM1300 PMIC.
 *
 * @param interface Pointer to the NPM1300 driver interface.
 * @param register_address The address of the register to write to.
 * @param data Pointer to the data to write.
 * @param data_length The number of bytes to write.
 *
 * @return A result indicating the success or failure of the operation.
 */
static result_t
   write_register(const npm1300_driver_t *self, uint16_t register_address, const uint8_t *data, uint8_t data_length);

/**
 * @brief Reads data from a specified register in the NPM1300 PMIC.
 *
 * @param interface Pointer to the NPM1300 driver interface.
 * @param register_address The address of the register to read from.
 * @param rx_data Pointer to the buffer where the read data will be stored.
 * @param data_length The number of bytes to read.
 *
 * @return A result indicating the success or failure of the operation.
 */
static result_t
   read_register(const npm1300_driver_t *self, uint16_t register_address, uint8_t *rx_data, uint8_t data_length);

static NPM1300_DRV_ERROR map_chargererrreason_to_enum(uint8_t charger_err_reason);
static NPM1300_DRV_ERROR map_chargererrsensor_to_enum(uint8_t charger_err_sensor);
static NPM1300_DRV_ERROR map_rstcause_to_enum(uint8_t rst_cause);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static result_t init_driver_helper_function(const npm1300_driver_t *self,
                                            size_t number_of_rows,
                                            const uint16_t buffer[number_of_rows][2])
{
   RETURN_ERR_IF_NULL(self, NPM1300_DRV_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   uint8_t tx_data = 0;
   for(size_t row = 0; row < number_of_rows; row++)
   {
      tx_data = (uint8_t)(buffer[row][1]); // NOSONAR: Overflow guaranteed not to happen since the buffer is defined as
                                           // a constant in npm1300.h (assuming the array was defined correctly)
      result = write_register(self, buffer[row][0], &tx_data, 1u);
      BREAK_ON_ERR(result);
      nrf_delay_ms(1u); // Wait for some time to allow the registers to settle
   }
   return result;
}

static result_t
   read_register(const npm1300_driver_t *self, uint16_t register_address, uint8_t *rx_data, uint8_t data_length)
{
   RETURN_ERR_IF_NULL(self, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(rx_data, NPM1300_DRV_ERROR_PTR_NULL);

   uint8_t tx_buffer[sizeof(register_address)] = {0};
   tx_buffer[0] = (uint8_t)((uint16_t)(register_address >> 8u)) & 0xFF; // MSB of register address
   tx_buffer[1] = (uint8_t)(register_address & 0xFF);                   // LSB of register address

   result_t result = self->_i2c_interface->transmit(
      self->_i2c_interface, self->_device_address, tx_buffer, sizeof(uint16_t), I2C_TIMEOUT_US);

   ON_ERR_DEBUG_ERROR(result, "PMIC driver: write to register failed.");

   if(IS_OK(result))
   {
      result = self->_i2c_interface->receive(
         self->_i2c_interface, self->_device_address, rx_data, data_length, I2C_TIMEOUT_US);

      ON_ERR_DEBUG_ERROR(result, "PMIC driver: read from register failed.");
   }

   return result;
}

static result_t
   write_register(const npm1300_driver_t *self, uint16_t register_address, const uint8_t *data, uint8_t data_length)
{
   RETURN_ERR_IF_NULL(self, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(data_length > (I2C_BUFFER_SIZE - sizeof(register_address)), NPM1300_DRV_ERROR_BUFFER_OVERFLOW);

   uint8_t tx_buffer[I2C_BUFFER_SIZE] = {0};

   tx_buffer[0] = (uint8_t)(((uint16_t)(register_address >> 8)) & 0xFF); // MSB of register address
   tx_buffer[1] = (uint8_t)(register_address & 0xFF);                    // LSB of register address

   memcpy(&tx_buffer[2], data, data_length);

   result_t result = self->_i2c_interface->transmit(
      self->_i2c_interface, self->_device_address, tx_buffer, data_length + sizeof(register_address), I2C_TIMEOUT_US);

   ON_ERR_DEBUG_ERROR(result, "PMIC driver: write to register failed.");

   return result;
}

static NPM1300_DRV_ERROR map_chargererrreason_to_enum(uint8_t charger_err_reason)
{
   if(charger_err_reason) // If there is an error
   {
      if(charger_err_reason & (1 << 6))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_TRICKLETIMEOUT;
      if(charger_err_reason & (1 << 5))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_CHARGETIMEOUT;
      if(charger_err_reason & (1 << 4))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_MEASTIMEOUT;
      if(charger_err_reason & (1 << 3))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_VTRICKLE;
      if(charger_err_reason & (1 << 2))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_VBATLOW;
      if(charger_err_reason & (1 << 1))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_VBATSENSORERR;
      if(charger_err_reason & (1 << 0))
         return NPM1300_DRV_ERROR_CHARGERERRREASON_NTCSENSORERR;
   }
   return NPM1300_DRV_ERROR_NONE;
}

static NPM1300_DRV_ERROR map_chargererrsensor_to_enum(uint8_t charger_err_sensor)
{
   if(charger_err_sensor) // If there is an error
   {
      if(charger_err_sensor & (1 << 7))
         return NPM1300_DRV_ERROR_SENSORVBATLOW;
      if(charger_err_sensor & (1 << 6))
         return NPM1300_DRV_ERROR_SENSORTRICKLE;
      if(charger_err_sensor & (1 << 5))
         return NPM1300_DRV_ERROR_SENSORRECHARGE;
      if(charger_err_sensor & (1 << 4))
         return NPM1300_DRV_ERROR_SENSORVTERM;
      if(charger_err_sensor & (1 << 3))
         return NPM1300_DRV_ERROR_SENSORNTCHOT;
      if(charger_err_sensor & (1 << 2))
         return NPM1300_DRV_ERROR_SENSORNTCWARM;
      if(charger_err_sensor & (1 << 1))
         return NPM1300_DRV_ERROR_SENSORNTCCOOL;
      if(charger_err_sensor & (1 << 0))
         return NPM1300_DRV_ERROR_SENSORNTCCOLD;
   }
   return NPM1300_DRV_ERROR_NONE;
}

static NPM1300_DRV_ERROR map_rstcause_to_enum(uint8_t rst_cause)
{
   if(rst_cause) // If there is an error
   {
      if(rst_cause & (1 << 6))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_SWRESET");
         return NPM1300_DRV_ERROR_RSTCAUSE_SWRESET;
      }
      if(rst_cause & (1 << 5))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_VSYSLOW");
         return NPM1300_DRV_ERROR_RSTCAUSE_VSYSLOW;
      }
      if(rst_cause & (1 << 4))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_THERMALSHUTDOWN");
         return NPM1300_DRV_ERROR_RSTCAUSE_THERMALSHUTDOWN;
      }
      if(rst_cause & (1 << 3))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_LONGPRESSTIMEOUT");
         return NPM1300_DRV_ERROR_RSTCAUSE_LONGPRESSTIMEOUT;
      }
      if(rst_cause & (1 << 2))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_WATCHDOGTIMEOUT");
         return NPM1300_DRV_ERROR_RSTCAUSE_WATCHDOGTIMEOUT;
      }
      if(rst_cause & (1 << 1))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_BOOTMONITORTIMEOUT");
         return NPM1300_DRV_ERROR_RSTCAUSE_BOOTMONITORTIMEOUT;
      }
      if(rst_cause & (1 << 0))
      {
         DEBUG_WARNING("NPM1300 RESET: NPM1300_DRV_ERROR_RSTCAUSE_SHIPMODEEXIT");
         return NPM1300_DRV_ERROR_RSTCAUSE_SHIPMODEEXIT;
      }
   }

   // If no errors are detected, return NPM1300_DRV_ERROR_MAX as a placeholder for "no error"
   return NPM1300_DRV_ERROR_NONE;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t enable_auto_measurements(const npm1300_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   // return early if auto measurements are already enabled - no point in ceaselessly writing the config that is already
   // applied
   RETURN_OK_IF_TRUE(interface->parent->_auto_enabled);

   size_t num_rows
      = (sizeof(system_monitor_enable_auto_measurement) / sizeof(system_monitor_enable_auto_measurement[0]));
   result_t result = init_driver_helper_function(interface->parent, num_rows, system_monitor_enable_auto_measurement);

   if(IS_OK(result))
   {
      interface->parent->_auto_enabled = true;
   }

   return result;
}

static result_t disable_auto_measurements(const npm1300_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   // return early if auto measurements are already disabled - no point in ceaselessly writing the config that is
   // already applied
   RETURN_OK_IF_TRUE(!interface->parent->_auto_enabled);

   size_t num_rows
      = (sizeof(system_monitor_disable_auto_measurement) / sizeof(system_monitor_disable_auto_measurement[0]));
   result_t result = init_driver_helper_function(interface->parent, num_rows, system_monitor_disable_auto_measurement);

   if(IS_OK(result))
   {
      interface->parent->_auto_enabled = false;
   }

   return result;
}

static result_t get_buck1_status(const npm1300_driver_interface_t *const interface, bool *is_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_enabled, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;

   uint8_t rx_data = 0;
   result_t result = read_register(self, REG_BUCK1ENASET, &rx_data, sizeof(rx_data));

   if(IS_OK(result))
   {
      *is_enabled = rx_data & BUCK1_ENABLE_MASK;
   }

   return result;
}

static result_t set_buck1_status(const npm1300_driver_interface_t *const interface, bool enable)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;
   result_t result = RESULT_OK;

   uint8_t tx_data = BUCK1_ENABLE_MASK;
   if(enable)
   {
      result = write_register(self, REG_BUCK1ENASET, &tx_data, sizeof(tx_data));
   }
   else
   {
      result = write_register(self, REG_BUCK1ENACLR, &tx_data, sizeof(tx_data));
   }

   return result;
}

static result_t set_switch1(const npm1300_driver_interface_t *const interface, bool switch_on)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;
   result_t result = RESULT_OK;

   uint8_t tx_data = TASKLDSW_ENABLE_MASK;

   if(switch_on)
   {
      result = write_register(self, REG_TASKLDSW1SET, &tx_data, sizeof(tx_data));
   }
   else
   {
      result = write_register(self, REG_TASKLDSW1CLR, &tx_data, sizeof(tx_data));
   }

   return result;
}

static result_t set_switch2(const npm1300_driver_interface_t *const interface, bool switch_on)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;
   result_t result = RESULT_OK;

   uint8_t tx_data = TASKLDSW_ENABLE_MASK;

   if(switch_on)
   {
      result = write_register(self, REG_TASKLDSW2SET, &tx_data, sizeof(tx_data));
   }
   else
   {
      result = write_register(self, REG_TASKLDSW2CLR, &tx_data, sizeof(tx_data));
   }

   return result;
}

static result_t set_shipmode(const npm1300_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;
   result_t result = RESULT_OK;

   // Get current status
   pmic_status_t status = {0};
   result = get_pmic_status(interface, &status);

   if(IS_OK(result))
   {
      // Check if VBUS is disconnected and discharged
      if(true == status.charger_connected)
      {
         SET_ERR(result, NPM1300_DRV_ERROR_INVALID_COMMAND);
      }

      uint8_t tx_data = TASKLDSW_ENABLE_MASK;

      IF_OK_RUN_AND_UPDATE(result, write_register(self, REG_TASKENTERSHIPMODE, &tx_data, sizeof(tx_data)));
   }

   return result;
}

static result_t get_switch2_status(const npm1300_driver_interface_t *const interface, bool *switch_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(switch_enabled, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;

   uint8_t rx_data = 0;

   result_t result = read_register(self, REG_LDSWSTATUS_R, &rx_data, sizeof(rx_data));

   if(IS_OK(result))
   {
      *switch_enabled = rx_data & SWITCH2_STATUS_MASK;
   }

   return result;
}

static result_t get_switch1_status(const npm1300_driver_interface_t *const interface, bool *switch_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(switch_enabled, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;

   uint8_t rx_data = 0;

   result_t result = read_register(self, REG_LDSWSTATUS_R, &rx_data, sizeof(rx_data));

   if(IS_OK(result))
   {
      *switch_enabled = rx_data & SWITCH1_STATUS_MASK;
   }

   return result;
}

static result_t get_measurements(const npm1300_driver_interface_t *const interface,
                                 pmic_battery_measurements_t *measurements)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(measurements, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;

   uint8_t rx_data[NPM1300_MEASUREMENT_DATA_SIZE] = {0};

   result_t result = read_register(self, REG_ADCVBATRESULTMSB_R, rx_data, NPM1300_MEASUREMENT_DATA_SIZE);

   if(IS_OK(result))
   {
      uint32_t vbat_adc_sum = 0;

      // See ADCVBAT0RESULTMSB to ADCGP1RESULTLSBS in the datasheet.
      vbat_adc_sum += (((uint32_t)rx_data[5]) << 2u) | ((rx_data[9] >> 0u) & 0x03);
      vbat_adc_sum += (((uint32_t)rx_data[6]) << 2u) | ((rx_data[9] >> 2u) & 0x03);
      vbat_adc_sum += (((uint32_t)rx_data[7]) << 2u) | ((rx_data[9] >> 4u) & 0x03);
      vbat_adc_sum += (((uint32_t)rx_data[8]) << 2u) | ((rx_data[9] >> 6u) & 0x03);

// Implicit conversions here verified to be safe. Prefer not to clutter with many confusing casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
      measurements->vbat_adc = (uint16_t)(vbat_adc_sum / VBAT_ADC_BURST_SIZE); // Average of vbat burst measurement
#pragma GCC diagnostic pop

      // Clear the EVENTADCVBATRDY flag
      uint8_t tx_data = 0x01;
      IF_OK_RUN_AND_UPDATE(result, write_register(self, REG_EVENTSADCCLR, &tx_data, sizeof(uint8_t)));

      // Specifically instruct PMIC to start next vsys measurement. Other measurements are automatically started - see
      // the
      // )
      tx_data = 0x01; // Start vsys measurement
      IF_OK_RUN_AND_UPDATE(result, write_register(self, REG_TASKVSYSMEASURE, &tx_data, sizeof(uint8_t)));
   }

   return result;
}

static result_t get_pmic_status(const npm1300_driver_interface_t *const interface, pmic_status_t *status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NPM1300_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(status, NPM1300_DRV_ERROR_PTR_NULL);
   const npm1300_driver_t *self = interface->parent;

   uint8_t rx_data[2] = {0};

   // Check if an external charger is connected
   result_t result = read_register(self, REG_VBUSINSTATUS_R, rx_data, 1u);

   if(IS_OK(result))
   {
      status->charger_connected = rx_data[0] & VBUS_DETECTED_MASK;
   }

   // Get charger connection status
   if((VBUS_GOOD_MASK == rx_data[0]) && IS_OK(result))
   {
      status->charger_good = true;
   }
   else if(IS_OK(result))
   {
      // Some error regarding the VBUS was detected
      status->charger_good = false;

      /**
       * @note The following conditions are not reported since it's part of normal operation.
       * - VBUS over-voltage protection active (always on)
       * - VBUS under-voltage detected (always reported when VBUS disconnected)
       */

      if(rx_data[0] & VBUSINCURRLIMACTIVE_MASK)
      {
         DEBUG_WARNING("PMIC driver status: VBUS current limit detected.");
      }
      if(rx_data[0] & VBUSINSUSPENDMODEACTIVE_MASK)
      {
         DEBUG_WARNING("PMIC driver status: VBUS suspended.");
      }
   }

   // Check if the battery was detected
   IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_BCHGCHARGESTATUS_R, rx_data, 1u));

   if(IS_OK(result))
   {
      status->battery_detected = rx_data[0] & BATTERY_DETECTED_MASK;
      // Check if the battery is charging
      status->charging_stopped_die_temp = (bool)(rx_data[0] & BATTERY_CHARGING_STOP_HIGH_DIE_TEMP_MASK);
      status->charging_completed = (bool)(rx_data[0] & BATTERY_CHARGING_COMPLETED_MASK);
      status->charging = (bool)((rx_data[0] & BATTERY_CHARGING_TRICKLE_MASK) || (rx_data[0] & BATTERY_CHARGING_CC_MASK)
                                || (rx_data[0] & BATTERY_CHARGING_CV_MASK));

      if(status->charging_stopped_die_temp)
      {
         status->charging = false;
         status->charging_completed = false;
      }
      else if(status->charging_completed)
      {
         status->charging = false;
      }
      else if(status->charging)
      {
         status->charging_completed = false;
      }
      else
      {
         status->charging_completed = false;
         status->charging = false;
      }
   }

   // Check if the next vbat measurement is available
   IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_EVENTSADCSET, rx_data, 1u));
   if(IS_OK(result))
   {
      if(rx_data[0] & VBAT_MEASUREMENT_READY_MASK)
      {
         status->vbat_measurement_ready = true;
      }
      else
      {
         status->vbat_measurement_ready = false;
      }
   }

   /**
    * Check if any PMIC errors have occurred.
    * The below error checking checks all three error registers of the PMIC to see if an error has occurred. The
    * checking has been arranged according to priority. If multiple errors occur, only one will be reported at a time.
    * The error registers are cleared afterwards.
    */
   result_t pmic_error_result = RESULT_OK;

   // Check charger error event
   IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_CHARGERERRREASON_R, rx_data, 1u));
   if(IS_OK(result))
   {
      NPM1300_DRV_ERROR err = map_chargererrreason_to_enum(rx_data[0]);
      if(NPM1300_DRV_ERROR_NONE != err)
      {
         DEBUG_WARNING("PMIC driver: PMIC error detected: CHARGERERRREASON = %d", rx_data[0]);
         SET_ERR(pmic_error_result, err);
      }
   }

   // Check sensor error event
   IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_CHARGERERRSENSOR_R, rx_data, 1u));
   if(IS_OK(result))
   {
      NPM1300_DRV_ERROR err = map_chargererrsensor_to_enum(rx_data[0]);
      if(NPM1300_DRV_ERROR_NONE != err)
      {
         DEBUG_WARNING("PMIC driver: PMIC error detected: CHARGERERRSENSOR = %d", rx_data[0]);
         SET_ERR(pmic_error_result, err);
      }
   }

   // Check reset event
   IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_RSTCAUSE_R, rx_data, 1u));
   if(IS_OK(result))
   {
      NPM1300_DRV_ERROR err = map_rstcause_to_enum(rx_data[0]);
      if(NPM1300_DRV_ERROR_NONE != err)
      {
         DEBUG_WARNING("PMIC driver: PMIC error detected: RSTCAUSE = %d", rx_data[0]);
         SET_ERR(pmic_error_result, err);
         status->is_reset_detected = true;
      }
   }

   // Clear error flags, if any
   if(IS_ERR(pmic_error_result))
   {
      uint8_t tx_data[1] = {0x01};
      result = write_register(self, REG_TASKCLRERRLOG, tx_data, sizeof(uint8_t)); // Clear error flags
      IF_OK_RUN_AND_UPDATE(
         result, write_register(self, REG_TASKCLEARCHGERR_W, tx_data, sizeof(uint8_t))); // Clear charger error flags

      IF_OK_RUN_AND_UPDATE(
         result,
         write_register(
            self, REG_TASKCLEARSAFETYTIMER_W, tx_data, sizeof(uint8_t))); // Clear charger safety timer error flags

      IF_OK_RUN_AND_UPDATE(
         result, write_register(self, REG_TASKRELEASEERR_W, tx_data, sizeof(uint8_t))); // Allow Charger to charge again
   }

   // Get NTC status
   IF_OK_RUN_AND_UPDATE(result, read_register(self, REG_NTCSTATUS_R, rx_data, 1u));

   if(IS_OK(result))
   {
      bool ntc_cold = (bool)(rx_data[0] & NTC_COLD_MASK);
      bool ntc_cool = (bool)(rx_data[0] & NTC_COOL_MASK);
      bool ntc_warm = (bool)(rx_data[0] & NTC_WARM_MASK);
      bool ntc_hot = (bool)(rx_data[0] & NTC_HOT_MASK);

      if(ntc_cold)
      {
         status->ntc_status = PMIC_NTC_STATUS_COLD;
      }
      else if(ntc_cool)
      {
         status->ntc_status = PMIC_NTC_STATUS_COOL;
      }
      else if(ntc_warm)
      {
         status->ntc_status = PMIC_NTC_STATUS_WARM;
      }
      else if(ntc_hot)
      {
         status->ntc_status = PMIC_NTC_STATUS_HOT;
      }
      else
      {
         status->ntc_status = PMIC_NTC_STATUS_UNKNOWN;
      }
   }

   // If a PMIC error occurred, copy that error as the return result
   if(IS_ERR(pmic_error_result))
   {
      CLONE_ERR(result, pmic_error_result);
   }

   return result;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t npm1300_driver_init(npm1300_driver_t *const self,
                             const i2c_driver_interface_t *i2c_interface,
                             uint8_t device_address)
{
   RETURN_ERR_IF_NULL(self, NPM1300_DRV_ERROR_INIT);
   RETURN_ERR_IF_NULL(i2c_interface, NPM1300_DRV_ERROR_INIT);
   RETURN_ERR_IF_TRUE(device_address > I2C_ADDRESS_MAX, NPM1300_DRV_ERROR_INVALID_ADDRESS);

   self->_initialized = false;
   self->interface.parent = self;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.get_buck1_status = get_buck1_status;
   self->interface.set_buck1_status = set_buck1_status;
   self->interface.get_measurements = get_measurements;
   self->interface.get_pmic_status = get_pmic_status;
   self->interface.set_load_switch2 = set_switch2;
   self->interface.enter_ship_mode = set_shipmode;
   self->interface.get_load_switch2_status = get_switch2_status;
   self->interface.disable_auto_measurements = disable_auto_measurements;
   self->interface.enable_auto_measurements = enable_auto_measurements;
   self->interface.get_load_switch1_status = get_switch1_status;
   self->interface.set_load_switch1 = set_switch1;

   self->_i2c_interface = i2c_interface;
   self->_device_address = device_address;

   size_t num_rows = 0;

   // Init System monitor registers
   num_rows = (sizeof(system_monitor_register_configs) / sizeof(system_monitor_register_configs[0]));
   result_t result = init_driver_helper_function(self, num_rows, system_monitor_register_configs);

   if(IS_OK(result))
   {
      // System monitor config has been applied so auto measurements are now enabled
      self->_auto_enabled = true;

      // Init VBUSIN registers
      num_rows = (sizeof(vbusin_register_configs) / sizeof(vbusin_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, vbusin_register_configs));

      // Init charger registers
      num_rows = (sizeof(charger_register_configs) / sizeof(charger_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, charger_register_configs));

      // Init buck registers
      num_rows = (sizeof(buck_register_configs) / sizeof(buck_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, buck_register_configs));

      // Init Load switch / LDO registers
      num_rows = (sizeof(ldo_switch_register_configs) / sizeof(ldo_switch_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, ldo_switch_register_configs));

      // Init Load LED registers
      num_rows = (sizeof(led_register_configs) / sizeof(led_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, led_register_configs));

      // Init Load GPIO registers
      num_rows = (sizeof(gpio_register_configs) / sizeof(gpio_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, gpio_register_configs));

      // Init Load TIMER registers
      num_rows = (sizeof(timer_register_configs) / sizeof(timer_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, timer_register_configs));

      // Init Load SHIP mode registers
      num_rows = (sizeof(ship_register_configs) / sizeof(ship_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, ship_register_configs));

      // Init Load event interrupt registers
      num_rows = (sizeof(event_interrupt_register_configs) / sizeof(event_interrupt_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, event_interrupt_register_configs));

      // Clear event registers
      num_rows = (sizeof(clear_event_interrupt_registers) / sizeof(clear_event_interrupt_registers[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, clear_event_interrupt_registers));

      // Init Load POF registers
      num_rows = (sizeof(pof_register_configs) / sizeof(pof_register_configs[0]));
      IF_OK_RUN_AND_UPDATE(result, init_driver_helper_function(self, num_rows, pof_register_configs));

      // Disable boot monitor timer
      uint8_t tx_data[1] = {0x00}; // Clear error flags
      IF_OK_RUN_AND_UPDATE(result, write_register(self, REG_SCRATCH0, tx_data, sizeof(uint8_t)));
   }

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
