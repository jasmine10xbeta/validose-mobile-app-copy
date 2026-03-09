/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ads1235.c
 * @brief Driver implementation for the ADS1235 ADC.
 *
 * Operation:
 *    - The ADS1235 is configured and controlled via SPI commands.
 *    - The initial configuration is static, but can be updated at runtime via the provided interface functions.
 *    - The ADS1235 operates in continuous conversion mode.
 *
 *    - The driver reads a new ADC value from the ADS1235 whenever the NDRDY pin signals that a new conversion is ready.
 *      That sample is immediately ingested to update rolling aggregates for the moving average and standard deviation
 *      calculations. The most recent 40 samples are stored in a circular buffer for these calculations.
 *
 *    - The moving average and standard deviation are calculated on demand from the rolling aggregates. The calculations
 *      are across a ADS1235_MOVING_AVERAGE_SAMPLES sample window.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "nordic_common.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include <app_util.h>
#include <app_util_platform.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Custom includes
#include "ads1235.h"
#include "custom_board.h"
#include "debug.h"
#include "sdk_config.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_ADS1235_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define ADS1235_DRDY_READY_TIMEOUT_MS                                                                                  \
   (100u) // DRDY-ready timeout after power/reset (~65536/fCLK; internal fCLK ~7.37 MHz => ~9 ms typical, testing showed
          // a timeout >50ms is necessary)
#define RREG_BUFFER_SIZE        (3u)
#define WREG_BUFFER_SIZE        (2u)
#define RDATA_BUFFER_SIZE       (6u)
#define SPI_GENERIC_BUFFER_SIZE (2u)

#define DEVICE_ID (0x0Cu) // from datasheet p.48 [Table 28. ID Register Field Descriptions]  (ver. SBAS824–OCTOBER 2018)

#define ADS1235_SAMPLE_BIT_WIDTH              (24u)
#define ADS1235_MAX_SAMPLE_SQUARE             (1ull << (2u * (ADS1235_SAMPLE_BIT_WIDTH - 1u)))
#define ADS1235_MAX_SQUARED_SAMPLES_IN_UINT64 (UINT64_MAX / ADS1235_MAX_SAMPLE_SQUARE)

STATIC_ASSERT(ADS1235_MOVING_AVERAGE_SAMPLES <= ADS1235_MAX_SQUARED_SAMPLES_IN_UINT64,
              "ADS1235_MOVING_AVERAGE_SAMPLES is too large and may cause uint64 overflow in stddev calculations");
STATIC_ASSERT(ADS1235_MOVING_AVERAGE_SAMPLES >= 2u,
              "ADS1235_MOVING_AVERAGE_SAMPLES must be at least 2 to compute standard deviation");

STATIC_ASSERT(
   NRFX_GPIOTE_CONFIG_IRQ_PRIORITY >= SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
   "NRFX_GPIOTE_CONFIG_IRQ_PRIORITY must be greater than or equal to SPI_DEFAULT_CONFIG_IRQ_PRIORITY to ensure "
   "SPI transactions can complete within the DRDY interrupt handler");

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Commands for the ADS1235
 */
typedef enum
{
   ADS1235_CMD_NOP = (0x00u),
   ADS1235_CMD_RESET = (0x06u),
   ADS1235_CMD_START = (0x08u),
   ADS1235_CMD_STOP = (0x0Au),
   ADS1235_CMD_RDATA = (0x12u),
   ADS1235_CMD_SYOCAL = (0x16u),
   ADS1235_CMD_GANCAL = (0x17u),
   ADS1235_CMD_SFOCAL = (0x19u),
   ADS1235_CMD_RREG = (0x20u), // + 5-bit register address
   ADS1235_CMD_WREG = (0x40u), // + 5-bit register address
   ADS1235_CMD_LOCK = (0xF2u),
   ADS1235_CMD_UNLOCK = (0xF5u)
} ADS1235_CMD;

/**
 * @brief Snapshot of the moving average data for safe concurrent access between ISR and application
 */
typedef struct
{
   uint32_t count;
   int64_t sum;
   uint64_t sum2;
} ma_data_snapshot_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t reset(const ads1235_interface_t *interface);
static result_t set_mode(const ads1235_interface_t *interface, ads1235_mode_t mode);
static result_t get_mode(const ads1235_interface_t *interface, ads1235_mode_t *out_mode);
static result_t set_data_rate(const ads1235_interface_t *interface, ads1235_data_rate_t data_rate);
static result_t get_data_rate(const ads1235_interface_t *interface, ads1235_data_rate_t *out_data_rate);
static result_t
   set_input_mux(const ads1235_interface_t *interface, ads1235_input_mux_t positive, ads1235_input_mux_t negative);
static result_t get_input_mux(const ads1235_interface_t *interface,
                              ads1235_input_mux_t *out_positive,
                              ads1235_input_mux_t *out_negative);
static result_t is_data_ready(const ads1235_interface_t *interface, bool *is_buffer_filled);
static result_t set_filter(const ads1235_interface_t *interface, ads1235_filter_t filter);
static result_t get_filter(const ads1235_interface_t *interface, ads1235_filter_t *out_filter);
static result_t set_reference(const ads1235_interface_t *interface,
                              ads1235_reference_positive_t positive,
                              ads1235_reference_negative_t negative);
static result_t get_reference(const ads1235_interface_t *interface,
                              ads1235_reference_positive_t *out_positive,
                              ads1235_reference_negative_t *out_negative);
