/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

// Todo: Update to match new result_macros.h
/*
Todo: refactor this module:
Replace RTT with debug logs.
Magic numbers
Yoda conditions


*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
// Custom includes
#include "nfc_reader_driver.h"
#include "rfal_nfca.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_NFC_READER_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MAILBOX_BUFFER_SIZE            (NFC_MAILBOX_BUFFER_SIZE)
#define MAX_POWER_LEVEL                (15u)
#define CHECK_TAG_PRESENCE_INTERVAL_MS (1000u)
#define TAG_DETECTION_INTERVAL_MS      (500u)

// NOTE: TAGV_WIRELESS_OUTPUT_PWR_LEVEL must be less than MAX_POWER_LEVEL.
#define TAGV_WIRELESS_OUTPUT_PWR_LEVEL (NFC_POWER_LEVEL_LOW)
#define TAGA_WIRELESS_OUTPUT_PWR_LEVEL (NFC_POWER_LEVEL_HIGH) // Not to be changed dynamically

// Number of times the presence check can fail before the tag is considered absent.
#define TAG_PRESENCE_CHECK_DEBOUNCE_NUM (3u)
// Minimum off-time when cycling the RF field to wake a quiet ISO15693 tag family.
#define FIELD_RESET_DELAY_MS (5u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
/**
 * @brief Send data to the NFC tag mailbox.
 *
 * @param interface Pointer to the comms driver interface.
 * @param data Pointer to the data buffer to send.
 * @param length Length of the data to send.
 * @return Result code indicating success or error.
 */
static result_t send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t length);

/**
 * @brief Retrieve data from the NFC tag mailbox.
 *
 * @param interface Pointer to the comms driver interface.
 * @param data Pointer to the buffer to store received data.
 * @param data_buffer_size Size of the data buffer.
 * @param data_length Pointer to store the actual length of received data.
 * @return Result code indicating success or error.
 */
static result_t get_packet(const comms_driver_interface_t *const interface, uint8_t *data, uint16_t data_buffer_size);

/**
 * @brief Set the output power level for the NFC driver.
 *
 * @param interface Pointer to the NFC driver interface.
 * @param level Output power level to set.
 * @return Result code indicating success or error.
 */
static result_t set_output_power(const nfc_driver_interface_t *const interface, uint8_t level);

static result_t get_max_packet_length(const comms_driver_interface_t *const interface, uint16_t *max_packet_len);

/**
 * @brief Get the current output power level of the NFC driver.
 *
 * @param interface Pointer to the NFC driver interface.
 * @param level Pointer to store the current output power level.
 * @return Result code indicating success or error.
 */
static result_t get_output_power(const nfc_driver_interface_t *const interface, uint8_t *level);

/**
 * @brief Check if a ring (NFC tag) is present.
 *
 * @param interface Pointer to the NFC driver interface.
 * @param is_ring_present Pointer to store the presence status.
 * @return Result code indicating success or error.
 */
static result_t is_ring_present(const nfc_driver_interface_t *const interface, bool *is_ring_present);

/**
 * @brief Set the detection type for the NFC driver (e.g., NFC-A or NFC-V).
 *
 * @param interface Pointer to the NFC driver interface.
 * @param tag_type Tag type to detect.
 * @param power_level Set the field strength (0-15)
 * @return Result code indicating success or error.
 */
static result_t
   set_detection_type(const nfc_driver_interface_t *const interface, NFC_TAG_TYPE tag_type, uint8_t power_level);

/**
 * @brief Process NFC driver state, including tag detection and presence checks.
 *
 * @param interface Pointer to the NFC driver interface.
 * @param enable_tag_detection Enable or disable tag detection.
 * @return Result code indicating success or error.
 */
static result_t process(const nfc_driver_interface_t *const interface, bool enable_tag_detection);

/**
 * @brief Perform automatic antenna tuning for the NFC driver.
 *
 * @param interface Pointer to the NFC driver interface.
 * @return Result code indicating success or error.
 */
static result_t auto_tune_antenna(const nfc_driver_interface_t *const interface);

/**
 * @brief Retrieve the UID of the currently detected NFC tag.
 *
 * @param interface Pointer to the NFC driver interface.
 * @param uid_buffer Buffer to store the UID.
 * @param uid_length Pointer to store the UID length.
 * @return Result code indicating success or error.
 */
static result_t get_tag_uid(const nfc_driver_interface_t *const interface, uint8_t *uid_buffer, uint8_t *uid_length);

/**
 * @brief Turn off the RF field (disable Tx/Rx) while keeping the chip awake.
 */
static result_t field_off(const nfc_driver_interface_t *const interface);

/**
 * @brief Turn on the RF field and start guard time (GT).
 */
static result_t field_on_and_start_gt(const nfc_driver_interface_t *const interface);

/**
 * @brief Put the chip into RFAL low-power mode (power-down).
 */
static result_t chip_power_down(const nfc_driver_interface_t *const interface);

/**
 * @brief Wake the chip from RFAL low-power mode.
 */
static result_t chip_power_up(const nfc_driver_interface_t *const interface);

/**
 * @brief Start ST25R wake-up mode (periodic low-power measurement).
 */
static result_t wakeup_mode_start(const nfc_driver_interface_t *const interface);

/**
 * @brief Stop ST25R wake-up mode.
 */
static result_t wakeup_mode_stop(const nfc_driver_interface_t *const interface);

/**
 * @brief Check whether wake-up mode has detected a wake event.
 */
static result_t wakeup_mode_has_woke(const nfc_driver_interface_t *const interface, bool *has_woke);

/**
 * @brief Query whether the RFAL guard time (GT) has expired.
 */
static result_t is_gt_expired(const nfc_driver_interface_t *const interface, bool *is_expired);

// Non-interface functions
/**
 * @brief Reset the detection state of the NFC driver.
 *
 * @param self Pointer to the NFC driver instance.
 */
static void reset_detection_state(nfc_driver_t *self);

/**
 * @brief Check if a time interval has elapsed.
 *
 * @param last_ms Last recorded time in milliseconds.
 * @param current_ms Current time in milliseconds.
 * @param interval_ms Interval to check in milliseconds.
 * @return true if the interval has elapsed, false otherwise.
 */
static bool has_interval_elapsed(uint64_t last_ms, uint64_t current_ms, uint32_t interval_ms);

/**
 * @brief Check if the guard time is ready for NFC-A operations.
 *
 * @param self Pointer to the NFC driver instance.
 * @return true if guard time is ready, false otherwise.
 */
static bool guard_time_ready(nfc_driver_t *self);

/**
 * @brief Discover a tag based on the currently configured tag type.
 *
 * @param self Pointer to the NFC driver instance.
 * @return Result code indicating success or error.
 */
static result_t discover_tag(nfc_driver_t *self);

/**
 * @brief Discover an NFC-V (ISO15693) tag.
 *
 * @param self Pointer to the NFC driver instance.
 * @return Result code indicating success or error.
 */
