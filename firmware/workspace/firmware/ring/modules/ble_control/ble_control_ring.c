
/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ble_control.c
 * @ingroup ble_module
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_error.h"
#include "app_timer.h"
#include "ble.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_bas.h"
#include "ble_conn_params.h"
#include "ble_conn_state.h"
#include "ble_dfu.h"
#include "ble_dis.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_nus.h"
#include "ble_racp.h"
#include "ble_srv_common.h"
#include "bsp_btn_ble.h"
#include "fds.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_lesc.h"
#include "nrf_ble_qwr.h"
#include "nrf_bootloader_info.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "nrf_power.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdm.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "sensorsim.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SEGGER_RTT.h"

// Custom includes
#include "ble_control_ring.h"
#include "ble_cs_ring.h"
#include "common.h"
#include "debug.h"
#include "version.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BLE_CONTROL_RING;
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define MAX_BONDED_DEVICES (1u)

#define NUS_SERVICE_UUID_TYPE                                                                                          \
   BLE_UUID_TYPE_VENDOR_BEGIN /**< UUID type for the Nordic UART Service (vendor specific). */

// DIS: Device Information Service details. @note Do not delete DIS macros, simply define empty strings if an item
// should not be included in the service.
#define BASE_DEVICE_NAME                                                                                               \
   "VAL-OP" // Name of device. Will get partial BLE MAC appended and be included in the advertising data. Update
            // `DEVICE_NAME_LEN` accordingly when changing this string.
#define DEVICE_NAME_LEN (14u) // strlen(BASE_DEVICE_NAME) + space + last 6 digits of BLE MAC + null terminator

#define APP_ADV_FAST_INTERVAL (400u) // The advertising interval (units = 0.625 ms).
#define APP_ADV_SLOW_INTERVAL (320u) // The slow advertising interval (units = 0.625 ms). NOT USED.

#define APP_ADV_DURATION (0u) // The advertising duration in units of 10 milliseconds. 0 = continuous advertising.
#define APP_BLE_OBSERVER_PRIO                                                                                          \
   3 // Application's BLE observer priority. You shouldn't need to modify this value. @note do not add brackets to this
     // value, it upsets the compiler.
#define APP_BLE_CONN_CFG_TAG (1u) // A tag identifying the SoftDevice BLE configuration.

#define MIN_CONN_INTERVAL MSEC_TO_UNITS(100, UNIT_1_25_MS) // Minimum acceptable connection interval (0.1 seconds).

#define MAX_CONN_INTERVAL MSEC_TO_UNITS(200, UNIT_1_25_MS) // Maximum acceptable connection interval (0.2 second).

#define SLAVE_LATENCY    (0u)                            // Slave latency.
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(4000, UNIT_10_MS) // Connection supervisory timeout (4 seconds).

#define FIRST_CONN_PARAMS_UPDATE_DELAY                                                                                 \
   APP_TIMER_TICKS(5000) // Time from initiating event (connect or start of notification) to first time
                         // sd_ble_gap_conn_param_update is called (5 seconds).
#define NEXT_CONN_PARAMS_UPDATE_DELAY                                                                                  \
   APP_TIMER_TICKS(30000) // Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds).
#define MAX_CONN_PARAMS_UPDATE_COUNT 3 // Number of attempts before giving up the connection parameter negotiation.

#define NOTIFICATION_INTERVAL APP_TIMER_TICKS(3000)

#define SEC_PARAM_BOND            (1u) // Perform bonding.
#define SEC_PARAM_MITM            (1u) // Man In The Middle protection
#define SEC_PARAM_LESC            (0u) // LE Secure Connections
#define SEC_PARAM_KEYPRESS        (0u) // Keypress notifications
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_DISPLAY_ONLY
#define SEC_PARAM_OOB             (0u)  // Out Of Band data
#define SEC_PARAM_MIN_KEY_SIZE    (16u) // Minimum encryption key size.
#define SEC_PARAM_MAX_KEY_SIZE    (16u) // Maximum encryption key size.

#define PASSKEY_TXT        "" // Message to be displayed together with the pass-key.
#define PASSKEY_TXT_LENGTH 0  // Length of message to be displayed together with the pass-key.

#define DEAD_BEEF                                                                                                      \
   0xDEADBEEF // Value used as error code on stack dump, can be used to identify stack location on stack unwind.

#define BLE_DISCONNECT_RETRY_DELAY_MS (2u)
// Limited pairing attempts
#define MAX_PAIRING_ATTEMPTS    (3u)
#define PAIRING_TIMEOUT_SECONDS (300u)

#define FAILURE_RETRY_DELAY_MS           (300u)
#define FAILURE_MAX_RETRY_COUNT          (3u)
#define STOP_ADVERTISING_MAX_RETRY_COUNT (5u)

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT); /**< BLE NUS service instance. NUS = Nordic UART Service */
NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);             // GATT module instance.
BLE_CS_DEF(m_cs);                   // Context for the Queued Write module.
BLE_BAS_DEF(m_bas);                 // Declare battery service instance.
                                    // Context for the Queued Write module.
BLE_ADVERTISING_DEF(m_advertising); // Advertising module instance.

APP_TIMER_DEF(m_pairing_timer); // Pairing block timer instance.
APP_TIMER_DEF(m_gap_evt_phy_update_retry_timer);
APP_TIMER_DEF(m_disconnect_retry_timer);
APP_TIMER_DEF(m_connect_qwr_retry_timer);
APP_TIMER_DEF(m_stop_advertising_timer);

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct
{
   ble_gap_addr_t addr; // Peer's Bluetooth address. @note this could be a Resolvable Private Address (RPA).
   ble_gap_irk_t irk;   // Identity Resolving Key.
} bonded_device_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
/**
 * @brief Handler for pairing timeout.
 *
 * This function resets the pairing attempts counter and allows new pairing attempts.
 *
 * @param[in] p_context Pointer to context. Not used in this function.
 */
static void pairing_timeout_handler(void *p_context);

/**
 * @brief Handler for pairing failure.
 *
 * This function increments the pairing attempts counter and disables pairing attempts if the maximum number of attempts
 * is reached.
 */
static void pairing_failed_handler(void);

/**
 * @brief Delete all peers except the specifified ID.
 *
 * @param protected_peer_id The peer ID to keep
 */
static void delete_all_peers_except(const pm_peer_id_t protected_peer_id);

/**
 * @brief Function for handling Peer Manager events.
 *
 * See peer_manager_types.h for types of events that can come from the @ref peer_manager module.
 *
 * This function processes the events generated by the Peer Manager module.
 * It checks for connection security success, retrieves the connection security status,
 * and logs relevant information.
 *
 * @param[in] p_evt  Pointer to the Peer Manager event structure.
 *
 * @return None.
 */
static void pm_evt_handler(pm_evt_t const *p_evt);

/**
 * @brief Function to start advertising.
 *
 * @param[in] allow_new_bond  Boolean indicating whether to bypass whitelist when advertising.
 *
 * @return void
 */
static void advertising_start(bool allow_new_bond);

/**
 * @brief Initializes the application timers for the BLE control module.
 *
 * @note This function should be called before any other BLE control functions are used.
 *
 * @return void
 */
static result_t ble_timers_init(void);

/**
 * @brief Function to initialize the GAP parameters for the BLE stack.
 *
 * This function sets up the GAP parameters, such as device name, appearance, and privacy settings.
 * It also configures the advertising parameters, including advertising interval, power level,
 * and advertising data.
 *
 * @return A notification object containing the status of the operation and any relevant error code.
 */
static result_t gap_params_init(void);

/**
 * @brief Function to initialize the Generic Attribute Profile (GATT) layer.
 *
 * This function sets up the GATT layer by initializing the attribute table,
 * and registering the services to be included in the advertisement packet.
 *
 * @note Remember to increase the NRF_SDH_BLE_VS_UUID_COUNT in sdk_config for every added UUID
 * @note Only include the device information service in the advertising packet.
 *
 * @return void
 */
static result_t gatt_init(void);

/**
 * @brief Function for handling Queued Write Module errors.
 *
 * A pointer to this function will be passed to each service which may need to inform the application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error);

/**
 * @brief Function for handling buttonless DFU events.
 *
 * @param[in]   event   The BLE buttonless DFU event.
 */
static void ble_dfu_event_handler(ble_dfu_buttonless_evt_type_t event);

/**
 * @brief Function for handling the Custom Service Service events.
 *
 * This function will be called for all Custom Service events which are passed to the application.
 *
 * @param[in]   p_cs_service  Custom Service structure.
 * @param[in]   p_evt          Event received from the Custom Service.
 */