static result_t set_gain(const ads1235_interface_t *interface, ads1235_gain_t gain);
static result_t get_gain(const ads1235_interface_t *interface, ads1235_gain_t *out_gain);
static result_t set_conversion_state(const ads1235_interface_t *interface, bool is_conversion_enabled);
static result_t is_conversion_enabled(const ads1235_interface_t *interface, bool *is_enabled);

static result_t
   get_adc_data(const ads1235_interface_t *interface, int32_t *ma_out, uint16_t *stddev_out, bool *is_data_stale);

// Non-interface functions

static ads1235_driver_t *get_instance_by_ndrdy_pin(nrfx_gpiote_pin_t drdy_pin);
static result_t add_instance(ads1235_driver_t *p_instance);
static void drdy_irq_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

static result_t read_sample(ads1235_driver_t *p_self, int32_t *raw_val_out);
static result_t ingest_sample(ads1235_driver_t *p_self, int32_t raw_val_in);

static result_t take_ma_data_snapshot(ads1235_driver_t *p_self, ma_data_snapshot_t *snapshot_out);
static result_t reset_ma(ads1235_driver_t *p_self);

static uint32_t int_sqrt(uint64_t value);
static result_t calculate_sample_stddev_u16(int64_t sum, uint64_t sum2, uint32_t count, uint16_t *stddev_out);

static result_t spi_interface(const ads1235_driver_t *self, const uint8_t *tx_data, uint8_t *rx_data, uint8_t size);
static result_t verify_device_id(const ads1235_driver_t *self);
static result_t clear_status(const ads1235_driver_t *self);
/// Driver functions for internal use
static result_t start_conversion(const ads1235_driver_t *self);
static result_t stop_conversion(const ads1235_driver_t *self);
static result_t read_register(const ads1235_driver_t *self, ads1235_reg_t reg, uint8_t *out_value);
static result_t write_register(const ads1235_driver_t *self, ads1235_reg_t reg, uint8_t value);
static result_t wait_for_ndrdy_ready(const ads1235_driver_t *self, uint32_t timeout_ms);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static const uint32_t m_spi_timeout_us = 10000u;

/** Map of GPIO pins to ADS1235 driver instances */
static ads1235_driver_t *m_instance_pin_map[ADS1235_INSTANCES_MAX] = {NULL};
static uint8_t m_instance_count = 0u;

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/**
 * @brief Get an ADS1235 driver instance based on the NDRDY pin number
 *
 * @param[in] ndrdy_pin The NDRDY pin number associated with the desired ADS1235 instance
 *
 * @return Pointer to the ads1235_driver_t instance associated with the given NDRDY pin, or NULL if no matching instance
 * is found
 */
static ads1235_driver_t *get_instance_by_ndrdy_pin(nrfx_gpiote_pin_t ndrdy_pin)
{
   ads1235_driver_t *p_instance = NULL;

   for(uint8_t idx = 0u; idx < m_instance_count; idx++)
   {
      if((NULL != m_instance_pin_map[idx]) && (ndrdy_pin == m_instance_pin_map[idx]->_ndrdy_pin))
      {
         p_instance = m_instance_pin_map[idx];
         break;
      }
   }

   return p_instance;
}

/**
 * @brief Add an ADS1235 driver instance to the global instance map
 *
 * Adds an ADS1235 driver instance to the global map of instances IF there is capacity AND it is not already added.
 *
 * @param[in] p_instance Pointer to the ads1235_driver_t instance to be added
 *
 * @retval RESULT_OK if the instance was successfully added or already exists in the map
 * @retval ADS1235_ERROR_NULL_POINTER if p_instance is NULL
 * @retval ADS1235_ERROR_MAX_INSTANCES_REACHED if the maximum number of instances has already been added
 */
static result_t add_instance(ads1235_driver_t *p_instance)
{
   RETURN_ERR_IF_NULL(p_instance, ADS1235_ERROR_NULL_POINTER);

   result_t result = RESULT_OK;
   bool already_added = false;

   for(uint8_t idx = 0u; idx < m_instance_count; idx++)
   {
      if(m_instance_pin_map[idx] == p_instance)
      {
         already_added = true;
         break;
      }
   }

   if(false == already_added)
   {
      if(m_instance_count >= ADS1235_INSTANCES_MAX)
      {
         SET_ERR(result, ADS1235_ERROR_MAX_INSTANCES_REACHED);
      }
      else
      {
         m_instance_pin_map[m_instance_count] = p_instance;
         m_instance_count++;
      }
   }

   return result;
}

/**
 * @brief Handles the DRDY interrupt for the ADS1235.
 *
 * @param[in] pin Pin that triggered this event
 * @param[in] action Action that led to triggering this event
 */
static void drdy_irq_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
   UNUSED_PARAMETER(action);

   ads1235_driver_t *p_self = get_instance_by_ndrdy_pin(pin);
   RETURN_VOID_IF_NULL(p_self);

   int32_t raw_val = 0;
   result_t result = read_sample(p_self, &raw_val);
   IF_OK_RUN_AND_UPDATE(result, ingest_sample(p_self, raw_val));

   if(IS_OK(result))
   {
      p_self->_new_samples_available = true;
   }
}