static result_t discover_nfcv_tag(nfc_driver_t *self);

/**
 * @brief Discover an NFC-A (ISO14443A) tag.
 *
 * @param self Pointer to the NFC driver instance.
 * @return Result code indicating success or error.
 */
static result_t discover_nfca_tag(nfc_driver_t *self);

/**
 * @brief Configure the detection mode for the specified tag type.
 *
 * @param self Pointer to the NFC driver instance.
 * @param tag_type Tag type to configure.
 * @param power_level Set the field strength (0-15)
 * @return Result code indicating success or error.
 */
static result_t configure_detection_mode(nfc_driver_t *self, NFC_TAG_TYPE tag_type, uint8_t power_level);

/**
 * @brief Check the presence of a tag and update internal state.
 *
 * @param self Pointer to the NFC driver instance.
 * @param is_tag_present Pointer to store presence status.
 * @return Result code indicating success or error.
 */
static result_t check_tag_presence(nfc_driver_t *self, bool *is_tag_present);

/**
 * @brief Check presence of an NFC-V tag using the mailbox.
 *
 * @param self Pointer to the NFC driver instance.
 * @param debounce_fail Number of allowed failures before considering tag absent.
 * @param is_tag_present Pointer to store presence status.
 * @return Result code indicating success or error.
 */
static result_t nfcv_check_presence_via_mailbox(nfc_driver_t *self, uint8_t debounce_fail, bool *is_tag_present);

/**
 * @brief Check presence of an NFC-A tag.
 *
 * @param self Pointer to the NFC driver instance.
 * @param debounce_fail Number of allowed failures before considering tag absent.
 * @param is_tag_present Pointer to store presence status.
 * @return Result code indicating success or error.
 */
static result_t nfca_check_presence(nfc_driver_t *self, uint8_t debounce_fail, bool *is_tag_present);

/**
 * @brief Configure the ST25R3916 hardware for operation.
 *
 * @return Result code indicating success or error.
 */
static result_t configure_st25r3916_hw(void);

/**
 * @brief Run automatic antenna tuning (AAT) for the NFC driver.
 *
 * @return Result code indicating success or error.
 */
static result_t run_aat_tune(void);

/**
 * @brief Try to read a full mailbox message from an NFC-V tag.
 *
 * @param uid UID of the tag (8 bytes).
 * @param rx_buffer Buffer to store received data.
 * @param rx_buffer_size Size of the receive buffer.
 * @param received_data_len Pointer to store the length of received data.
 * @return Result code indicating success or error.
 */
static result_t
   st25dv_mb_try_read(const uint8_t uid[8], uint8_t *rx_buffer, uint16_t rx_buffer_size, uint16_t *received_data_len);

/**
 * @brief Try to write a mailbox message to an NFC-V tag.
 *
 * @param uid UID of the tag (8 bytes).
 * @param data Data buffer to write.
 * @param len Length of data to write.
 * @return Result code indicating success or error.
 */
static result_t st25dv_mb_try_write(const uint8_t uid[8], const uint8_t *data, uint16_t len);

/**
 * @brief Refresh the cached state for a given tag family.
 *
 * Preserves the most recent UID and clears the quiet-state flag when the tag is
 * newly detected, ensuring the family can be quieted again during mode switches.
 *
 * @param self NFC reader driver instance.
 * @param type Tag family being updated.
 * @param present True when the family is currently detected.
 * @param uid Pointer to the UID buffer (optional when @p present is false).
 * @param uid_len UID length in bytes.
 */
static result_t
   update_tag_cache(nfc_driver_t *self, NFC_TAG_TYPE type, bool present, const uint8_t *uid, uint8_t uid_len);

/**
 * @brief Issue a stay-quiet command for the provided tag family, once per activation.
 *
 * The function is a no-op if the tag family is absent or already quieted. Upon a
 * successful stay-quiet command the cached quiet flag is latched until the tag is
 * reactivated through an RF field reset.
 *
 * @param self NFC reader driver instance.
 * @param type Tag family to quiet.
 */
static result_t sleep_tag_if_needed(nfc_driver_t *self, NFC_TAG_TYPE type);

/**
 * @brief Clear cached tag state that is no longer valid after RF/Power changes.
 *
 * When the RF field is turned off or the chip enters a low-power state, cached tag presence and quiet-state flags
 * must be cleared to avoid stale state when the field is enabled again.
 *
 * @param self NFC driver instance.
 */
static void clear_cached_tag_state(nfc_driver_t *self);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
#if MAILBOX_BUFFER_SIZE > UINT8_MAX
#   error "MAILBOX_BUFFER_SIZE exceeds the range handled by send_data length parameter"
#endif

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static void reset_detection_state(nfc_driver_t *self)
{
   RETURN_VOID_IF_NULL(self);

   self->_is_tag_present = false;
   self->_tag_uid_len = 0;
   memset(self->_tag_uid, 0, sizeof(self->_tag_uid));
   self->_last_presence_check_ms = 0;
   self->_last_discovery_ms = 0;
   self->_gt_ready = false;
   self->_gt_started = false;
}

static void clear_cached_tag_state(nfc_driver_t *self)
{
   RETURN_VOID_IF_NULL(self);

   reset_detection_state(self);

   for(size_t idx = 0; idx < NFC_TAG_TYPE_MAX; idx++)
   {
      self->_tag_cache[idx].is_present = false;
      self->_tag_cache[idx].uid_len = 0;
      memset(self->_tag_cache[idx].uid, 0, sizeof(self->_tag_cache[idx].uid));
      self->_tag_quiet[idx] = false;
   }
}

static result_t
   update_tag_cache(nfc_driver_t *self, NFC_TAG_TYPE type, bool present, const uint8_t *uid, uint8_t uid_len)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE((type >= NFC_TAG_TYPE_MAX), NFC_R_ERROR_OUT_OF_RANGE);
   RETURN_ERR_IF_TRUE(present && (NULL == uid), NFC_R_ERROR_PTR_NULL);

   nfc_tag_cache_t *cache = &self->_tag_cache[type];
   cache->is_present = present;

   result_t result = RESULT_OK;

   if(present && (NULL != uid) && (uid_len > 0U) && (uid_len <= NFC_TAG_MAX_UID_SIZE))
   {
      cache->uid_len = uid_len;
      memset(cache->uid, 0, sizeof(cache->uid));
      memcpy(cache->uid, uid, uid_len);
      self->_tag_quiet[type] = false;
   }
   else
   {
      cache->uid_len = 0;
      memset(cache->uid, 0, sizeof(cache->uid));
      self->_tag_quiet[type] = false;
   }

   return result;
}

