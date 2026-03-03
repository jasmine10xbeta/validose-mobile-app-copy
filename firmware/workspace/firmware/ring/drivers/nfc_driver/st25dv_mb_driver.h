/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file st25dv_mb_driver.h
 * @ingroup nfc_tag_driver
 * @brief ST25DV NFC tag driver for the ring platform.
 *
 * @details
 * This header defines the driver structures, error codes, and initialization function for the ST25DV NFC tag.
 * It provides mailbox and EEPROM access, error handling, and hardware abstraction for use in ring firmware.
 *
 * @par Usage
 * - Call @ref st25dv_driver_init to initialize the driver and hardware interfaces.
 * - Access mailbox and EEPROM via the exposed interfaces in @ref st25dv_driver_t.
 *
 * @par Limitations
 * - Designed for ST25DV04K (4kbit EEPROM, mailbox, GPO interrupt).
 * - All operations are blocking and intended for use in non-interrupt context.
 */

#ifndef ST25DV_DRIVER_H_
#define ST25DV_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

// Custom includes
#include "common.h"
#include "comms_driver_interface.h"
#include "debug.h"
#include "i2c_driver_interface.h"
#include "nfc_tag_driver_interface.h"
#include "st25dv.h"
#include "system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Main driver structure for ST25DV NFC tag.
 *
 * This structure contains all state and interfaces required for mailbox, comms, I2C, and system time operations.
 */
typedef struct comms_driver
{
   nfc_tag_driver_interface_t interface; /**< Exposed NFC-tag interface (status, EEPROM, etc.). */
   comms_driver_interface_t data_ifc;    /**< Mailbox comms interface used by the higher-level comms driver. */

   const i2c_driver_interface_t *_i2c_interface; /**< Backing I2C driver provided at init. */
   const system_time_interface_t *_systick_ifc;  /**< System time interface for delays and timeouts. */
   ST25DV_Object_t _st_device;                   /**< Instance of ST's driver/state machine. */
   ST25DV_IO_t _st_bus_io;                       /**< IO callbacks handed to the ST driver. */
   uint32_t _total_eeprom_bytes;                 /**< Cached total EEPROM size in bytes. */
   uint32_t _area_end_bytes[4];                  /**< Cached end byte (inclusive) of each user area. */

   bool _initialized; /**< Driver initialization status. */

} st25dv_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initialize the ST25DV NFC tag driver and hardware interfaces.
 *
 * Sets up the driver structure, I2C and system time interfaces, and configures the NFC tag hardware.
 * Must be called before any other driver operations.
 *
 * @param self Pointer to driver structure to initialize.
 * @param i2c_interface Pointer to I2C driver interface.
 * @param interrupt_pin GPIO pin number for NFC tag interrupt (GPO).
 * @param systick_ifc Pointer to system time interface for delays and timeouts.
 * @return result_t Result code indicating success or error.
 */
result_t st25dv_driver_init(st25dv_driver_t *const self,
                            const i2c_driver_interface_t *i2c_interface,
                            uint8_t interrupt_pin,
                            const system_time_interface_t *systick_ifc);

#endif // ST25DV_DRIVER_H_