/**
 * @brief Read a single ADC sample from the ADS1235
 *
 * @param[in] p_self Pointer to the driver instance
 * @param[out] raw_val_out Pointer to store the raw ADC value
 *
 * @return Result of the operation
 *
 * @note This function is used by the DRDY ISR. It must not take too long to execute.
 */
static result_t read_sample(ads1235_driver_t *p_self, int32_t *raw_val_out)
{
   RETURN_ERR_IF_NULL(p_self, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(raw_val_out, ADS1235_ERROR_NULL_POINTER);

   uint8_t tx_data[RDATA_BUFFER_SIZE] = {0};
   uint8_t rx_data[RDATA_BUFFER_SIZE] = {0};

   tx_data[0u] = ADS1235_CMD_RDATA;

   result_t result = spi_interface(p_self, tx_data, rx_data, RDATA_BUFFER_SIZE);

   if(IS_OK(result))
   {
      uint8_t status_reg_val = rx_data[2u];
      if(ADS1235_STATUS_IS_ERROR_SET(status_reg_val))
      {
         SET_ERR(result, ADS1235_ERROR_STATUS_REG_ERROR);
      }
   }

   if(IS_OK(result))
   {
      uint8_t msb_data = rx_data[3u];
      uint8_t mid_data = rx_data[4u];
      uint8_t lsb_data = rx_data[5u];
      uint8_t sign = 0x00u;

      // Sign-extend from 24-bit to 32-bit
      if(msb_data & 0x80u)
      {
         sign = 0xFFu;
      }

      *raw_val_out = (int32_t)(((uint32_t)sign << 24u) | ((uint32_t)msb_data << 16u) | ((uint32_t)mid_data << 8u)
                               | (uint32_t)lsb_data);
   }

   return result;
}

/**
 * @brief Ingest a single ADC sample into the driver's data processing pipeline
 *
 * This function is called from the DRDY ISR to process a new ADC sample.
 * It puts the sample in the ring buffer and updates aggregates for moving average and standard deviation calculations.
 *
 * @param[in] p_self Pointer to the driver instance
 * @param[in] raw_val_in The raw ADC value to ingest
 *
 * @return Result of the operation
 *
 * @note This function is used by the DRDY ISR. It must not take too long to execute.
 */
static result_t ingest_sample(ads1235_driver_t *p_self, int32_t raw_val_in)
{
   RETURN_ERR_IF_NULL(p_self, ADS1235_ERROR_NULL_POINTER);

   int64_t new64 = (int64_t)raw_val_in;
   int64_t old64 = 0;

   if(p_self->_ma_sample_count == ADS1235_MOVING_AVERAGE_SAMPLES)
   {
      old64 = (int64_t)p_self->_ma_sample_buf[p_self->_ma_sample_head];
   }

   p_self->_ma_sample_buf[p_self->_ma_sample_head] = raw_val_in;
   p_self->_ma_sample_head = (p_self->_ma_sample_head + 1) % ADS1235_MOVING_AVERAGE_SAMPLES;
   if(p_self->_ma_sample_count < ADS1235_MOVING_AVERAGE_SAMPLES)
   {
      p_self->_ma_sample_count++;
   }

   p_self->_ma_rolling_sum += (new64 - old64);
   p_self->_ma_rolling_sum2 += ((uint64_t)(new64 * new64) - (uint64_t)(old64 * old64));

   return RESULT_OK;
}

/**
 * @brief Take a snapshot of the moving average data for safe concurrent access between ISR and application
 *
 * @param[in] p_self Pointer to the driver instance
 * @param[out] snapshot_out Pointer to the ma_data_snapshot_t struct to be filled with the snapshot data
 *
 * @return Result of the operation
 */
static result_t take_ma_data_snapshot(ads1235_driver_t *p_self, ma_data_snapshot_t *snapshot_out)
{
   RETURN_ERR_IF_NULL(p_self, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(snapshot_out, ADS1235_ERROR_NULL_POINTER);

   CRITICAL_REGION_ENTER(); // Enter critical section to ensure consistent snapshot of moving average data

   snapshot_out->count = p_self->_ma_sample_count;
   snapshot_out->sum = p_self->_ma_rolling_sum;
   snapshot_out->sum2 = p_self->_ma_rolling_sum2;

   CRITICAL_REGION_EXIT(); // Exit critical section

   return RESULT_OK;
}

/**
 * @brief Reset the moving average data and related aggregates
 *
 * This function is used to clear the moving average buffer and reset the rolling aggregates. It does so inside a
 * critical section to prevent concurrent access issues with the DRDY ISR which may be updating the moving average data.
 *
 * @param[in] p_self Pointer to the driver instance
 *
 * @return Result of the operation
 */
static result_t reset_ma(ads1235_driver_t *p_self)
{
   RETURN_ERR_IF_NULL(p_self, ADS1235_ERROR_NULL_POINTER);

   CRITICAL_REGION_ENTER(); // Enter critical section to prevent concurrent access to moving average data during reset

   memset(p_self->_ma_sample_buf, 0, sizeof(p_self->_ma_sample_buf));
   p_self->_ma_sample_head = 0u;
   p_self->_ma_sample_count = 0u;
   p_self->_ma_rolling_sum = 0;
   p_self->_ma_rolling_sum2 = 0u;
   p_self->_last_ma_value = 0;
   p_self->_last_ma_stddev = 0u;
   p_self->_new_samples_available = false;

   CRITICAL_REGION_EXIT();

   return RESULT_OK;
}

/**
 * @brief Computes the integer square root of a 64-bit unsigned integer using the binary method
 *
 * @param[in] value The 64-bit unsigned integer for which to compute the integer square root
 *
 * @return The integer square root of the input value
 */
static uint32_t int_sqrt(uint64_t value)
{
   uint64_t remainder = value;
   uint64_t out = 0u;
   uint64_t bit = 1ULL << 62u;

   while(bit > remainder)
   {
      bit >>= 2u;
   }

   while(bit != 0u)
   {
      if(remainder >= (out + bit))
      {
         remainder -= (out + bit);
         out = (out >> 1u) + bit;
      }
      else
      {
         out >>= 1u;
      }
      bit >>= 2u;
   }

   return (out > UINT32_MAX) ? UINT32_MAX : (uint32_t)out;
}

/**
 * @brief Calculates the sample standard deviation from the given sum, sum of squares, and count
 *
 * @param[in] sum The sum of the samples
 * @param[in] sum2 The sum of squares of the samples
 * @param[in] count The number of samples. Must be at least 2
 * @param[out] stddev_out Pointer where the calculated standard deviation will be stored
 *
 * @retval `RESULT_OK` if the calculation was successful
 * @retval `ADS1235_ERROR_NULL_POINTER` if `stddev_out` is NULL
 * @retval `ADS1235_ERROR_INVALID_ARGUMENT` if `count` is less than 2
 * @retval `ADS1235_ERROR_INVALID_ARGUMENT` if the combination of sum, sum2, and count is implausible (e.g., `sum2 *
 * count < (sum*sum)`)
 * @retval `ADS1235_ERROR_INTERNAL` if the intermediate calculations overflow 64-bit integer limits
 */
static result_t calculate_sample_stddev_u16(int64_t sum, uint64_t sum2, uint32_t count, uint16_t *stddev_out)
{
   RETURN_ERR_IF_NULL(stddev_out, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(count < 2u, ADS1235_ERROR_INVALID_ARGUMENT);

   uint64_t n64 = (uint64_t)count;
   result_t result = RESULT_OK;

   // Calculate variance accounting for potential overflow.
   // var = ((n * sum2) - (sum * sum)) / (n * (n - 1))
   uint64_t sum_abs = (sum >= 0) ? (uint64_t)sum : (uint64_t)(-(sum + 1LL)) + 1u;

   uint64_t sum_sq = 0u;
   uint64_t n_sum2 = 0u;
   uint64_t var = 0u;
   uint64_t var_num = 0u;
   uint64_t var_den = 0u;

   if((sum_abs > 0u) && (sum_abs > (UINT64_MAX / sum_abs)))
   {
      SET_ERR(result, ADS1235_ERROR_INTERNAL); // sum*sum would overflow
   }

   if(sum2 > 0u && n64 > (UINT64_MAX / sum2))
   {
      SET_ERR(result, ADS1235_ERROR_INTERNAL); // n*sum2 would overflow
   }

   if(IS_OK(result))
   {
      sum_sq = sum_abs * sum_abs;
      n_sum2 = n64 * sum2;

      if(n_sum2 < sum_sq)
      {
         SET_ERR(result, ADS1235_ERROR_INVALID_ARGUMENT); // implausible combination of sum, sum2, and count
      }
   }

   if(IS_OK(result))
   {
      var_num = n_sum2 - sum_sq;
      var_den = n64 * (n64 - 1u);

      if(var_num > (UINT64_MAX - (var_den >> 1u)))
      {
         SET_ERR(result, ADS1235_ERROR_INTERNAL); // rounding addition would overflow
      }
   }

   if(IS_OK(result))
   {
      var = (var_num + (var_den >> 1u)) / var_den; // rounding division
   }

   // Calculate standard deviation
   if(IS_OK(result))
   {
      uint32_t stddev32 = int_sqrt(var);
      *stddev_out = (stddev32 > UINT16_MAX) ? UINT16_MAX : (uint16_t)stddev32;
   }

   return result;
}

/**
 * @brief Performs an SPI transfer with the ADS1235.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the spi interface and device configuration.
 * @param tx_data Pointer to the data buffer to be transmitted.
 * @param rx_data Pointer to the buffer where received data will be stored.
 * @param size The number of bytes to transfer.
 * @return result_t The result of the SPI transfer operation. Returns RESULT_OK on success, or
 * ADS1235_ERROR_SPI_ERROR on failure.
 */
static result_t spi_interface(const ads1235_driver_t *self, const uint8_t *tx_data, uint8_t *rx_data, uint8_t size)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(tx_data, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(rx_data, ADS1235_ERROR_NULL_POINTER);

   result_t result = self->_spi_interface->spi_driver_transfer_blocking(
      self->_spi_interface, tx_data, size, rx_data, size, m_spi_timeout_us);

   UPDATE_ERR(result, ADS1235_ERROR_SPI_ERROR);

   return result;
}

/**
 * @brief Verifies the device ID of the ADS1235.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the SPI interface and device configuration.
 * @return result_t RESULT_OK if the device ID matches the expected value, ADS1235_ERROR_DEVICE_ID_MISMATCH
 * otherwise.
 */
static result_t verify_device_id(const ads1235_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);

   ads1235_reg_id_t chip_id;

   result_t result = read_register(self, ADS1235_REG_ID, &chip_id);

   if((ADS1235_ID_DEV_ID(chip_id) != DEVICE_ID) && (IS_OK(result)))
   {
      DEBUG_ERROR("ADC ID: %d", (ADS1235_ID_DEV_ID(chip_id)));
      SET_ERR(result, ADS1235_ERROR_DEVICE_ID_MISMATCH);
   }

   return result;
}

/**
 * @brief Clears the status register of the ADS1235.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the SPI interface and device configuration.
 * @return result_t RESULT_OK on success, ADS1235_ERROR_NULL_POINTER if self is NULL.
 */
static result_t clear_status(const ads1235_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);

   return write_register(self, ADS1235_REG_STATUS, 0x00u);
}

/**
 * @brief Starts the conversion of the ADS1235.
 *
 * This function will send the START command to the ADS1235 to start the conversion process.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the SPI interface and device configuration.
 * @return result_t RESULT_OK on success, ADS1235_ERROR_NULL_POINTER if self is NULL or ADS1235_ERROR_SPI_ERROR if
 * the SPI transfer fails.
 *
 * @note Start can also be triggered by driving START pin to high
 */
static result_t start_conversion(const ads1235_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);

   uint8_t data[SPI_GENERIC_BUFFER_SIZE] = {0u};
   uint8_t rx_buffer[SPI_GENERIC_BUFFER_SIZE] = {0u};
   data[0u] = ADS1235_CMD_START;

   return spi_interface(self, data, rx_buffer, SPI_GENERIC_BUFFER_SIZE);
}