static result_t sleep_tag_if_needed(nfc_driver_t *self, NFC_TAG_TYPE type)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE((type >= NFC_TAG_TYPE_MAX), NFC_R_ERROR_OUT_OF_RANGE);

   if(self->_tag_quiet[type])
   {
      return RESULT_OK;
   }

   const nfc_tag_cache_t *cache = &self->_tag_cache[type];
   if(!cache->is_present)
   {
      return RESULT_OK;
   }

   result_t result = RESULT_OK;

   if((NFC_TAG_TYPE_V == type) && (NFCV_TAG_UID_SIZE == cache->uid_len))
   {
      ReturnCode err = rfalNfcvPollerSleep(RFAL_NFCV_REQ_FLAG_DEFAULT, cache->uid);
      if(RFAL_ERR_NONE == err)
      {
         // Remember that this family has been silenced to avoid duplicate commands.
         self->_tag_quiet[type] = true;
         DEBUG_INFO("NFC-V tag moved to quiet state");
      }
      else
      {
         DEBUG_WARNING("Failed to quiet NFC-V tag: %d", err);
      }
   }

   return result;
}

static bool has_interval_elapsed(uint64_t last_ms, uint64_t current_ms, uint32_t interval_ms)
{
   if(0u == interval_ms)
   {
      return true;
   }

   if((0u == last_ms) || (current_ms < last_ms))
   {
      return true;
   }

   return ((current_ms - last_ms) >= interval_ms);
}

static bool guard_time_ready(nfc_driver_t *self)
{
   if(NULL == self)
   {
      return false;
   }

   if(!self->_gt_started)
   {
      ReturnCode err = rfalFieldOnAndStartGT();
      if(RFAL_ERR_NONE != err)
      {
         DEBUG_ERROR("Failed to start GT: %d", err);
         self->_gt_started = false;
         self->_gt_ready = false;
         return false;
      }

      self->_gt_started = true;
      self->_gt_ready = false;
   }

   if(!self->_gt_ready)
   {
      if(rfalIsGTExpired())
      {
         self->_gt_ready = true;
      }
   }

   return self->_gt_ready;
}

static result_t configure_st25r3916_hw(void)
{
   result_t result = RESULT_OK;

   uint8_t io_conf2;
   uint8_t io2_readback;
   ReturnCode err = RFAL_ERR_NONE;

   if(IS_OK(result))
   {
      err = st25r3916ReadRegister(ST25R3916_REG_IO_CONF2, &io_conf2);
   }
   if(IS_OK(result) && (RFAL_ERR_NONE == err))
   {
      io_conf2 |= (1u << 7); /* sup3V=1 (2.4–3.6V) */
      err = st25r3916WriteRegister(ST25R3916_REG_IO_CONF2, io_conf2);
   }
   if(IS_OK(result) && (RFAL_ERR_NONE == err))
   {
      err = st25r3916ReadRegister(ST25R3916_REG_IO_CONF2, &io2_readback);
   }

   if(IS_OK(result) && (RFAL_ERR_NONE == err) && (io2_readback != io_conf2))
   {
      SET_ERR(result, NFC_R_ERROR_FAILED_I2C);
   }

   return result;
}

static result_t nfca_check_presence(nfc_driver_t *self, uint8_t debounce_fail, bool *is_tag_present)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_tag_present, NFC_R_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   static uint8_t fail = 0;
   rfalNfcaSensRes sens_res = {0};

   ReturnCode err = rfalNfcaPollerCheckPresence(RFAL_14443A_SHORTFRAME_CMD_WUPA, &sens_res);

   if(err == RFAL_ERR_NONE)
   {
      fail = 0;
      *is_tag_present = true;
      // Cache the NFC-A UID so the driver can switch modes without re-reading it.
      update_tag_cache(self, NFC_TAG_TYPE_A, true, self->_tag_uid, self->_tag_uid_len);
   }
   else
   {
      fail++;
      if(fail >= debounce_fail)
      {
         *is_tag_present = false;
         DEBUG_INFO("NFC-A tag not present");
         self->_gt_ready = false;
         ReturnCode gt_err = rfalFieldOnAndStartGT();
         if(RFAL_ERR_NONE == gt_err)
         {
            self->_gt_started = true;
         }
         else
         {
            DEBUG_ERROR("Failed to restart GT after NFC-A absence: %d", gt_err);
            self->_gt_started = false;
         }
         // Invalidate the cache so higher layers know the Type A tag disappeared.
         update_tag_cache(self, NFC_TAG_TYPE_A, false, NULL, 0);
      }
      else
      {
         *is_tag_present = true;
         // Continue reporting the cached UID while we debounce the removal.
         update_tag_cache(self, NFC_TAG_TYPE_A, true, self->_tag_uid, self->_tag_uid_len);
      }
   }

   return result;
}

static result_t nfcv_check_presence_via_mailbox(nfc_driver_t *self, uint8_t debounce_fail, bool *is_tag_present)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_tag_present, NFC_R_ERROR_PTR_NULL);

   RETURN_ERR_IF_TRUE(self->_tag_uid_len != NFCV_TAG_UID_SIZE, NFC_R_ERROR_NO_TAG);

   result_t result = RESULT_OK;

   static uint8_t fail = 0;
   uint8_t len_m1;

   ReturnCode err = rfalST25xVPollerFastReadMsgLength(
      RFAL_NFCV_REQ_FLAG_DEFAULT | RFAL_NFCV_REQ_FLAG_ADDRESS, self->_tag_uid, &len_m1);
   if(err == RFAL_ERR_NONE)
   {
      // Tag detected, reset debouncing count
      fail = 0;
      *is_tag_present = true;
      // Record the latest ISO15693 UID/presence for later quiet/wake decisions.
      update_tag_cache(self, NFC_TAG_TYPE_V, true, self->_tag_uid, self->_tag_uid_len);
   }
   else
   {
      fail++;
   }

   if(fail >= debounce_fail)
   {
      *is_tag_present = false;
      DEBUG_INFO("Tag not present");
      // Clear the cache so future logic knows this family is absent.
      update_tag_cache(self, NFC_TAG_TYPE_V, false, NULL, 0);
   }
   else
   {
      *is_tag_present = true; // not “gone” yet, still debouncing
   }

   return result;
}

static void rtt_dump_3916_core(void)
{
   struct
   {
      uint8_t reg;
      const char *name;
   } lst[] = {
      {ST25R3916_REG_IO_CONF1, "IO_CONF1"},
      {ST25R3916_REG_IO_CONF2, "IO_CONF2"},
      {ST25R3916_REG_OP_CONTROL, "OP_CONTROL"},
      {ST25R3916_REG_RX_CONF1, "RX_CONF1"},
      {ST25R3916_REG_RX_CONF2, "RX_CONF2"},
      {ST25R3916_REG_ANT_TUNE_A, "ANT_TUNE_A"}, // names vary by header
      {ST25R3916_REG_ANT_TUNE_B, "ANT_TUNE_B"},
      {ST25R3916_REG_AUX_DISPLAY, "AUX_DISPLAY"},
   };
   SEGGER_RTT_printf(0, "NFC core register dump:\n");
   for(size_t i = 0; i < ARRAY_SIZE(lst); i++)
   {
      uint8_t v = 0;
      st25r3916ReadRegister(lst[i].reg, &v);
      SEGGER_RTT_printf(0, "%s(0x%02X)=0x%02X\n", lst[i].name, lst[i].reg, v);
   }
}

