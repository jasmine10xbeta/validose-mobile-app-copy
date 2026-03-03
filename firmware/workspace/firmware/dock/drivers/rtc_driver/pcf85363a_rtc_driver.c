/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "nrf_delay.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"
#include "pcf85363a.h"
#include "rtc_system_time.h"

static const uint8_t THIS_UNIT_ID = SW_UNIT_ID_RTC_PCF85363A_DRIVER;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define TIME_PARAM_BUFFER_SIZE (8u) // 100th-sec sec, min, hour, day, weekday, month, year (since 2000)

#define DRIVER_CONFIG_INIT_WRITE_DELAY_MS (1u)

#define RTC_TIMEOUT_US (1000u)

#define NOT_LEAP_DIVISOR        (100u)
#define IS_LEAP_DIVISOR         (400u)
#define QUARTER_LEAP_DIVISOR    (4u)
#define DAYS_IN_YEAR_NON_LEAP   (365u)
#define DAYS_IN_YEAR_LEAP       (366u)
#define EPOCH_REFERENCE_YEAR    (1970u)
#define REFERENCE_YEAR          ((uint16_t)2000u)
#define EPOCH_REFERENCE_WEEKDAY (4u) // 4 = THURSDAY

#define BCD_TEN_DIVISOR   (10u)
#define BCD_SHIFT_BITS    (4u)
#define BCD_BITMASK_LOWER (0x0F)

// To check time bounds
#define SECONDS_AND_MINUTES_CHECK (59u)
#define HOURS_CHECK               (23u)
#define YEARS_CHECK               (99u) // from datasheet
#define DAYS_CHECK                (31u)
#define FEBRUARY_NUM_MONTH        (2u)

#define IS_LEAP_YEAR(year)                                                                                             \
   (((0 == (year) % QUARTER_LEAP_DIVISOR) && (0 != (year) % NOT_LEAP_DIVISOR)) || (0 == (year) % IS_LEAP_DIVISOR))

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface function
static result_t set_time_systime(const rtc_system_time_interface_t *const interface, const sys_time_t *time);
static result_t set_time_unix(const rtc_system_time_interface_t *const interface, const uint32_t unix_timestamp);
static result_t get_time_systime(const rtc_system_time_interface_t *const interface, sys_time_t *time);
static result_t get_time_unix(const rtc_system_time_interface_t *const interface, uint32_t *unix_time);

// Internal functions
/**
 * @brief Converts a decimal number to binary-coded decimal (BCD).
 *
 * @param value The decimal number to convert.
 * @return The BCD representation of the input decimal number.
 */
static uint8_t decimal_to_bcd(uint8_t value);

/**
 * @brief Converts a binary-coded decimal (BCD) number to a decimal number.
 *
 * @param value The BCD number to convert.
 * @return The decimal representation of the input BCD number.
 */
static uint8_t bcd_to_decimal(uint8_t value);

/**
 * @brief Writes data to the RTC device at the specified address.
 *
 * @param interface The interface to the RTC device.
 * @param write_address The address to write the data to.
 * @param data The data to write.
 * @param length The length of the data to write.
 * @return A result indicating the success or failure of the operation.
 */
static result_t rtc_write(const rtc_system_time_interface_t *const interface,
                          uint8_t write_address,
                          const uint8_t *data,
                          uint8_t length);

/**
 * @brief Reads data from the RTC device at the specified address.
 *
 * @param interface The interface to the RTC device.
 * @param read_address The address to read the data from.
 * @param rx_data The buffer to store the read data.
 * @param length The length of the data to read.
 * @return A result indicating the success or failure of the operation.
 */
static result_t
   rtc_read(const rtc_system_time_interface_t *const interface, uint8_t read_address, uint8_t *rx_data, uint8_t length);

/**
 * @brief Perform register read-modify-write-verify to update specific bits in a register.
 *
 * @param interface The interface to the RTC device.
 * @param update_address The address to read/write the data.
 * @param update_data The data to write as update.
 * @param update_mask The bit mask only preserving the relevant bits to update in `update_data`.
 * @param is_value_changed Set to `true` if the update changed the value of the register; `false` otherwise.
 * @return A result indicating the success or failure of the operation.
 */
