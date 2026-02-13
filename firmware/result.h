/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @ingroup common
 * @brief Defines the result type and associated macros used for function status returns and the efficient handling
 * thereof.
 * @details
 *
 * @file result.h
 * @ingroup common
 * @brief
 */

#ifndef RESULT_H_
#define RESULT_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "result_macros.h"
#include <app_error.h>
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief This enum contains the unit IDs of all the firmware units in this application.
 *
 * @note Will be cast to uint8_t by debug unit; cannot exceed 255 identifiers.
 */
typedef enum
{
   // Firmware units - common
   SW_UNIT_ID_INVALID = 0,
   SW_UNIT_ID_DEBUG,
   SW_UNIT_ID_QUEUE,
   SW_UNIT_ID_UART_DRV,
   SW_UNIT_ID_TWI_DRV,
   SW_UNIT_ID_SYSTEM_TIME,
   SW_UNIT_ID_IMU_DRV,
   SW_UNIT_ID_STATE_MACHINE,
   SW_UNIT_ID_BUTTON_DRIVER,
   SW_UNIT_ID_MESSAGE_PROTOCOL,
   SW_UNIT_ID_SERIALIZER_PROTOCOL,
   SW_UNIT_ID_UART_IR_DRIVER,
   SW_UNIT_ID_SPI_DRV,
   // Firmware units - Dock
   SW_UNIT_ID_DOCK_SOURCE_MANAGER,
   SW_UNIT_ID_DOCK_COMMS_DRIVER,
   SW_UNIT_ID_RING_COMMS_DRIVER,
   SW_UNIT_ID_RING_SOURCE_MANAGER,
   SW_UNIT_ID_MAIN_DOCK,
   SW_UNIT_ID_GENERAL_CONTROL_DOCK,
   SW_UNIT_ID_BLE_CONTROL_DOCK,
   SW_UNIT_ID_BATTERY_MANAGER_DOCK,
   SW_UNIT_ID_DOSE_SCHEDULER_DOCK,
   SW_UNIT_ID_LED_CONTROL_DOCK,
   SW_UNIT_ID_BUZZER_DOCK,
   SW_UNIT_ID_BLE_CS_DOCK, // Not used
   SW_UNIT_ID_BLE_CS,
   SW_UNIT_ID_BLE_CONTROL,
   SW_UNIT_ID_BMS_NPM1300_DRV,
   SW_UNIT_ID_NOTIFICATIONS_MODULE,
   SW_UNIT_ID_RING_MANAGER,
   SW_UNIT_ID_RTC_PCF85363A_DRIVER,
   SW_UNIT_ID_SYSTEM_FSM,
   SW_UNIT_ID_ADS1235_DRV,
   SW_UNIT_ID_TEMP_DRV,
   SW_UNIT_ID_WEIGHT_SENSOR,
   SW_UNIT_ID_DOSE_SIZE_DETECTION,
   SW_UNIT_ID_DOSE_SIZE_DETECTION_FSM,
   SW_UNIT_ID_DOSE_DETECTION_DOCK,
   SW_UNIT_ID_NFC_READER,
   SW_UNIT_ID_MAIN_RING,
   SW_UNIT_ID_DOCK_MANAGER,
   SW_UNIT_ID_GENERAL_CONTROL_RING,
   SW_UNIT_ID_BATTERY_MANAGER_RING,
   SW_UNIT_ID_PROX_SENSOR_TMD2635_DRV,
   SW_UNIT_ID_CAP_DETECTION_MODULE,
   SW_UNIT_ID_TILT_DETECTION_MODULE,
   SW_UNIT_ID_DOSE_DETECTION_MODULE,
   SW_UNIT_ID_DOSE_DETECTION_FSM,
   SW_UNIT_ID_BLE_CONTROL_RING,
   SW_UNIT_ID_BLE_CS_RING,
   SW_UNIT_ID_BMS_BQ25150_DRV,
   SW_UNIT_ID_NFC_TAG_DRV,
   SW_UNIT_ID_NFC_TAG_EEPROM_QUEUE,
   SW_UNIT_ID_NFC_READER_DRV,
   SW_UNIT_ID_NFC_RFAL_GLUE_DOCK,
   SW_UNIT_ID_NFC_MOCK_SERIAL_DRIVER,
   SW_UNIT_ID_NFC_UT_MSG_PROT,
   SW_UNIT_ID_FLASH_DRV,
   SW_UNIT_ID_FILESYSTEM,
   SW_UNIT_ID_DOCK_DATA_MANAGER,
   SW_UNIT_ID_APP_MANAGER,
   SW_UNIT_ID_BASELINING_FSM,
   SW_UNIT_ID_MAX
} SW_UNIT_ID;

typedef enum
{
   DEBUG_DATA_ID_INVALID = 0,
   DEBUG_DATA_ID_DOCK,
   DEBUG_DATA_ID_RING,
   DEBUG_DATA_ID_DOCK_DATA,
   DEBUG_DATA_ID_MAX
} DEBUG_DATA_ID;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
#endif // RESULT_H_