static result_t discover_nfcv_tag(nfc_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);

   rfalNfcvInventoryRes invRes;
   uint16_t rcvBits = 0;
   result_t result = RESULT_OK;
   bool tag_found = false;

   ReturnCode err = rfalNfcvPollerInventory(RFAL_NFCV_NUM_SLOTS_1, 0, NULL, &invRes, &rcvBits);

   switch(err)
   {
      case RFAL_ERR_NONE:
         // Tag detected
         memcpy(self->_tag_uid, invRes.UID, NFCV_TAG_UID_SIZE);
         self->_tag_uid_len = NFCV_TAG_UID_SIZE;
         self->_is_tag_present = true;
         DEBUG_INFO("NFC-V tag detected");
         SEGGER_RTT_printf(0,
                           "NFC-V UID: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                           invRes.UID[7],
                           invRes.UID[6],
                           invRes.UID[5],
                           invRes.UID[4],
                           invRes.UID[3],
                           invRes.UID[2],
                           invRes.UID[1],
                           invRes.UID[0]);
         tag_found = true;
         break;
      case RFAL_ERR_WRONG_STATE:
         // Treat as transient/not-ready state and retry on the next process loop.
         DEBUG_DEBUG("NFC-V discovery skipped: RFAL wrong state");
         err = RFAL_ERR_NONE;
         break;
      case RFAL_ERR_PARAM:
         DEBUG_ERROR("RFAL_ERR_PARAM");
         break;
      case RFAL_ERR_IO:
         DEBUG_ERROR("RFAL_ERR_IO");
         break;
      case RFAL_ERR_RF_COLLISION:
         DEBUG_ERROR("RFAL_ERR_RF_COLLISION");
         break;
      case RFAL_ERR_CRC:
         DEBUG_ERROR("RFAL_ERR_RF_CRC");
         break;
      case RFAL_ERR_PROTO:
         DEBUG_ERROR("RFAL_ERR_RF_PROTO");
         break;
      case RFAL_ERR_TIMEOUT:
         // No tag, clear error.
         err = RFAL_ERR_NONE;
         break;
      case RFAL_ERR_NOTFOUND:
         err = RFAL_ERR_NONE;
         break;
      default:
         DEBUG_ERROR("Tag discovery, err=%d", err);
         SET_ERR(result, NFC_R_ERROR_DISCOVER_TAG);
         break;
   }

   if(tag_found && (self->_tag_uid_len > 0U))
   {
      // Synchronize the ISO15693 cache with the discovered UID.
      update_tag_cache(self, NFC_TAG_TYPE_V, true, self->_tag_uid, self->_tag_uid_len);
   }
   else if(!tag_found)
   {
      self->_is_tag_present = false;
      self->_tag_uid_len = 0;
      memset(self->_tag_uid, 0, sizeof(self->_tag_uid));
      // Explicitly clear the cache when no tag is reported.
      update_tag_cache(self, NFC_TAG_TYPE_V, false, NULL, 0);
   }

   if((RFAL_ERR_NONE != err) && !tag_found)
   {
      SET_ERR(result, NFC_R_ERROR_DISCOVER_TAG);
   }

   return result;
}

static result_t discover_nfca_tag(nfc_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   rfalNfcaSensRes sens_res = {0};
   ReturnCode err = rfalNfcaPollerTechnologyDetection(RFAL_COMPLIANCE_MODE_ISO, &sens_res);
   bool coll_pending = false;
   rfalNfcaSelRes sel_res = {0};
   uint8_t uid[NFC_TAG_MAX_UID_SIZE] = {0};
   uint8_t uid_len = 0;
   bool tag_found = false;

   if(RFAL_ERR_NONE == err)
   {
      err = rfalNfcaPollerSingleCollisionResolution(1U, &coll_pending, &sel_res, uid, &uid_len);
   }

   if((RFAL_ERR_NONE == err) && !coll_pending && (uid_len > 0U) && (uid_len <= NFC_TAG_MAX_UID_SIZE))
   {
      memcpy(self->_tag_uid, uid, uid_len);
      self->_tag_uid_len = uid_len;
      self->_is_tag_present = true;
      DEBUG_INFO("NFC-A tag detected");

      SEGGER_RTT_printf(0, "NFC-A UID:");
      for(int32_t i = (int32_t)uid_len - 1; i >= 0; i--)
      {
         SEGGER_RTT_printf(0, "%02X", uid[i]);
      }
      SEGGER_RTT_printf(0, "\n");
      tag_found = true;
   }
   else if((RFAL_ERR_TIMEOUT == err) || (RFAL_ERR_NOTFOUND == err) || (RFAL_ERR_PROTO == err))
   {
      if(RFAL_ERR_PROTO == err)
      {
         DEBUG_INFO("NFC-A protocol mismatch, retrying");
      }
      // No tag or unexpected frame, clear error so discovery can retry.
      err = RFAL_ERR_NONE;
   }
   else if(RFAL_ERR_NONE == err)
   {
      DEBUG_INFO("NFC-A collision detected, retrying");
   }
   else
   {
      DEBUG_ERROR("NFC-A discovery err=%d", err);
      SET_ERR(result, NFC_R_ERROR_DISCOVER_TAG);
   }

   if(tag_found && (self->_tag_uid_len > 0U))
   {
      // Store the discovered NFC-A UID for later presence checks.
      update_tag_cache(self, NFC_TAG_TYPE_A, true, self->_tag_uid, self->_tag_uid_len);
   }
   else if(!tag_found)
   {
      self->_is_tag_present = false;
      self->_tag_uid_len = 0;
      memset(self->_tag_uid, 0, sizeof(self->_tag_uid));
      // Drop cached state when no Type A tag is available.
      update_tag_cache(self, NFC_TAG_TYPE_A, false, NULL, 0);
   }

   return result;
}

static result_t discover_tag(nfc_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   switch(self->_configured_tag_type)
   {
      case NFC_TAG_TYPE_V:
         result = discover_nfcv_tag(self);
         break;
      case NFC_TAG_TYPE_A:
         result = discover_nfca_tag(self);
         break;
      case NFC_TAG_TYPE_MAX:
      default:
         SET_ERR(result, NFC_R_ERROR_OUT_OF_RANGE);
         break;
   }

   return result;
}

