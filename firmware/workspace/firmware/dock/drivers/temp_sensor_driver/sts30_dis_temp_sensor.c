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
#include "nordic_common.h"
#include "nrf.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "sts30_dis_temp_sensor.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_TEMP_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MSB_TOP_BIT        (0x80u)
#define STALE_AGAIN_FACTOR (1.5f)

// Celsius conversion constants (datasheet): T[°C] = -45 + 175 * raw / 65535
#define STS30_TEMP_OFFSET_DC   (int32_t)(-450)  // -45.0°C in deci-degC
#define STS30_TEMP_SCALE_DC    (int32_t)(1750)  // 175.0°C in deci-degC
#define STS30_RAW_DENOMINATOR  (int32_t)(65535) // (2^16 - 1)
#define STS30_RAW_DIV_ROUNDING (int32_t)(32767) // (STS30_RAW_DENOMINATOR / 2) for rounding

// Fahrenheit conversion derived from Celsius formula:
// T[°F] = T[°C] * 9/5 + 32
#define STS30_DECI_FAHRENHEIT_NUM    (9u)
#define STS30_DECI_FAHRENHEIT_DEN    (5u)
#define STS30_DECI_FAHRENHEIT_OFFSET (320u)

#define STS30_TEMP_DATA_SIZE_BYTES (3u) // 2 bytes data + 1 byte CRC

#define I2C_TIMEOUT_US (100u)

// Macro to return error if STS30 is not ready
#define RETURN_ERR_IF_STS30_NOT_READY(self, err_code)                                                                  \
   do                                                                                                                  \
   {                                                                                                                   \
      bool ready                                                                                                       \
         = ((self)->_reset_timer_elapsed && (self)->_command_timer_elapsed && (self)->_temp_meas_timer_elapsed);       \
      if(!ready)                                                                                                       \
      {                                                                                                                \
         RETURN_ERR((err_code));                                                                                       \
      }                                                                                                                \
   } while(0)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t soft_reset(const sts30_dis_temp_sensor_interface_t *const interface);
static result_t
   get_temperature(const sts30_dis_temp_sensor_interface_t *const interface, int16_t *temp_decidegc, bool *is_stale);
static result_t set_heater(const sts30_dis_temp_sensor_interface_t *const interface, bool enable);
static result_t temp_sensor_process(const sts30_dis_temp_sensor_interface_t *const interface,
                                    uint64_t temp_read_freq_ms);

// Non-interface functions
/// Internal functions
static result_t start_single_shot(sts30_dis_temp_sensor_driver_t *const self, STS30_REP rep_val, STS30_CS cs_val);
static result_t write_cmd(sts30_dis_temp_sensor_driver_t *const self, uint16_t cmd);
static result_t read_word(sts30_dis_temp_sensor_driver_t *const self, uint16_t *word);
static result_t read_temp(sts30_dis_temp_sensor_driver_t *const self, int16_t *temp_decidegC);
static result_t command_timer_start(sts30_dis_temp_sensor_driver_t *const self);
static result_t reset_timer_start(sts30_dis_temp_sensor_driver_t *const self, uint32_t delay_ms);
static result_t temp_tmeas_timer_start(sts30_dis_temp_sensor_driver_t *const self, uint32_t delay_ms);
// Helper functions
static result_t convert_temp_decidegc(uint16_t raw, int16_t *temp_decidegc);
static result_t get_single_shot_cmd(STS30_REP rep_val, STS30_CS cs_val, uint16_t *cmd_value);
static result_t get_tmeas_max_ms(STS30_REP rep_val, uint8_t *ms_value);
static uint8_t get_crc8_2bytes(uint8_t data_MSB, uint8_t data_LSB);
/// Interrupt handlers
static void reset_timer_handler(void *p_context);
static void command_timer_handler(void *p_context);
static void temp_tmeas_timer_handler(void *p_context);
static result_t execute_temp_tmeas_completion(sts30_dis_temp_sensor_driver_t *const self);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
typedef uint32_t m_ret_code_t;
/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
/**
 * @brief Starts the command delay timer for the STS30 driver.
 *
 * This function starts a timer that enforces the minimum required delay between I2C commands
 * to the STS30 sensor, as specified in the datasheet. It is used to ensure proper timing
 * between sensor operations.
 *
 * @param self Pointer to the driver instance.
 * @return RESULT_OK on success, or error code on failure.
 */
