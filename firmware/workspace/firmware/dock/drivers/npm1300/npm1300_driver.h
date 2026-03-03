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
 * @file npm1300_driver.h
 * @ingroup pmic_driver
 * @brief
 */

#ifndef NPM1300_DRIVER_H_
#define NPM1300_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"
#include "npm1300_driver_interface.h"


/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   NPM1300_DRV_ERROR_NONE = 0,
   NPM1300_DRV_ERROR_PTR_NULL,
   NPM1300_DRV_ERROR_INIT,
   NPM1300_DRV_ERROR_FAILED_I2C,
   NPM1300_DRV_ERROR_BUFFER_OVERFLOW,
   NPM1300_DRV_ERROR_INVALID_ADDRESS,
   NPM1300_DRV_ERROR_INVALID_PMIC_SWITCH,
   NPM1300_DRV_ERROR_INVALID_COMMAND,
   // CHARGERERRREASON Errors
   NPM1300_DRV_ERROR_CHARGERERRREASON_NTCSENSORERR,   // NTC thermistor sensor error
   NPM1300_DRV_ERROR_CHARGERERRREASON_VBATSENSORERR,  // VBAT Sensor Error
   NPM1300_DRV_ERROR_CHARGERERRREASON_VBATLOW,        // VBAT Low Error
   NPM1300_DRV_ERROR_CHARGERERRREASON_VTRICKLE,       // Vtrickle Error
   NPM1300_DRV_ERROR_CHARGERERRREASON_MEASTIMEOUT,    // Measurement Timeout Error
   NPM1300_DRV_ERROR_CHARGERERRREASON_CHARGETIMEOUT,  // Charge Timeout Error
   NPM1300_DRV_ERROR_CHARGERERRREASON_TRICKLETIMEOUT, // Trickle Timeout Error

   // CHARGERERRSENSOR Errors
   NPM1300_DRV_ERROR_SENSORNTCCOLD,  // NTC thermistor Cold sensor value during error
   NPM1300_DRV_ERROR_SENSORNTCCOOL,  // NTC thermistor Cool sensor value during error
   NPM1300_DRV_ERROR_SENSORNTCWARM,  // NTC thermistor Warm sensor value during error
   NPM1300_DRV_ERROR_SENSORNTCHOT,   // NTC thermistor Hot sensor value during error
   NPM1300_DRV_ERROR_SENSORVTERM,    // Vterm sensor value during error
   NPM1300_DRV_ERROR_SENSORRECHARGE, // Recharge sensor value during error
   NPM1300_DRV_ERROR_SENSORTRICKLE,  // Vtrickle sensor value during error
   NPM1300_DRV_ERROR_SENSORVBATLOW,  // VbatLow sensor value during error

   // RSTCAUSE Errors
   NPM1300_DRV_ERROR_RSTCAUSE_SHIPMODEEXIT,       // Internal reset caused by shipmode exit
   NPM1300_DRV_ERROR_RSTCAUSE_BOOTMONITORTIMEOUT, // Internal reset caused by boot monitor timeout
   NPM1300_DRV_ERROR_RSTCAUSE_WATCHDOGTIMEOUT,    // Internal reset caused by watchdog timeout
   NPM1300_DRV_ERROR_RSTCAUSE_LONGPRESSTIMEOUT,   // Internal reset caused by long press timeout
   NPM1300_DRV_ERROR_RSTCAUSE_THERMALSHUTDOWN,    // Internal reset caused by thermal shutdown
   NPM1300_DRV_ERROR_RSTCAUSE_VSYSLOW,            // Internal reset caused by VSYS low
   NPM1300_DRV_ERROR_RSTCAUSE_SWRESET,            // Internal reset caused by software reset

   NPM1300_DRV_ERROR_MAX,
} NPM1300_DRV_ERROR;

typedef struct npm1300_driver
{
   npm1300_driver_interface_t interface;

   const i2c_driver_interface_t *_i2c_interface;
   uint8_t _device_address;

   bool _auto_enabled;
   bool _initialized;
} npm1300_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t npm1300_driver_init(npm1300_driver_t *const self,
                             const i2c_driver_interface_t *i2c_interface,
                             uint8_t device_address);

#endif // NPM1300_DRIVER_H_
