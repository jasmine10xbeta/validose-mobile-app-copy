/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup ble_module Bluetooth Manager
 * @ingroup modules
 * @brief Bluetooth SoftDevice management and functionality
 * @details
 *
 * @file ble_control.h
 * @ingroup ble_module
 * @brief
 */

/**
 * USING THIS MODULE:
 * - ble_cs.c and ble_cs.h define the custom service but does not run application logic.
 * - on_cs_evt is called when a custom service event is received.
 * - ble_evt_handler CB for general BLE events
 */

#ifndef BLE_CONTROL_RING_H_
#define BLE_CONTROL_RING_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define BLE_PASSKEY_LENGTH (7u) // Passkey string length including null terminator

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef enum
{
   BLE_DEVICE_COMMAND_NONE = 0,
   BLE_DEVICE_COMMAND_ENTER_SHIP_MODE,
   BLE_DEVICE_COMMAND_RESET_DEVICE,
   BLE_DEVICE_COMMAND_CLEAR_DOSE_EVENTS,
   BLE_DEVICE_COMMAND_MAX,
} BLE_DEVICE_COMMAND;

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   // Bluetooth related errors
   BLE_CONTROL_ERROR_NONE = 0,
   BLE_CONTROL_ERROR_PTR_NULL,
   BLE_CONTROL_ERROR_NRF_ERROR,
   BLE_CONTROL_ERROR_INTERNAL,
   BLE_CONTROL_ERROR_SEC_PARAMS_REPLY,
   BLE_CONTROL_ERROR_POPULATE_WHITELIST,
   BLE_CONTROL_ERROR_LOAD_BONDING_DATA,
   BLE_CONTROL_ERROR_PM_EVT_HANDLER,
   BLE_CONTROL_ERROR_PM_EVT_ERROR_UNEXPECTED,
   BLE_CONTROL_ERROR_PM_EVT_PEER_DELETE_FAILED,
   BLE_CONTROL_ERROR_PM_EVT_PEER_DATA_UPDATE_FAILED,
   BLE_CONTROL_ERROR_ON_CONN_ERR_HANDLER,
   BLE_CONTROL_ERROR_ON_CONN_PARAMS_EVT,
   BLE_CONTROL_ERROR_ON_ADV_EVT,
   BLE_CONTROL_ERROR_DISCONNECT,
   BLE_CONTROL_ERROR_SECURE_CONNECTION,
   BLE_CONTROL_ERROR_QWR_CONN_HANDLE_ASSIGN,
   BLE_CONTROL_ERROR_GAP_PHY_UPDATE,
   BLE_CONTROL_ERROR_PM_PEER_ID_LIST,
   BLE_CONTROL_ERROR_PM_DEVICE_ID_LIST_SET,
   BLE_CONTROL_ERROR_PEERS_DELETE,
   BLE_CONTROL_ERROR_ADV_START,
   BLE_CONTROL_ERROR_PASSKEY_SET_FAILURE,
   BLE_CONTROL_ERROR_INVALID_INPUT,
   BLE_CONTROL_ERROR_MAX
} BLE_CONTROL_ERROR;

/**
 * @brief Struct for callback handlers.
 *
 * This struct contains function pointers to callback functions that will be called when specific events occur in the
 * BLE control module.
 *
 */
typedef struct
{
   void (*on_ble_connection_status_update)(DEVICE_BLE_EVT event);
   void (*on_ble_device_commands_update)(const uint32_t command);
   void (*on_ble_nus_rx_data)(const uint8_t *p_data, size_t length);

   // Add more callback types if needed
} ble_control_evt_handlers_t;
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes the BLE control module.
 *
 * This function initializes the BLE stack, gap parameters, GATT, services, advertising, connection parameters,
 * and the Peer Manager. It also initializes and populates the manual whitelist with bonded devices.
 *
 * @param[in] event_handlers  Pointer to the struct containing event handlers for general control for BLE events.
 *
 * @return result_t indicating success or failure.
 *
 * @note This function should be called before any other BLE control functions.
 */
result_t ble_control_init(const ble_control_evt_handlers_t *event_handlers);

/**
 * @brief Function to stop advertising.
 *
 * @note This function also disconnects any active BLE connections.
 */
void ble_control_stop_advertising(void);

/**
 * @brief Starts BLE advertising with the specified options.
 *
 * @details This function starts BLE advertising with the given options. If `allow_new_bond` is true,
 *          it populates the BLE passkey with the decrypted passkey text. Then, it starts advertising
 *          with the specified options.
 *
 * @param[in] allow_new_bond  A boolean indicating whether to allow new bonds.
 * @param[in] decrypted_passkey_text  A pointer to the decrypted passkey text. Pass NULL if no passkey is needed.
 *
 * @return A result_t indicating the success or failure of the function.
 */
result_t ble_control_start_advertising(bool allow_new_bond, const char *decrypted_passkey_text);

/**
 * @brief Function to check if there are any bonded peers.
 *
 * @param[out] is_bonded A pointer to a boolean variable that will be set to true if there are bonded peers, false
 * otherwise.
 *
 * @return result_t indicating success or failure.
 */
result_t ble_control_get_bonded_status(volatile bool *is_bonded);

/**
 * @brief Function to check if bonding with new peers is currently permitted.
 *
 * @param[out] is_permitting_new_bonds A pointer to a boolean variable that will be set to true if the whitelist is
 * disabled and thus new bonds are permitted.
 *
 * @return result_t indicating success or failure.
 */
result_t ble_control_is_permitting_new_bonds(volatile bool *is_permitting_new_bonds);

/**
 * @brief Function to report the battery level to the connected BLE app.
 *
 * @param[in] battery_level  The current battery level in percentage (0-100).
 *
 * @return result_t indicating success or failure.
 *
 * @note This function should be called when the battery level needs to be updated.
 * @note The function assumes that the BLE stack and the custom service are already initialized.
 * @note The function does not handle the case where there is no BLE connection.
 * @note The function does not check if the battery level is valid.
 * @note The battery level is reported as a percentage (0-100).
 */
result_t ble_control_report_battery_level(uint8_t battery_level);

/**
 * @brief Send an arbitrary byte buffer over BLE NUS (TX characteristic).
 *
 * @param[in] p_data   Pointer to the data to transmit.
 * @param[in] length   Number of bytes to send from @p p_data.
 *
 * NOTE: Splits the buffer into MTU-sized chunks so it works with any
 *       negotiated ATT MTU.  Returns immediately if no central is connected.
 */
void ble_control_send_debug_data(const uint8_t *p_data, size_t length);

#endif // BLE_CONTROL_RING_H_