static result_t command_timer_start(sts30_dis_temp_sensor_driver_t *const self)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;

   m_ret_code_t err_code = app_timer_start(self->_command_delay_timer.timer_id,
                                           APP_TIMER_TICKS(STS30_COMMAND_DELAY_MS),
                                           &self->_command_delay_timer.temp_timer_context);

   UPDATE_IF_NRF_ERR(err_code, result, STS30_DIS_TEMP_SENSOR_ERROR_TIMER);

   return result;
}

/**
 * @brief Starts the reset recovery timer for the STS30 driver.
 *
 * This function starts a timer that enforces the required recovery delay after a sensor reset.
 * The delay duration is specified in milliseconds.
 *
 * @param self Pointer to the driver instance.
 * @param delay_ms The delay in milliseconds to wait after reset.
 * @return RESULT_OK on success, or error code on failure.
 */
static result_t reset_timer_start(sts30_dis_temp_sensor_driver_t *const self, uint32_t delay_ms)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(0u == delay_ms, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);

   result_t result = RESULT_OK;

   m_ret_code_t err_code = app_timer_start(
      self->_reset_delay_timer.timer_id, APP_TIMER_TICKS(delay_ms), &self->_reset_delay_timer.temp_timer_context);

   UPDATE_IF_NRF_ERR(err_code, result, STS30_DIS_TEMP_SENSOR_ERROR_TIMER);

   return result;
}

/**
 * @brief Starts the temperature measurement timer for the STS30 driver.
 *
 * This function starts a timer that waits for the maximum measurement time required by the sensor
 * before reading the temperature result. The delay duration is specified in milliseconds.
 *
 * @param self Pointer to the driver instance.
 * @param delay_ms The measurement delay in milliseconds.
 * @return RESULT_OK on success, or error code on failure.
 */
static result_t temp_tmeas_timer_start(sts30_dis_temp_sensor_driver_t *const self, uint32_t delay_ms)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(0u == delay_ms, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);

   result_t result = RESULT_OK;

   m_ret_code_t err_code = app_timer_start(self->_temp_tmeas_delay_timer.timer_id,
                                           APP_TIMER_TICKS(delay_ms),
                                           &self->_temp_tmeas_delay_timer.temp_timer_context);

   UPDATE_IF_NRF_ERR(err_code, result, STS30_DIS_TEMP_SENSOR_ERROR_TIMER);

   return result;
}

/**
 * @brief Timer handler for reset recovery completion.
 *
 * This function is called when the reset recovery timer elapses. It marks the driver as ready
 * to proceed with further operations after a reset.
 *
 * @param p_context Pointer to the timer context (should contain driver instance).
 */
static void reset_timer_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);
   timer_context_t *context = (timer_context_t *)p_context;
   sts30_dis_temp_sensor_driver_t *self = context->driver_instance;

   self->_reset_timer_elapsed = true;
}

/**
 * @brief Timer handler for temperature measurement completion.
 *
 * This function is called when the temperature measurement timer elapses. It updates the driver's state to indicate
 * that the measurement time has completed, allowing the driver to proceed with reading the temperature result from the
 * sensor. If the temperature result still is not ready, the driver will handle retries as needed.
 *
 * @param p_context Pointer to the timer context (should contain driver instance).
 */
static void temp_tmeas_timer_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);
   timer_context_t *context = (timer_context_t *)p_context;
   sts30_dis_temp_sensor_driver_t *self = context->driver_instance;

   self->_temp_meas_timer_elapsed = true;
}

