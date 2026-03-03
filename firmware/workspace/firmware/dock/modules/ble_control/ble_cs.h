/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ble_cs.h
 * @ingroup ble_module
 * @brief Defines the custom BLE service for the device.
 */

#ifndef BLE_CS_H_
#define BLE_CS_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "ble.h"
#include "ble_srv_common.h"
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @brief Macro to define and initialize a ble_cs_t instance.
 *
 * This macro creates a static instance of the ble_cs_t structure and initializes it with the provided name.
 * It also registers a BLE observer for the custom service, which will call the ble_cs_on_ble_evt function
 * whenever a BLE event occurs.
 *
 * @param[in] _name The name of the ble_cs_t instance to be created.
 */
#define BLE_CS_DEF(_name)                                                                                              \
   static ble_cs_t _name;                                                                                              \
   NRF_SDH_BLE_OBSERVER(_name##_obs, BLE_HRS_BLE_OBSERVER_PRIO, ble_cs_on_ble_evt, &_name)

#define CONTROL_SERVICE_UUID_BASE                                                                                      \
   {                                                                                                                   \
      0x86, 0xBF, 0x11, 0x42, 0xAD, 0xC7, 0xFF, 0xA8, 0x0A, 0x43, 0x00, 0xEB, 0xAA, 0x7C                               \
   }

#define CONTROL_SERVICE_UUID 0x1500 // Custom Service UUID
// Input to device:
#define UUID_DATA_RX 0x1508 // App -> device data pipe (replaces NUS RX)
#define UUID_DATA_TX 0x1509 // Device -> app data pipe (replaces NUS TX, notify)

// Bit positions in the Characteristic Properties bit-field - See ble_gatt_char_props_t in ble_gatts.h
#define BLE_GATT_CHAR_PROPERTIES_BROADCAST          (1 << 0) // Broadcasting of the value permitted.
#define BLE_GATT_CHAR_PROPERTIES_READ               (1 << 1) // Reading the value permitted.
#define BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESP (1 << 2) // Writing the value with Write Command permitted.
#define BLE_GATT_CHAR_PROPERTIES_WRITE              (1 << 3) // Writing the value with Write Request permitted.
#define BLE_GATT_CHAR_PROPERTIES_NOTIFY             (1 << 4) // Notification of the value permitted.
#define BLE_GATT_CHAR_PROPERTIES_INDICATE           (1 << 5) // Indications of the value permitted.
#define BLE_GATT_CHAR_PROPERTIES_AUTH_SIGNED_WR     (1 << 6) // Writing the value with Signed Write Command permitted.

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   BLE_CS_ERROR_NONE = 0,
   BLE_CS_ERROR_NULL_PTR,
   BLE_CS_ERROR_NRF_ERROR,
   BLE_CS_ERROR_INVALID_LENGTH,
   BLE_CS_ERROR_FAILED_TO_PARSE_DOSE_SCHEDULE,
   BLE_CS_ERROR_MAX
} BLE_CS_ERROR;

/**
 * @brief Enumeration of Custom Service events.
 *
 * This enumeration lists the different events that can occur in the Custom Service.
 */
typedef enum
{
   BLE_CS_EVT_NOTIFICATION_ENABLED,  // Custom value notification enabled event. */
   BLE_CS_EVT_NOTIFICATION_DISABLED, // Custom value notification disabled event. */
   BLE_CS_EVT_DISCONNECTED,
   BLE_CS_EVT_CONNECTED,
   // Custom service events
   BLE_CS_EVT_RX_DATA_RECEIVED,
} BLE_CS_EVT;

/**
 * @brief Custom service event structure.
 *
 */
typedef struct
{                       // Events for characteristics being written to
   BLE_CS_EVT evt_type; // Type of event.
   const uint8_t *p_rx_data;
   uint16_t rx_data_len;
} ble_cs_evt_t;

// Forward declaration of the ble_cs_t type.
typedef struct ble_cs_s ble_cs_t;

// Custom Service event handler type.
typedef void (*ble_cs_evt_handler_t)(ble_cs_t *p_cs, ble_cs_evt_t *p_evt);

// Custom Service initialization structure.
typedef struct
{
   ble_cs_evt_handler_t evt_handler; // Event handler to be called for handling events in the Custom Service.
   uint8_t initial_custom_value;     // Initial custom value
   ble_srv_cccd_security_mode_t
      custom_value_char_attr_md;     // Initial security level for Custom characteristics attribute
} ble_cs_init_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/**
 * @brief Custom service structure. This contains all data relevant to the Custom Service.
 *
 */
struct ble_cs_s
{
   ble_cs_evt_handler_t evt_handler; // Event handler to be called for handling events in the Custom Service.
   uint16_t service_handle;          // Handle of Custom Service (as provided by the BLE stack).
   ble_gatts_char_handles_t epoch_time_handle;
   ble_gatts_char_handles_t set_dose_schedule_handle;
   ble_gatts_char_handles_t error_handle;
   ble_gatts_char_handles_t dose_event_handle;
   ble_gatts_char_handles_t temperature_handle;
   ble_gatts_char_handles_t calibration_handle;
   ble_gatts_char_handles_t device_command_handle;
   ble_gatts_char_handles_t data_rx_handle;
   ble_gatts_char_handles_t data_tx_handle;
   uint16_t conn_handle; // Handle of the current connection (as provided by the BLE stack, is BLE_CONN_HANDLE_INVALID
                         // if not in a connection).
   uint8_t uuid_type;
};

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Function for initializing the Custom Service.
 *
 * @param[out]  p_cs       Custom Service structure. This structure will have to be supplied by
 *                          the application. It will be initialized by this function, and will later
 *                          be used to identify this particular service instance.
 * @param[in]   p_cs_init  Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
ret_code_t ble_cs_init(ble_cs_t *p_cs, const ble_cs_init_t *p_cs_init);

/**
 * @brief Function for handling the Application's BLE Stack events.
 *
 * Handles all events from the BLE stack of interest to the Custom Service. This is related to incoming events to the
 * peripheral device, not outgoing events to the central device (typically an mobile phone).
 *
 * @param[in]   p_cs      Custom Service structure.
 * @param[in]   p_ble_evt Event received from the BLE stack.
 */
void ble_cs_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**
 * @brief Updates a characteristic in the BLE service.
 *
 * This function writes a new value to the specified characteristic in the BLE service.
 * If the 'send_notify' parameter is set to true, it also sends a notification to the connected peer.
 *
 * @param[in] p_cs             Pointer to the BLE Custom Service structure.
 * @param[in] handle           The handle of the characteristic to be updated.
 * @param[in] p_value          Pointer to the new value to be written to the characteristic.
 * @param[in] value_len        The length of the new value.
 * @param[in] send_notify      If true, a notification will be sent to the connected peer.
 *
 * @return A result code indicating the success or failure of the operation.
 */
result_t ble_cs_characteristic_update(
   const ble_cs_t *p_cs, uint16_t handle, const void *p_value, uint16_t value_len, bool send_notify);

#endif // BLE_CS_H_