static void on_cs_evt(ble_cs_t *p_cs_service, ble_cs_evt_t *p_evt);

/**
 * @brief Function for initializing services that will be used by the application.
 */
static result_t services_init(void);

/**
 * @brief Function for handling events from the GATT library.
 * */
static void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt);

/**
 * @brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt);

/**
 * @brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error);

/**
 * @brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt);

/**
 * @brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context);

/**
 * @brief Function to disconnect the device from a BLE connection.
 *
 * This function attempts to disconnect the device from the specified BLE connection.
 * If the disconnection is successful, it logs a success message.
 * If the disconnection fails due to invalid parameters or an invalid connection handle, it logs a warning message.
 * If the disconnection fails due to a disconnection in progress or an unestablished link, it logs an informational
 * message and retries.
 * If the disconnection fails due to any other error, it logs an error message.
 *
 * @param conn_handle The connection handle of the BLE connection to be disconnected.
 *
 * @return void
 *
 * @note This function sends a disconnect request to the specified connection handle. The actual disconnection is a
 * synchronous operation and might be actioned some time after calling the disconnect function.
 */
static void ble_disconnect(uint16_t conn_handle, uint8_t hci_status_code);

/**
 * @brief Function for initializing the BLE stack.
 *
 * Initializes the SoftDevice and the BLE event interrupt.
 */
static result_t ble_stack_init(void);

/**
 * @brief Function for the Peer Manager initialization.
 */
static result_t peer_manager_init(void);

/**
 * @brief Function for initializing the Advertising functionality.
 */
static result_t advertising_init(void);

/**
 * @brief Function to initialize the connection parameters.
 *
 * This function sets up the connection parameters for the BLE connection.
 * It configures the minimum and maximum connection intervals, the slave latency,
 * and the connection supervision timeout.
 *
 * @return void
 */
static result_t conn_params_init(void);

/**
 * @brief Function to populate the manual whitelist with bonded devices.
 *
 * This function retrieves the list of bonded devices from the Peer Manager,
 * and populates the manual whitelist with their addresses and IRKs.
 *
 * @note The manual whitelist is used for filtering connections during advertising.
 *
 * @return None.
 */
static void manual_whitelist_populate(void);

/**
 * @brief Function to check if a device address is in the whitelist.
 *
 * This function checks if a given device address is present in the manual whitelist.
 * The function uses a simple linear search to iterate over the whitelist and compare
 * each address with the given address.
 *
 * @param[in] addr  Pointer to the device address to check.
 *
 * @return True if the address is found in the whitelist, False otherwise.
 */
static bool is_device_in_whitelist(const ble_gap_addr_t *addr);

/**
 * @brief Function to clear the manual whitelist.
 *
 * This function resets the whitelist array and size to zero.
 * It is used to prepare for a new set of bonded devices to be added to the whitelist.
 *
 * @note This function does not perform any operations related to the Peer Manager or the SoftDevice.
 *
 * @return void
 */
void manual_whitelist_clear(void);

/**
 * @brief Function to print a BLE GAP address in a readable format.
 *
 * @details This function takes a pointer to a @ref ble_gap_addr_t structure and prints the address in a readable
 * format. The address is printed in the format "Address: XX:XX:XX:XX:XX:XX", where XX represents each byte of the
 * address.
 *
 * @param[in] addr  Pointer to the BLE GAP address to print.
 *
 * @return void
 */
static void print_ble_gap_addr(const ble_gap_addr_t *addr);

/**
 * @brief Function for handling advertising errors.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void ble_advertising_error_handler(uint32_t nrf_error);

/**
 * @brief This function handles the retry timer for GAP events related to PHY update.
 *
 * @details This function is called when the timer for retrying a GAP event related to PHY update expires.
 *          It checks if the connection handle is valid. If it is, it attempts to update the PHY.
 *          If the update fails due to a busy condition and the retry count is less than the maximum retry count,
 *          it starts another timer for the next retry. If the update fails due to other reasons or the retry count
 *          exceeds the maximum, it escalates the error to the application.
 *
 * @param[in] p_context: Pointer to the context. Not used in this function.
 */
static void retry_timer_handler_gap_evt_phy_update(void *p_context);

/**
 * @brief Function to retrieve the security parameters for BLE connections. This is the single source of truth for
 * security parameters.
 *
 * @param[out] sec_param Pointer to a ble_gap_sec_params_t structure where the security parameters will be stored.
 *
 * @return A result_t value indicating the success or failure of the function.
 *         - RESULT_OK if the security parameters were successfully retrieved.
 *         - BLE_CONTROL_ERROR_PTR_NULL if the provided pointer to the security parameters is NULL.
 */
static result_t get_security_params(ble_gap_sec_params_t *sec_param);

/**
 * @brief Handles the retry mechanism for disconnecting from a BLE connection.
 *
 * This function is called when the disconnect retry timer expires. It attempts to disconnect from the BLE connection
 * using the SoftDevice API. If the disconnect fails due to an invalid state, it will retry the disconnect after a
 * delay. If the maximum number of retry attempts is reached, it will set an error result and enqueue it to the main
 * application.
 *
 * @param[in] p_context: Unused parameter required by the SoftDevice BLE event observer.
 */
static void disconnect_retry_handler(void *p_context);

/**
 * @brief Timer handler for trying to stop advertising.
 *
 * This function is called when the stop advertising timer expires. It attempts to stop advertising
 * a maximum of STOP_ADVERTISING_MAX_RETRY_COUNT times. If the advertising is still running after
 * the maximum number of retries, the system is reset to prevent uncertain behavior.
 *
 * @param[in] p_context Pointer to the context passed to the timer handler. Not used in this function.
 *
 * @note The function @p sd_ble_gap_adv_stop() is only called in this function so
 * @p stop_advertising_retry_timer_handler() serves both as the first try and the retry mechanism to attempt to stop
 * advertising.
 */
static void stop_advertising_retry_timer_handler(void *p_context);

/**
 * @brief Function for handling the data from the Nordic UART Service. This function will process the data received from
 * the Nordic UART BLE Service.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
static void nus_rx_data_handler(ble_nus_evt_t *p_evt);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/*Globals*/

static uint8_t m_disconnect_retry_count = 0;
static uint8_t m_bt_passkey[BLE_PASSKEY_LENGTH] = {0x00}; // This should be ASCII values for digits.

static char m_bt_devicename[DEVICE_NAME_LEN] = {0x00};

static pm_peer_id_t m_peer_id;

static ble_control_evt_handlers_t m_event_handlers; // Event handler for general control for BLE events.

static ble_opt_t m_static_pin_option; // Pointer to the struct containing static pin option. */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; // Handle of the current connection. */
static uint16_t m_retry_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t m_qwr_retry_count = 0;

// Manual whitelist
static bonded_device_t m_whitelist[MAX_BONDED_DEVICES];
static uint8_t m_whitelist_size = 0;
static bool m_is_whitelist_enabled = true;
static bool m_is_advertising = false;

// Limited pairing attempts
static uint8_t m_pairing_attempts = 0;
static bool m_is_pairing_allowed = true;

// Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module.
static uint16_t m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;

/**
 * @brief UUIDs to be included in the advertisement packet.
 *
 * Other services which can be included in the advertisement packet can be added here.
 * Examples:
 * {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE},
 * {CONTROL_SERVICE_UUID, BLE_UUID_TYPE_VENDOR_BEGIN}
 *
 * @note Remember to increase the NRF_SDH_BLE_VS_UUID_COUNT in sdk_config for every added UUID
 */
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},
                                   {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE},
                                   {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}};

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/