/**
 * @brief Timer handler for command delay completion.
 *
 * This function is called when the command delay timer elapses. It updates the driver's state to
 * indicate that the command delay has completed.
 *
 * @param p_context Pointer to the timer context (should contain driver instance).
 */
static void command_timer_handler(void *p_context)
{
   RETURN_VOID_IF_NULL(p_context);
   timer_context_t *context = (timer_context_t *)p_context;
   sts30_dis_temp_sensor_driver_t *self = context->driver_instance;

   self->_command_timer_elapsed = true;
   self->_is_busy_command = false;  // allow new commands now that delay has elapsed
   self->_command_num_retries = 0u; // reset command retry count on successful delay completion
}

/**
 * @brief Returns the maximum measurement time for a given repeatability setting.
 *
 * This function provides the maximum time, in milliseconds, required for a temperature measurement
 * based on the specified repeatability (low, medium, or high) as defined by the STS30 sensor datasheet.
 *
 * @param rep_val The repeatability setting (STS30_REP_LOW, STS30_REP_MEDIUM, or STS30_REP_HIGH).
 * @return Maximum measurement time in milliseconds for the given repeatability.
 */
static result_t get_tmeas_max_ms(STS30_REP rep_val, uint8_t *ms_value)
{
   RETURN_ERR_IF_NULL(ms_value, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);

   result_t result = RESULT_OK;
   switch(rep_val)
   {
      case STS30_REP_HIGH:
         *ms_value = STS30_TMEAS_MAX_HIGH_MS;
         break;
      case STS30_REP_MEDIUM:
         *ms_value = STS30_TMEAS_MAX_MED_MS;
         break;
      case STS30_REP_LOW:
         *ms_value = STS30_TMEAS_MAX_LOW_MS;
         break;
      case STS30_REP_MAX:
      default:
         DEBUG_ERROR("Invalid repeatability value.");
         SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);
         break;
   }

   return result;
}

/**
 * @brief Calculates the CRC-8 checksum for two data bytes.
 *
 * This function computes the CRC-8 checksum for exactly two data bytes, as used by the STS30 sensor.
 * The sensor sends temperature as 2 data bytes followed by 1 CRC byte. The driver recomputes
 * the CRC over the 2 received bytes and compares it to the third CRC byte from the sensor to detect
 * corrupted I²C data.
 *
 * @param data Pointer to the two data bytes.
 * @return The computed CRC-8 checksum.
 */
static uint8_t get_crc8_2bytes(uint8_t data_MSB, uint8_t data_LSB)
{
   uint8_t crc = STS30_CRC8_INIT;
   uint8_t data[CRC_BUFFER_SIZE] = {data_MSB, data_LSB};

   for(uint32_t count = 0u; count < CRC_BUFFER_SIZE; count++) // for each byte
   {
      crc ^= data[count]; // mix in the next byte

      for(uint32_t bit = 0u; bit < COMMON_BITS_PER_BYTE; bit++) // for each bit
      {
         if((crc & MSB_TOP_BIT) != 0u) // if MSB is 1..
         {
            crc = (uint8_t)((crc << 1u) ^ STS30_CRC8_POLY); // shift + XOR polynomial (0x31)
         }
         else
         {
            crc = (uint8_t)(crc << 1u); // just shift
         }
      }
   }

   return crc;
}

/**
 * @brief Returns the command code for single-shot measurement mode.
 *
 * This function selects the appropriate command code for the STS30 sensor to start a single-shot
 * temperature measurement, based on the specified repeatability and clock stretching settings.
 *
 * @param rep_val The repeatability setting (STS30_REP_LOW, STS30_REP_MEDIUM, or STS30_REP_HIGH).
 * @param cs_val The clock stretching setting (STS30_CS_ENABLED or STS30_CS_DISABLED).
 * @return The command code to send to the sensor for the given settings.
 */