static result_t configure_detection_mode(nfc_driver_t *self, NFC_TAG_TYPE tag_type, uint8_t power_level)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   ReturnCode err = RFAL_ERR_NONE;
   bool gt_started = false;

   switch(tag_type)
   {
      case NFC_TAG_TYPE_V:
         if(self->_tag_quiet[NFC_TAG_TYPE_V])
         {
            // Wake the quiet ISO15693 family by briefly dropping the RF field.
            ReturnCode err = rfalFieldOff();
            if(err != RFAL_ERR_NONE)
            {
               DEBUG_WARNING("rfalFieldOff failed while waking NFC-V tag: %d", err);
            }
            nrf_delay_ms(FIELD_RESET_DELAY_MS);
            self->_tag_quiet[NFC_TAG_TYPE_V] = false;
         }
         result = set_output_power(&self->interface, power_level);
         if(IS_OK(result))
         {
            err = rfalNfcvPollerInitialize();
         }
         break;
      case NFC_TAG_TYPE_A:
         // Put ISO15693 tags to sleep while the reader works a Type A anticollision cycle.
         result = sleep_tag_if_needed(self, NFC_TAG_TYPE_V);
         if(IS_OK(result))
         {
            result = set_output_power(&self->interface, power_level);
         }

         if(IS_OK(result))
         {
            err = rfalNfcaPollerInitialize();
         }
         break;
      case NFC_TAG_TYPE_MAX:
      default:
         SET_ERR(result, NFC_R_ERROR_OUT_OF_RANGE);
         break;
   }

   if(IS_OK(result) && (RFAL_ERR_NONE != err))
   {
      DEBUG_ERROR("rfal poller init failed: %d", err);
      SET_ERR(result, NFC_R_ERROR_INIT);
   }

   if(IS_OK(result))
   {
      err = rfalFieldOnAndStartGT();
      if(RFAL_ERR_NONE != err)
      {
         DEBUG_ERROR("rfalFieldOnAndStartGT failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_INIT);
      }
      else
      {
         gt_started = true;
      }
   }

   if(IS_OK(result))
   {
      reset_detection_state(self);
      self->_gt_started = gt_started;
      self->_gt_ready = false;
      self->_configured_tag_type = tag_type;
   }

   return result;
}

static result_t check_tag_presence(nfc_driver_t *self, bool *is_tag_present)
{
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_tag_present, NFC_R_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   switch(self->_configured_tag_type)
   {
      case NFC_TAG_TYPE_V:
         result = nfcv_check_presence_via_mailbox(self, TAG_PRESENCE_CHECK_DEBOUNCE_NUM, is_tag_present);
         break;
      case NFC_TAG_TYPE_A:
         result = nfca_check_presence(self, TAG_PRESENCE_CHECK_DEBOUNCE_NUM, is_tag_present);
         break;
      case NFC_TAG_TYPE_MAX:
      default:
         SET_ERR(result, NFC_R_ERROR_OUT_OF_RANGE);
         break;
   }

   if(IS_OK(result) && !(*is_tag_present))
   {
      memset(self->_tag_uid, 0, sizeof(self->_tag_uid));
      self->_tag_uid_len = 0;
   }

   return result;
}

static result_t run_aat_tune(void)
{
   // Broad & safe initial sweep for a new custom coil:
   // - Start both caps in the middle (0x80)
   // - Cover the whole range [0x00..0xFF] with modest step width
   // - Enable dynamic step reduction so the algo zooms in near the optimum
   // Targets:
   //   - Phase target ~90° (quadrature) is a good general-purpose aim
   //   - Amplitude target: mid-high (~200 on an 8-bit scale) – adjust later
   struct st25r3916AatTuneParams param = {
      .aat_a_min = 0x00,
      .aat_a_max = 0xFF,
      .aat_a_start = 0x80,
      .aat_a_stepWidth = 0x08, // 8-count coarse steps to start

      .aat_b_min = 0x00,
      .aat_b_max = 0xFF,
      .aat_b_start = 0x80,
      .aat_b_stepWidth = 0x08,

      .phaTarget = 90u,  // target phase ~90° (tweak if you have a spec)
      .phaWeight = 8u,   // give phase more weight initially
      .ampTarget = 200u, // target amplitude – board-dependent
      .ampWeight = 4u,   // less weight than phase

      .doDynamicSteps = true, // auto-reduce step size near best point
      .measureLimit = 120u    // cap total measurements to keep it quick
   };

   struct st25r3916AatTuneResult tune_result = {0};

   ReturnCode err = RFAL_ERR_NONE;
   result_t result = RESULT_OK;

   err = st25r3916AatTune(&param, &tune_result);

   if(IS_OK(result) && (err != RFAL_ERR_NONE))
   {
      DEBUG_ERROR("AAT tune failed: %d", err);
      SET_ERR(result, NFC_R_ERROR_TUNE_ANT);
   }

   if(IS_OK(result))
   {
      DEBUG_INFO("AAT ok: A=%d B=%d phase=%d amp=%d measures=%d",
                 tune_result.aat_a,
                 tune_result.aat_b,
                 tune_result.pha,
                 tune_result.amp,
                 tune_result.measureCnt);
      // Some RFAL variants already write ANT_TUNE_A/B internally.
      // To be explicit and future-proof, write them back:
      result = st25r3916WriteRegister(ST25R3916_REG_ANT_TUNE_A, tune_result.aat_a);
   }

   if(IS_OK(result))
   {
      result = st25r3916WriteRegister(ST25R3916_REG_ANT_TUNE_B, tune_result.aat_b);
   }

   if(IS_OK(result))
   { // Do not re-assert IO_CONF1; RFAL controls analog config per mode
      result = RESULT_OK;
   }

   return result;
}

/**
 * @brief Mailbox helper using rfal_st25xv.h
 *
 * These use the ST convenience APIs only (no manual frames). Works on ST25DVxxx (Type 5, NFC-V).
 * Try to read a full mailbox message into rx_buffer.
 *    - Returns RFAL_ERR_NONE  -> message copied, length in *received_data_len
 *    - Returns RFAL_ERR_BUSY  -> no message available right now
 *    - Other RFAL errors      -> transport/protocol issue
 *
 *  TODO: update the documentation
 */
