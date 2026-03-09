/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NFC_READER_DRIVER_H_
#define NFC_READER_DRIVER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver_interface.h"
// #include "nfc_reader_driver_data_interface.h"
#include "comms_driver_interface.h"
#include "nfc_reader_driver_interface.h"
#include "rfal_nrf5_port.h"
#include "system_time.h"

// RFAL dependencies
#include "rfal_analogConfig.h"
#include "rfal_nfc.h"
#include "rfal_nfcv.h"
#include "rfal_nrf5_port.h"
#include "rfal_platform.h"
#include "rfal_rf.h"
#include "rfal_st25xv.h"
#include "st25r3916.h"
#include "st25r3916_aat.h"
#include "st25r3916_com.h"
#include "st25r3916_irq.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define NFCV_TAG_UID_SIZE (8u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   NFC_R_ERROR_NONE = 0,
   NFC_R_ERROR_PTR_NULL,
   NFC_R_ERROR_INIT,
   NFC_R_ERROR_FAILED_I2C,
   NFC_R_ERROR_BUFFER_OVERFLOW,
   NFC_R_ERROR_DISCOVER_TAG,
   NFC_R_ERROR_BUSY,
   NFC_R_ERROR_GET_DATA,
   NFC_R_ERROR_SEND_DATA,
   NFC_R_ERROR_NO_TAG,
   NFC_R_ERROR_TAG_PRESENT,
   NFC_R_ERROR_UNIT_UNINITIALIZED,
   NFC_R_ERROR_OUT_OF_RANGE,
   NFC_R_ERROR_SET_OUTPUT_PWR,
   NFC_R_ERROR_TUNE_ANT,
   NFC_R_ERROR_WRONG_STATE,
   NFC_R_ERROR_POWER_STATE,
   NFC_R_ERROR_MAX,
} NFC_R_ERROR;

/**
 * @brief Caches discovery and quiet-state information for each supported tag type.
 */
typedef struct
{
   bool is_present;                   /**< Indicates whether a tag of this type is currently detected. */
   uint8_t uid[NFC_TAG_MAX_UID_SIZE]; /**< Most recently observed UID for this tag type (MSB first). */
   uint8_t uid_len;                   /**< Stored UID length in bytes. */
} nfc_tag_cache_t;

typedef struct comms_driver
{
   // nfc_driver_data_interface_t data_ifc;
   comms_driver_interface_t data_ifc;
   nfc_driver_interface_t interface;
   const i2c_driver_interface_t *_i2c_ifc;
   const system_time_interface_t *_systick_ifc;

   bool _is_tag_present;
   uint8_t _tag_uid[NFC_TAG_MAX_UID_SIZE];
   uint8_t _tag_uid_len;
   NFC_TAG_TYPE _configured_tag_type;
   uint8_t _output_power_level; /**< Output power level (0-15, max power = 15) */
   uint64_t _last_presence_check_ms;
   uint64_t _last_discovery_ms;
   bool _gt_ready;
   bool _gt_started;

   nfc_tag_cache_t _tag_cache[NFC_TAG_TYPE_MAX]; /**< Cached state per tag family (A / V). */
   bool _tag_quiet[NFC_TAG_TYPE_MAX];            /**< Tracks whether a tag family has been put into quiet state. */

   bool _initializing;
   bool _initialized;
} nfc_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t nfc_driver_init(nfc_driver_t *const self,
                         const i2c_driver_interface_t *i2c_interface,
                         const system_time_interface_t *systick_interface);

#endif // NFC_READER_DRIVER_H_