static result_t get_single_shot_cmd(STS30_REP rep_val, STS30_CS cs_val, uint16_t *cmd_value)
{
   RETURN_ERR_IF_NULL(cmd_value, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   result_t result = RESULT_OK;

   switch(cs_val)
   {
      case STS30_CS_ENABLED:
         switch(rep_val)
         {
            case STS30_REP_HIGH:
               *cmd_value = CMD_SS_HIGH_CS_EN;
               break;
            case STS30_REP_MEDIUM:
               *cmd_value = CMD_SS_MED_CS_EN;
               break;
            case STS30_REP_LOW:
               *cmd_value = CMD_SS_LOW_CS_EN;
               break;
            case STS30_REP_MAX:
            default:
               DEBUG_ERROR("Invalid repeatability value.");
               SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);
               break;
         }
         break;

      case STS30_CS_DISABLED:
         switch(rep_val)
         {
            case STS30_REP_HIGH:
               *cmd_value = CMD_SS_HIGH_CS_DIS;
               break;
            case STS30_REP_MEDIUM:
               *cmd_value = CMD_SS_MED_CS_DIS;
               break;
            case STS30_REP_LOW:
               *cmd_value = CMD_SS_LOW_CS_DIS;
               break;
            case STS30_REP_MAX:
            default:
               DEBUG_ERROR("Invalid repeatability value.");
               SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);
               break;
         }
         break;

      case STS30_CS_MAX:
      default:
         DEBUG_ERROR("Invalid clock stretching value.");
         SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);
         break;
   }

   return result;
}

/**
 * @brief Converts raw sensor value to temperature in deci-degrees Celsius.
 *
 * This function applies the STS30 datasheet formula to convert a raw 16-bit sensor value
 * to a temperature in deci-degrees Celsius (0.1°C units).
 *
 * Example: 253 means 25.3°C, -45 means -4.5°C.
 *
 * Formula from datasheet: T = -45 + 175 * (SR / 2^16-1)
 * T[d°C] = STS30_TEMP_OFFSET_DC + (STS30_TEMP_SCALE_DC * raw) / STS30_RAW_DENOMINATOR
 *
 * @note Fahrenheit conversion is not implemented, as the project only requires Celsius.
 *
 * @param raw The raw 16-bit sensor value.
 * @param temp_decidegc Output: temperature in deci-degrees Celsius.
 * @return RESULT_OK on success, or error code on failure.
 */
static result_t convert_temp_decidegc(uint16_t raw, int16_t *temp_decidegc)
{
   RETURN_ERR_IF_NULL(temp_decidegc, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);

   int32_t num = STS30_TEMP_SCALE_DC * (int32_t)raw;
   int32_t frac = ((num + STS30_RAW_DIV_ROUNDING) / STS30_RAW_DENOMINATOR);
   int32_t temp_dc = STS30_TEMP_OFFSET_DC + frac;

   // Clamp to int16_t bounds to prevent overflow/underflow
   if(temp_dc > INT16_MAX)
   {
      temp_dc = INT16_MAX;
   }
   else if(temp_dc < INT16_MIN)
   {
      temp_dc = INT16_MIN;
   }

   *temp_decidegc = (int16_t)temp_dc;

   return RESULT_OK;
}

/**
 * @brief Sends a command to the STS30 sensor over I2C.
 *
 * This function transmits a 16-bit command to the STS30 temperature sensor using the configured I2C interface.
 * The command is split into two bytes (MSB first) and sent to the sensor's I2C address.
 *
 * @param self Pointer to the driver instance.
 * @param cmd The 16-bit command to send to the sensor.
 * @return RESULT_OK on success, or error code on failure.
 */
