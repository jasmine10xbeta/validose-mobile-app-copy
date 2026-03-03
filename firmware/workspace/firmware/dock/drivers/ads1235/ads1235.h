/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef ADS1235_H
#define ADS1235_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <nrf_drv_spi.h>
#include <nrfx_gpiote.h>

// Custom includes
#include "ads1235_interface.h"
#include "common.h"
#include "spi_driver.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief ADS1235 Driver Struct
 *
 * Contains all state and interfaces for the ADS1235 ADC driver
 */
typedef struct ads1235_driver
{
   ads1235_interface_t interface;

   nrfx_gpiote_pin_t _ndrdy_pin; /**< NDRDY (not data ready) pin */
   uint8_t _start_pin;           /**< START pin */
   uint8_t _pwr_en_pin;          /**< Power enable pin */

   const spi_driver_interface_t *_spi_interface;
   nrf_drv_spi_config_t _spi_config;

   volatile bool _is_conversion_enabled; /**< Whether ADC conversion is currently enabled */

   int32_t _ma_sample_buf[ADS1235_MOVING_AVERAGE_SAMPLES]; /**< Buffer for moving average samples */
   uint32_t _ma_sample_head;                               /**< Write index for the moving average sample buffer */

   uint32_t _ma_sample_count; /**< Number of samples currently in the moving average buffer */
   int64_t _ma_rolling_sum;   /**< Rolling sum of samples in the moving average window */
   uint64_t _ma_rolling_sum2; /**< Rolling sum of squares of samples in the moving average window */

   int32_t _last_ma_value;   /**< Last computed moving average value */
   uint16_t _last_ma_stddev; /**< Last computed standard deviation value */

   volatile bool _new_samples_available; /**< Whether new ADC samples are available since the last data read */

   bool _initialized;  /**< Whether this instance has been initialized */
   bool _initializing; /**< Whether this instance is currently initializing */

} ads1235_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initialize an ADS1235 driver instance
 *
 * @param[in,out] self Pointer to the driver instance to initialize
 * @param[in] spi_interface Pointer to the SPI interface that the driver will use for communication
 *
 * @return result_t Result of the operation
 */
result_t ads1235_init(ads1235_driver_t *const self, const spi_driver_interface_t *const spi_interface);

#endif // ADS1235_H