/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ble_cs.c
 * @ingroup ble_module
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "ble_srv_common.h"
#include "boards.h"
#include "nrf_gpio.h"
#include "sdk_common.h"
#include <string.h>

// Custom includes
#include "ble_cs.h"
#include "common.h"
#include "debug.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_BLE_CS;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/*
 * Pick a bound that is (a) guaranteed to cover the largest single ATT PDU you will ever send in one go and (b) small
 * enough to live on the stack.
 *
 * The maximum ATT payload you can put in one Notification or Write-Cmd is (negotiated_MTU − 3) bytes.
 *
 *      – Default MTU in the nRF5 SDK            : 23  → 20-byte payload
 *      – With GATT MTU negotiation + DLE enabled: 247 → 244-byte payload
 *
 * Compile-time symbol NRF_SDH_BLE_GATT_MAX_MTU_SIZE already carries the max-MTU you configured in sdk_config.h, so
 * we subtract the 3-byte ATT header and that limit stays correct if you ever raise the MTU.
 *  */
#define MAX_VALUE_LEN (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - 3)

/**
 * @brief Check if a @p err_code variable is not @p NRF_SUCCESS and return the @p err_code error code if true.
 *
 * @param[in] err_code The pointer variable to check.
 */
#define RETURN_RET_CODE_IF_NRF_ERR(err_code)                                                                           \
   do                                                                                                                  \
   {                                                                                                                   \
      if((err_code) != NRF_SUCCESS)                                                                                    \
      {                                                                                                                \
         return err_code;                                                                                              \
      }                                                                                                                \
   } while(0)

/**
 * @brief Check if a given pointer variable is NULL and return the NRF_ERROR_NULL error code if true.
 *
 * @param[in] pointer_var The pointer variable to check.
 */
#define RETURN_NRF_ERR_CODE_IF_NULL(pointer_var)                                                                       \
   do                                                                                                                  \
   {                                                                                                                   \
      if((pointer_var) == NULL)                                                                                        \
      {                                                                                                                \
         return NRF_ERROR_NULL;                                                                                        \
      }                                                                                                                \
   } while(0)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
/**
 * @brief Function for handling the Connect event.
 *
 * @param[in]   p_cs       Custom Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_cs_t *p_cs, ble_evt_t const *p_ble_evt);

/**
 *  @brief Function for handling the Disconnect event.
 *
 * @param[in]   p_cs       Custom Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_cs_t *p_cs, ble_evt_t const *p_ble_evt);

/**
 * @brief Function for handling the Write event from the central device to this device.
 *
 * @param[in]   p_cs       Custom Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_cs_t *p_cs, ble_evt_t const *p_ble_evt);

/**
 * @brief Adds a characteristic to the BLE service.
 *
 * @param[in] p_cs Pointer to the BLE service context.
 * @param[in] p_cs_init Pointer to the BLE service initialization structure.
 * @param[in] p_uuid Pointer to the UUID of the characteristic.
 * @param[out] p_handles Pointer to the characteristic handles.
 * @param[in] char_props_mask Bitmask of characteristic properties.
 * @param[in] init_len Initial length of the characteristic value.
 * @param[in] max_len Maximum length of the characteristic value.
 * @param[in] p_user_desc Pointer to a UTF-8 encoded string (non-NULL terminated), NULL if the descriptor is not
 * required.
 * @param[in] user_desc_len Length of the user description. 0 = auto.
 * @param[in] p_char_pf Pointer to a presentation format structure or NULL if the CPF descriptor is not required.
 *
 * @return NRF_SUCCESS on success, otherwise an error code from nrf_error.h.
 */
static ret_code_t add_characteristic(const ble_cs_t *p_cs,
                                     const ble_cs_init_t *p_cs_init,
                                     const ble_uuid_t *p_uuid,
                                     ble_gatts_char_handles_t *p_handles,
                                     uint8_t char_props_mask,
                                     uint16_t init_len,
                                     uint16_t max_len,
                                     const uint8_t *p_user_desc,
                                     uint16_t user_desc_len,
                                     const ble_gatts_char_pf_t *p_char_pf);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