static result_t write_cmd(sts30_dis_temp_sensor_driver_t *const self, uint16_t cmd)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_STS30_NOT_READY(self, STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET);

   uint8_t cmd_buffer[COMMAND_SIZE_BYTES] = {0u};

   cmd_buffer[0u] = (uint8_t)(cmd >> 8u);   // MSB
   cmd_buffer[1u] = (uint8_t)(cmd & 0xFFu); // LSB

   result_t result = self->_i2c_interface->transmit(
      self->_i2c_interface, self->_i2c_address, cmd_buffer, COMMAND_SIZE_BYTES, I2C_TIMEOUT_US);

   if(IS_OK(result))
   {
      // Initiate command gap delay
      result = command_timer_start(self);
      if(IS_OK(result))
      {
         self->_command_timer_elapsed = false;
      }
   }
   else if((TWI_DRV_ERROR_ANAK == GET_ERR_CODE(result)) && (SW_UNIT_ID_TEMP_DRV == GET_ERR_UNIT(result))
           && (self->_command_num_retries < MAX_NUM_COMMAND_RETRIES))
   {
      // NACK can occur if the sensor is not ready to be read from after measurement time - catch and release
      // error
      result = RESULT_OK;
      DEBUG_TRACE("Temp not ready, retry on next read attempt.");
      self->_command_num_retries++;
      self->_is_busy_command = true; // keep as busy to prevent new measurement attempts until we can read the
                                     // result or reach max retries
   }
   else if((TWI_DRV_ERROR_ANAK == GET_ERR_CODE(result)) && (SW_UNIT_ID_TEMP_DRV == GET_ERR_UNIT(result))
           && (self->_command_num_retries >= MAX_NUM_COMMAND_RETRIES))
   {
      // Max retries reached, handle error
      DEBUG_WARNING("Max command retries reached. Failed to send command.");
      self->_command_num_retries = 0u;
      self->_is_busy_command = false; // allow new command attempts, but log that we failed to send this command
   }
   else
   {
      SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_I2C_ERROR);
      DEBUG_ERROR("Failed to send command. Error: %d", result);
   }

   return result;
}

/**
 * @brief Reads a 16-bit word from the STS30 sensor over I2C.
 *
 * This function receives two data bytes and a CRC byte from the STS30 sensor, verifies the CRC,
 * and combines the data bytes into a 16-bit word. If the CRC check fails, an error is returned.
 *
 * @param self Pointer to the driver instance.
 * @param word Output: pointer to store the received 16-bit word.
 * @return RESULT_OK on success, or error code on failure (including CRC mismatch).
 */
static result_t read_word(sts30_dis_temp_sensor_driver_t *const self, uint16_t *word)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(word, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_STS30_NOT_READY(self, STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET);

   uint8_t read_buffer[STS30_TEMP_DATA_SIZE_BYTES] = {0u};

   result_t result = self->_i2c_interface->receive(
      self->_i2c_interface, self->_i2c_address, read_buffer, STS30_TEMP_DATA_SIZE_BYTES, I2C_TIMEOUT_US);

   if(IS_OK(result))
   {
      uint8_t received_crc = read_buffer[2u];
      uint8_t computed_crc = get_crc8_2bytes(read_buffer[0u], read_buffer[1u]);
      if(received_crc != computed_crc)
      {
         SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_CRC_MISMATCH);
      }
      else
      {
         uint16_t msb = (uint16_t)read_buffer[0u];
         uint16_t lsb = (uint16_t)read_buffer[1u];
         *word = (uint16_t)((msb << 8u) | lsb);
      }
   }

   return result;
}

/**
 * @brief Reads and converts the temperature from the STS30 sensor.
 *
 * This function reads a raw 16-bit temperature value from the STS30 sensor over I2C,
 * then converts it to deci-degrees Celsius (0.1°C units) using the datasheet formula.
 *
 * Example: If the sensor returns a raw value corresponding to 25.3°C, the output will be 253.
 *
 * @param self Pointer to the driver instance.
 * @param temp_decidegC Output: pointer to store the temperature in deci-degrees Celsius.
 * @return RESULT_OK on success, or error code on failure (including CRC mismatch or delay not met).
 */
