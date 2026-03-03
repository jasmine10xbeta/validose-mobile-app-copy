/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @brief Test File for Dock RTC Driver Using Mock I2C Driver with Updated Error Handling
 * This file contains unit tests for the RTC driver, utilizing the mocked I2C driver.
 *
 * The tests are written using the Google Test framework to validate the RTC driver's functionality
 * under controlled conditions provided by the mock I2C driver. Error handling is aligned with result.h.
 * Tests included:
 * Test functioning of interface functions set_time_epoch & get_time_epoch to test UNIX <-> sys_time_t conversions
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <cstdio>
#include <gtest/gtest.h>

extern "C"
{
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"
#include "i2c_driver_mock.h" // Mock interface for the I2C driver
#include "nordic_common.h"
#include "pcf85363a.h"
#include "result.h"
#include "rtc_system_time.h"
#include "rtc_system_time_interface.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define THIS_UNIT_ID     SW_UNIT_ID_RTC_PCF85363A_DRIVER // Define unit ID for tagging errors
#define MOCK_I2C_ADDRESS (0x51)                          // TODO: Get real I2C address of RTC driver

#define LEN_TX_DATA_SYSTIME    (9u)
#define LEN_RX_DATA_SYSTIME    (8u)
#define LEN_TX_DATA_FOR_RX     (1u)
#define LEN_TX_DATA_REG_INIT   (2u)
#define RTC_DRV_REGISTER_COUNT (165u) // Largest register address is A4 (164u)

// 2020-01-01 00:00:00 UTC (Wednesday)
#define TEST_TIME_SET_SECONDS (0x00)
#define TEST_TIME_SET_MINUTES (0x00)
#define TEST_TIME_SET_HOUR    (0x00)
#define TEST_TIME_SET_DAY     (0x01)
#define TEST_TIME_SET_WEEKDAY (0x03)
#define TEST_TIME_SET_MONTH   (0x01)
#define TEST_TIME_SET_YEAR    (0x20)

std::map<uint32_t, bool> gpio_output_state;
std::set<uint32_t> configured_output_pins;
std::set<uint32_t> configured_input_pins;
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static uint8_t m_ic_response_systime[LEN_TX_DATA_SYSTIME] = {0};
static const uint32_t m_mock_epoch_time = 1577836800; // 2020-01-01 00:00:00 UTC (Wednesday)

// Fake the RTC DRV register map.
static uint16_t m_dummy_ic_registers[RTC_DRV_REGISTER_COUNT] = {0x00};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

class rtcDriverTestSuit: public ::testing::Test
{
protected:
   void SetUp() override
   {
      memset(m_dummy_ic_registers, 0, sizeof(m_dummy_ic_registers));
   }

   void TearDown() override
   {
   }
};

void on_i2c_tx_data(const uint8_t *tx_data, uint8_t tx_data_len) // mock write
{
   if(NULL == tx_data)
   {
      return;
   }

   ASSERT_TRUE(tx_data[0] <= RTC_DRV_REGISTER_COUNT);

   // REGISTER WRITE
   if(tx_data_len >= LEN_TX_DATA_REG_INIT)
   {
      uint8_t write_addr = tx_data[0];
      uint8_t write_len = tx_data_len - 1u;

      for(uint8_t index = 0; index < tx_data_len; index++)
      {
         m_ic_response_systime[index] = tx_data[index];
      }

      // Persist to dummy register map so readback/verification paths behave like hardware.
      for(uint8_t index = 0u; index < write_len; index++)
      {
         uint16_t reg_addr = (uint16_t)write_addr + index;
         // PCF85363A time-load sequence rolls over from 0x2F to 0x00.
         if(reg_addr > PCF85363A_CTRL_RESET_REG_ADR)
         {
            reg_addr -= (PCF85363A_CTRL_RESET_REG_ADR + 1u);
         }
         if(reg_addr < RTC_DRV_REGISTER_COUNT)
         {
            m_dummy_ic_registers[reg_addr] = tx_data[index + 1u];
         }
      }
   }

   // REGISTER READ
   if(LEN_TX_DATA_FOR_RX == tx_data_len)
   {
      memset(m_ic_response_systime, 0, sizeof(m_ic_response_systime));

      m_ic_response_systime[0] = tx_data[0];

      if(PCF85363A_REG_100TH_SECONDS == tx_data[0])
      {
         for(uint8_t index = 0u; index < LEN_RX_DATA_SYSTIME; index++)
         {
            m_ic_response_systime[index] = m_dummy_ic_registers[PCF85363A_REG_100TH_SECONDS + index];
         }

         mock_i2c_set_rx_data(m_ic_response_systime, LEN_RX_DATA_SYSTIME);
      }
      else if(PCF85363A_REG_SECONDS == tx_data[0])
      {
         // Used by oscillator-stopped flag check/update path.
         m_ic_response_systime[0] = m_dummy_ic_registers[PCF85363A_REG_SECONDS];
         mock_i2c_set_rx_data(m_ic_response_systime, LEN_TX_DATA_FOR_RX);
      }
   }
}

/**
 * @brief Tests the initialization of the RTC
 *
 * This function initializes the RTC and verifies that the initialization was successful.
 *
 * @return None
 */

TEST_F(rtcDriverTestSuit, rtc_driver_init_test)
{
   result_t result = RESULT_OK;

   i2c_driver_t mock_i2c_driver = {0};
   rtc_system_time_t rtc_system_time_instance = {0};

   result = mock_i2c_driver_init(&mock_i2c_driver, &on_i2c_tx_data, MOCK_I2C_ADDRESS);
   ASSERT_TRUE(IS_OK(result));

   sys_time_t system_time_gtest
      = {.seconds = 0, .minutes = 0, .hours = 0, .day = 0, .weekday = 0, .month = 0, .year = 0};

   if(IS_OK(result))
   {
      // Mock the initialization of the IC hardware registers
      m_dummy_ic_registers[PCF85363A_FUNC_CTRL_ADR] = PCF85363A_FUNC_CTRL_EN_RTC_DIS_PI_DEF_FREQ;
      m_dummy_ic_registers[PCF85363A_CTRL_OSC_ADR] = PCF85363A_CTRL_OSC;
      m_dummy_ic_registers[PCF85363A_CTRL_PIN_IO_ADR] = PCF85363A_CTRL_PIN_IO;
      m_dummy_ic_registers[PCF85363A_CTRL_INTA_EN_ADR] = PCF85363A_CTRL_INT_EN_DISABLE_ALL_INT;
      m_dummy_ic_registers[PCF85363A_CTRL_INTB_EN_ADR] = PCF85363A_CTRL_INT_EN_DISABLE_ALL_INT;
      m_dummy_ic_registers[PCF85363A_CTRL_BATT_ADR] = PCF85363A_CTRL_BATT_DEFAULT;

      result = rtc_system_time_init(
         &rtc_system_time_instance, MOCK_I2C_ADDRESS, &mock_i2c_driver.interface, system_time_gtest);

      ASSERT_TRUE(IS_OK(result));
   }

   EXPECT_EQ(rtc_system_time_instance._initialized, true);
}

TEST_F(rtcDriverTestSuit, set_time_epoch)
{
   result_t result = RESULT_OK;

   i2c_driver_t mock_i2c_driver = {0};
   rtc_system_time_t rtc_system_time_instance = {0};

   result = mock_i2c_driver_init(&mock_i2c_driver, &on_i2c_tx_data, MOCK_I2C_ADDRESS);
   ASSERT_TRUE(IS_OK(result));

   sys_time_t system_time_gtest
      = {.seconds = 0, .minutes = 0, .hours = 0, .day = 0, .weekday = 0, .month = 0, .year = 0};

   if(IS_OK(result))
   {
      result = rtc_system_time_init(
         &rtc_system_time_instance, MOCK_I2C_ADDRESS, &mock_i2c_driver.interface, system_time_gtest);
      ASSERT_TRUE(IS_OK(result));
   }

   EXPECT_EQ(rtc_system_time_instance._initialized, true);

   result = rtc_system_time_instance.interface.set_time_unix(&rtc_system_time_instance.interface, m_mock_epoch_time);
   ASSERT_TRUE(IS_OK(result));

   ASSERT_EQ(TEST_TIME_SET_SECONDS, m_dummy_ic_registers[PCF85363A_REG_SECONDS]);
   ASSERT_EQ(TEST_TIME_SET_MINUTES, m_dummy_ic_registers[PCF85363A_REG_MINUTES]);
   ASSERT_EQ(TEST_TIME_SET_HOUR, m_dummy_ic_registers[PCF85363A_REG_HOURS]);
   ASSERT_EQ(TEST_TIME_SET_DAY, m_dummy_ic_registers[PCF85363A_REG_DAYS]);
   ASSERT_EQ(TEST_TIME_SET_WEEKDAY, m_dummy_ic_registers[PCF85363A_REG_WEEKDAYS]);
   ASSERT_EQ(TEST_TIME_SET_MONTH, m_dummy_ic_registers[PCF85363A_REG_MONTHS]);
   ASSERT_EQ(TEST_TIME_SET_YEAR, m_dummy_ic_registers[PCF85363A_REG_YEARS]);
}

TEST_F(rtcDriverTestSuit, get_time_unix)
{
   result_t result = RESULT_OK;

   i2c_driver_t mock_i2c_driver = {0};
   rtc_system_time_t rtc_system_time_instance = {0};

   result = mock_i2c_driver_init(&mock_i2c_driver, &on_i2c_tx_data, MOCK_I2C_ADDRESS);
   ASSERT_TRUE(IS_OK(result));

   sys_time_t system_time_gtest
      = {.seconds = 0, .minutes = 0, .hours = 0, .day = 0, .weekday = 0, .month = 0, .year = 0};

   if(IS_OK(result))
   {
      result = rtc_system_time_init(
         &rtc_system_time_instance, MOCK_I2C_ADDRESS, &mock_i2c_driver.interface, system_time_gtest);
      ASSERT_TRUE(IS_OK(result));
   }

   EXPECT_EQ(rtc_system_time_instance._initialized, true);

   uint32_t m_mock_received_epoch_time = 0;

   result = rtc_system_time_instance.interface.set_time_unix(&rtc_system_time_instance.interface, m_mock_epoch_time);
   ASSERT_TRUE(IS_OK(result));

   // Mock IC registers to contain system time
   // Set system time in BCD format
   m_dummy_ic_registers[PCF85363A_REG_SECONDS] = 0x00;
   m_dummy_ic_registers[PCF85363A_REG_MINUTES] = 0x00;
   m_dummy_ic_registers[PCF85363A_REG_HOURS] = 0x00;
   m_dummy_ic_registers[PCF85363A_REG_DAYS] = 0x01;
   m_dummy_ic_registers[PCF85363A_REG_WEEKDAYS] = 0x03;
   m_dummy_ic_registers[PCF85363A_REG_MONTHS] = 0x01;
   m_dummy_ic_registers[PCF85363A_REG_YEARS] = 0x20; // (BCD format)

   result = rtc_system_time_instance.interface.get_time_unix(&rtc_system_time_instance.interface,
                                                             &m_mock_received_epoch_time);

   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(m_mock_received_epoch_time, 1577836800);
}