/**
 * @brief Stops the conversion of the ADS1235.
 *
 * This function will send the STOP command to the ADS1235 to stop the conversion process.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the SPI interface and device configuration.
 * @return result_t RESULT_OK on success, ADS1235_ERROR_NULL_POINTER if self is NULL or ADS1235_ERROR_SPI_ERROR if
 * the SPI transfer fails.
 *
 * @note Start can also be triggered by driving START pin to low
 *
 */
static result_t stop_conversion(const ads1235_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);

   uint8_t data[SPI_GENERIC_BUFFER_SIZE] = {0u};
   uint8_t rx_buffer[SPI_GENERIC_BUFFER_SIZE] = {0u};
   data[0u] = ADS1235_CMD_STOP;

   return spi_interface(self, data, rx_buffer, SPI_GENERIC_BUFFER_SIZE);
}

/**
 * @brief Reads a value from the specified register in the ADS1235.
 *
 * This function selects the ADS1235 chip, sends the read command and register address, reads the value from the
 * register, and then deselects the chip. It uses the provided SPI driver interface to execute the transfer.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the spi interface and device configuration.
 * @param reg The register to be read from.
 * @param out_value Pointer to the location where the read value will be stored.
 * @return result_t The result of the SPI transfer operation. Returns RESULT_OK on success, or
 * ADS1235_ERROR_SPI_ERROR on failure.
 */
