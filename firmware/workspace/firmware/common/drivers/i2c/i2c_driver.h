/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup i2c_driver I2C
 * @ingroup drivers
 * @brief Second-layer driver providing some additional abstraction to the Nordic TWI driver.
 * @details
 *
 * @file i2c_driver.h
 * @ingroup i2c_driver
 * @brief
 */

#ifndef I2C_DRIVER_H_
#define I2C_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "nrf_drv_twi.h"
#include "nrf_mtx.h"

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct i2c_driver
{
   i2c_driver_interface_t interface;

   const nrf_drv_twi_t *_twi_instance;
   const nrf_drv_twi_config_t *_twi_config;
   volatile bool _twi_transfer_done;
   volatile bool _anak_received;
   volatile bool _dnak_received;

   nrf_mtx_t _twi_mutex;

   bool _initialized;
} i2c_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t
   i2c_driver_init(i2c_driver_t *const self, const nrf_drv_twi_t *twi_instance, const nrf_drv_twi_config_t *twi_config);

#endif // I2C_DRIVER_H_