static result_t
   st25dv_mb_try_read(const uint8_t uid[8], uint8_t *rx_buffer, uint16_t rx_buffer_size, uint16_t *received_data_len)
{
   RETURN_ERR_IF_NULL(uid, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(rx_buffer, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(received_data_len, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0 == rx_buffer_size, COMMS_DRIVER_ERROR_OUT_OF_RANGE);
   RETURN_ERR_IF_TRUE(rx_buffer_size < MAILBOX_BUFFER_SIZE, COMMS_DRIVER_ERROR_OVERFLOW);

   result_t result = RESULT_OK;

   // 1) Ask for mailbox length first; if not available, treat as BUSY
   uint8_t len_m1 = 0;
   ReturnCode err = rfalST25xVPollerFastReadMsgLength(RFAL_NFCV_REQ_FLAG_DEFAULT, uid, &len_m1);
   if(err != RFAL_ERR_NONE)
   {
      SET_ERR(result, COMMS_DRIVER_ERROR_BUSY); // no message yet / contended
   }

   const uint16_t expected_len = (uint16_t)len_m1 + 1u;

   uint8_t rx_temp[1 + 256]; // 1 status + up to 256 data bytes
   uint16_t rcv_len = 0;
   const uint8_t mb_ptr = 0x00;
   const uint8_t num_bytes = 0x00; // 0 => full message (MB_LEN bytes)

   if(IS_OK(result))
   {
      // 2) Read the whole message in one shot. NOTE: rx_temp[0] is ISO15693 response flags (0x00 on success).

      err = rfalST25xVPollerFastReadMessage(
         RFAL_NFCV_REQ_FLAG_DEFAULT, uid, mb_ptr, num_bytes, rx_temp, sizeof(rx_temp), &rcv_len);
      if((err != RFAL_ERR_NONE) || (rcv_len < 1))
      {
         SET_ERR(result, COMMS_DRIVER_ERROR_BUSY);
      }
   }

   if(IS_OK(result) && (rx_temp[0] != 0x00))
   {
      SET_ERR(result, COMMS_DRIVER_ERROR_COMM_RX); // RFAL_ERR_PROTO
      DEBUG_ERROR("COMMS_DRIVER_ERROR_COMM_RX");
   }

   const uint16_t msg_len = (uint16_t)(rcv_len - 1u);

   // 3) Sanity check: did we receive exactly N = MB_LEN_Dyn + 1 bytes?
   if(IS_OK(result) && (msg_len != expected_len))
   {
      // Common causes: mailbox changed mid-read; partial transfer; stale length. Treat as transient and retry later.
      SET_ERR(result, COMMS_DRIVER_ERROR_BUSY);
   }

   // Copy payload to caller's buffer
   if(IS_OK(result))
   {
      uint16_t copy_len = (msg_len > rx_buffer_size) ? rx_buffer_size : msg_len;
      memcpy(rx_buffer, &rx_temp[1], copy_len);

      if(received_data_len)
      {
         *received_data_len = copy_len;
      }
   }

   return result;
}

/**
 * @brief Try to write one mailbox message (<=256 bytes).
 *
 * @param uid
 * @param tx_data
 * @param len
 * @return result_t
 *
 * - Returns NFC_R_ERROR_NONE  -> accepted by the tag
 * - Returns NFC_R_ERROR_BUSY  -> mailbox not free yet
 */
static result_t st25dv_mb_try_write(const uint8_t uid[8], const uint8_t *tx_data, uint16_t len)
{
   RETURN_ERR_IF_NULL(uid, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(tx_data, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0 == len, COMMS_DRIVER_ERROR_OUT_OF_RANGE);
   RETURN_ERR_IF_TRUE(len > MAILBOX_BUFFER_SIZE, COMMS_DRIVER_ERROR_OVERFLOW);
   RETURN_ERR_IF_TRUE((uint16_t)(len - 1u) > UINT8_MAX, COMMS_DRIVER_ERROR_OVERFLOW);

   uint8_t msg_len_minus_1 = (uint8_t)(len - 1u);

   // API expects "msg_len = number of data bytes - 1" and a temp tx buffer. We keep a small stack buffer for the
   // command builder.
   static uint8_t txBuf[MAILBOX_BUFFER_SIZE]; // small builder buffer used internally by RFAL

   ReturnCode err = rfalST25xVPollerFastWriteMessage(
      RFAL_NFCV_REQ_FLAG_DEFAULT, uid, msg_len_minus_1, tx_data, txBuf, sizeof(txBuf));

   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      // If mailbox is busy (peer hasn't consumed last message), tags typically NACK with a specific error that maps to
      // a non-NONE RFAL code.
      SET_ERR(result, COMMS_DRIVER_ERROR_BUSY);
   }

   return result;
}
/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t set_output_power(const nfc_driver_interface_t *const interface, uint8_t level)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized && !self->_initializing, NFC_R_ERROR_UNIT_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(level > MAX_POWER_LEVEL, NFC_R_ERROR_OUT_OF_RANGE);
   RETURN_OK_IF_TRUE(level == self->_output_power_level);

   result_t result = RESULT_OK;

   // Strongest TX drive (lowest resistance => most current into antenna)
   uint8_t d_res = (uint8_t)(MAX_POWER_LEVEL - level);

   uint8_t cur_val = 0;
   uint8_t new_val = 0;

   ReturnCode err = st25r3916ReadRegister(ST25R3916_REG_TX_DRIVER, &cur_val);

   if(RFAL_ERR_NONE == err)
   {
      new_val = (uint8_t)((cur_val & (uint8_t)~TX_DRIVER_D_RES_MASK) | (d_res & TX_DRIVER_D_RES_MASK));
      err = st25r3916WriteRegister(ST25R3916_REG_TX_DRIVER, new_val);
   }

   if(RFAL_ERR_NONE == err)
   {
      // Readback to verify successful write
      err = st25r3916ReadRegister(ST25R3916_REG_TX_DRIVER, &cur_val);
   }

   if((RFAL_ERR_NONE != err) || (cur_val != new_val))
   {
      SET_ERR(result, NFC_R_ERROR_FAILED_I2C);
   }

   // Re-trim rails for higher TX load
   uint16_t mv = 0;
   if(IS_OK(result))
   {
      err = st25r3916AdjustRegulators(&mv);
   }

   if(RFAL_ERR_NONE != err)
   {
      SET_ERR(result, NFC_R_ERROR_SET_OUTPUT_PWR);
   }

   if(IS_OK(result))
   {
      self->_output_power_level = level;
   }

   return result;
}

static result_t get_output_power(const nfc_driver_interface_t *const interface, uint8_t *level)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(level, NFC_R_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   *level = self->_output_power_level;

   return RESULT_OK;
}

static result_t is_ring_present(const nfc_driver_interface_t *const interface, bool *is_ring_present)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_ring_present, NFC_R_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   *is_ring_present = self->_is_tag_present;

   return RESULT_OK;
}

static result_t
   set_detection_type(const nfc_driver_interface_t *const interface, NFC_TAG_TYPE tag_type, uint8_t power_level)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized && !self->_initializing, NFC_R_ERROR_UNIT_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(tag_type >= NFC_TAG_TYPE_MAX, NFC_R_ERROR_OUT_OF_RANGE);
   result_t result = RESULT_OK;

   // Re-apply configuration when type changes OR when RF/GT was torn down (e.g. after chip power cycle).
   if((self->_configured_tag_type != tag_type) || (!self->_gt_started))
   {
      result = configure_detection_mode(self, tag_type, power_level);
   }
   return result;
}

static result_t send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, COMMS_DRIVER_ERROR_PTR_NULL);
#if MAILBOX_BUFFER_SIZE > UINT8_MAX
   RETURN_ERR_IF_TRUE(length > MAILBOX_BUFFER_SIZE, COMMS_DRIVER_ERROR_OVERFLOW);
#endif
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, COMMS_DRIVER_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(self->_configured_tag_type != NFC_TAG_TYPE_V,
                      COMMS_DRIVER_ERROR_BUSY); // Reporting "Busy" allows the comms stack to handle the Link Layer
                                                // (this unit) as temporarily offline which simply triggers a retry at a
                                                // later stage in the message protocol.

   result_t result = RESULT_OK;

   if(self->_is_tag_present)
   {
      result = st25dv_mb_try_write(self->_tag_uid, data, length);
   }
   return result;
}