static result_t read_register(const ads1235_driver_t *self, ads1235_reg_t reg, uint8_t *out_value)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_value, ADS1235_ERROR_NULL_POINTER);

   uint8_t data[RREG_BUFFER_SIZE] = {0u};
   uint8_t rx_buffer[RREG_BUFFER_SIZE] = {0u};

   data[0u] = ADS1235_CMD_RREG + (uint8_t)reg;
   result_t result = spi_interface(self, data, rx_buffer, RREG_BUFFER_SIZE);

   if(IS_OK(result))
   {
      *out_value = rx_buffer[RREG_BUFFER_SIZE - 1];
   }

   return result;
}

/**
 * @brief Writes a value to the specified register in the ADS1235.
 *
 * This function selects the ADS1235 chip, sends the write command and register address, sends the value to be
 * written, and then deselects the chip. It uses the provided SPI driver interface to execute the transfer.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the spi interface and device configuration.
 * @param reg The register to be written to.
 * @param value The value to be written to the register.
 * @return result_t The result of the SPI transfer operation. Returns RESULT_OK on success, or
 * ADS1235_ERROR_SPI_ERROR on failure.
 */
static result_t write_register(const ads1235_driver_t *self, ads1235_reg_t reg, uint8_t value)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);

   uint8_t data[WREG_BUFFER_SIZE] = {0u};
   uint8_t rx_buffer[WREG_BUFFER_SIZE] = {0u};

   data[0u] = ADS1235_CMD_WREG + (uint8_t)reg;
   data[1u] = value;
   return spi_interface(self, data, rx_buffer, WREG_BUFFER_SIZE);
}

/**
 * @brief Waits for the NDRDY (data not ready) pin to indicate readiness, with a timeout.
 *
 * Polls the NDRDY pin until it is ready (logic HIGH), or until the specified timeout elapses. Used to ensure the
 * ADS1235 is ready for communication after reset or power-up.
 *
 * @param self Pointer to the ads1235_driver_t instance containing the NDRDY pin configuration.
 * @param timeout_ms Timeout in milliseconds to wait for the NDRDY pin to become ready. Must be greater than 0.
 * @return result_t RESULT_OK if NDRDY becomes ready within the timeout, ADS1235_ERROR_NULL_POINTER if self is NULL,
 * or ADS1235_ERROR_RESET_TIMEOUT if the timeout is reached.
 */