static void nus_rx_data_handler(ble_nus_evt_t *p_evt)
{
   if(p_evt->type == BLE_NUS_EVT_RX_DATA)
   {
      DEBUG_INFO("BLE NUS data received.");
      m_event_handlers.on_ble_nus_rx_data(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
   }
}

static void stop_advertising_retry_timer_handler(void *p_context) // NOSONAR: p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   static uint8_t retry_count = 0;
   ret_code_t err_code = NRF_SUCCESS;

   // If advertising has already been stopped, do nothing
   if(!m_is_advertising)
   {
      DEBUG_WARNING("Advertising already stopped. Ignoring retry.");
      return;
   }

   DEBUG_INFO("Trying to stop advertising (Attempt %d/%d)...", retry_count + 1, STOP_ADVERTISING_MAX_RETRY_COUNT);

   // Check if there are active connections. If so, wait for disconnection before retrying
   if(BLE_CONN_HANDLE_INVALID == m_conn_handle)
   {
      err_code = sd_ble_gap_adv_stop(m_advertising.adv_handle);
      if(NRF_SUCCESS == err_code)
      {
         DEBUG_INFO("Advertising stopped successfully.");
         m_is_advertising = false;
         m_is_pairing_allowed = false;
         retry_count = 0;
         return;
      }
      else if(NRF_ERROR_INVALID_STATE == err_code)
      {
         DEBUG_WARNING("Failed to stop advertising. NRF_ERROR_INVALID_STATE");
         retry_count++;
      }
      else
      {
         DEBUG_WARNING("Failed to stop advertising. Error: 0x%02X", err_code);
         retry_count++;
      }
   }
   else
   {
      DEBUG_WARNING("Cannot stop advertising while there are active connections. Waiting for disconnection...");
      retry_count++;
   }

   if(retry_count < STOP_ADVERTISING_MAX_RETRY_COUNT)
   {
      err_code = app_timer_start(m_stop_advertising_timer,
                                 APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), // Retry delay in ms
                                 NULL);
      if(err_code != NRF_SUCCESS)
      {
         DEBUG_WARNING("Failed to start stop advertising timer. Error: 0x%02X", err_code);
      }
   }
   else
   {
      DEBUG_WARNING("Maximum retries reached. Could not stop advertising.");
      retry_count = 0;
      // Unable to recover from something that went wrong. To prevent uncertain behavior, reset the system.
      APP_ERROR_CHECK(err_code);
   }
}

static void connect_qwr_timeout_handler(void *p_context) // NOSONAR: p_context required by SDK
{
   UNUSED_PARAMETER(p_context); // No context used
   ret_code_t err_code;

   err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_retry_conn_handle);
   if(NRF_SUCCESS == err_code)
   {
      DEBUG_INFO("QWR connection handle assigned successfully.");
      m_qwr_retry_count = 0; // Reset retry counter

      // Set TX power
      err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_retry_conn_handle, 4); // +4 dBm
      switch(err_code)
      {
         case NRF_SUCCESS:
            DEBUG_WARNING("Transmit power set successfully");
            break;

         case NRF_ERROR_INVALID_PARAM:
            DEBUG_WARNING("sd_ble_gap_tx_power_set() returned: NRF_ERROR_INVALID_PARAM");
            break;

         case BLE_ERROR_INVALID_ADV_HANDLE:
            DEBUG_WARNING("sd_ble_gap_tx_power_set() returned: BLE_ERROR_INVALID_ADV_HANDLE");
            break;

         case BLE_ERROR_INVALID_CONN_HANDLE:
            DEBUG_WARNING("sd_ble_gap_tx_power_set() returned: BLE_ERROR_INVALID_CONN_HANDLE");
            break;

         default:
            DEBUG_WARNING("sd_ble_gap_tx_power_set() returned unknown error");
            break;
      }

      // Push event to the main application
      m_event_handlers.on_ble_connection_status_update(DEVICE_BLE_EVT_CONNECTED);
      m_conn_handle = m_retry_conn_handle; // Store the connection handle globally
   }
   else
   {
      DEBUG_WARNING("Failed to assign QWR conn handle (retry %d). Error: 0x%02X", m_qwr_retry_count + 1, err_code);
      m_qwr_retry_count++;

      if(m_qwr_retry_count <= FAILURE_MAX_RETRY_COUNT)
      {
         // Restart timer for next retry
         err_code = app_timer_start(m_connect_qwr_retry_timer, APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), NULL);
         if(err_code != NRF_SUCCESS)
         {
            DEBUG_WARNING("Failed to restart QWR retry timer. Error: 0x%02X", err_code);
         }
      }
      else
      {
         DEBUG_ERROR("Max retry attempts reached for QWR connection handle assignment.");
      }
   }
}

static void pairing_timeout_handler(void *p_context) // NOSONAR: p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   // Reset pairing attempts and allow new pairing attempts
   m_pairing_attempts = 0;
   m_is_pairing_allowed = true;
}

static void pairing_failed_handler(void)
{
   m_pairing_attempts++;
   if(m_pairing_attempts >= MAX_PAIRING_ATTEMPTS)
   {
      m_is_pairing_allowed = false;
      app_timer_start(m_pairing_timer, APP_TIMER_TICKS(PAIRING_TIMEOUT_SECONDS * 1000), NULL);
   }
}

static void ble_advertising_error_handler(uint32_t nrf_error)
{
   UNUSED_PARAMETER(nrf_error); // Unused if debug disabled
   DEBUG_WARNING("Advertising failed with error code: %d", nrf_error);
}

static void print_ble_gap_addr(const ble_gap_addr_t *addr)
{
   UNUSED_PARAMETER(addr); // Unused if debug disabled
   DEBUG_INFO("Address: %02X:%02X:%02X:%02X:%02X:%02X",
              addr->addr[5],
              addr->addr[4],
              addr->addr[3],
              addr->addr[2],
              addr->addr[1],
              addr->addr[0]);
}

void manual_whitelist_clear(void)
{
   memset(m_whitelist, 0, sizeof(m_whitelist));
   m_whitelist_size = 0;

   DEBUG_INFO("Manual whitelist cleared.");
}

static void manual_whitelist_populate(void)
{
   pm_peer_id_t peer_ids[MAX_BONDED_DEVICES];
   uint32_t peer_id_count = MAX_BONDED_DEVICES;
   ret_code_t err_code;

   err_code = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
   if(NRF_SUCCESS != err_code)
   {
      DEBUG_ERROR("Failed pm_peer_id_list(), error: %d", err_code);
   }

   m_whitelist_size = 0;
   pm_peer_data_bonding_t peer_data;

   for(uint32_t idx = 0; idx < peer_id_count; idx++)
   {
      err_code = pm_peer_data_bonding_load(peer_ids[idx], &peer_data);

      // Delay necessary to give pm_peer_data_bonding_load() time to complete. Leads to system reset if not included.
      // The 5ms was the smallest delay (determined empirically) that consistently resolved the issue.
      nrf_delay_ms(5);

      if(NRF_SUCCESS != err_code)
      {
         DEBUG_ERROR("Failed pm_peer_data_bonding_load(), error: %d", err_code);
      }

      if(m_whitelist_size < MAX_BONDED_DEVICES)
      {
         memcpy(&m_whitelist[m_whitelist_size].addr, &peer_data.peer_ble_id.id_addr_info, sizeof(ble_gap_addr_t));
         memcpy(&m_whitelist[m_whitelist_size].irk, &peer_data.peer_ble_id.id_info, sizeof(ble_gap_irk_t));
         m_whitelist_size++;
      }
   }
   DEBUG_INFO("Manual whitelist populated with %d devices.", m_whitelist_size);
}

static bool is_device_in_whitelist(const ble_gap_addr_t *addr)
{
   for(uint8_t idx = 0; idx < m_whitelist_size; idx++)
   {
      if(0 == (memcmp(addr->addr, m_whitelist[idx].addr.addr, BLE_GAP_ADDR_LEN))
         || (pm_address_resolve(addr, &m_whitelist[idx].irk)))
      {
         return true;
      }
   }
   return false;
}

static void delete_all_peers_except(const pm_peer_id_t protected_peer_id)
{
   uint32_t peer_count = PM_PEER_ID_N_AVAILABLE_IDS;
   pm_peer_id_t peer_ids[PM_PEER_ID_N_AVAILABLE_IDS];

   if(PM_PEER_ID_INVALID == protected_peer_id)
   {
      DEBUG_ERROR("Attempt to delete invalid peer ID!");
      return;
   }

   DEBUG_INFO("Deleting all peers except ID %u", protected_peer_id);

   ret_code_t err_code = pm_peer_id_list(peer_ids, &peer_count, PM_PEER_ID_INVALID, PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);

   if(NRF_SUCCESS != err_code)
   {
      DEBUG_ERROR("Unable to retrieve peer list: %u", err_code);
      return;
   }

   for(uint32_t idx = 0; idx < peer_count; idx++)
   {
      if(protected_peer_id != peer_ids[idx])
      {
         pm_peer_delete(peer_ids[idx]);
      }
   }
}