/**
 * @brief Initialize a BLE service and add its characteristics.
 *
 * This function sets up the custom BLE service, adds its UUID, and creates one or more characteristics as specified.
 * It is intended to be used to initialize a custom service and add characteristics with desired properties.
 *
 * @param[in,out] p_cs Pointer to the BLE service structure to initialize.
 * @param[in] p_cs_init Pointer to the initialization structure containing configuration and event handler.
 * @return NRF_SUCCESS on success, otherwise an error code from nrf_error.h.
 *
 * @details
 * To add a new characteristic to your BLE service, follow this example:
 *
 * @code
 * // Define the UUID for your characteristic in ble_cs.h
 * ble_uuid_t char_uuid;
 * char_uuid.uuid = YOUR_CHAR_UUID;
 * char_uuid.type = p_cs->uuid_type;
 *
 * // Set the desired properties (e.g., read, write, notify)
 * uint8_t properties = BLE_GATT_CHAR_PROPERTIES_READ | BLE_GATT_CHAR_PROPERTIES_WRITE |
 * BLE_GATT_CHAR_PROPERTIES_NOTIFY;
 *
 * // Optionally, provide a user description and presentation format
 * static const uint8_t user_desc[] = "Example Characteristic";
 * static const ble_gatts_char_pf_t presentation_format = {
 *     .format = BLE_GATT_CPF_FORMAT_UINT16,
 *     .unit = BLE_GATT_UNIT_PERCENTAGE,
 *     .name_space = 1,
 *     .desc = 0x0100
 * };
 *
 * // Add the characteristic to the service
 * ret_code_t err = add_characteristic(
 *     p_cs,
 *     p_cs_init,
 *     &char_uuid,
 *     &p_cs->your_char_handle,
 *     properties,
 *     sizeof(uint16_t), // initial length
 *     sizeof(uint16_t), // max length
 *     user_desc,
 *     sizeof(user_desc),
 *     &presentation_format
 * );
 * @endcode
 */
ret_code_t ble_cs_init(ble_cs_t *p_cs, const ble_cs_init_t *p_cs_init)
{
   RETURN_NRF_ERR_CODE_IF_NULL(p_cs);
   RETURN_NRF_ERR_CODE_IF_NULL(p_cs_init);

   ret_code_t err_code;
   ble_uuid_t ble_uuid;

   // Initialize service structure
   p_cs->evt_handler = p_cs_init->evt_handler;
   p_cs->conn_handle = BLE_CONN_HANDLE_INVALID;

   // Add Custom Service UUID
   ble_uuid128_t base_uuid = {CONTROL_SERVICE_UUID_BASE};
   err_code = sd_ble_uuid_vs_add(&base_uuid, &p_cs->uuid_type);

   if(NRF_SUCCESS == err_code)
   {
      ble_uuid.uuid = CONTROL_SERVICE_UUID;
      ble_uuid.type = p_cs->uuid_type;

      // Add the Custom Service
      err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_cs->service_handle);
   }

   if(NRF_SUCCESS == err_code)
   {
      uint8_t properties;

      if(NRF_SUCCESS == err_code)
      {
         ble_uuid.uuid = UUID_DATA_RX;
         properties = BLE_GATT_CHAR_PROPERTIES_WRITE | BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESP;
         err_code = add_characteristic(
            p_cs, p_cs_init, &ble_uuid, &p_cs->data_rx_handle, properties, 1, MAX_VALUE_LEN, NULL, 0, NULL);
      }

      if(NRF_SUCCESS == err_code)
      {
         ble_uuid.uuid = UUID_DATA_TX;
         properties = BLE_GATT_CHAR_PROPERTIES_NOTIFY | BLE_GATT_CHAR_PROPERTIES_READ;
         err_code = add_characteristic(
            p_cs, p_cs_init, &ble_uuid, &p_cs->data_tx_handle, properties, 1, MAX_VALUE_LEN, NULL, 0, NULL);
      }
   }

   return err_code;
}

void ble_cs_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
   RETURN_VOID_IF_NULL(p_ble_evt);
   RETURN_VOID_IF_NULL(p_context);

   ble_cs_t *p_cs = (ble_cs_t *)p_context;

   switch(p_ble_evt->header.evt_id)
   {
      case BLE_GAP_EVT_CONNECTED:
      {
         on_connect(p_cs, p_ble_evt);
      }
      break;

      case BLE_GAP_EVT_DISCONNECTED:
      {
         on_disconnect(p_cs, p_ble_evt);
      }
      break;

      case BLE_GATTS_EVT_WRITE:
      {
         on_write(p_cs, p_ble_evt);
      }
      break;
      default:
         // No implementation needed. These events are handled in BLE control.
         break;
   }
}