static result_t read_temp(sts30_dis_temp_sensor_driver_t *const self, int16_t *temp_decidegC)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(temp_decidegC, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);

   uint16_t raw_temp = 0;
   int16_t temp_decidegc_get = 0;

   result_t result = read_word(self, &raw_temp);

   IF_OK_RUN_AND_UPDATE(result, convert_temp_decidegc(raw_temp, &temp_decidegc_get));

   if(IS_OK(result))
   {
      *temp_decidegC = temp_decidegc_get;
   }

   return result;
}

static result_t execute_temp_tmeas_completion(sts30_dis_temp_sensor_driver_t *const self)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(!self->_command_timer_elapsed, STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET);

   result_t result = RESULT_OK;
   int16_t temp = 0;
   uint64_t time_ms = 0u;
   system_time_interface_t const *systick_interface = self->_system_time_interface;

   result = systick_interface->get_time_ms(systick_interface, &time_ms);

   IF_OK_RUN_AND_UPDATE(result, read_temp(self, &temp));
   if(IS_OK(result))
   {
      self->_ss_current_temp_decidegC = temp;
      self->_ss_is_temp_measurement_stale = false;
      self->_last_temp_read_time_ms = time_ms;
      self->_read_temp_num_retries = 0u;
      self->_is_busy_measuring = false;
   }
   else if((TWI_DRV_ERROR_ANAK == GET_ERR_CODE(result)) && (SW_UNIT_ID_TEMP_DRV == GET_ERR_UNIT(result))
           && (self->_read_temp_num_retries < MAX_NUM_TEMP_READ_RETRIES))
   {
      // NACK can occur if the sensor is not ready to be read from after measurement time - catch and release
      // error
      CLEAR_ERR(result);
      DEBUG_TRACE("Temp not ready, retry on next read attempt.");
      self->_read_temp_num_retries++;
      self->_is_busy_measuring = true; // keep as busy to prevent new measurement attempts until we can read the
                                       // result or reach max retries
   }
   else if((TWI_DRV_ERROR_ANAK == GET_ERR_CODE(result)) && (SW_UNIT_ID_TEMP_DRV == GET_ERR_UNIT(result))
           && (self->_read_temp_num_retries >= MAX_NUM_TEMP_READ_RETRIES))
   {
      // Max retries reached, handle error
      DEBUG_WARNING("Max temp read retries reached. Failed to read temperature.");
      self->_read_temp_num_retries = 0u;
      self->_is_busy_measuring
         = false; // allow new measurement attempts, but log that we failed to read this measurement
   }
   else if(IS_ERR(result) && (TWI_DRV_ERROR_ANAK != GET_ERR_CODE(result)))
   {
      // Any non-NACK error: do NOT stay stuck busy-measuring forever.
      // Allow the process loop to start a fresh measurement next cycle.
      self->_read_temp_num_retries = 0u;
      self->_is_busy_measuring = false;

      SET_ERR(result, STS30_DIS_TEMP_SENSOR_ERROR_I2C_ERROR);
      DEBUG_ERROR("Failed to read temperature. Error: %d", GET_ERR_CODE(result));
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t temp_sensor_process(const sts30_dis_temp_sensor_interface_t *const interface,
                                    uint64_t temp_read_freq_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(temp_read_freq_ms < STS30_TMEAS_MAX_HIGH_MS, STS30_DIS_TEMP_SENSOR_ERROR_INVALID_ARGUMENT);

   sts30_dis_temp_sensor_driver_t *self = interface->parent;
   system_time_interface_t const *systick_interface = self->_system_time_interface;
   uint64_t time_ms = 0u;
   uint64_t elapsed_ms = 0u;
   bool elapsed = false;

   result_t result = systick_interface->get_time_ms(systick_interface, &time_ms);

   if(self->_is_first_read)
   {
      IF_OK_RUN_AND_UPDATE(result, start_single_shot(self, STS30_REP_LOW, STS30_CS_DISABLED));
      if(IS_OK(result))
      {
         self->_is_first_read = false;
      }
   }
   else
   {
      if(IS_OK(result))
      {
         elapsed_ms = time_ms - self->_last_temp_read_time_ms;
         elapsed = (elapsed_ms >= temp_read_freq_ms);
      }

      // Only attempt to read a measurement result if a measurement actually started
      // and the t_meas timer has elapsed.
      if((self->_is_busy_measuring) && (self->_temp_meas_timer_elapsed))
      {
         IF_OK_RUN_AND_UPDATE(result, execute_temp_tmeas_completion(self));
      }

      if((elapsed) && (!self->_is_busy_measuring))
      {
         // Time to update temperature reading
         // Use low repeatability, clock stretching disabled for fastest single-shot measurement and lowest power usage
         // must be in single-shot mode to read on demand - used for this implementation
         IF_OK_RUN_AND_UPDATE(result, start_single_shot(self, STS30_REP_LOW, STS30_CS_DISABLED));
      }
   }

   if((STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET == GET_ERR_CODE(result))
      && (SW_UNIT_ID_TEMP_DRV == GET_ERR_UNIT(result)))
   {
      // Not ready for next measurement yet - this is expected to happen often, so just clear error and continue
      CLEAR_ERR(result);
   }

   return result;
}

static result_t soft_reset(const sts30_dis_temp_sensor_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_STS30_NOT_READY(interface->parent, STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET);

   sts30_dis_temp_sensor_driver_t *self = interface->parent;
   result_t result = write_cmd(self, CMD_SOFT_RESET);

   if(IS_OK(result))
   {
      // Datasheet: wait at least 1ms after soft reset
      result = reset_timer_start(self, STS30_RESET_RECOVERY_MS);
      if(IS_OK(result))
      {
         self->_reset_timer_elapsed = false;
      }
   }

   return result;
}

/**
 * @brief Starts a single-shot temperature measurement.
 *
 * @param interface Pointer to the STS30 interface.
 * @param rep_val Repeatability setting (STS30_REP_LOW, _MEDIUM, _HIGH).
 * @param cs_val Clock stretching setting (STS30_CS_ENABLED or _DISABLED).
 * @return result_t indicating success or failure.
 */
static result_t start_single_shot(sts30_dis_temp_sensor_driver_t *const self, STS30_REP rep_val, STS30_CS cs_val)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!self->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_STS30_NOT_READY(self, STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET);

   uint16_t cmd = 0u;
   result_t result = get_single_shot_cmd(rep_val, cs_val, &cmd);

   IF_OK_RUN_AND_UPDATE(result, write_cmd(self, cmd));

   if(IS_OK(result))
   {
      uint8_t tmeas_max_ms = 0u;
      IF_OK_RUN_AND_UPDATE(result, get_tmeas_max_ms(rep_val, &tmeas_max_ms));
      IF_OK_RUN_AND_UPDATE(result, temp_tmeas_timer_start(self, tmeas_max_ms));
      if(IS_OK(result))
      {
         // Successfully started timer for measurement time
         // Measurement command accepted -> now busy measuring until the result is successfully read (or given up)
         self->_is_busy_measuring = true;
         self->_read_temp_num_retries = 0u;

         // Wait for measurement time
         self->_ss_is_temp_measurement_stale = true;
         self->_temp_meas_timer_elapsed = false;
      }
   }

   return result;
}