static void pm_evt_handler(pm_evt_t const *p_evt)
{
   // current
   ret_code_t err_code;

   pm_handler_on_pm_evt(p_evt);
   pm_handler_disconnect_on_sec_failure(p_evt);
   pm_handler_flash_clean(p_evt);

   switch(p_evt->evt_id)
   {
      case PM_EVT_CONN_SEC_SUCCEEDED:
      {
         pm_conn_sec_status_t conn_sec_status;

         m_peer_id = p_evt->peer_id; // Assign peer ID.

         // Check if the link is authenticated (meaning at least MITM).
         err_code = pm_conn_sec_status_get(p_evt->conn_handle, &conn_sec_status);
         if(NRF_SUCCESS != err_code)
         {
            DEBUG_ERROR("Failed pm_conn_sec_status_get(), err_code %d", err_code);
            break;
         }

         DEBUG_INFO(
            "Link security status:\n connected: %d\n encrypted: %d\n mitm_protected: %d\n bonded: %d\n lesc: %d",
            conn_sec_status.connected,
            conn_sec_status.encrypted,
            conn_sec_status.mitm_protected,
            conn_sec_status.bonded,
            conn_sec_status.lesc);

         if(!conn_sec_status.mitm_protected)
         {
            DEBUG_WARNING("Link not MITM-secured, disconnecting and deleting peer.");
            err_code = pm_peer_delete(m_peer_id);
            if(err_code != NRF_SUCCESS)
            {
               DEBUG_WARNING("Failed to delete peer. Error: 0x%02X", err_code);
            }
            ble_disconnect(p_evt->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
         }
      }
      break;
      case PM_EVT_BONDED_PEER_CONNECTED:
      {
         /* A connected peer has been identified as one with which we have a bond. When performing bonding with a
         peer for the first time, this event will not be sent until a new connection is established with the peer.
         When we are central, this event is always sent when the Peer Manager receives the @ref BLE_GAP_EVT_CONNECTED
         event. When we are peripheral, this event might in rare cases arrive later. */
         DEBUG_INFO("Connected to a previously bonded device.");

         // Re-enable whitelist after successfully connected to a previously-bonded device.
         m_is_whitelist_enabled = true;
      }
      break;

      case PM_EVT_CONN_SEC_FAILED:
      {
         /* Often, when securing fails, it shouldn't be restarted, for security reasons.
            Other times, it can be restarted directly.
            Sometimes it can be restarted, but only after changing some Security Parameters.
            Sometimes, it cannot be restarted until the link is disconnected and reconnected.
            Sometimes it is impossible, to secure the link, or the peer device does not support it.
            How to handle this error is highly application dependent. */

         DEBUG_WARNING("Connection security failed for conn_handle: %d", p_evt->conn_handle);
      }
      break;

      case PM_EVT_CONN_SEC_CONFIG_REQ:
      {
         // Reject pairing request from an already bonded peer.
         DEBUG_INFO("Previously-bonded peer is requesting to pair again. Permitting...");
         pm_conn_sec_config_t conn_sec_config = {.allow_repairing = true};
         pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
      }
      break;

      case PM_EVT_PEERS_DELETE_SUCCEEDED:
      {
         DEBUG_INFO("PM_EVT_PEERS_DELETE_SUCCEEDED");
      }
      break;

      case PM_EVT_PEER_DATA_UPDATE_FAILED:
      {
         DEBUG_ERROR("Peer data update failed. Peer ID: %d, Error: 0x%02X",
                     p_evt->peer_id,
                     p_evt->params.peer_data_update_failed.error);
         if(NRF_ERROR_STORAGE_FULL == p_evt->params.peer_data_update_failed.error)
         {
            DEBUG_WARNING("Flash storage full. Attempting to delete peer.");
            err_code = pm_peer_delete(p_evt->peer_id);
            if(err_code != NRF_SUCCESS)
            {
               DEBUG_ERROR("Failed to delete peer. Error: 0x%02X", err_code);
            }
         }
      }
      break;

      case PM_EVT_PEER_DELETE_FAILED:
      {
         DEBUG_ERROR(
            "Peer deletion failed. Peer ID: %d, Error: 0x%02X", p_evt->peer_id, p_evt->params.peer_delete_failed.error);

         static uint8_t retry_count = 0;
         if(retry_count < FAILURE_MAX_RETRY_COUNT)
         {
            retry_count++;
            DEBUG_ERROR("Retrying peer delete. Attempt: %d", retry_count);
            err_code = pm_peer_delete(p_evt->peer_id);
            if(err_code != NRF_SUCCESS)
            {
               DEBUG_ERROR("Retry failed. Error: 0x%02X", err_code);
            }
         }
         else
         {
            DEBUG_ERROR("Max retries reached. Peer deletion aborted.");
            retry_count = 0; // Reset retry count
         }
      }
      break;

      case PM_EVT_PEERS_DELETE_FAILED:
      {
         // Not currently used, but included for completeness.
         DEBUG_INFO("PM_EVT_PEERS_DELETE_FAILED");
         // This typically requires a manual cleanup if persistent.
      }
      break;

      case PM_EVT_ERROR_UNEXPECTED:
      {
         DEBUG_ERROR("Unexpected error in Peer Manager. Error: 0x%02X", p_evt->params.error_unexpected.error);
         APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
      }
      break;

      case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
         if(p_evt->params.peer_data_update_succeeded.flash_changed
            && (PM_PEER_DATA_ID_BONDING == p_evt->params.peer_data_update_succeeded.data_id))
         {
            DEBUG_INFO("New Bond. Adding the peer to the whitelist.");

            // Update whitelist. The whitelist shall only contain the newest bonded device credentials.
            manual_whitelist_clear();
            manual_whitelist_populate();

            // Re-enable whitelist after successfully bonding to new device.
            m_is_whitelist_enabled = true;
         }
         break;
      case PM_EVT_CONN_CONFIG_REQ:
      {
         DEBUG_INFO("PM_EVT_CONN_SEC_CONFIG_REQ received. Conn handle: %d", p_evt->conn_handle);

         pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false}; // Default: disallow repairing

         // Respond to the event
         pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
      }
      break;
      case PM_EVT_CONN_SEC_PARAMS_REQ:
      {
         /* Security parameters (@ref ble_gap_sec_params_t) are needed for an ongoing security procedure.
          * Reply with @ref pm_conn_sec_params_reply before the event handler returns. If no reply is sent, the
          * parameters given in @ref pm_sec_params_set are used. If a peripheral connection, the central's sec_params
          * will be available in the event. */

         // Use parameters given in pm_sec_params_set during peer_manager_init.
      }
      break;

      // NOSONAR case PM_EVT_STORAGE_FULL:
      case PM_EVT_CONN_SEC_START:
      {
         DEBUG_INFO("PM_EVT_CONN_SEC_START");
      }
      break;
      // NOSONAR case PM_EVT_PEER_DELETE_SUCCEEDED:
      case PM_EVT_LOCAL_DB_CACHE_APPLIED:
      {
         DEBUG_INFO("PM_EVT_LOCAL_DB_CACHE_APPLIED");
      }
      break;
      case PM_EVT_PEER_DELETE_SUCCEEDED:
         DEBUG_INFO("PM_EVT_PEER_DELETE_SUCCEEDED");
         break;
      case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
         DEBUG_ERROR("PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED");
         break;
      case PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED:
         DEBUG_ERROR("PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED");
         break;
      case PM_EVT_STORAGE_FULL:
         // Fallthrough
      case PM_EVT_SERVICE_CHANGED_IND_SENT:
         // Fallthrough
      case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
      // Fallthrough
      case PM_EVT_SLAVE_SECURITY_REQ:
         // Fallthrough
      case PM_EVT_FLASH_GARBAGE_COLLECTED:
      // Fallthrough
      default:
         DEBUG_INFO("Unhandled Peer Manager event: %d", p_evt->evt_id);
         break;
   }
}

static void retry_timer_handler_gap_evt_phy_update(void *p_context) // NOSONAR: p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   if(BLE_CONN_HANDLE_INVALID == m_conn_handle)
   {
      DEBUG_WARNING("Invalid connection handle. Aborting retry.");
      return;
   }

   ble_gap_phys_t const phys = {
      .rx_phys = BLE_GAP_PHY_AUTO,
      .tx_phys = BLE_GAP_PHY_AUTO,
   };

   static uint8_t retry_count = 0; // Retry counter

   ret_code_t err_code = sd_ble_gap_phy_update(m_conn_handle, &phys);

   if(NRF_SUCCESS == err_code)
   {
      DEBUG_INFO("PHY update retry succeeded.");
   }
   else if((NRF_ERROR_BUSY == err_code) && (retry_count < FAILURE_MAX_RETRY_COUNT))
   {
      retry_count++;
      DEBUG_WARNING("Retrying PHY update (attempt %d).", retry_count);
      app_timer_start(m_gap_evt_phy_update_retry_timer,
                      APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS),
                      NULL); // Re-pass conn_handle for next retry
   }
   else
   {
      DEBUG_ERROR("Failed to process PHY update after retries. Error: 0x%02X", err_code);
   }
}

