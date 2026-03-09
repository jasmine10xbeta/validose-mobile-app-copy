/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file nfc_tag_driver_interface.h
 * @ingroup nfc_driver
 * @brief NFC tag driver interface for ST25DV04K on the ring platform.
 *
 * This header defines the interface for mailbox status, interrupt handling, and EEPROM access for the ST25DV NFC tag.
 * It provides blocking functions for mailbox queries, interrupt status, and EEPROM read/write operations, abstracting
 * hardware details for higher-level modules on the ring device.
 *
 * Usage:
 *   - Use get_status() to query mailbox state and message availability.
 *   - Use get_interrupt_status() to check and clear NFC interrupt events.
 *   - Use read_eeprom_blocking() and write_eeprom_blocking() for EEPROM access.
 *
 * Limitations:
 *   - EEPROM size is limited to 512 bytes (ST25DV04K).
 *   - All operations are blocking and intended for use in non-interrupt context.
 */

#ifndef NFC_TAG_DRIVER_INTERFACE_H_
#define NFC_TAG_DRIVER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define NFC_TAG_MAX_EEPROM_SIZE         (512u) // ST25DV04K has 4kbit (512 byte) EEPROM
#define NFC_TAG_DATA_AREA_START_ADDRESS (32u)  // First 32 bytes reserved. Data area starts at byte 32.
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @enum ST25DV_DRV_ERROR
 * @brief Error codes for ST25DV NFC driver operations.
 *
 * Enumerates possible error conditions for mailbox, EEPROM, and I2C operations.
 */
typedef enum
{
   ST25DV_DRV_ERROR_NONE = 0,           /**< No error. */
   ST25DV_DRV_ERROR_PTR_NULL,           /**< Null pointer passed to function. */
   ST25DV_DRV_ERROR_INIT,               /**< Initialization error. */
   ST25DV_DRV_ERROR_FAILED_I2C,         /**< I2C communication failure. */
   ST25DV_DRV_ERROR_BUFFER_OVERFLOW,    /**< Buffer overflow detected. */
   ST25DV_DRV_ERROR_OVERFLOW,           /**< General overflow error. */
   ST25DV_DRV_ERROR_UNIT_UNINITIALIZED, /**< Driver or hardware not initialized. */
   ST25DV_DRV_ERROR_NO_MESSAGE,         /**< No mailbox message available. */
   ST25DV_DRV_ERROR_BUSY,               /**< Device or resource busy. */
   ST25DV_DRV_ERROR_FAILED_I2C_UNLOCK,  /**< Failed to unlock I2C session. */
   ST25DV_DRV_ERROR_OUT_OF_RANGE,       /**< Argument out of valid range. */
   ST25DV_DRV_ERROR_NOT_SUPPORTED,      /**< Operation not supported in this build. */
   ST25DV_DRV_ERROR_MAX                 /**< Maximum error code value. */
} ST25DV_DRV_ERROR;

/**
 * @struct st25dv_mb_status_t
 * @brief Structure representing the status of the ST25DV NFC tag mailbox.
 *
 * Contains mailbox control flags, message length, and status bits indicating message presence and mailbox enable state.
 */
typedef struct
{
   uint8_t raw_ctrl;   /**< MB_CTRL_Dyn snapshot */
   uint8_t raw_len_m1; /**< MB_LEN_Dyn snapshot (N-1) */
   uint16_t msg_len;   /**< computed length N = raw_len_m1 + 1 */
   bool enabled;       /**< MB_EN set */
   bool rf_put_msg;    /**< RF->I2C message present */
   bool host_put_msg;  /**< I2C->RF message present */
} st25dv_mb_status_t;

struct comms_driver;                                                // Forward declaration
typedef struct nfc_tag_driver_interface nfc_tag_driver_interface_t; // Forward declaration

typedef struct nfc_tag_driver_interface
{
   struct comms_driver *parent; // Reference to the containing instance.

   /**
    * @brief Get the current status of the NFC tag mailbox.
    *
    * @param interface Pointer to the NFC tag driver interface.
    * @param status Pointer to mailbox status structure to be filled.
    * @return result_t Result code indicating success or error.
    */
   result_t (*get_status)(const nfc_tag_driver_interface_t *const interface, st25dv_mb_status_t *status);

   /**
    * @brief Get and clear the NFC tag interrupt status.
    *
    * @param interface Pointer to the NFC tag driver interface.
    * @param interrupt_fired Pointer to bool to receive interrupt status (true if interrupt fired).
    * @return result_t Result code indicating success or error.
    */
   result_t (*get_interrupt_status)(const nfc_tag_driver_interface_t *const interface, bool *interrupt_fired);

   /**
    * @brief Read data from the NFC tag EEPROM in blocking mode. Note the max size is 512 bytes for ST25DV04K.
    *
    * @param interface Pointer to the NFC tag driver interface.
    * @param address EEPROM address to start reading from.
    * @param buffer Pointer to buffer to store read data.
    * @param length Number of bytes to read.
    * @return result_t Result code indicating success or error.
    */
   result_t (*read_eeprom_blocking)(const nfc_tag_driver_interface_t *const interface,
                                    uint16_t address,
                                    uint8_t *buffer,
                                    uint16_t length);

   /**
    * @brief Write data to the NFC tag EEPROM in blocking mode. Note the max size is 512 bytes for ST25DV04K.
    *
    * @param interface Pointer to the NFC tag driver interface.
    * @param address EEPROM address to start writing to.
    * @param data Pointer to data to write.
    * @param length Number of bytes to write.
    * @return result_t Result code indicating success or error. Writes to Area 1 (first 32 bytes) are rejected.
    */
   result_t (*write_eeprom_blocking)(const nfc_tag_driver_interface_t *const interface,
                                     uint16_t address,
                                     const uint8_t *data,
                                     uint16_t length);

} nfc_tag_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // NFC_TAG_DRIVER_INTERFACE_H_