result_t ble_cs_characteristic_update(
   const ble_cs_t *p_cs, uint16_t handle, const void *p_value, uint16_t value_len, bool send_notify)
{
   RETURN_ERR_IF_NULL(p_cs, BLE_CS_ERROR_NULL_PTR);
   RETURN_ERR_IF_NULL(p_value, BLE_CS_ERROR_NULL_PTR);
   result_t result = RESULT_OK;

   // Write the new value to the SoftDevice DB
   ble_gatts_value_t gatts_val = {0};

   /**
    * @note The SoftDevice needs the data buffer to stay alive after the function call which means a static buffer needs
    * to be sent to this function. The compiler complains about the drop of the 'const' qualifier due to an API mismatch
    * in the Nordic SDK - The SDK does not alter the received data so the const qualifier can safely be ignored in this
    * instance. Because the SoftDevice expects a static buffer, a local buffer cannot be created since this function
    * services all read characteristics.
    */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual" // acknowledge const-drop
   gatts_val.p_value = (uint8_t *)p_value;   // API expects uint8_t*
#pragma GCC diagnostic pop

   gatts_val.len = value_len;
   gatts_val.offset = 0;

   ret_code_t err_code = sd_ble_gatts_value_set(p_cs->conn_handle, handle, &gatts_val);
   if(err_code != NRF_SUCCESS)
   {
      DEBUG_ERROR("Failed to update characteristic: %d\n", err_code);
   }
   UPDATE_IF_NRF_ERR(err_code, result, BLE_CS_ERROR_NRF_ERROR);

   if(IS_OK(result) && (send_notify && (p_cs->conn_handle != BLE_CONN_HANDLE_INVALID)))
   {
      // Optionally push a notification to the peer
      ble_gatts_hvx_params_t hvx;
      memset(&hvx, 0, sizeof(hvx));

      ret_code_t hvx_err = NRF_SUCCESS;
      hvx.handle = handle;
      hvx.type = BLE_GATT_HVX_NOTIFICATION;
      hvx.p_len = &gatts_val.len;
      hvx.p_data = gatts_val.p_value;

      // Verify CCCD is enabled for notifications before attempting HVX to avoid NRF_ERROR_INVALID_STATE.
      uint16_t cccd_handle = BLE_GATT_HANDLE_INVALID;
      if(handle == p_cs->error_handle.value_handle)
      {
         cccd_handle = p_cs->error_handle.cccd_handle;
      }
      else if(handle == p_cs->dose_event_handle.value_handle)
      {
         cccd_handle = p_cs->dose_event_handle.cccd_handle;
      }
      else if(handle == p_cs->data_tx_handle.value_handle)
      {
         cccd_handle = p_cs->data_tx_handle.cccd_handle;
      }

      if(cccd_handle != BLE_GATT_HANDLE_INVALID)
      {
         uint8_t cccd_value_buf[2] = {0};
         ble_gatts_value_t cccd_value = {.len = sizeof(cccd_value_buf), .offset = 0, .p_value = cccd_value_buf};

         hvx_err = sd_ble_gatts_value_get(p_cs->conn_handle, cccd_handle, &cccd_value);
         if(hvx_err != NRF_SUCCESS)
         {
            DEBUG_WARNING("Failed to read CCCD value for handle 0x%04X: %d\n", handle, hvx_err);
         }
         else if(!ble_srv_is_notification_enabled(cccd_value.p_value))
         {
            DEBUG_WARNING("Notifications disabled for handle 0x%04X\n", handle);
            hvx_err = NRF_ERROR_INVALID_STATE;
         }
      }

      if(hvx_err == NRF_SUCCESS)
      {
         hvx_err = sd_ble_gatts_hvx(p_cs->conn_handle, &hvx);
      }
      else
      {
         DEBUG_WARNING("Failed to send notification: %d\n", hvx_err);
      }
   }

   return result;
}

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/

static void on_connect(ble_cs_t *p_cs, ble_evt_t const *p_ble_evt)
{
   RETURN_VOID_IF_NULL(p_ble_evt);
   RETURN_VOID_IF_NULL(p_cs);

   p_cs->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

   ble_cs_evt_t evt;

   evt.evt_type = BLE_CS_EVT_CONNECTED;

   p_cs->evt_handler(p_cs, &evt);
}

static void on_disconnect(ble_cs_t *p_cs, ble_evt_t const *p_ble_evt)
{
   RETURN_VOID_IF_NULL(p_cs);
   UNUSED_PARAMETER(p_ble_evt);
   p_cs->conn_handle = BLE_CONN_HANDLE_INVALID;

   ble_cs_evt_t evt;

   evt.evt_type = BLE_CS_EVT_DISCONNECTED;

   p_cs->evt_handler(p_cs, &evt);
}