static result_t wait_for_ndrdy_ready(const ads1235_driver_t *self, uint32_t timeout_ms)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(0u == timeout_ms, ADS1235_ERROR_INVALID_ARGUMENT);

   result_t result = RESULT_OK;

   uint32_t elapsed_ms = 0u;
   uint32_t delay_ms = 1u; // Small delay to avoid tight spinning

   // NDRDYn is active-low: "ready for communication" => pin HIGH
   bool ready = (nrf_gpio_pin_read(self->_ndrdy_pin) != 0u);

   while(!ready && (elapsed_ms < timeout_ms)) // Poll NDRDY pin for ready (timeout after specified ms)
   {
      nrf_delay_ms(delay_ms);
      elapsed_ms += delay_ms;

      ready = (nrf_gpio_pin_read(self->_ndrdy_pin) != 0u);
   }

   if(!ready)
   {
      SET_ERR(result, ADS1235_ERROR_RESET_TIMEOUT);
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t reset(const ads1235_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Reset the chip
   uint8_t data[SPI_GENERIC_BUFFER_SIZE] = {0u};
   uint8_t rx_buffer[SPI_GENERIC_BUFFER_SIZE] = {0u};
   data[0u] = ADS1235_CMD_RESET;
   result_t result = spi_interface(self, data, rx_buffer, SPI_GENERIC_BUFFER_SIZE);

   return result;
}

static result_t set_conversion_state(const ads1235_interface_t *interface, bool is_conversion_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;

   if(is_conversion_enabled && !self->_is_conversion_enabled)
   {
      // Reset the moving average data to avoid stale samples from before the conversion was stopped.
      result = reset_ma(self);

      IF_OK_RUN_AND_UPDATE(result, start_conversion(self));
      if(IS_OK(result))
      {
         self->_is_conversion_enabled = true;
      }
   }
   else if(!is_conversion_enabled && self->_is_conversion_enabled)
   {
      result = stop_conversion(self);
      if(IS_OK(result))
      {
         self->_is_conversion_enabled = false;
      }
   }

   return result;
}

static result_t is_conversion_enabled(const ads1235_interface_t *interface, bool *is_enabled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_enabled, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   *is_enabled = self->_is_conversion_enabled;

   return RESULT_OK;
}

static result_t set_mode(const ads1235_interface_t *interface, ads1235_mode_t mode)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(mode >= ADS1235_MAX_MODE, ADS1235_ERROR_INVALID_ARGUMENT);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Mode 1 Register contains mode setting
   ads1235_reg_mode1_t mode1;
   result_t result = read_register(self, ADS1235_REG_MODE1, &mode1);

   if(IS_OK(result))
   {
      mode1 = (ads1235_reg_mode1_t)((mode1 & ~ADS1235_MODE1_CHOP_MASK) | ((mode << 5u) & ADS1235_MODE1_CHOP_MASK));
      result = write_register(self, ADS1235_REG_MODE1, mode1);
   }

   return result;
}

static result_t get_mode(const ads1235_interface_t *interface, ads1235_mode_t *out_mode)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_mode, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Get mode from register
   ads1235_reg_mode1_t mode1;
   result_t result = read_register(self, ADS1235_REG_MODE1, &mode1);

   if(IS_OK(result))
   {
      *out_mode = (ads1235_mode_t)ADS1235_MODE1_CHOP(mode1);
   }

   return result;
}

static result_t set_data_rate(const ads1235_interface_t *interface, ads1235_data_rate_t data_rate)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(data_rate >= ADS1235_MAX_DATA_RATE, ADS1235_ERROR_INVALID_ARGUMENT);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Mode 0 setting contains data rate setting
   ads1235_reg_mode0_t mode0;
   result_t result = read_register(self, ADS1235_REG_MODE0, &mode0);

   if(IS_OK(result))
   {
      mode0 = (ads1235_reg_mode0_t)((mode0 & ~ADS1235_MODE0_DATA_RATE_MASK)
                                    | ((data_rate << 3u) & ADS1235_MODE0_DATA_RATE_MASK));
      result = write_register(self, ADS1235_REG_MODE0, mode0);
   }

   return result;
}

static result_t get_data_rate(const ads1235_interface_t *interface, ads1235_data_rate_t *out_data_rate)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_data_rate, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Get data rate from register
   ads1235_reg_mode0_t mode0;
   result_t result = read_register(self, ADS1235_REG_MODE0, &mode0);

   if(IS_OK(result))
   {
      *out_data_rate = (ads1235_data_rate_t)ADS1235_MODE0_DATA_RATE(mode0);
   }

   return result;
}

static result_t
   set_input_mux(const ads1235_interface_t *interface, ads1235_input_mux_t positive, ads1235_input_mux_t negative)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   ads1235_reg_input_mux_t input_mux = (ads1235_reg_input_mux_t)(((positive & 0x0Fu) << 4u) | (negative & 0x0Fu));
   result_t result = write_register(self, ADS1235_REG_INPMUX, input_mux);

   return result;
}

static result_t get_input_mux(const ads1235_interface_t *interface,
                              ads1235_input_mux_t *out_positive,
                              ads1235_input_mux_t *out_negative)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_positive, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_negative, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   ads1235_reg_input_mux_t input_mux;
   result_t result = read_register(self, ADS1235_REG_INPMUX, &input_mux);

   if(IS_OK(result))
   {
      *out_positive = (ads1235_input_mux_t)ADS1235_INPMUX_MUXP(input_mux);
      *out_negative = (ads1235_input_mux_t)ADS1235_INPMUX_MUXN(input_mux);
   }

   return result;
}

