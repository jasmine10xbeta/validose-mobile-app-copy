/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file spi_driver.h
 * @brief SPI driver
 *
 * Second-layer driver providing some additional abstraction to the Nordic nrf_drv_spi (SPIM) driver.
 */

#ifndef SPI_DRIVER_MOCK_H_
#define SPI_DRIVER_MOCK_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>

#include "nrf_drv_spi.h"
#include "nrf_mtx.h"

// Custom includes
#include "common.h"
#include "spi_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef enum
{
   SPI_DRV_ERROR_NONE = 0,
   SPI_DRV_ERROR_NULL,
   SPI_DRV_ERROR_TIMEOUT,
   SPI_DRV_ERROR_INIT_SPI,
   SPI_DRV_ERROR_TRANSFER,
   SPI_DRV_ERROR_BUSY,
   SPI_DRV_ERROR_MAX,
} SPI_DRV_ERROR;

typedef struct spi_driver
{
   spi_driver_interface_t interface;

   const nrf_drv_spi_t *_spi_instance;
   uint8_t _spi_chip_select;
   volatile bool _spi_transfer_done;

   nrf_mtx_t _mutex;

   bool _initialized;
} spi_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t mock_spi_driver_transfer_blocking_success(const spi_driver_interface_t *const interface,
                                                   uint8_t const *p_tx_buffer,
                                                   uint8_t tx_buffer_length,
                                                   uint8_t *p_rx_buffer,
                                                   uint8_t rx_buffer_length,
                                                   uint32_t timeout_us);

result_t mock_spi_driver_transfer_blocking_fail(const spi_driver_interface_t *const interface,
                                                uint8_t const *p_tx_buffer,
                                                uint8_t tx_buffer_length,
                                                uint8_t *p_rx_buffer,
                                                uint8_t rx_buffer_length,
                                                uint32_t timeout_us);
result_t mock_spi_driver_init(spi_driver_t *const self, bool transfer_success);

#endif // SPI_DRIVER_MOCK_H_