static result_t ble_timers_init(void)
{
   ret_code_t err_code;

   result_t result = RESULT_OK;

   // Timer for pairing timeout after too many failed pairing attempts.
   err_code = app_timer_create(&m_pairing_timer, APP_TIMER_MODE_SINGLE_SHOT, pairing_timeout_handler);
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   if(IS_OK(result))
   {
      err_code = app_timer_create(
         &m_gap_evt_phy_update_retry_timer, APP_TIMER_MODE_SINGLE_SHOT, retry_timer_handler_gap_evt_phy_update);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   if(IS_OK(result))
   {
      err_code = app_timer_create(&m_disconnect_retry_timer, APP_TIMER_MODE_SINGLE_SHOT, disconnect_retry_handler);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   if(IS_OK(result))
   {
      err_code = app_timer_create(&m_connect_qwr_retry_timer, APP_TIMER_MODE_SINGLE_SHOT, connect_qwr_timeout_handler);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   if(IS_OK(result))
   {
      err_code = app_timer_create(
         &m_stop_advertising_timer, APP_TIMER_MODE_SINGLE_SHOT, stop_advertising_retry_timer_handler);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }

   return result;
}

static result_t gap_params_init(void)
{
   ret_code_t err_code;
   ble_gap_conn_params_t gap_conn_params;
   ble_gap_conn_sec_mode_t sec_mode;
   result_t result = RESULT_OK;

   BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

   ble_gap_addr_t addr = {0};
   sd_ble_gap_addr_get(&addr);
   snprintf(
      m_bt_devicename, DEVICE_NAME_LEN, "%s %02X%02X%02X", BASE_DEVICE_NAME, addr.addr[2], addr.addr[1], addr.addr[0]);
   m_bt_devicename[DEVICE_NAME_LEN - 1] = '\0';
   uint16_t name_length = (uint16_t)strlen(m_bt_devicename);

   err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)m_bt_devicename, name_length);
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   if(IS_OK(result))
   {
      memset(&gap_conn_params, 0, sizeof(gap_conn_params));
      gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
      gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
      gap_conn_params.slave_latency = SLAVE_LATENCY;
      gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;
      err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }

   // NOTE - passkey is only loaded on advertising start
   return result;
}

static void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt)
{
   if((m_conn_handle == p_evt->conn_handle) && (NRF_BLE_GATT_EVT_ATT_MTU_UPDATED == p_evt->evt_id))
   {
      m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
      DEBUG_INFO("BLE NUS: Data len is set to (%d)", m_ble_nus_max_data_len);
   }
   DEBUG_INFO("ATT MTU exchange completed. central %d peripheral %d",
              p_gatt->att_mtu_desired_central,
              p_gatt->att_mtu_desired_periph);
}

static result_t gatt_init(void)
{
   ret_code_t err_code;
   uint8_t retry_count = 0;
   result_t result = RESULT_OK;

   do
   {
      err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
      if(NRF_SUCCESS != err_code)
      {
         DEBUG_WARNING("Failed to initialize GATT. Retrying (attempt %d).", retry_count);
         retry_count++;
         nrf_delay_ms(FAILURE_RETRY_DELAY_MS);
      }
   } while((NRF_SUCCESS != err_code) && (retry_count <= FAILURE_MAX_RETRY_COUNT));

   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   return result;
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
   APP_ERROR_HANDLER(nrf_error);
}

static void ble_dfu_event_handler(ble_dfu_buttonless_evt_type_t event)
{
   switch(event)
   {
      case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE:
         DEBUG_INFO("Device preparing to enter bootloader mode...");
         break;
      case BLE_DFU_EVT_BOOTLOADER_ENTER:
         DEBUG_INFO("Device entering bootloader mode");
         break;
      case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
         DEBUG_INFO("Device failed to enter bootloader mode");
         break;
      case BLE_DFU_EVT_RESPONSE_SEND_ERROR:
         DEBUG_INFO("Device DFU response send error occurred");
         break;
      default:
         DEBUG_WARNING("Unknown event from ble_dfu.");
         break;
   }
}

static void on_cs_evt(ble_cs_t *p_cs_service, ble_cs_evt_t *p_evt)
{
   UNUSED_PARAMETER(p_cs_service);

   switch(p_evt->evt_type)
   {
      case BLE_CS_EVT_NOTIFICATION_ENABLED:
      {
         DEBUG_INFO("SC: notifications enabled");
      }
      break;

      case BLE_CS_EVT_NOTIFICATION_DISABLED:

      {
         DEBUG_INFO("SC: notifications disabled");
      }
      break;

      case BLE_CS_EVT_DEVICE_COMMANDS_UPDATED:
      {
         uint32_t command = p_evt->device_command;

         if(NULL != m_event_handlers.on_ble_device_commands_update)
         {
            // Pass event to main application.
            m_event_handlers.on_ble_device_commands_update(command);
         }
      }
      break;

      case BLE_CS_EVT_DISCONNECTED:
         // Fallthrough
      case BLE_CS_EVT_CONNECTED:
         // Fallthrough
      default:
         // No implementation needed.
         break;
   }
}

