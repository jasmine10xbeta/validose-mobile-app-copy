/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup pmic_driver PMIC
 * @ingroup drivers
 * @brief Driver for controlling the device Power Management IC.
 * @details
 *
 * @file bq25150_driver.h
 * @ingroup pmic_driver
 * @brief
 */

#ifndef BQ25150_DRIVER_H_
#define BQ25150_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "nrf_gpio.h"

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"
#include "pmic_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct
{
   /* Charger -----------------------------------------------------------------*/
   uint16_t vbat_reg_mV;  /**< Battery regulation voltage (3600-4600)   */
   uint16_t ichg_mA;      /**< Fast-charge current (1-500)              */
   uint16_t iprechg_mA;   /**< Pre-charge current (1-77)                */
   uint16_t iterm_mA;     /**< Termination current (mA or % if < 31)    */
   uint16_t iin_limit_mA; /**< Input current limit (50-600)             */
   bool enable_charging;  /**< true = enable charger after init         */

   /* LDO / load switch -------------------------------------------------------*/
   bool ldo_enable;         /**< true = enable LS/LDO output              */
   uint16_t ldo_voltage_mV; /**< 600-3700 mV (rounded to 100 mV steps)    */
   bool ldo_as_switch;      /**< true = LS mode, false = regulated LDO    */

   /* Misc --------------------------------------------------------------------*/
   bool jeita_en;         /**< true = JEITA temp-based derating         */
   bool vindpm_en;        /**< true = enable VIN-DPM loop               */
   uint16_t vindpm_th_mV; /**< VIN-DPM threshold 4200-4900 mV           */
   uint16_t ldo_voltage_mv;
} bq25150_cfg_t;

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   BQ25150_DRV_ERROR_NONE = 0,
   BQ25150_DRV_ERROR_PTR_NULL,
   BQ25150_DRV_ERROR_INIT,
   BQ25150_DRV_ERROR_FAILED_I2C,
   BQ25150_DRV_ERROR_BUFFER_OVERFLOW,
   BQ25150_DRV_ERROR_INVALID_ADDRESS,
   BQ25150_DRV_ERROR_INVALID_PMIC_SWITCH,
   BQ25150_DRV_ERROR_INVALID_COMMAND,
   BQ25150_DRV_ERROR_VIN_OVP,
   BQ25150_DRV_ERROR_BAT_OCP,
   BQ25150_DRV_ERROR_LDO_OCP,
   BQ25150_DRV_ERROR_BAT_UVLO,
   BQ25150_DRV_ERROR_SAFETY_TIMER,
   BQ25150_DRV_ERROR_WATCHDOG,
   BQ25150_DRV_ERROR_IMAX_OPEN,
   BQ25150_DRV_ERROR_TS_OPEN,
   BQ25150_DRV_ERROR_MAX
} BQ25150_DRV_ERROR;

typedef struct bq25150_driver
{
   bq25150_driver_interface_t interface;

   const i2c_driver_interface_t *_i2c_interface;
   uint8_t _device_address;

   bool _auto_enabled;
   bool _initialized;
   uint32_t _lp_pin; // Low power mode enable pin. Drive low to enable low power mode. Note: LP mode disables I2C.
   uint32_t _ce_pin; // Charge enable pin. Drive low to enable charging.
} bq25150_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initializes the BQ25150 PMIC driver.
 *
 * This function initializes the BQ25150 PMIC driver by setting up the I2C interface,
 * device address, and GPIO pins for low power mode and charge enable.
 *
 * @param self Pointer to the BQ25150 driver instance.
 * @param i2c_interface Pointer to the I2C driver interface to be used for communication.
 * @param device_address The 7-bit I2C device address of the BQ25150 PMIC.
 * @param lp_pin The GPIO pin number for the low power mode enable pin. Drive low to enable low power mode.
 *               Note: LP mode disables I2C.
 * @param ce_pin The GPIO pin number for the charge enable pin. Drive low to enable charging.
 *
 * @return Result of the initialization process.
 */
result_t bq25150_driver_init(bq25150_driver_t *const self,
                             const i2c_driver_interface_t *i2c_interface,
                             uint8_t device_address,
                             uint32_t lp_pin,
                             uint32_t ce_pin);

#endif // BQ25150_DRIVER_H_