static result_t update(const rtc_system_time_interface_t *const interface,
                       uint8_t update_address,
                       uint8_t update_data,
                       uint8_t update_mask,
                       bool *is_value_changed);

/**
 * @brief Check OS flag to determine if oscillator was stopped. Clear the OS flag if it was set.
 *
 * @param interface The interface to the RTC device.
 * @param is_osc_inactive Set to `true` if the OS flag was set; `false` otheriwse.
 */
static result_t check_and_clear_is_oscillator_stopped(const rtc_system_time_interface_t *const interface,
                                                      bool *is_osc_inactive);
/**
 * @brief Converts a RTC time to an epoch time.
 *
 * @param rtc_time The RTC time to convert.
 * @return The epoch time corresponding to the input RTC time.
 */
static uint32_t rtc_to_epoch(const sys_time_t *rtc_time);

/**
 * @brief Converts an epoch time to RTC time.
 *
 * @param unix_timestamp The epoch time to convert.
 * @return The RTC time corresponding to the input epoch time.
 */
static result_t epoch_to_rtc(const uint32_t unix_timestamp, sys_time_t *system_time);

/**
 * @brief Checks if the given RTC time is within valid bounds.
 *
 * @param[in] time  Pointer to the time structure to be checked.
 *
 * @return Result of the operation.
 */
static result_t check_time_bounds(const sys_time_t *time);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static const uint8_t m_days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// i2c interface shared by all driver instances
static const i2c_driver_interface_t *mp_i2c_driver_interface;

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static result_t check_time_bounds(const sys_time_t *time)
{
   result_t result = RESULT_OK;

   RETURN_ERR_IF_TRUE(time->seconds > SECONDS_AND_MINUTES_CHECK, RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);
   // Check minutes: 0-59
   RETURN_ERR_IF_TRUE(time->minutes > SECONDS_AND_MINUTES_CHECK, RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);
   // Check hours: 0-23
   RETURN_ERR_IF_TRUE(time->hours > HOURS_CHECK, RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);
   // Check day: 1-31
   RETURN_ERR_IF_TRUE((time->day < 1u) || (time->day > DAYS_CHECK), RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);
   // Check weekday
   RETURN_ERR_IF_TRUE(time->weekday >= SYS_TIME_WEEKDAY_MAX, RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);
   // Check month: 1-12
   RETURN_ERR_IF_TRUE((time->month < 1u) || (time->month > MAX_NUM_MONTHS), RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);
   // Check year: 0-99 (see datasheet)
   RETURN_ERR_IF_TRUE(time->year > YEARS_CHECK, RTC_SYSTEM_TIME_ERROR_OUT_OF_RANGE);

   return result;
}

static uint32_t rtc_to_epoch(const sys_time_t *rtc_time)
{
   RETURN_VALUE_IF_NULL(rtc_time, ERROR_RETURN_VALUE);

   // Calculate total days since 1970-01-01 to the beginning of the given year
   uint16_t year = REFERENCE_YEAR + rtc_time->year; // Convert year since 2000 to full year
   uint32_t days = 0;

   for(uint16_t year_counter = EPOCH_REFERENCE_YEAR; year_counter < year; year_counter++)
   {
      days += IS_LEAP_YEAR(year_counter) ? DAYS_IN_YEAR_LEAP : DAYS_IN_YEAR_NON_LEAP;
   }

   // Calculate days from the beginning of the current year to the current month
   for(uint8_t month_counter = 1u; month_counter < rtc_time->month; month_counter++)
   {
      days += m_days_in_month[month_counter - 1u]; // Add days for each month
      if((FEBRUARY_NUM_MONTH == month_counter) && (IS_LEAP_YEAR(year)))
      {
         days += 1u; // Add extra day for February in leap years
      }
   }

   // Add days for the current month
   days += rtc_time->day - 1u; // Subtract 1 because day starts from 1, not 0

   // Convert days to seconds and add hours, minutes, and seconds
   uint32_t unix_time = days * SECONDS_PER_DAY;
   unix_time += rtc_time->hours * SECONDS_PER_HOUR;
   unix_time += rtc_time->minutes * SECONDS_PER_MINUTE;
   unix_time += rtc_time->seconds;

   return unix_time;
}