static result_t services_init(void)
{
   ret_code_t err_code;
   nrf_ble_qwr_init_t qwr_init = {0};
   ble_dfu_buttonless_init_t dfus_init = {0};
   result_t result = RESULT_OK;

   // Initialize Queued Write Module.
   qwr_init.error_handler = nrf_qwr_error_handler;

   err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   if(IS_OK(result))
   {
      dfus_init.evt_handler = ble_dfu_event_handler;
      err_code = ble_dfu_buttonless_init(&dfus_init);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   // Initialize Custom Service (CS)
   if(IS_OK(result))
   {
      ble_cs_init_t cs_init = {0};
      cs_init.evt_handler = on_cs_evt;

      BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cs_init.custom_value_char_attr_md.cccd_write_perm);
      BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cs_init.custom_value_char_attr_md.read_perm);
      BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cs_init.custom_value_char_attr_md.write_perm);

      err_code = ble_cs_init(&m_cs, &cs_init);
      if(NRF_SUCCESS != err_code)
      {
         DEBUG_WARNING("BLE: Failed to initialize Custom Service (CS): %d", err_code);
      }
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }

   // Initialize Battery Service (BS)
   if(IS_OK(result))
   {
      ble_bas_init_t bas_init;

      memset(&bas_init, 0, sizeof(bas_init));

      bas_init.evt_handler = NULL; // No event handler is needed
      bas_init.support_notification = true;
      bas_init.p_report_ref = NULL;
      bas_init.initial_batt_level = 100; // Start with 100% battery level

      // Set security levels for reading battery level and writing CCCD
      bas_init.bl_rd_sec = SEC_OPEN;
      bas_init.bl_cccd_wr_sec = SEC_OPEN;
      bas_init.bl_report_rd_sec = SEC_OPEN;

      err_code = ble_bas_init(&m_bas, &bas_init);
      if(NRF_SUCCESS != err_code)
      {
         DEBUG_WARNING("BLE: Failed to initialize the battery service: %d", err_code);
      }
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   // Initialize DIS service
   if(IS_OK(result))
   {
      ble_dis_init_t dis_init;

      memset(&dis_init, 0, sizeof(dis_init));

      // Device Information Service (DIS)
      static char hw_version_ascii[] = HW_VERSION_STR;
      static char app_version_ascii[] = RING_APP_VERSION;
      ble_srv_ascii_to_utf8(&dis_init.hw_rev_str, hw_version_ascii);  // Set Hardware Revision String
      ble_srv_ascii_to_utf8(&dis_init.fw_rev_str, app_version_ascii); // Set Firmware Revision String

      // Set security permissions
      dis_init.dis_char_rd_sec = SEC_OPEN;

      // Initialize the service
      err_code = ble_dis_init(&dis_init);
      if(NRF_SUCCESS != err_code)
      {
         DEBUG_WARNING("BLE: Failed to initialize the Device Information Service: %d", err_code);
      }
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   // Initialize the NUS service
   if(IS_OK(result))
   {
      ble_nus_init_t nus_init;
      memset(&nus_init, 0, sizeof(nus_init));

      nus_init.data_handler = nus_rx_data_handler;

      err_code = ble_nus_init(&m_nus, &nus_init);
      if(NRF_SUCCESS != err_code)
      {
         DEBUG_WARNING("BLE: Failed to initialize the Nordic UART Service: %d", err_code);
      }
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   return result;
}

static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
   if(BLE_CONN_PARAMS_EVT_FAILED == p_evt->evt_type)
   {
      ble_disconnect(p_evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
   }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
   if(NRF_SUCCESS != nrf_error)
   {
      DEBUG_ERROR("BLE Control error on conn err handler. NRF Error: 0x%02X", nrf_error);
   }
   APP_ERROR_HANDLER(nrf_error);
}

static result_t conn_params_init(void)
{
   ret_code_t err_code;
   ble_conn_params_init_t cp_init;
   result_t result = RESULT_OK;

   memset(&cp_init, 0, sizeof(cp_init));

   cp_init.p_conn_params = NULL;
   cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
   cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
   cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
   cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
   cp_init.disconnect_on_fail = false;
   cp_init.evt_handler = on_conn_params_evt;
   cp_init.error_handler = conn_params_error_handler;

   err_code = ble_conn_params_init(&cp_init);
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   return result;
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
   ret_code_t err_code;
   switch(ble_adv_evt)
   {
      case BLE_ADV_EVT_FAST:
      {
         DEBUG_INFO("Fast advertising.");
      }
      break;

      case BLE_ADV_EVT_SLOW:
      {
         DEBUG_INFO("Slow advertising.");
      }
      break;

      case BLE_ADV_EVT_FAST_WHITELIST:
      {
         DEBUG_INFO("Fast advertising with whitelist.");
      }
      break;

      case BLE_ADV_EVT_SLOW_WHITELIST:
      {
         DEBUG_INFO("Slow advertising with whitelist.");
      }
      break;

      case BLE_ADV_EVT_IDLE:
      {
         DEBUG_INFO("BLE_ADV_EVT_IDLE");
      }
      break;

      case BLE_ADV_EVT_PEER_ADDR_REQUEST:
      {
         pm_peer_data_bonding_t peer_bonding_data;
         ble_gap_addr_t *p_peer_addr = NULL;
         DEBUG_INFO("BLE_ADV_EVT_PEER_ADDR_REQUEST");

         // Check if m_peer_id corresponds to a valid bonded device in the whitelist
         if(PM_PEER_ID_INVALID != m_peer_id)
         {
            // Load bonding data for the peer
            err_code = pm_peer_data_bonding_load(m_peer_id, &peer_bonding_data);

            // Delay necessary to give pm_peer_data_bonding_load() time to complete. Leads to system reset if not
            // included. The 5ms was the smallest delay (determined empirically) that consistently resolved the issue.
            nrf_delay_ms(5);

            if(err_code == NRF_SUCCESS)
            {
               p_peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);

               // Check if the peer address is in the manual whitelist
               if(is_device_in_whitelist(p_peer_addr))
               {
                  // Reply with the peer address
                  err_code = ble_advertising_peer_addr_reply(&m_advertising, p_peer_addr);
                  if(err_code != NRF_SUCCESS)
                  {
                     DEBUG_ERROR("Failed to reply with peer address. Error: 0x%02X", err_code);
                  }
               }
               else
               {
                  DEBUG_WARNING("Peer address request denied: Peer not in manual whitelist.");
               }
            }
            else
            {
               DEBUG_WARNING("Failed to load bonding data for peer ID %d. Error: 0x%02X", m_peer_id, err_code);
            }
         }
         else
         {
            DEBUG_WARNING("Peer address request denied: Invalid peer ID.");
         }
      }
      break;
      case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
         // Fallthrough
      case BLE_ADV_EVT_WHITELIST_REQUEST:
         // Fallthrough
      case BLE_ADV_EVT_DIRECTED:
         // Fallthrough
      default:
         break;
   }
}

static void disconnect_retry_handler(void *p_context) // NOSONAR: p_context required by SDK
{
   UNUSED_PARAMETER(p_context); // No context is used
   ret_code_t err_code;

   if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
   {
      DEBUG_WARNING("No valid connection handle for retry.");
      return;
   }

   err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

   switch(err_code)
   {
      case NRF_SUCCESS:
         DEBUG_INFO("Disconnect request sent successfully.");
         m_disconnect_retry_count = 0; // Reset retry counter on success
         break;

      case NRF_ERROR_INVALID_PARAM:
         DEBUG_WARNING("Disconnect failed: Invalid parameter(s) supplied.");
         break;

      case BLE_ERROR_INVALID_CONN_HANDLE:
         DEBUG_WARNING("Disconnect failed: Invalid connection handle.");
         break;
      case NRF_ERROR_INVALID_STATE:
         DEBUG_WARNING("Disconnect failed: Invalid state. Retry attempt %d.", m_disconnect_retry_count + 1);

         if(++m_disconnect_retry_count <= FAILURE_MAX_RETRY_COUNT)
         {
            // Restart the timer for the next retry
            ret_code_t timer_err
               = app_timer_start(m_disconnect_retry_timer, APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), NULL);
            if(timer_err != NRF_SUCCESS)
            {
               DEBUG_WARNING("Failed to restart disconnect retry timer. Error: 0x%02X", timer_err);
            }
         }
         else
         {
            DEBUG_WARNING("Max retry attempts reached for disconnect.");
            m_disconnect_retry_count = 0; // Reset retry counter
         }
         break;

      default:
         DEBUG_WARNING("Unexpected error code: 0x%02X.", err_code);
         break;
   }
}

static void ble_disconnect(uint16_t conn_handle, uint8_t hci_status_code)
{
   ret_code_t err_code = NRF_SUCCESS;

   m_conn_handle = conn_handle;
   m_disconnect_retry_count = 0;

   if(BLE_CONN_HANDLE_INVALID == m_conn_handle)
   {
      // No active connection, nothing to disconnect
      return;
   }

   err_code = sd_ble_gap_disconnect(m_conn_handle, hci_status_code);

   switch(err_code)
   {
      case BLE_ERROR_INVALID_CONN_HANDLE:
         DEBUG_WARNING("BLE disconnect failed: BLE_ERROR_INVALID_CONN_HANDLE.");
         break;
      case NRF_SUCCESS:
         DEBUG_INFO("BLE disconnect request sent successfully.");
         break;
      case NRF_ERROR_INVALID_STATE:
         DEBUG_WARNING("BLE disconnect failed: Invalid state. Starting retry timer.");
         // Start the retry timer for subsequent attempts
         err_code = app_timer_start(m_disconnect_retry_timer, APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), NULL);
         if(err_code != NRF_SUCCESS)
         {
            DEBUG_WARNING("Failed to start disconnect retry timer. Error: 0x%02X", err_code);
         }
         break;
      default:
         DEBUG_WARNING("BLE disconnect failed with error: 0x%02X.", err_code);
         break;
   }
}