static result_t set_filter(const ads1235_interface_t *interface, ads1235_filter_t filter)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(filter >= ADS1235_FILTER_MAX, ADS1235_ERROR_INVALID_ARGUMENT);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Mode 0 setting contains filter setting
   ads1235_reg_mode0_t mode0;
   result_t result = read_register(self, ADS1235_REG_MODE0, &mode0);

   if(IS_OK(result))
   {
      mode0 = (ads1235_reg_mode0_t)((mode0 & ~ADS1235_MODE0_FILTER_MASK) | (filter & ADS1235_MODE0_FILTER_MASK));
      result = write_register(self, ADS1235_REG_MODE0, mode0);
   }

   return result;
}

static result_t get_filter(const ads1235_interface_t *interface, ads1235_filter_t *out_filter)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_filter, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   // Get filter from register
   ads1235_reg_mode0_t mode0;
   result_t result = read_register(self, ADS1235_REG_MODE0, &mode0);

   if(IS_OK(result))
   {
      *out_filter = (ads1235_filter_t)ADS1235_MODE0_FILTER(mode0);
   }

   return result;
}

static result_t set_reference(const ads1235_interface_t *interface,
                              ads1235_reference_positive_t positive,
                              ads1235_reference_negative_t negative)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   ads1235_reg_ref_t ref = (ads1235_reg_ref_t)(((positive & 0x03u) << 2u) | (negative & 0x03u));
   return write_register(self, ADS1235_REG_REF, ref);
}

static result_t get_reference(const ads1235_interface_t *interface,
                              ads1235_reference_positive_t *out_positive,
                              ads1235_reference_negative_t *out_negative)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_positive, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_negative, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   ads1235_reg_ref_t ref;
   result_t result = read_register(self, ADS1235_REG_REF, &ref);

   if(IS_OK(result))
   {
      *out_positive = (ads1235_reference_positive_t)ADS1235_REF_RMUXP(ref);
      *out_negative = (ads1235_reference_negative_t)ADS1235_REF_RMUXN(ref);
   }

   return result;
}

static result_t set_gain(const ads1235_interface_t *interface, ads1235_gain_t gain)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   ads1235_reg_pga_t pga;
   result_t result = read_register(self, ADS1235_REG_PGA, &pga);

   if(IS_OK(result))
   {
      pga = (ads1235_reg_pga_t)((pga & ~ADS1235_PGA_BYPASS_MASK) | (0u << 7u));
      pga = (ads1235_reg_pga_t)((pga & ~ADS1235_PGA_GAIN_MASK) | (gain & ADS1235_PGA_GAIN_MASK));
      result = write_register(self, ADS1235_REG_PGA, pga);
   }

   return result;
}

static result_t get_gain(const ads1235_interface_t *interface, ads1235_gain_t *out_gain)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_gain, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(((false == self->_initialized) && !(true == self->_initializing)), ADS1235_ERROR_NOT_INITIALIZED);

   ads1235_reg_pga_t pga;
   result_t result = read_register(self, ADS1235_REG_PGA, &pga);

   if(IS_OK(result))
   {
      *out_gain = (ads1235_gain_t)ADS1235_PGA_GAIN(pga);
   }

   return result;
}

static result_t
   get_adc_data(const ads1235_interface_t *interface, int32_t *ma_out, uint16_t *stddev_out, bool *is_data_stale)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(ma_out, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(stddev_out, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_data_stale, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, ADS1235_ERROR_NOT_INITIALIZED);

   result_t result = RESULT_OK;

   // uint8_t (or bool) read/write is atomically safe on nRF52 processor, so no need for critical section.
   bool new_samples_available = p_self->_new_samples_available;

   // Calculate new moving average and stddev if new samples are available since last read
   if(new_samples_available)
   {
      ma_data_snapshot_t ma_data = {0};
      int32_t new_ma_value = 0;
      uint16_t new_ma_stddev = 0u;

      result = take_ma_data_snapshot(p_self, &ma_data);
      if(IS_OK(result) && (ma_data.count < ADS1235_MOVING_AVERAGE_SAMPLES))
      {
         SET_ERR(result, ADS1235_ERROR_NOT_ENOUGH_SAMPLES);
      }

      if(IS_OK(result))
      {
         new_ma_value = (int32_t)(ma_data.sum / ma_data.count); // (sum of 24-bit samples / count) will always fit
         result = calculate_sample_stddev_u16(ma_data.sum, ma_data.sum2, ma_data.count, &new_ma_stddev);
      }

      if(IS_OK(result))
      {
         p_self->_last_ma_value = new_ma_value;
         p_self->_last_ma_stddev = new_ma_stddev;
         p_self->_new_samples_available = false; // Clear new samples flag after updating latest values
      }
   }

   // Output latest values and data staleness
   if(IS_OK(result))
   {
      *ma_out = p_self->_last_ma_value;
      *stddev_out = p_self->_last_ma_stddev;
      *is_data_stale = (false == new_samples_available);
   }

   return result;
}