static result_t epoch_to_rtc(const uint32_t unix_timestamp, sys_time_t *system_time)
{
   RETURN_ERR_IF_NULL(system_time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   uint32_t seconds = unix_timestamp;
   uint16_t year = EPOCH_REFERENCE_YEAR;
   uint8_t month = 1u; // calendar months are 1-based (Jan =1)

   // Step 1: Extract time of day
   system_time->seconds = (uint8_t)(seconds % SECONDS_PER_MINUTE); // num seconds past the most recent full minute
   seconds /= SECONDS_PER_MINUTE;                                  // convert remaining time to minutes
   system_time->minutes = (uint8_t)(seconds % SECONDS_PER_MINUTE); // num minutes past the most recent full hour
   seconds /= SECONDS_PER_MINUTE;                                  // convert remaining time to hours
   system_time->hours = (uint8_t)(seconds % HOURS_PER_DAY);        // num hours past midnight
   seconds /= HOURS_PER_DAY;                                       // convert remaining time to days
   /* @p seconds now holds the num full days since 1970-01-01
    use modulo 7 (days per week) to wrap around the week correctly */
   system_time->weekday = (uint8_t)((seconds + EPOCH_REFERENCE_WEEKDAY) % DAYS_PER_WEEK);

   // Step 2: Calculate date
   // Start from 1970

   while(true)
   {
      // Calculate how many days are in the current year
      uint16_t days_in_year = IS_LEAP_YEAR(year) ? DAYS_IN_YEAR_LEAP : DAYS_IN_YEAR_NON_LEAP;
      if(seconds >= days_in_year)
      {
         // if num days left > full year, subtract year's worth of days and add full year
         seconds -= days_in_year;
         year++;
      }
      else
      {
         // reamining days are less than a full year -> current year
         break;
      }
   }

   system_time->year = (uint8_t)(year - REFERENCE_YEAR);

   // @p seconds now holds the number of days into the current year

   // Step 3: Find month
   while(month < MAX_NUM_MONTHS)
   {
      uint8_t num_days = m_days_in_month[month - 1u];
      if((FEBRUARY_NUM_MONTH == month) && (IS_LEAP_YEAR(year)))
         num_days += 1u; // Feb in leap year

      // if remaining days > days in current month, subtract month's worth of days and add full month
      if(seconds >= num_days)
      {
         seconds -= num_days;
         month++;
      }
      else
      {
         // remaining days less than days in current month -> current month
         break;
      }
   }

   system_time->month = month;
   system_time->day = (uint8_t)(seconds + 1u); // +1: days are also 1-based (day 1 = 1, not 0)

   return result;
}

static uint8_t decimal_to_bcd(uint8_t value)
{
   return (uint8_t)(((uint8_t)(value / BCD_TEN_DIVISOR) << BCD_SHIFT_BITS) | ((uint8_t)(value % BCD_TEN_DIVISOR)));
}

static uint8_t bcd_to_decimal(uint8_t value)
{
   return (uint8_t)(((value >> BCD_SHIFT_BITS) * BCD_TEN_DIVISOR) + (value & BCD_BITMASK_LOWER));
}

static result_t rtc_write(const rtc_system_time_interface_t *const interface,
                          uint8_t write_address,
                          const uint8_t *data,
                          uint8_t length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   const rtc_system_time_t *self = interface->parent;

   uint8_t tx_data[length + 1u];
   tx_data[0] = write_address;

   for(uint8_t index = 0; index < length; index++)
   {
      tx_data[index + 1u] = data[index];
   }

   result_t result = mp_i2c_driver_interface->transmit(
      mp_i2c_driver_interface, self->_ic_i2c_address, tx_data, length + 1u, RTC_TIMEOUT_US);

   return result;
}

static result_t
   rtc_read(const rtc_system_time_interface_t *const interface, uint8_t read_address, uint8_t *rx_data, uint8_t length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(rx_data, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   const rtc_system_time_t *self = interface->parent;

   uint8_t tx_data[1u] = {read_address};

   result_t result
      = mp_i2c_driver_interface->transmit(mp_i2c_driver_interface, self->_ic_i2c_address, tx_data, 1u, RTC_TIMEOUT_US);

   IF_OK_RUN_AND_UPDATE(result,
                        mp_i2c_driver_interface->receive(
                           mp_i2c_driver_interface, self->_ic_i2c_address, rx_data, length, RTC_TIMEOUT_US));

   return result;
}

static result_t update(const rtc_system_time_interface_t *const interface,
                       uint8_t update_address,
                       uint8_t update_data,
                       uint8_t update_mask,
                       bool *is_value_changed)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_value_changed, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   uint8_t read_value = 0u;
   uint8_t write_value = 0u;
   uint8_t verify_value = 0u;
   *is_value_changed = false;

   result_t result = rtc_read(interface, update_address, &read_value, sizeof(read_value));

   if(IS_OK(result))
   {
      write_value = (uint8_t)(read_value & ~update_mask);
      write_value |= update_data & update_mask;

      if(write_value != read_value)
      {
         *is_value_changed = true;
         result = rtc_write(interface, update_address, &write_value, sizeof(write_value));
      }
   }

   if(IS_OK(result) && *is_value_changed)
   {
      verify_value = 0u;
      result = rtc_read(interface, update_address, &verify_value, sizeof(verify_value));

      if(IS_OK(result) && (verify_value != write_value))
      {
         SET_ERR(result, RTC_SYSTEM_TIME_ERROR_VERIFY_FAILED);
      }
   }

   return result;
}

static result_t check_and_clear_is_oscillator_stopped(const rtc_system_time_interface_t *const interface,
                                                      bool *is_osc_inactive)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_osc_inactive, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   return update(interface, PCF85363A_REG_SECONDS, 0x00, 0x80, is_osc_inactive);
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t set_time_systime(const rtc_system_time_interface_t *const interface, const sys_time_t *time)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   result_t result = check_time_bounds(time);
   uint8_t data[TIME_PARAM_BUFFER_SIZE + 2u];

   if(IS_OK(result))
   {
      // Single burst write from 0x2E:
      // [STOP=1] -> [CPR instruction] -> auto-rollover to 0x00 -> [100th sec..year]
      data[0u] = PCF85363A_CTRL_STOP_SET;
      data[1u] = PCF85363A_CTRL_RESET_CLEAR_PRESCALER;
      data[2u] = 0u; // 100th seconds not used in this implementation
      data[3u] = decimal_to_bcd(time->seconds);
      data[4u] = decimal_to_bcd(time->minutes);
      data[5u] = decimal_to_bcd(time->hours);
      data[6u] = decimal_to_bcd(time->day);
      data[7u] = decimal_to_bcd(time->weekday);
      data[8u] = decimal_to_bcd(time->month);
      data[9u] = decimal_to_bcd(time->year);
   }

   IF_OK_RUN_AND_UPDATE(result, rtc_write(interface, PCF85363A_CTRL_STOP_ADR, data, sizeof(data)));

   // Clear STOP bit
   uint8_t cmd = PCF85363A_CTRL_STOP_CLEAR;
   IF_OK_RUN_AND_UPDATE(result, rtc_write(interface, PCF85363A_CTRL_STOP_ADR, &cmd, 1u));

   return result;
}