static result_t get_security_params(ble_gap_sec_params_t *sec_param)
{
   RETURN_ERR_IF_NULL(sec_param, BLE_CONTROL_ERROR_PTR_NULL);

   memset(sec_param, 0, sizeof(ble_gap_sec_params_t));

   // Security parameters to be used for all security procedures.
   sec_param->bond = SEC_PARAM_BOND;
   sec_param->mitm = SEC_PARAM_MITM;
   sec_param->lesc = SEC_PARAM_LESC;
   sec_param->keypress = SEC_PARAM_KEYPRESS;
   sec_param->io_caps = SEC_PARAM_IO_CAPABILITIES;
   sec_param->oob = SEC_PARAM_OOB;
   sec_param->min_key_size = SEC_PARAM_MIN_KEY_SIZE;
   sec_param->max_key_size = SEC_PARAM_MAX_KEY_SIZE;
   sec_param->kdist_own.enc = 1;  // Long Term Key (LTK) and Master Identification (ediv, rand)->
   sec_param->kdist_own.id = 1;   // Identity Resolving Key (IRK) and Identity Address Information->
   sec_param->kdist_own.sign = 0; // Connection Signature Resolving Key (CSRK) for signed data->
   sec_param->kdist_own.link = 0; // Derive Link Key from LTK for classic Bluetooth compatibility->
   sec_param->kdist_peer.enc = 1;
   sec_param->kdist_peer.id = 1;
   sec_param->kdist_peer.sign = 0;
   sec_param->kdist_peer.link = 0;

   return RESULT_OK;
}

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
   UNUSED_PARAMETER(p_context);

   result_t result;

   switch(p_ble_evt->header.evt_id)
   {
      case BLE_GAP_EVT_DISCONNECTED:
      {
         // Push event to the main application.
         m_event_handlers.on_ble_connection_status_update(DEVICE_BLE_EVT_DISCONNECTED);
         // Reset connection handle
         m_conn_handle = BLE_CONN_HANDLE_INVALID;
      }
      break;

      case BLE_GAP_EVT_CONNECTED:
      {
         ret_code_t err_code;
         bool is_connection_allowed = true;
         bool is_connected_successful = false;

         if(!m_is_pairing_allowed)
         {
            DEBUG_WARNING("Pairing not allowed. Disconnecting device.");
            ble_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            is_connection_allowed = false;
         }
         else
         {
            DEBUG_INFO("Device connected:");
            print_ble_gap_addr(&p_ble_evt->evt.gap_evt.params.connected.peer_addr);
         }

         // Check whitelist
         if(is_connection_allowed && m_is_whitelist_enabled
            && !is_device_in_whitelist(&p_ble_evt->evt.gap_evt.params.connected.peer_addr))
         {
            DEBUG_WARNING("Device not in whitelist. Disconnecting.");
            ble_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            is_connection_allowed = false;
         }

         if(is_connection_allowed)
         {
            // Attempt QWR connection handle assignment with retries using app timer
            m_retry_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            m_qwr_retry_count = 0;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_retry_conn_handle);

            if(err_code != NRF_SUCCESS)
            {
               DEBUG_WARNING("Failed to assign QWR conn handle (initial attempt). Error: 0x%02X", err_code);

               // Start the retry timer
               err_code = app_timer_start(m_connect_qwr_retry_timer, APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), NULL);
               if(err_code != NRF_SUCCESS)
               {
                  DEBUG_WARNING("Failed to start QWR retry timer. Error: 0x%02X", err_code);
               }
            }
            else
            {
               DEBUG_INFO("QWR connection handle assigned successfully.");
               is_connected_successful = true;
            }
         }
         if(is_connected_successful)
         {
            // Set TX power
            err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_retry_conn_handle, 4); // +4 dBm
            switch(err_code)
            {
               case NRF_SUCCESS:
                  DEBUG_WARNING("Transmit power set successfully");
                  break;

               case NRF_ERROR_INVALID_PARAM:
                  DEBUG_WARNING("sd_ble_gap_tx_power_set() returned: NRF_ERROR_INVALID_PARAM");
                  break;

               case BLE_ERROR_INVALID_ADV_HANDLE:
                  DEBUG_WARNING("sd_ble_gap_tx_power_set() returned: BLE_ERROR_INVALID_ADV_HANDLE");
                  break;

               case BLE_ERROR_INVALID_CONN_HANDLE:
                  DEBUG_WARNING("sd_ble_gap_tx_power_set() returned: BLE_ERROR_INVALID_CONN_HANDLE");
                  break;

               default:
                  DEBUG_WARNING("sd_ble_gap_tx_power_set() returned unknown error");
                  break;
            }

            // Push event to the main application
            m_event_handlers.on_ble_connection_status_update(DEVICE_BLE_EVT_CONNECTED);
            m_conn_handle = m_retry_conn_handle;
         }
      }
      break;

      case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
      {
         DEBUG_INFO("PHY update request received.");

         ble_gap_phys_t const phys = {
            .rx_phys = BLE_GAP_PHY_AUTO,
            .tx_phys = BLE_GAP_PHY_AUTO,
         };

         uint8_t retry_count = 0;
         ret_code_t err_code;

         do
         {
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);

            if(NRF_SUCCESS == err_code)
            {
               DEBUG_INFO("PHY update initiated successfully.");
               break;
            }
            else if(NRF_ERROR_BUSY == err_code)
            {
               DEBUG_WARNING("PHY update failed (NRF_ERROR_BUSY). Retrying...");
               retry_count++;

               // Start a timer to retry the PHY update after a delay.
               m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
               app_timer_start(m_gap_evt_phy_update_retry_timer, APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), NULL);
            }
            else
            {
               DEBUG_WARNING("PHY update failed with error: 0x%02X", err_code);
               break; // Stop retrying on non-transient errors
            }
         } while(retry_count < FAILURE_MAX_RETRY_COUNT);

         if(err_code != NRF_SUCCESS)
         {
            DEBUG_WARNING("Failed to process PHY update after %d retries. Error: 0x%02X", retry_count, err_code);

            SET_ERR(result, BLE_CONTROL_ERROR_GAP_PHY_UPDATE);
         }
      }
      break;

      case BLE_GATTC_EVT_TIMEOUT:
      {
         DEBUG_WARNING("GATT Client Timeout on conn_handle: %d.", p_ble_evt->evt.gattc_evt.conn_handle);
         ble_disconnect(p_ble_evt->evt.gattc_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      }
      break;

      case BLE_GATTS_EVT_TIMEOUT:
      {
         DEBUG_INFO("GATT Server Timeout on conn_handle: %d.", p_ble_evt->evt.gatts_evt.conn_handle);
         ble_disconnect(p_ble_evt->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      }
      break;

      case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
      {
         DEBUG_INFO("BLE_GAP_EVT_SEC_PARAMS_REQUEST received.");

         if(!m_is_pairing_allowed)
         {
            DEBUG_WARNING("Pairing not allowed. Rejecting security request.");

            ret_code_t err_code
               = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);

            if(err_code != NRF_SUCCESS)
            {
               DEBUG_WARNING("Failed to send pairing rejection. Error: 0x%02X", err_code);
            }
            break;
         }

         ble_gap_sec_params_t sec_params;

         result = get_security_params(&sec_params);
         if(IS_ERR(result))
         {
            DEBUG_ERROR("Failed to get security parameters. Error: 0x%02X", result);
         }

         ret_code_t err_code = sd_ble_gap_sec_params_reply(
            p_ble_evt->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &sec_params, NULL);
         if(err_code != NRF_SUCCESS)
         {
            DEBUG_WARNING("Failed to reply to security parameters request. Error: 0x%02X", err_code);
         }
         else
         {
            DEBUG_INFO("Security parameters response sent successfully.");
         }
      }
      break;

      case BLE_GAP_EVT_AUTH_KEY_REQUEST:
      {
         DEBUG_INFO("BLE_GAP_EVT_AUTH_KEY_REQUEST received.");

         if(!m_is_pairing_allowed)
         {
            DEBUG_WARNING("Pairing not allowed. Rejecting auth key request.");
            ret_code_t err_code
               = sd_ble_gap_auth_key_reply(p_ble_evt->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
            if(err_code != NRF_SUCCESS)
            {
               DEBUG_WARNING("Failed to reject pairing request. Error: 0x%02X", err_code);
            }
            break;
         }

         if(BLE_GAP_AUTH_KEY_TYPE_PASSKEY == p_ble_evt->evt.gap_evt.params.auth_key_request.key_type)
         {
            DEBUG_INFO("Sending passkey.");
            ret_code_t err_code = sd_ble_gap_auth_key_reply(
               p_ble_evt->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, m_bt_passkey);
            if(err_code != NRF_SUCCESS)
            {
               DEBUG_WARNING("Failed to send passkey. Error: 0x%02X", err_code);

               if(NRF_ERROR_BUSY == err_code)
               {
                  DEBUG_WARNING("BLE stack busy.");
               }
            }
         }
         else
         {
            DEBUG_WARNING("Unsupported key type in auth key request.");
         }
      }
      break;

      case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
      {
         DEBUG_INFO("BLE_GAP_EVT_LESC_DHKEY_REQUEST");
      }
      break;

      case BLE_GAP_EVT_AUTH_STATUS:
      {
         DEBUG_INFO("BLE_GAP_EVT_AUTH_STATUS: status=0x%02X bond=0x%02X lv4: %d kdist_own:0x%02X kdist_peer:0x%02X",
                    p_ble_evt->evt.gap_evt.params.auth_status.auth_status,
                    p_ble_evt->evt.gap_evt.params.auth_status.bonded,
                    p_ble_evt->evt.gap_evt.params.auth_status.sm1_levels.lv4,
                    *((const uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_own),
                    *((const uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_peer));

         if(BLE_GAP_SEC_STATUS_SUCCESS == p_ble_evt->evt.gap_evt.params.auth_status.auth_status)
         {
            if(p_ble_evt->evt.gap_evt.params.auth_status.bonded)
            {
               // Bonding succeeded; delete other peers
               pm_peer_id_t current_peer_id = PM_PEER_ID_INVALID;
               pm_peer_id_get(p_ble_evt->evt.gap_evt.conn_handle, &current_peer_id);
               delete_all_peers_except(current_peer_id);
            }
         }
         else
         {
            pairing_failed_handler();
         }
      }
      break;

      default:
         // No implementation needed.
         break;
   }
}

static result_t ble_stack_init(void)
{
   ret_code_t err_code;
   result_t result = RESULT_OK;

   err_code = nrf_sdh_enable_request();
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   // Configure the BLE stack using the default settings.
   // Fetch the start address of the application RAM.
   uint32_t ram_start = 0;
   if(IS_OK(result))
   {
      err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   // Enable BLE stack.
   if(IS_OK(result))
   {
      err_code = nrf_sdh_ble_enable(&ram_start);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }

   // Register a handler for BLE events.
   if(IS_OK(result))
   {
      NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
   }
   return result;
}

static result_t peer_manager_init(void)
{
   result_t result = RESULT_OK;
   ble_gap_sec_params_t sec_param;
   ret_code_t err_code;

   err_code = pm_init();
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   IF_OK_RUN_AND_UPDATE(result, get_security_params(&sec_param));

   if(IS_OK(result))
   {
      err_code = pm_sec_params_set(&sec_param);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }

   if(IS_OK(result))
   {
      err_code = pm_register(pm_evt_handler);
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }

   return result;
}

static result_t advertising_init(void)
{
   ret_code_t err_code;
   ble_advertising_init_t init;
   result_t result = RESULT_OK;

   memset(&init, 0, sizeof(init));

   init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
   init.advdata.include_appearance = false;
   init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
   init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
   init.advdata.uuids_complete.p_uuids = m_adv_uuids;
   init.config.ble_adv_on_disconnect_disabled = false; // Automatically restart advertising on disconnect.

   init.config.ble_adv_whitelist_enabled = false;
   init.config.ble_adv_directed_enabled = false;
   init.config.ble_adv_directed_interval = 0;
   init.config.ble_adv_directed_timeout = 0;
   init.config.ble_adv_fast_enabled = true;
   init.config.ble_adv_fast_interval = APP_ADV_FAST_INTERVAL;
   init.config.ble_adv_fast_timeout = APP_ADV_DURATION;

   init.evt_handler = on_adv_evt;
   init.error_handler = ble_advertising_error_handler;

   err_code = ble_advertising_init(&m_advertising, &init);
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   if(IS_OK(result))
   {
      ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
   }
   return result;
}

static void advertising_start(bool allow_new_bond)
{
   ret_code_t err_code;

   uint8_t retry_count = 0;

   if(allow_new_bond)
   {
      m_is_whitelist_enabled = false; // Disable whitelist and allow any peer to connect.

      if(!m_is_advertising)
      {
         do
         {
            err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            if(NRF_SUCCESS != err_code)
            {
               DEBUG_WARNING("BLE_ADVERTISING_START failed: 0x%02X. Retrying...", err_code);
               retry_count++;
               nrf_delay_ms(FAILURE_RETRY_DELAY_MS);
            }
         } while((NRF_SUCCESS != err_code) && (retry_count <= FAILURE_MAX_RETRY_COUNT));
         if(NRF_SUCCESS != err_code)
         {
            DEBUG_WARNING("BLE_ADVERTISING_START failed: 0x%02X", err_code);
         }
         APP_ERROR_CHECK(err_code);

         DEBUG_INFO("Starting general advertising.");
         m_is_advertising = true;
         m_is_pairing_allowed = true;
      }
   }
   else
   {
      m_is_whitelist_enabled = true; // Only allow peers in the whitelist to connect.

      bool is_bonded = false;
      result_t result = ble_control_get_bonded_status(&is_bonded);

      DEBUG_INFO("Is bonded: %d", is_bonded);

      if((IS_OK(result)) && is_bonded && (!m_is_advertising))
      {
         do
         {
            err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            if(NRF_SUCCESS != err_code)
            {
               DEBUG_WARNING("BLE_ADVERTISING_START failed: 0x%02X. Retrying...", err_code);
               retry_count++;
               nrf_delay_ms(FAILURE_RETRY_DELAY_MS);
            }
         } while((NRF_SUCCESS != err_code) && (retry_count <= FAILURE_MAX_RETRY_COUNT));
         if(NRF_SUCCESS != err_code)
         {
            DEBUG_WARNING("BLE_ADVERTISING_START failed: 0x%02X", err_code);
         }
         APP_ERROR_CHECK(err_code);

         DEBUG_INFO("Starting whitelist advertising.");
         m_is_advertising = true;
         m_is_pairing_allowed = true;
      }
   }
}

result_t ble_control_get_bonded_status(volatile bool *is_bonded)
{
   RETURN_ERR_IF_NULL(is_bonded, BLE_CONTROL_ERROR_PTR_NULL);

   pm_peer_id_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
   uint32_t peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
   result_t result = RESULT_OK;

   ret_code_t err_code = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);

   if(IS_OK(result))
   {
      *is_bonded = (peer_id_count != 0);
   }

   return result;
}

result_t ble_control_is_permitting_new_bonds(volatile bool *is_permitting_new_bonds)
{
   RETURN_ERR_IF_NULL(is_permitting_new_bonds, BLE_CONTROL_ERROR_PTR_NULL);

   *is_permitting_new_bonds = !m_is_whitelist_enabled;

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t ble_control_init(const ble_control_evt_handlers_t *event_handlers)
{
   RETURN_ERR_IF_NULL(event_handlers, BLE_CONTROL_ERROR_PTR_NULL);

   m_event_handlers = *event_handlers;

   // Initialize.
   result_t result = ble_timers_init();

   IF_OK_RUN_AND_UPDATE(result, ble_stack_init());
   IF_OK_RUN_AND_UPDATE(result, gap_params_init());
   IF_OK_RUN_AND_UPDATE(result, gatt_init());
   IF_OK_RUN_AND_UPDATE(result, services_init());
   IF_OK_RUN_AND_UPDATE(result, advertising_init());
   IF_OK_RUN_AND_UPDATE(result, conn_params_init());
   IF_OK_RUN_AND_UPDATE(result, peer_manager_init());

   if(IS_OK(result))
   {
      manual_whitelist_clear();
      manual_whitelist_populate();

      m_is_whitelist_enabled = true;
   }

   return result;
}

/**
 * @brief Callback function for asserts in the SoftDevice.
 *
 * This function will be called in case of an assert in the SoftDevice.
 *
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
   app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

result_t ble_control_report_battery_level(uint8_t battery_level)
{
   ret_code_t err_code = ble_bas_battery_level_update(&m_bas, battery_level, BLE_CONN_HANDLE_ALL);
   result_t result = RESULT_OK;
   if((NRF_SUCCESS != err_code) && (NRF_ERROR_BUSY != err_code) && (NRF_ERROR_RESOURCES != err_code)
      && (NRF_ERROR_FORBIDDEN != err_code) && (NRF_ERROR_INVALID_STATE != err_code)
      && (BLE_ERROR_GATTS_SYS_ATTR_MISSING != err_code))
   {
      UPDATE_IF_NRF_ERR(err_code, result, BLE_CONTROL_ERROR_NRF_ERROR);
   }
   return result;
}
void ble_control_stop_advertising(void)
{
   // Disconnect all active connections
   ble_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

   // Wait for all BLE connections to be disconnected before attempting to stop advertising.
   ret_code_t err_code = app_timer_start(m_stop_advertising_timer, APP_TIMER_TICKS(FAILURE_RETRY_DELAY_MS), NULL);
   if(err_code != NRF_SUCCESS)
   {
      DEBUG_WARNING("Failed to start stop advertising timer. Error: 0x%02X", err_code);
   }
}

result_t ble_control_start_advertising(bool allow_new_bond, const char *decrypted_passkey_text)
{
   // Note: Null check should not be implemented here. See function documentation.

   // Populate passkey when `allow_new_bond` is true
   if(allow_new_bond)
   {
      memcpy(m_bt_passkey, decrypted_passkey_text, BLE_PASSKEY_LENGTH);
      m_static_pin_option.gap_opt.passkey.p_passkey = m_bt_passkey;
      ret_code_t err_code = sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &m_static_pin_option);

      if(NRF_SUCCESS != err_code)
      {
         DEBUG_ERROR("Failed to set BLE passkey. NRF error code: %u", err_code);
         RETURN_ERR(BLE_CONTROL_ERROR_PASSKEY_SET_FAILURE);
      }
   }
   // Otherwise clear the passkey from RAM
   else
   {
      memset(m_bt_passkey, 0, BLE_PASSKEY_LENGTH);
   }

   advertising_start(allow_new_bond);

   return RESULT_OK;
}

void ble_control_send_debug_data(const uint8_t *p_data, size_t length)
{
   if(BLE_CONN_HANDLE_INVALID == m_conn_handle)
   {
      return; // no central connected
   }

   while(length)
   {
      // Never push more than the peer’s max-payload in one notification.
      uint16_t chunk = (length > m_ble_nus_max_data_len) ? m_ble_nus_max_data_len : (uint16_t)length;
      uint16_t to_send = chunk;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers" // acknowledge const-drop
      (void)ble_nus_data_send(&m_nus, p_data, &to_send, m_conn_handle);
#pragma GCC diagnostic pop

      p_data += chunk;
      length -= chunk;
   }
}