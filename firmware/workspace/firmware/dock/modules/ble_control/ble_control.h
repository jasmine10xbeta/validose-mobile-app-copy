/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * USING THIS MODULE:
 * - ble_cs.c and ble_cs.h define the custom service but does not run
 * application logic.
 * - on_cs_evt is called when a custom service event is received.
 * - ble_evt_handler CB for general BLE events
 */

/**
 * @brief
 *
 *
 * @note This module is intended to be a singleton.
 */

#ifndef BLE_CONTROL_H_
#define BLE_CONTROL_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "ble_control_interface.h"
#include "common.h"
#include "comms_driver_interface.h"
#include "debug.h"
#include "queue.h"
#include "system_time.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#ifndef OPCODE_LENGTH
#   define OPCODE_LENGTH (1u)
#endif
#ifndef HANDLE_LENGTH
#   define HANDLE_LENGTH (2u)
#endif

#define BLE_PASSKEY_LENGTH (7u) // Passkey string length including null terminator
#define MAX_BLE_MTU_SIZE   (NRF_SDH_BLE_GATT_MAX_MTU_SIZE)
#ifndef BLE_RECEIVE_DATA_BUFFER_SIZE
#   define BLE_RECEIVE_DATA_BUFFER_SIZE (256u) // Size of buffer for receiving data over BLE RX characteristic
#endif

#define BLE_RX_PACKET_MAX_ELEMENTS (12u)
#define BLE_RX_PACKET_ELEMENT_SIZE                                                                                     \
   (MAX_BLE_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH) // Assume biggest possible MTU size
#define BLE_RX_PACKET_QUEUE_SIZE (BLE_RX_PACKET_ELEMENT_SIZE * BLE_RX_PACKET_MAX_ELEMENTS)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief Struct for callback handlers.
 *
 * This struct contains function pointers to callback functions that will be
 * called when specific events occur in the BLE control module.
 *
 */
typedef struct
{
   void (*on_ble_connection_status_update)(DEVICE_BLE_EVT event);
   // Add more callback types if needed
} ble_control_evt_handlers_t;

typedef struct ble_control
{
   comms_driver_interface_t data_ifc;
   ble_control_interface_t interface;

   bool _initialized;
} ble_control_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes the BLE control module.
 *
 * This function initializes the BLE stack, gap parameters, GATT, services,
 * advertising, connection parameters, and the Peer Manager. It also initializes
 * and populates the manual whitelist with bonded devices.
 *
 * @param[in] event_handlers  Pointer to the struct containing event handlers
 * for general control for BLE events.
 *
 * @return result_t indicating success or failure.
 *
 * @note This function should be called before any other BLE control functions.
 */
result_t ble_control_init(ble_control_t *const self,
                          const ble_control_evt_handlers_t *event_handlers,
                          const queue_interface_t *ble_rx_queue_ifc);

#endif // BLE_CONTROL_H_