static result_t
   get_temperature(const sts30_dis_temp_sensor_interface_t *const interface, int16_t *temp_decidegc, bool *is_stale)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(temp_decidegc, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_stale, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);

   sts30_dis_temp_sensor_driver_t *self = interface->parent;

   *temp_decidegc = self->_ss_current_temp_decidegC;
   *is_stale = self->_ss_is_temp_measurement_stale;

   return RESULT_OK;
}

static result_t set_heater(const sts30_dis_temp_sensor_interface_t *const interface, bool enable)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(!interface->parent->_initialized, STS30_DIS_TEMP_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_STS30_NOT_READY(interface->parent, STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET);

   sts30_dis_temp_sensor_driver_t *self = interface->parent;
   uint16_t cmd = enable ? CMD_HEATER_ENABLE : CMD_HEATER_DISABLE;

   result_t result = write_cmd(self, cmd);

   if((STS30_DIS_TEMP_SENSOR_ERROR_DELAY_NOT_MET == GET_ERR_CODE(result))
      && (SW_UNIT_ID_TEMP_DRV == GET_ERR_UNIT(result)))
   {
      // Not ready for next measurement yet - this is expected to happen often, so just clear error and continue
      DEBUG_WARNING("STS30 not ready for heater command, likely due to measurement in progress.");
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t sts30_dis_temp_sensor_init(sts30_dis_temp_sensor_driver_t *const self,
                                    const i2c_driver_interface_t *i2c_interface,
                                    const system_time_interface_t *system_time_interface,
                                    uint8_t i2c_address)
{
   RETURN_ERR_IF_NULL(self, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_INTERFACE_NULL(i2c_interface, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_INTERFACE_NULL(system_time_interface, STS30_DIS_TEMP_SENSOR_ERROR_NULL_POINTER);

   result_t result = RESULT_OK;

   self->_initialized = false;
   self->interface.parent = self;

   self->interface.get_temperature = get_temperature;
   self->interface.set_heater = set_heater;
   self->interface.soft_reset = soft_reset;
   self->interface.temp_sensor_process = temp_sensor_process;

   self->_i2c_interface = i2c_interface;
   self->_system_time_interface = system_time_interface;
   self->_i2c_address = i2c_address;
   self->_is_busy_measuring
      = true; // start as busy to prevent operations until first measurement is done and we have a valid temp reading
   self->_is_busy_command = false;
   self->_command_num_retries = 0u;
   self->_is_first_read = true;
   self->_reset_timer_elapsed = true;
   self->_command_timer_elapsed = true;
   self->_temp_meas_timer_elapsed = true;
   self->_ss_current_temp_decidegC = 0;
   self->_read_temp_num_retries = 0u;
   self->_ss_is_temp_measurement_stale = false;
   self->_last_temp_read_time_ms = 0u;

   self->_reset_delay_timer.temp_timer_context.driver_instance = self->interface.parent;
   self->_command_delay_timer.temp_timer_context.driver_instance = self->interface.parent;
   self->_temp_tmeas_delay_timer.temp_timer_context.driver_instance = self->interface.parent;

   self->_reset_delay_timer.timer_id = &self->_reset_delay_timer.timer_storage;
   self->_command_delay_timer.timer_id = &self->_command_delay_timer.timer_storage;
   self->_temp_tmeas_delay_timer.timer_id = &self->_temp_tmeas_delay_timer.timer_storage;

   m_ret_code_t error_code
      = app_timer_create(&self->_reset_delay_timer.timer_id, APP_TIMER_MODE_SINGLE_SHOT, reset_timer_handler);
   UPDATE_IF_NRF_ERR(error_code, result, STS30_DIS_TEMP_SENSOR_ERROR_TIMER);

   if(IS_OK(result))
   {
      error_code
         = app_timer_create(&self->_command_delay_timer.timer_id, APP_TIMER_MODE_SINGLE_SHOT, command_timer_handler);
      UPDATE_IF_NRF_ERR(error_code, result, STS30_DIS_TEMP_SENSOR_ERROR_TIMER);
   }

   if(IS_OK(result))
   {
      error_code = app_timer_create(
         &self->_temp_tmeas_delay_timer.timer_id, APP_TIMER_MODE_SINGLE_SHOT, temp_tmeas_timer_handler);
      UPDATE_IF_NRF_ERR(error_code, result, STS30_DIS_TEMP_SENSOR_ERROR_TIMER);
   }

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}