static result_t set_time_unix(const rtc_system_time_interface_t *const interface, const uint32_t unix_timestamp)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   result_t result = epoch_to_rtc(unix_timestamp, &interface->parent->_system_time);

   IF_OK_RUN_AND_UPDATE(result, set_time_systime(interface, &interface->parent->_system_time));

   return result;
}

static result_t get_time_systime(const rtc_system_time_interface_t *const interface, sys_time_t *time)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   uint8_t data[TIME_PARAM_BUFFER_SIZE];
   memset(data, 0, TIME_PARAM_BUFFER_SIZE);

   // Verify whether oscillator was stopped since the previous call
   bool b_is_osc_inactive = false;
   result_t result = check_and_clear_is_oscillator_stopped(interface, &b_is_osc_inactive);

   if(b_is_osc_inactive)
   {
      SET_ERR(result, RTC_SYSTEM_TIME_ERROR_OSC_INACTIVE);
   }

   IF_OK_RUN_AND_UPDATE(result, rtc_read(interface, PCF85363A_REG_100TH_SECONDS, data, TIME_PARAM_BUFFER_SIZE));

   if(IS_OK(result))
   {
      // 100th seconds (data[0]) not used
      time->seconds = bcd_to_decimal(data[1u] & 0x7F); // Mask OS bit (bit 7)
      time->minutes = bcd_to_decimal(data[2u] & 0x7F); // Mask EMON bit (bit 7)
      time->hours = bcd_to_decimal(data[3u] & 0x3F);   // Mask bits 7 and 6
      time->day = bcd_to_decimal(data[4u] & 0x3F);     // Mask bits 7 and 6
      time->weekday = bcd_to_decimal(data[5u] & 0x07); // Mask bits 7 to 3
      time->month = bcd_to_decimal(data[6u] & 0x1F);   // Mask bits 7 to 5
      time->year = bcd_to_decimal(data[7u]);           // No masking needed
   }

   IF_OK_RUN_AND_UPDATE(result, check_time_bounds(time));

   return result;
}

