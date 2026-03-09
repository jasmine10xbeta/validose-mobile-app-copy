/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef BLE_CONTROL_INTERFACE_H_
#define BLE_CONTROL_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"

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
   BLE_CONTROL_ERROR_ADV_STOP,
   BLE_CONTROL_ERROR_PASSKEY_SET_FAILURE,
   BLE_CONTROL_ERROR_INVALID_INPUT,
   BLE_CONTROL_ERROR_UNINITIALIZED,
   BLE_CONTROL_ERROR_MAC_ADDRESS_GET_FAILURE,
   BLE_CONTROL_ERROR_INVALID_MAC_ADDRESS_LENGTH,
   BLE_CONTROL_ERROR_MAX
} BLE_CONTROL_ERROR;

/**
 * @brief BLE control unit status structure.
 */
typedef struct
{
   bool is_bonded;              // True if there are bonded peers
   bool is_connected;           // True if currently connected
   bool is_advertising;         // True if currently advertising
   bool allow_new_bond;         // True if new bond is allowed
   bool received_data_overflow; // True if data received over BLE exceeded buffer size
} ble_control_status_t;

struct ble_control; // Forward declaration
typedef struct ble_control_interface ble_control_interface_t;

typedef struct ble_control_interface
{
   struct ble_control *parent; // Reference to the containing instance.

   /**
    * @brief Function to stop advertising.
    *
    * @note This function also disconnects any active BLE connections.
    * @note A success result here does not necessarily mean the advertising has fully stopped,
    * as the operation is asynchronous. If the advertising ultimately fails, it will lead to a device reset to prevent
    * an unkown state.
    */
   result_t (*stop_advertising)(const ble_control_interface_t *const interface);

   /**
    * @brief Starts BLE advertising with the specified options.
    *
    * @details This function starts BLE advertising with the given options. If
    * `allow_new_bond` is true, it populates the BLE passkey with the decrypted
    * passkey text. Then, it starts advertising with the specified options.
    *
    * @param[in] allow_new_bond  A boolean indicating whether to allow new bonds.
    * @param[in] decrypted_passkey_text  A pointer to the decrypted passkey text.
    * Pass NULL if no passkey is needed.
    *
    * @return A result_t indicating the success or failure of the function.
    */
   result_t (*start_advertising)(const ble_control_interface_t *const interface,
                                 bool allow_new_bond,
                                 const char *decrypted_passkey_text);

   /**
    * @brief Function pointer to report BLE control unit status.
    *
    * @param[out] status Pointer to ble_control_status_t to be filled with
    * current status.
    *
    * @return result_t indicating success or failure.
    */
   result_t (*get_ble_status)(const ble_control_interface_t *const interface, ble_control_status_t *status);

   result_t (*get_mac_address)(const ble_control_interface_t *const interface,
                               uint8_t *mac_address,
                               uint16_t buffer_size);

} ble_control_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // BLE_CONTROL_INTERFACE_H_