static result_t get_packet(const comms_driver_interface_t *const interface, uint8_t *data, uint16_t data_buffer_size)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, COMMS_DRIVER_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, COMMS_DRIVER_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(data_buffer_size < MAILBOX_BUFFER_SIZE, COMMS_DRIVER_ERROR_OVERFLOW);

   RETURN_ERR_IF_TRUE(self->_configured_tag_type != NFC_TAG_TYPE_V,
                      COMMS_DRIVER_ERROR_BUSY); // Reporting "Busy" allows the comms stack to handle the Link Layer
                                                // (this unit) as temporarily offline which simply triggers a retry at a
                                                // later stage in the message protocol.

   result_t result = RESULT_OK;

   if(self->_is_tag_present)
   {
      uint16_t rx_len = 0;
      result = st25dv_mb_try_read(self->_tag_uid, data, MAILBOX_BUFFER_SIZE, &rx_len);
      if(IS_OK(result))
      {
         RETURN_ERR_IF_TRUE(rx_len > UINT8_MAX, COMMS_DRIVER_ERROR_OVERFLOW);
      }
   }

   return result;
}

static result_t get_max_packet_length(const comms_driver_interface_t *const interface, uint16_t *max_packet_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(max_packet_len, COMMS_DRIVER_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, COMMS_DRIVER_ERROR_UNINITIALIZED);

   *max_packet_len = MAILBOX_BUFFER_SIZE;

   return RESULT_OK;
}

static result_t process(const nfc_driver_interface_t *const interface, bool enable_tag_detection)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   result_t result = RESULT_OK;

   rfalWorker(); // Process() equivalent for the RFAL driver. Needs to be run often.

   uint64_t current_ms = 0;

   if(IS_OK(result))
   {
      result = self->_systick_ifc->get_time_ms(self->_systick_ifc, &current_ms);
   }

   if(self->_is_tag_present && IS_OK(result)
      && has_interval_elapsed(self->_last_presence_check_ms, current_ms, CHECK_TAG_PRESENCE_INTERVAL_MS))
   {
      bool ready = true;
      if(NFC_TAG_TYPE_A == self->_configured_tag_type)
      {
         ready = guard_time_ready(self);
      }

      if(ready)
      {
         self->_last_presence_check_ms = current_ms;
         result = check_tag_presence(self, &self->_is_tag_present); // Check tag presence
         if(IS_OK(result) && !self->_is_tag_present)
         {
            // Tag disappeared, restart guard time before next detection
            self->_last_discovery_ms = current_ms;
            self->_gt_ready = false;
            self->_gt_started = false;
         }
      }
   }

   // If no tag is present, try to discover one at regular intervals if detection is enabled
   if(IS_OK(result) && enable_tag_detection && !self->_is_tag_present)
   {
      bool ready = true;
      if(NFC_TAG_TYPE_A == self->_configured_tag_type)
      {
         ready = guard_time_ready(self);
      }

      if(ready && has_interval_elapsed(self->_last_discovery_ms, current_ms, TAG_DETECTION_INTERVAL_MS))
      {
         self->_last_discovery_ms = current_ms;
         result = discover_tag(self); // _is_tag_present is set inside discover_tag if a tag is found
         if(IS_OK(result) && self->_is_tag_present)
         {
            self->_last_presence_check_ms = current_ms;
            if(NFC_TAG_TYPE_A == self->_configured_tag_type)
            {
               self->_gt_ready = false;
               self->_gt_started = false;
            }
         }
      }
   }

   return result;
}

static result_t auto_tune_antenna(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   // There should be no tag present when doing the auto tuning
   RETURN_ERR_IF_TRUE(self->_is_tag_present, NFC_R_ERROR_TAG_PRESENT);

   result_t result = RESULT_OK;
   ReturnCode err = RFAL_ERR_NONE;

   uint16_t result_mV = 0;
   err = st25r3916AdjustRegulators(&result_mV);
   if(RFAL_ERR_NONE != err)
   {
      SET_ERR(result, NFC_R_ERROR_TUNE_ANT);
   }

   if(IS_OK(result))
   {
      result = run_aat_tune();
   }

   return result;
}

static result_t get_tag_uid(const nfc_driver_interface_t *const interface, uint8_t *uid_buffer, uint8_t *uid_length)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(uid_buffer, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(uid_length, NFC_R_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(!self->_is_tag_present, NFC_R_ERROR_NO_TAG);
   RETURN_ERR_IF_TRUE(*uid_length < self->_tag_uid_len, NFC_R_ERROR_BUFFER_OVERFLOW);

   memcpy(uid_buffer, self->_tag_uid, self->_tag_uid_len);
   *uid_length = self->_tag_uid_len;

   return RESULT_OK;
}

static result_t field_off(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   ReturnCode err = rfalFieldOff();
   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Already off / not in an active RF state: idempotent request.
         DEBUG_DEBUG("rfalFieldOff skipped: already in target state");
      }
      else
      {
         DEBUG_WARNING("rfalFieldOff failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
      }
   }

   clear_cached_tag_state(self);
   return result;
}

static result_t field_on_and_start_gt(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   ReturnCode err = rfalFieldOnAndStartGT();
   result_t result = RESULT_OK;
   bool gt_already_running = false;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Field/GT may already be active from prior configuration.
         DEBUG_DEBUG("rfalFieldOnAndStartGT skipped: already in target state");
         gt_already_running = true;
         self->_gt_started = true;
         self->_gt_ready = rfalIsGTExpired();
      }
      else
      {
         DEBUG_WARNING("rfalFieldOnAndStartGT failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
         return result;
      }
   }

   if(IS_OK(result))
   {
      self->_gt_started = true;
      if(!gt_already_running)
      {
         self->_gt_ready = false;
      }
   }
   return result;
}

static result_t chip_power_down(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   ReturnCode err = rfalLowPowerModeStart(RFAL_LP_MODE_PD);
   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Already in low-power state: idempotent request.
         DEBUG_DEBUG("rfalLowPowerModeStart skipped: already in target state");
      }
      else
      {
         DEBUG_WARNING("rfalLowPowerModeStart failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
         return result;
      }
   }

   clear_cached_tag_state(self);
   return result;
}

static result_t chip_power_up(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   ReturnCode err = rfalLowPowerModeStop();
   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Not an error if we are already active
         DEBUG_DEBUG("rfalLowPowerModeStop skipped: already active");
      }
      else
      {
         DEBUG_WARNING("rfalLowPowerModeStop failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
      }

      return result;
   }

   clear_cached_tag_state(self);
   return result;
}

static result_t wakeup_mode_start(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   ReturnCode err = rfalWakeUpModeStart(NULL);
   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Wake-up mode already enabled or incompatible transient state.
         DEBUG_DEBUG("rfalWakeUpModeStart skipped: already in target state");
      }
      else
      {
         DEBUG_WARNING("rfalWakeUpModeStart failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
         return result;
      }
   }

   clear_cached_tag_state(self);
   return result;
}

