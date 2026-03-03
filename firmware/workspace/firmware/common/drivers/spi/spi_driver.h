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

#ifndef SPI_DRV_H_
#define SPI_DRV_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>

#include "nrf_drv_spi.h"
#include "nrf_mtx.h"

// Custom includes
#include "../../common.h"
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

result_t
   spi_driver_init(spi_driver_t *const self, const nrf_drv_spi_t *spi_instance, const nrf_drv_spi_config_t *spi_config);

#endif // SPI_DRV_H_