static result_t get_time_unix(const rtc_system_time_interface_t *const interface, uint32_t *unix_time)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(unix_time, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   result_t result = get_time_systime(interface, &interface->parent->_system_time);

   if(IS_OK(result))
   {
      *unix_time = rtc_to_epoch(&interface->parent->_system_time);
      if(UINT32_MAX == *unix_time)
      {
         SET_ERR(result, RTC_SYSTEM_TIME_ERROR_PTR_NULL); // if rtc_time (time) has NULL value
      }
   }

   return result;
}
/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t rtc_system_time_init(rtc_system_time_t *const self,
                              uint8_t ic_i2c_address,
                              const i2c_driver_interface_t *i2c_interface,
                              sys_time_t system_time)
{
   result_t result = RESULT_OK;

   RETURN_ERR_IF_INTERFACE_NULL(i2c_interface, RTC_SYSTEM_TIME_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(self, RTC_SYSTEM_TIME_ERROR_PTR_NULL);

   self->_initialized = false;
   self->interface.parent = self;
   self->interface.set_time_systime = set_time_systime;
   self->interface.get_time_systime = get_time_systime;
   self->interface.get_time_unix = get_time_unix;
   self->interface.set_time_unix = set_time_unix;
   self->interface.sys_time_to_unix = rtc_to_epoch;
   self->interface.unix_to_sys_time = epoch_to_rtc;

   self->_ic_i2c_address = ic_i2c_address;
   self->_system_time = system_time;
   mp_i2c_driver_interface = i2c_interface;

   // Init RTC
   uint8_t config_commands[][2u] = {{PCF85363A_FUNC_CTRL_ADR, PCF85363A_FUNC_CTRL_EN_RTC_DIS_PI_DEF_FREQ},
                                    {PCF85363A_CTRL_OSC_ADR, PCF85363A_CTRL_OSC},
                                    {PCF85363A_CTRL_PIN_IO_ADR, PCF85363A_CTRL_PIN_IO},
                                    {PCF85363A_CTRL_INTA_EN_ADR, PCF85363A_CTRL_INT_EN_DISABLE_ALL_INT},
                                    {PCF85363A_CTRL_INTB_EN_ADR, PCF85363A_CTRL_INT_EN_DISABLE_ALL_INT},
                                    {PCF85363A_CTRL_BATT_ADR, PCF85363A_CTRL_BATT_DEFAULT}};

   uint8_t config_register_size = sizeof(config_commands) / sizeof(config_commands[0]);

   for(uint8_t index = 0; index < config_register_size; index++)
   {
      result = rtc_write(
         &self->interface, config_commands[index][0], &config_commands[index][1u], sizeof(config_commands[index][1u]));

      UPDATE_ERR(result, RTC_SYSTEM_TIME_ERROR_I2C);
      BREAK_ON_ERR(result);

      if(IS_OK(result))
      {
         nrf_delay_ms(DRIVER_CONFIG_INIT_WRITE_DELAY_MS);
      }
   }

   // Clear Oscillator Stopped (OS) flag
   bool is_oscillator_stopped;
   IF_OK_RUN_AND_UPDATE(result, check_and_clear_is_oscillator_stopped(&self->interface, &is_oscillator_stopped));

   UPDATE_ERR(result, RTC_SYSTEM_TIME_ERROR_OSC_INACTIVE);

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