static result_t wakeup_mode_stop(const nfc_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   ReturnCode err = rfalWakeUpModeStop();
   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Wake-up mode is already stopped.
         DEBUG_DEBUG("rfalWakeUpModeStop skipped: already in target state");
      }
      else
      {
         DEBUG_WARNING("rfalWakeUpModeStop failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
         return result;
      }
   }

   clear_cached_tag_state(self);
   return result;
}

static result_t wakeup_mode_has_woke(const nfc_driver_interface_t *const interface, bool *has_woke)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(has_woke, NFC_R_ERROR_PTR_NULL);
   const nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   rfalWorker();

   rfalWakeUpInfo info = {0};
   ReturnCode err = rfalWakeUpModeGetInfo(false, &info);
   result_t result = RESULT_OK;
   if(RFAL_ERR_NONE != err)
   {
      if(RFAL_ERR_WRONG_STATE == err)
      {
         // Wake-up mode is not active; no wake event to report.
         DEBUG_DEBUG("rfalWakeUpModeGetInfo skipped: wake-up mode inactive");
         *has_woke = false;
      }
      else
      {
         DEBUG_WARNING("rfalWakeUpModeGetInfo failed: %d", err);
         SET_ERR(result, NFC_R_ERROR_POWER_STATE);
         return result;
      }
   }

   *has_woke = info.indAmp.irqWu || info.indPha.irqWu || info.cap.irqWu;
   return result;
}

static result_t is_gt_expired(const nfc_driver_interface_t *const interface, bool *is_expired)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(is_expired, NFC_R_ERROR_PTR_NULL);
   nfc_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, NFC_R_ERROR_UNIT_UNINITIALIZED);

   const bool expired = rfalIsGTExpired();
   *is_expired = expired;
   if(expired)
   {
      self->_gt_ready = true;
   }
   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t nfc_driver_init(nfc_driver_t *const self,
                         const i2c_driver_interface_t *i2c_interface,
                         const system_time_interface_t *systick_interface)
{
   // Example implementation of module_name_init below
   RETURN_ERR_IF_NULL(self, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(i2c_interface, NFC_R_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(systick_interface, NFC_R_ERROR_PTR_NULL);

   self->_initialized = false;
   self->_initializing = true;
   self->interface.parent = self;
   self->data_ifc.parent = self;
   self->_i2c_ifc = i2c_interface;
   self->_systick_ifc = systick_interface;
   self->_is_tag_present = false;
   memset(self->_tag_uid, 0, sizeof(self->_tag_uid));
   self->_tag_uid_len = 0;
   self->_configured_tag_type = NFC_TAG_TYPE_MAX;
   self->_output_power_level = 0;
   self->_last_presence_check_ms = 0;
   self->_last_discovery_ms = 0;
   self->_gt_ready = false;
   self->_gt_started = false;

   // Reset cached tag metadata before enabling the RF front-end.
   for(size_t idx = 0; idx < NFC_TAG_TYPE_MAX; idx++)
   {
      self->_tag_cache[idx].is_present = false;
      self->_tag_cache[idx].uid_len = 0;
      memset(self->_tag_cache[idx].uid, 0, sizeof(self->_tag_cache[idx].uid));
      self->_tag_quiet[idx] = false;
   }

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.get_output_power = get_output_power;
   self->interface.is_ring_present = is_ring_present;
   self->interface.set_output_power = set_output_power;
   self->interface.set_detection_type = set_detection_type;
   self->interface.process = process;
   self->interface.auto_tune_antenna = auto_tune_antenna;
   self->data_ifc.get_packet = get_packet;
   self->data_ifc.send_packet = send_packet;
   self->data_ifc.get_max_packet_length = get_max_packet_length;
   self->interface.get_tag_uid = get_tag_uid;
   self->interface.field_off = field_off;
   self->interface.field_on_and_start_gt = field_on_and_start_gt;
   self->interface.chip_power_down = chip_power_down;
   self->interface.chip_power_up = chip_power_up;
   self->interface.wakeup_mode_start = wakeup_mode_start;
   self->interface.wakeup_mode_stop = wakeup_mode_stop;
   self->interface.wakeup_mode_has_woke = wakeup_mode_has_woke;
   self->interface.is_gt_expired = is_gt_expired;

   // Initialize NRF5 - ST RFAL glue unit
   result_t result = rfal_nrf_platform_init(self->_i2c_ifc, self->_systick_ifc);

   // 1) IRQ FIRST: make sure 3916 IRQ can fire during init
   if(IS_OK(result))
   {
      platformIrqST25RPinInitialize();
      platformIrqST25RSetCallback(st25r3916Isr);
   }

   // 2) RFAL init (this powers the chip, starts XTAL, waits for osc_ok via IRQ)
   ReturnCode err = rfalInitialize();
   if(RFAL_ERR_NONE != err)
   {
      DEBUG_ERROR("rfalInitialize failed: %d", err);
   }

   /* 2.1) RFAL NFC init  */
   if(RFAL_ERR_NONE == err)
   {
      err = rfalNfcInitialize();
      if(RFAL_ERR_NONE != err)
      {
         DEBUG_ERROR("rfalNfcInitialize failed: %d", err);
      }
   }

   // 3) Optional: basic HW tweaks once the chip is up
   if(RFAL_ERR_NONE == err)
   {
      result = configure_st25r3916_hw();
   }

   // 4) Configure default detection mode
   if((RFAL_ERR_NONE == err) && IS_OK(result))
   {
      result = self->interface.set_detection_type(&self->interface, NFC_TAG_TYPE_V, TAGV_WIRELESS_OUTPUT_PWR_LEVEL);
   }

   // Set default wireless output power
   if((RFAL_ERR_NONE == err) && IS_OK(result))
   {
      rtt_dump_3916_core();
      result = set_output_power(&self->interface, TAGV_WIRELESS_OUTPUT_PWR_LEVEL);
      if(IS_OK(result))
      {
         self->_output_power_level = TAGV_WIRELESS_OUTPUT_PWR_LEVEL;
      }
      else
      {
         DEBUG_ERROR("nfc_set_nfc_power failed: %d", err);
      }
   }

   if((RFAL_ERR_NONE != err) && IS_OK(result))
   {
      SET_ERR(result, NFC_R_ERROR_INIT);
   }

   if(IS_OK(result))
   {
      self->_is_tag_present = false;
      memset(self->_tag_uid, 0, sizeof(self->_tag_uid));
      self->_tag_uid_len = 0;
      self->_initialized = true;

#ifdef DOCK_NFC_AUTOTUNE_ON_BOOT
      result_t tune_status = self->interface.auto_tune_antenna(&self->interface);
      if(!IS_OK(tune_status))
      {
         DEBUG_ERROR("Auto-tune on boot failed: %d", tune_status);
      }
      else
      {
         DEBUG_INFO("Auto-tune on boot completed");
      }
#endif
   }

   self->_initializing = false;

   return result;
}