static result_t is_data_ready(const ads1235_interface_t *interface, bool *is_buffer_filled)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_buffer_filled, ADS1235_ERROR_NULL_POINTER);

   ads1235_driver_t *p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, ADS1235_ERROR_NOT_INITIALIZED);

   *is_buffer_filled = (p_self->_ma_sample_count >= ADS1235_MOVING_AVERAGE_SAMPLES);

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t ads1235_init(ads1235_driver_t *const self, const spi_driver_interface_t *const spi_interface)
{
   RETURN_ERR_IF_NULL(self, ADS1235_ERROR_NULL_POINTER);
   RETURN_ERR_IF_INTERFACE_NULL(spi_interface, ADS1235_ERROR_NULL_POINTER);

   self->_initialized = false;
   self->_initializing = true;

   // Assign interface
   self->interface.parent = self;
   self->interface.reset = reset;
   self->interface.set_mode = set_mode;
   self->interface.get_mode = get_mode;
   self->interface.set_data_rate = set_data_rate;
   self->interface.get_data_rate = get_data_rate;
   self->interface.set_input_mux = set_input_mux;
   self->interface.get_input_mux = get_input_mux;
   self->interface.set_filter = set_filter;
   self->interface.get_filter = get_filter;
   self->interface.set_reference = set_reference;
   self->interface.get_reference = get_reference;
   self->interface.set_gain = set_gain;
   self->interface.get_gain = get_gain;
   self->interface.is_conversion_enabled = is_conversion_enabled;
   self->interface.set_conversion_state = set_conversion_state;
   self->interface.is_data_ready = is_data_ready;
   self->interface.get_adc_data = get_adc_data;

   // Assign dependencies
   self->_spi_interface = spi_interface;

   // Initialize state
   /** @todo Pass pins as configuration parameters */
   self->_ndrdy_pin = ADS1235_DRDY_PIN;
   self->_start_pin = ADS1235_START_PIN;
   self->_pwr_en_pin = ADS1235_5V_EN_PIN;

   result_t result = reset_ma(self);

   if(IS_OK(result))
   {
      // 1) Configure power enable pin as output; set to high
      nrf_gpio_pin_set(self->_pwr_en_pin);
      nrf_gpio_cfg_output(self->_pwr_en_pin);

      // 2) Hold START low (no conversions yet)
      nrf_gpio_pin_clear(self->_start_pin);
      nrf_gpio_cfg_output(self->_start_pin);
      nrf_delay_ms(10u); // Wait for 10ms after reset

      // 3) Configure DRDY as input w/ pullup BEFORE waiting
      nrf_gpio_cfg_input(self->_ndrdy_pin, NRF_GPIO_PIN_PULLUP);
      result = wait_for_ndrdy_ready(self, ADS1235_DRDY_READY_TIMEOUT_MS);
   }

   // Configure DRDY pin
   nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
   in_config.pull = NRF_GPIO_PIN_PULLUP;

   if(IS_OK(result) && !nrfx_gpiote_is_init())
   {
      nrfx_err_t err = nrfx_gpiote_init();
      if(err != NRFX_SUCCESS)
      {
         DEBUG_ERROR("Failed to initialize GPIOTE module. Error code: %d", err);
         SET_ERR(result, ADS1235_ERROR_INTERNAL);
      }
   }

   if(IS_OK(result))
   {
      nrfx_err_t err = nrfx_gpiote_in_init(self->_ndrdy_pin, &in_config, drdy_irq_handler);
      if(err != NRFX_SUCCESS)
      {
         DEBUG_ERROR("Failed to initialize GPIOTE input for NDRDY pin. Error code: %d", err);
         SET_ERR(result, ADS1235_ERROR_INTERNAL);
      }
   }

   // Add the instance
   IF_OK_RUN_AND_UPDATE(result, add_instance(self));

   IF_OK_RUN_AND_UPDATE(result, reset(&self->interface));
   IF_OK_RUN_AND_UPDATE(result, wait_for_ndrdy_ready(self, ADS1235_DRDY_READY_TIMEOUT_MS));
   IF_OK_RUN_AND_UPDATE(result, verify_device_id(self));
   IF_OK_RUN_AND_UPDATE(result, clear_status(self));
   IF_OK_RUN_AND_UPDATE(result, set_data_rate(&self->interface, ADS1235_SPS_400));
   IF_OK_RUN_AND_UPDATE(result, set_filter(&self->interface, ADS1235_FILTER_FIR));
   IF_OK_RUN_AND_UPDATE(result, set_gain(&self->interface, ADS1235_GAIN_128)); // to be changed in future
   IF_OK_RUN_AND_UPDATE(result, set_reference(&self->interface, ADS1235_REF_REFP0, ADS1235_REF_REFN0));
   IF_OK_RUN_AND_UPDATE(result, set_input_mux(&self->interface, ADS1235_AIN4, ADS1235_AIN5));

   ads1235_reg_mode3_t mode3;
   IF_OK_RUN_AND_UPDATE(result, read_register(self, ADS1235_REG_MODE3, &mode3));
   mode3 = (mode3 & ~ADS1235_MODE3_STATENB_MASK) | ADS1235_MODE3_STATENB_MASK; // Enable Status with conversion
   IF_OK_RUN_AND_UPDATE(result, write_register(self, ADS1235_REG_MODE3, mode3));

   // Begin Conversion
   IF_OK_RUN_AND_UPDATE(result, start_conversion(self));

   if(IS_OK(result))
   {
      self->_is_conversion_enabled = true;
      self->_initialized = true;
      nrfx_gpiote_in_event_enable(self->_ndrdy_pin, true);
   }
   self->_initializing = false;
   return result;
}