static void on_write(ble_cs_t *p_cs, ble_evt_t const *p_ble_evt)
{
   RETURN_VOID_IF_NULL(p_cs);
   RETURN_VOID_IF_NULL(p_ble_evt);

   ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

   if(p_evt_write->handle == p_cs->data_rx_handle.value_handle)
   {
      ble_cs_evt_t evt;
      evt.evt_type = BLE_CS_EVT_RX_DATA_RECEIVED;
      evt.p_rx_data = p_evt_write->data;
      evt.rx_data_len = p_evt_write->len;
      p_cs->evt_handler(p_cs, &evt);
   }

   // Check if the Custom value CCCD is written to and that the value is the appropriate length, i.e 2 bytes. (2 bytes
   // is specified in the BLE spec_)
   if((p_evt_write->handle == p_cs->data_tx_handle.cccd_handle) && (2 == p_evt_write->len))
   {
      // CCCD written, call application event handler
      ble_cs_evt_t evt;

      if(ble_srv_is_notification_enabled(p_evt_write->data))
      {
         evt.evt_type = BLE_CS_EVT_NOTIFICATION_ENABLED;
      }
      else
      {
         evt.evt_type = BLE_CS_EVT_NOTIFICATION_DISABLED;
      }
      // Call the application event handler.
      p_cs->evt_handler(p_cs, &evt);
   }
}

static ret_code_t add_characteristic(
   const ble_cs_t *p_cs,
   const ble_cs_init_t *p_cs_init,
   const ble_uuid_t *p_uuid,
   ble_gatts_char_handles_t *p_handles,
   uint8_t char_props_mask,
   uint16_t init_len,
   uint16_t max_len,
   const uint8_t
      *p_user_desc, // Pointer to a UTF-8 encoded string (non-NULL terminated), NULL if the descriptor is not required.
   uint16_t user_desc_len, //  0 = auto
   const ble_gatts_char_pf_t
      *p_char_pf)          // Pointer to a presentation format structure or NULL if the CPF descriptor is not required.
{
   // CCCD only if NOTIFY or INDICATE are requested
   bool enable_notify
      = (char_props_mask & BLE_GATT_CHAR_PROPERTIES_NOTIFY) || (char_props_mask & BLE_GATT_CHAR_PROPERTIES_INDICATE);

   ble_gatts_attr_md_t cccd_md = {0};
   if(enable_notify)
   {
      BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
      cccd_md.write_perm = p_cs_init->custom_value_char_attr_md.cccd_write_perm;
      cccd_md.vloc = BLE_GATTS_VLOC_STACK;
   }

   // Characteristic metadata
   ble_gatts_char_md_t char_md = {0}; // Note ble_gatts_char_md_t
   if(sizeof(char_props_mask) == sizeof(ble_gatt_char_props_t))
   {
      memcpy(&char_md.char_props, &char_props_mask, sizeof(char_props_mask)); // <<< (2)(3)
   }
   else
   {
      DEBUG_ERROR("Invalid size of char_props_mask");
      return NRF_ERROR_INVALID_LENGTH;
   }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
   // Acknowledge that we deliberately drop 'const'
   char_md.p_char_user_desc = (uint8_t *)p_user_desc;
   char_md.p_char_pf = (ble_gatts_char_pf_t *)p_char_pf;
#pragma GCC diagnostic pop

   char_md.p_cccd_md = enable_notify ? &cccd_md : NULL;
   char_md.char_user_desc_size = p_user_desc ? user_desc_len : 0;
   char_md.char_user_desc_max_size = char_md.char_user_desc_size;

   // Attribute metadata
   ble_gatts_attr_md_t attr_md = {.read_perm = p_cs_init->custom_value_char_attr_md.read_perm,
                                  .write_perm = p_cs_init->custom_value_char_attr_md.write_perm,
                                  .vloc = BLE_GATTS_VLOC_STACK,
                                  .vlen = (init_len != max_len) ? 1 : 0};

   // Attribute value
   ble_gatts_attr_t attr_val
      = {.p_uuid = p_uuid, .p_attr_md = &attr_md, .init_len = init_len, .init_offs = 0, .max_len = max_len};

   //  SoftDevice call
   ret_code_t err_code = sd_ble_gatts_characteristic_add(p_cs->service_handle, &char_md, &attr_val, p_handles);

   return err_code;
}
