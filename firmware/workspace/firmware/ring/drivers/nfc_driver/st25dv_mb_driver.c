/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file st25dv_mb_driver.c
 * @ingroup nfc_tag_driver
 * @brief ST25DV NFC tag driver implementation for the ring platform.
 *
 * Implements mailbox, interrupt, and EEPROM access for the ST25DV NFC tag, including integration with the ST driver and
 * project-specific error handling. All operations are blocking and intended for use in non-interrupt context.
 *
 * References:
 *   - ST25DV Datasheet DS10925 - Rev 11
 *   - ST25DV04K (4kbit EEPROM, mailbox, GPO interrupt)
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include "nrf_delay.h"

// Custom includes
#include "st25dv_mb_driver.h"

// Forward declaration from external ST library
/**
 * @brief Initialize the ST25DV object.
 * @param pObj Pointer to ST25DV object.
 * @return Status code.
 */
int32_t ST25DV_Init(ST25DV_Object_t *const pObj);

/**
 * @brief Read data from ST25DV tag.
 * @param pObj Pointer to ST25DV object.
 * @param pData Pointer to data buffer.
 * @param TarAddr Target address to read from.
 * @param NbByte Number of bytes to read.
 * @return Status code.
 */
int32_t ST25DV_ReadData(const ST25DV_Object_t *const pObj,
                        uint8_t *const pData,
                        const uint16_t TarAddr,
                        const uint16_t NbByte);

/**
 * @brief Write data to ST25DV tag.
 * @param pObj Pointer to ST25DV object.
 * @param pData Pointer to data buffer.
 * @param TarAddr Target address to write to.
 * @param NbByte Number of bytes to write.
 * @return Status code.
 */
int32_t ST25DV_WriteData(const ST25DV_Object_t *const pObj,
                         const uint8_t *const pData,
                         const uint16_t TarAddr,
                         const uint16_t NbByte);

/**
 * @brief Configure GPO pin on ST25DV tag.
 * @param pObj Pointer to ST25DV object.
 * @param ITConf Interrupt configuration value.
 * @return Status code.
 */
int32_t ST25DV_ConfigureGPO(const ST25DV_Object_t *const pObj, const uint16_t ITConf);

/**
 * @brief Get GPO status from ST25DV tag.
 * @param pObj Pointer to ST25DV object.
 * @param pGPOStatus Pointer to GPO status value.
 * @return Status code.
 */
int32_t ST25DV_GetGPOStatus(const ST25DV_Object_t *const pObj, uint16_t *const pGPOStatus);

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_NFC_TAG_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define MAILBOX_BUFFER_SIZE (NFC_MAILBOX_BUFFER_SIZE)

#define I2C_TIMEOUT_US (10000u) // Allow ample time for full-length (255B) transfers at 400kHz plus margin
#define I2C_INTER_WRITE_DELAY_MS                                                                                       \
   (20u) // Delay between consecutive I2C writes during startup only. Determined empirically.
#define I2C_PASSWORD_DELAY_MS (10u)

#define ST25DV_I2C_MAX_WRITE_CHUNK (UINT8_MAX - sizeof(uint16_t)) // I2C driver accepts uint8_t lengths.
#define ST25DV_I2C_MAX_WRITE_FRAME (ST25DV_I2C_MAX_WRITE_CHUNK + sizeof(uint16_t))
#define ST25DV_I2C_MAX_READ_CHUNK  (UINT8_MAX)

#define MB_WDG_TIME_30_MS (1u) // Watch dog duration = 2(MB_WDG-1)x30ms±6. See Table 17 in datasheet.

/**
 * EEPROM write timing calculations:
 *
 * The ST25DV04K has a 4kbit (512 byte) EEPROM. Writes are done internally in 4 byte pages. Written data does not need
 * to be byte aligned. The page boundaries only affect write timing. From the datasheet section 6.4.2, the EEPROM write
 * time (tw in Table 248) is max 5ms per 4 byte page. Total EEPROM write time is tw x number of pages written.
 */
#define BYTES_PER_EEPROM_PAGE                                (4u)
#define ST25DV04K_MAX_EEPROM_WRITE_TIME_MS_PER_4BYTE_PAGE_MS (6u) // Max time per 4 byte page write including margin
// Max time for full EEPROM write for ST25DV04K chip
#define ST25DV04K_MAX_EEPROM_WRITE_TIME_MS                                                                             \
   (ST25DV04K_MAX_EEPROM_WRITE_TIME_MS_PER_4BYTE_PAGE_MS * NFC_TAG_MAX_EEPROM_SIZE)
#define ST25DV_EEPROM_WRITE_TIME_MS(length)                                                                            \
   (((((length) + 3u) / 4u) * ST25DV04K_MAX_EEPROM_WRITE_TIME_MS_PER_4BYTE_PAGE)                                       \
    + ST25DV04K_MAX_EEPROM_WRITE_TIME_MS_PER_4BYTE_PAGE_MS) // Total max write time plus margin
#define EEPROM_READY_POLL_TRIALS      (20u)                 // Poll trials for EEPROM ready status
#define EEPROM_READY_POLL_INTERVAL_MS (2u)                  // Poll interval when waiting for EEPROM write to complete

// Macros for end area indexes
#define ST25DV_AREA_END1_IDX (0u)
#define ST25DV_AREA_END2_IDX (1u)
#define ST25DV_AREA_END3_IDX (2u)
#define ST25DV_AREA_END4_IDX (3u)

#define ST25DV_AREA_END_AREA_GRANULARITY_BYTES                                                                         \
   (32u) // Area end granularity in bytes. See datasheet section 4.2.1 User memory areas

/**
 * @brief Returns the last valid byte index for a given total size.
 *
 * If total > 0, returns total - 1; otherwise returns 0.
 *
 * @param total The total number of bytes.
 * @return The last valid byte index.
 */
static inline uint32_t last_valid_byte(uint32_t total)
{
   return (total > 0u) ? (total - 1u) : 0u;
}

/**
 * @brief Returns a value if a statement evaluates to true.
 *
 * This macro checks if the provided statement is true. If so, it immediately returns the specified value from the
 * calling function.
 *
 * @param[in] statement The condition to evaluate.
 * @param[in] val The value to return if the condition is true.
 *
 * @note This macro is useful for early returns in error checking and validation scenarios.
 */
#define RETURN_VAL_IF_TRUE(statement, val)                                                                             \
   do                                                                                                                  \
   {                                                                                                                   \
      if((statement))                                                                                                  \
      {                                                                                                                \
         return val;                                                                                                   \
      }                                                                                                                \
   } while(0)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
/**
 * @brief Retrieve mailbox data from the tag.
 * @param interface Pointer to comms driver interface.
 * @param buf Buffer to store received data.
 * @param max_len Maximum length to read.
 * @return Result code.
 */
static result_t get_packet(const comms_driver_interface_t *const interface, uint8_t *buf, uint16_t max_len);

/**
 * @brief Send mailbox data to the tag.
 * @param interface Pointer to comms driver interface.
 * @param data Data buffer to send.
 * @param len Length of data to send.
 * @return Result code.
 */
static result_t send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t len);

static result_t get_max_packet_length(const comms_driver_interface_t *const interface, uint16_t *max_packet_len);

/**
 * @brief Get mailbox status from the tag.
 * @param interface Pointer to NFC tag driver interface.
 * @param status Pointer to mailbox status structure.
 * @return Result code.
 */
static result_t get_status(const nfc_tag_driver_interface_t *const interface, st25dv_mb_status_t *status);

/**
 * @brief Get interrupt status from the tag.
 * @param interface Pointer to NFC tag driver interface.
 * @param interrupt_fired Pointer to store interrupt status.
 * @return Result code.
 */
static result_t get_interrupt_status(const nfc_tag_driver_interface_t *const interface, bool *interrupt_fired);

/**
 * @brief Read EEPROM data from the tag (blocking).
 * @param interface Pointer to NFC tag driver interface.
 * @param address EEPROM address to read from.
 * @param buffer Buffer to store read data.
 * @param length Number of bytes to read.
 * @return Result code.
 */
static result_t read_eeprom_blocking(const nfc_tag_driver_interface_t *const interface,
                                     uint16_t address,
                                     uint8_t *buffer,
                                     uint16_t length);

/**
 * @brief Write EEPROM data to the tag (blocking).
 * @param interface Pointer to NFC tag driver interface.
 * @param address EEPROM address to write to.
 * @param data Data buffer to write.
 * @param length Number of bytes to write.
 * @return Result code.
 */
static result_t write_eeprom_blocking(const nfc_tag_driver_interface_t *const interface,
                                      uint16_t address,
                                      const uint8_t *data,
                                      uint16_t length);

// Non-interface helpers
/**
 * @brief Initialize the ST25DV bus.
 * @return Status code.
 */
static int32_t st25dv_bus_init(void);

/**
 * @brief Deinitialize the ST25DV bus.
 * @return Status code.
 */
static int32_t st25dv_bus_deinit(void);

/**
 * @brief Write data to ST25DV over I2C bus.
 * @param dev_addr Device address.
 * @param mem_addr Memory address.
 * @param data Data buffer to write.
 * @param size Number of bytes to write.
 * @return Status code.
 */
static int32_t st25dv_bus_write(uint16_t dev_addr, uint16_t mem_addr, const uint8_t *data, uint16_t size);

/**
 * @brief Read data from ST25DV over I2C bus.
 * @param dev_addr Device address.
 * @param mem_addr Memory address.
 * @param data Buffer to store read data.
 * @param size Number of bytes to read.
 * @return Status code.
 */
static int32_t st25dv_bus_read(uint16_t dev_addr, uint16_t mem_addr, uint8_t *data, uint16_t size);

/**
 * @brief Check if ST25DV device is ready for communication.
 * @param dev_addr Device address.
 * @param trials Number of trials to check.
 * @return Status code.
 */
static int32_t st25dv_bus_is_ready(uint16_t dev_addr, uint32_t trials);

/**
 * @brief Get current tick value from system timer.
 * @return Tick value.
 */
static int32_t st25dv_bus_get_tick(void);

/**
 * @brief Translate ST's NFCTAG status codes into the local result scheme.
 *
 * Converts ST driver return codes to project-specific error codes, preserving BUSY/NACK/timeout conditions.
 * Ensures all NFC driver errors are consistently encoded for higher-level error handling.
 *
 * @param status Status code returned by ST driver API.
 * @param default_err Default error code to use for unknown errors.
 * @return result_t Project-specific result code.
 */
static result_t convert_nfctag_status(int32_t status, ST25DV_DRV_ERROR default_err);

/**
 * @brief Map mailbox-facing ST25DV errors to COMMS-layer errors.
 *
 * The comms interface must classify transient mailbox states as BUSY so
 * higher layers can treat them as "no packet available right now."
 *
 * @param mailbox_result Result returned by mailbox/status operations.
 * @param transport_err COMMS error used for non-transient transport failures.
 * @return Result in the COMMS error domain.
 */
static result_t map_mailbox_result_to_comms(result_t mailbox_result, COMMS_DRIVER_ERROR transport_err);

/**
 * @brief Ensure I2C session is unlocked for configuration.
 *
 * The tag protects the system registers behind a password gate. During init we must verify
 * the session state and, if necessary, present the default password and poll until the tag
 * confirms the session is open. Subsequent register writes depend on this guard.
 *
 * @param self Pointer to driver structure.
 * @return Result code.
 */
static result_t ensure_i2c_session_open(st25dv_driver_t *self);

/**
 * @brief Verify EEPROM write by reading back and comparing data.
 *
 * For large transfers, only a prefix and suffix are checked to conserve memory.
 *
 * @param self Pointer to driver structure.
 * @param address EEPROM address.
 * @param data Data buffer to verify.
 * @param length Number of bytes to verify.
 * @return Result code.
 */
static result_t verify_eeprom_write(st25dv_driver_t *self, uint16_t address, const uint8_t *data, uint16_t length);

/**
 * @brief Cache the EEPROM size so future accesses can be bounds-checked.
 *
 * Reads and stores the EEPROM geometry from the ST25DV tag for use in bounds checking and area mapping.
 *
 * @param self Pointer to driver structure.
 * @return result_t Result code indicating success or error.
 */
static result_t update_mem_size(st25dv_driver_t *self);

/**
 * @brief Ensure an EEPROM transaction stays within the discovered memory size.
 *
 * Validates that the requested EEPROM address and length are within the tag's memory bounds and do not cross area
 * boundaries.
 *
 * @param self Pointer to driver structure.
 * @param address EEPROM address to check.
 * @param length Number of bytes to check.
 * @return result_t Result code indicating success or error.
 */
static result_t validate_bounds(st25dv_driver_t *self, uint16_t address, uint16_t length);

/**
 * @brief Get mailbox enabled state.
 * @param self Pointer to driver structure.
 * @param enabled Pointer to store enabled state.
 * @return Result code.
 */
static result_t get_mailbox_enabled(st25dv_driver_t *self, bool *enabled);

/**
 * @brief Set mailbox enabled state.
 * @param self Pointer to driver structure.
 * @param enable Enable or disable mailbox.
 * @return Result code.
 */
static result_t set_mailbox_enabled(st25dv_driver_t *self, bool enable);

/**
 * @brief Update cached area boundaries.
 * @param self Pointer to driver structure.
 * @return Result code.
 */
static result_t update_area_boundaries(st25dv_driver_t *self);

/**
 * @brief Get cached end byte of an EEPROM area.
 * @param self Pointer to driver structure.
 * @param area_index Index of area.
 * @param end_byte Pointer to store end byte value.
 * @return Result code.
 */
static result_t get_area_end_byte(const st25dv_driver_t *self, uint8_t area_index, uint32_t *end_byte);

/**
 * @brief Configure EEPROM areas and RF protections to block RF access to user data.
 *
 * Shrinks Area 1 to the minimum (32 bytes) and locks Areas 2+ against RF read/write access while leaving I2C access
 * unchanged. Also locks configuration to prevent RF-side re-enabling of access.
 *
 * @param self Pointer to driver structure.
 * @return Result code.
 */
static result_t configure_eeprom_protection(st25dv_driver_t *self);
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

// This is necessary to access this driver inside function calls from the external ST driver
static st25dv_driver_t *mp_self = NULL;
static uint8_t m_interrupt_pin = 0;
static volatile bool m_gpo_detected = false;
static const ST25DV_PASSWD DEFAULT_I2C_PASSWORD = {.MsbPasswd = 0u, .LsbPasswd = 0u};
static uint64_t m_end_of_last_write_ms = 0; // Timestamp of last completed EEPROM write
static bool m_write_eeprom = false;         // Indicates if the last operation was a write

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

static result_t convert_nfctag_status(int32_t status, ST25DV_DRV_ERROR default_err)
{
   result_t result = RESULT_OK;

   switch(status)
   {
      case NFCTAG_OK:
         // Nothing to do.
         break;
      case NFCTAG_BUSY:
         SET_ERR(result, ST25DV_DRV_ERROR_BUSY);
         break;
      case NFCTAG_TIMEOUT:
         SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         break;
      case NFCTAG_NACK:
         SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         break;
      default:
         SET_ERR(result, default_err);
         break;
   }

   return result;
}

static result_t map_mailbox_result_to_comms(result_t mailbox_result, COMMS_DRIVER_ERROR transport_err)
{
   if(IS_OK(mailbox_result))
   {
      return RESULT_OK;
   }

   result_t comms_result = RESULT_OK;
   const bool is_local_mailbox_busy = (GET_ERR_UNIT(mailbox_result) == THIS_UNIT_ID)
                                      && ((GET_ERR_CODE(mailbox_result) == ST25DV_DRV_ERROR_BUSY)
                                          || (GET_ERR_CODE(mailbox_result) == ST25DV_DRV_ERROR_NO_MESSAGE));

   if(is_local_mailbox_busy)
   {
      SET_ERR(comms_result, COMMS_DRIVER_ERROR_BUSY);
   }
   else
   {
      SET_ERR(comms_result, transport_err);
   }

   return comms_result;
}

static result_t update_mem_size(st25dv_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);

   ST25DV_MEM_SIZE mem_info = {0};
   int32_t nfctag_status = ST25DV_ReadMemSize(&self->_st_device, &mem_info);
   result_t result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

   if(IS_OK(result))
   {
      const uint32_t total_bytes = ((uint32_t)(mem_info.Mem_Size) + 1u) * ((uint32_t)(mem_info.BlockSize) + 1u);
      self->_total_eeprom_bytes = total_bytes;
      for(size_t idx = 0u; idx < ARRAY_SIZE(self->_area_end_bytes); idx++)
      {
         self->_area_end_bytes[idx] = last_valid_byte(total_bytes);
      }
      DEBUG_INFO("ST25DV EEPROM geometry: Mem_Size=%u BlockSize=%u TotalBytes=%u",
                 mem_info.Mem_Size,
                 mem_info.BlockSize,
                 total_bytes);
   }
   else
   {
      DEBUG_ERROR("Failed to read EEPROM geometry, error=(%d,%d)", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   return result;
}

static result_t validate_bounds(st25dv_driver_t *self, uint16_t address, uint16_t length)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);

   // Zero-length operations are permitted.
   RETURN_OK_IF_TRUE(0u == length);

   result_t result = RESULT_OK;
   const uint32_t end = (uint32_t)address + (uint32_t)length;

   if((0u == self->_total_eeprom_bytes) || (end > self->_total_eeprom_bytes))
   {
      SET_ERR(result, ST25DV_DRV_ERROR_OVERFLOW);
      DEBUG_ERROR("Out of range.");
   }

   if(IS_OK(result) && (0u == self->_area_end_bytes[ST25DV_AREA_END3_IDX]) && (self->_total_eeprom_bytes > 0u))
   {
      // Refresh area map if it was never populated.
      result = update_area_boundaries(self);
   }

   // Check area boundaries: sequential operations cannot cross into the next area.
   if(IS_OK(result))
   {
      uint32_t area_end = 0u;
      if(IS_ERR(get_area_end_byte(self, 0u, &area_end)))
      {
         area_end = self->_total_eeprom_bytes ? (self->_total_eeprom_bytes - 1u) : 0u;
      }

      if(address > area_end)
      {
         // If starting beyond area1, walk areas until we find the containing one.
         for(uint8_t area = ST25DV_AREA_END2_IDX; area <= ST25DV_AREA_END4_IDX; area++)
         {
            if(IS_OK(get_area_end_byte(self, area, &area_end)) && (address <= area_end))
            {
               break;
            }
         }
      }

      if((uint32_t)address + (uint32_t)length - 1u > area_end)
      {
         SET_ERR(result, ST25DV_DRV_ERROR_OVERFLOW);
         DEBUG_ERROR("Write/read crosses area boundary.");
      }
   }

   return result;
}

static result_t get_mailbox_enabled(st25dv_driver_t *self, bool *enabled)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(enabled, ST25DV_DRV_ERROR_PTR_NULL);

   ST25DV_EN_STATUS mben = ST25DV_DISABLE;
   int32_t nfctag_status = ST25DV_GetMBEN_Dyn(&self->_st_device, &mben);
   result_t result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

   if(IS_OK(result))
   {
      *enabled = (ST25DV_ENABLE == mben);
   }

   return result;
}

static result_t set_mailbox_enabled(st25dv_driver_t *self, bool enable)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);

   int32_t nfctag_status = enable ? ST25DV_SetMBEN_Dyn(&self->_st_device) : ST25DV_ResetMBEN_Dyn(&self->_st_device);
   result_t result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

   return result;
}

static result_t update_area_boundaries(st25dv_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   // Ensure we have total size first.
   if(0u == self->_total_eeprom_bytes)
   {
      result = update_mem_size(self);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("Failed to update memory size before reading area boundaries, error=(%d,%d)",
                     GET_ERR_UNIT(result),
                     GET_ERR_CODE(result));
      }
   }

   uint8_t enda_raw[3u] = {0};
   int32_t status = 0;

   if(IS_OK(result))
   {
      status = ST25DV_ReadEndZonex(&self->_st_device, ST25DV_ZONE_END1, &enda_raw[0u]);
      result = convert_nfctag_status(status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   if(IS_OK(result))
   {
      status = ST25DV_ReadEndZonex(&self->_st_device, ST25DV_ZONE_END2, &enda_raw[1u]);
      result = convert_nfctag_status(status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   if(IS_OK(result))
   {
      status = ST25DV_ReadEndZonex(&self->_st_device, ST25DV_ZONE_END3, &enda_raw[2u]);
      result = convert_nfctag_status(status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   // Convert ENDAx values (32-byte units) to byte addresses, clamped to total size and monotonic.
   uint32_t total = self->_total_eeprom_bytes;
   uint32_t prev_end = last_valid_byte(total);
   if(IS_OK(result))
   {
      for(int8_t idx = 2; idx >= 0; idx--)
      {
         uint32_t end_byte = (((uint32_t)enda_raw[idx]) + 1u) * ST25DV_AREA_END_AREA_GRANULARITY_BYTES;
         if(end_byte > 0u)
         {
            end_byte -= 1u; // inclusive end
         }
         if(end_byte >= total)
         {
            end_byte = last_valid_byte(total);
         }
         // Enforce monotonic growth: lower indices cannot extend beyond later ones.
         if(end_byte > prev_end)
         {
            end_byte = prev_end;
         }
         self->_area_end_bytes[idx] = end_byte;
         prev_end = end_byte;
         DEBUG_INFO("ST25DV Area %d end byte: %lu", idx + 1, (unsigned long)end_byte);
      }

      // Area4 always ends at total-1. ST25DV_AREA_END1_IDX
      self->_area_end_bytes[ST25DV_AREA_END4_IDX] = last_valid_byte(total);
      DEBUG_INFO("ST25DV Area 4 end byte: %lu", (unsigned long)self->_area_end_bytes[ST25DV_AREA_END4_IDX]);
   }
   else
   {
      DEBUG_ERROR("Failed to read area boundaries, error=%d", GET_ERR_CODE(result));
   }

   return result;
}

static result_t get_area_end_byte(const st25dv_driver_t *self, uint8_t area_index, uint32_t *end_byte)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(end_byte, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(area_index >= ARRAY_SIZE(self->_area_end_bytes), ST25DV_DRV_ERROR_OUT_OF_RANGE);

   *end_byte = self->_area_end_bytes[area_index];
   return RESULT_OK;
}

static result_t configure_eeprom_protection(st25dv_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   if(0u == self->_total_eeprom_bytes)
   {
      result = update_mem_size(self);
   }

   uint32_t total_bytes = self->_total_eeprom_bytes;

   if(IS_OK(result) && (total_bytes < ST25DV_AREA_END_AREA_GRANULARITY_BYTES))
   {
      SET_ERR(result, ST25DV_DRV_ERROR_OVERFLOW);
      DEBUG_ERROR("EEPROM too small to configure areas.");
   }

   // Compute the max ENDA value for the present part (last 32-byte chunk index).
   uint8_t max_enda = 0u;
   if(IS_OK(result))
   {
      max_enda = (uint8_t)(((total_bytes - 1u) / ST25DV_AREA_END_AREA_GRANULARITY_BYTES) & 0xFFu);
   }

   // Read current ENDAx values to avoid redundant writes.
   uint8_t current_enda1 = 0u;
   uint8_t current_enda2 = 0u;
   uint8_t current_enda3 = 0u;
   int32_t nfctag_status = NFCTAG_OK;
   if(IS_OK(result))
   {
      nfctag_status = ST25DV_ReadEndZonex(&self->_st_device, ST25DV_ZONE_END1, &current_enda1);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_ReadEndZonex(&self->_st_device, ST25DV_ZONE_END2, &current_enda2);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_ReadEndZonex(&self->_st_device, ST25DV_ZONE_END3, &current_enda3);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   bool needs_area_config = false;

   // Check if reconfiguration is needed. We want Area 1 to end at 0, Areas 2 and 3 to end at max.
   if(IS_OK(result) && ((current_enda1 != 0u) || (current_enda2 != max_enda) || (current_enda3 != max_enda)))
   {
      needs_area_config = true;
   }

   // Per datasheet guidance (see Area size programming), program upper boundaries first, then shrink Area 1.
   if(IS_OK(result) && needs_area_config)
   {
      nfctag_status = ST25DV_WriteEndZonex(&self->_st_device, ST25DV_ZONE_END3, max_enda);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   if(IS_OK(result) && needs_area_config)
   {
      nfctag_status = ST25DV_WriteEndZonex(&self->_st_device, ST25DV_ZONE_END2, max_enda);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   if(IS_OK(result) && needs_area_config)
   {
      nfctag_status = ST25DV_WriteEndZonex(&self->_st_device, ST25DV_ZONE_END1, 0u);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   // Refresh area cache after reconfiguration (or if previously unset).
   if(IS_OK(result))
   {
      result = update_area_boundaries(self);
   }

   // RF protections: Area 1 write-protected; Areas 2-4 require RF password and deny RF read/write.
   if(IS_OK(result))
   {
      ST25DV_RF_PROT_ZONE area1_prot = {.PasswdCtrl = ST25DV_NOT_PROTECTED, .RWprotection = ST25DV_WRITE_PROT};
      ST25DV_RF_PROT_ZONE current_zone = {0};
      nfctag_status = ST25DV_ReadRFZxSS(&self->_st_device, ST25DV_PROT_ZONE1, &current_zone);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

      if(IS_OK(result)
         && ((current_zone.PasswdCtrl != area1_prot.PasswdCtrl)
             || (current_zone.RWprotection != area1_prot.RWprotection)))
      {
         nfctag_status = ST25DV_WriteRFZxSS(&self->_st_device, ST25DV_PROT_ZONE1, area1_prot);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }
   }

   if(IS_OK(result))
   {
      ST25DV_RF_PROT_ZONE locked_zone = {.PasswdCtrl = ST25DV_PROT_PASSWD3, .RWprotection = ST25DV_READWRITE_PROT};
      ST25DV_RF_PROT_ZONE current_zone = {0};

      nfctag_status = ST25DV_ReadRFZxSS(&self->_st_device, ST25DV_PROT_ZONE2, &current_zone);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      if(IS_OK(result)
         && ((current_zone.PasswdCtrl != locked_zone.PasswdCtrl)
             || (current_zone.RWprotection != locked_zone.RWprotection)))
      {
         nfctag_status = ST25DV_WriteRFZxSS(&self->_st_device, ST25DV_PROT_ZONE2, locked_zone);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }

      if(IS_OK(result))
      {
         nfctag_status = ST25DV_ReadRFZxSS(&self->_st_device, ST25DV_PROT_ZONE3, &current_zone);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }

      if(IS_OK(result)
         && ((current_zone.PasswdCtrl != locked_zone.PasswdCtrl)
             || (current_zone.RWprotection != locked_zone.RWprotection)))
      {
         nfctag_status = ST25DV_WriteRFZxSS(&self->_st_device, ST25DV_PROT_ZONE3, locked_zone);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }

      if(IS_OK(result))
      {
         nfctag_status = ST25DV_ReadRFZxSS(&self->_st_device, ST25DV_PROT_ZONE4, &current_zone);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }

      if(IS_OK(result)
         && ((current_zone.PasswdCtrl != locked_zone.PasswdCtrl)
             || (current_zone.RWprotection != locked_zone.RWprotection)))
      {
         nfctag_status = ST25DV_WriteRFZxSS(&self->_st_device, ST25DV_PROT_ZONE4, locked_zone);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }
   }

   // Lock configuration registers against RF-side changes; I2C can still modify when authenticated.
   if(IS_OK(result))
   {
      ST25DV_LOCK_STATUS lock_cfg = ST25DV_UNLOCKED;
      nfctag_status = ST25DV_ReadLockCFG(&self->_st_device, &lock_cfg);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

      if(IS_OK(result) && (lock_cfg != ST25DV_LOCKED))
      {
         nfctag_status = ST25DV_WriteLockCFG(&self->_st_device, ST25DV_LOCKED);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      }
   }

   return result;
}

static result_t ensure_i2c_session_open(st25dv_driver_t *self)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);

   ST25DV_I2CSSO_STATUS session = ST25DV_SESSION_CLOSED;
   int32_t status = ST25DV_ReadI2CSecuritySession_Dyn(&self->_st_device, &session);
   result_t result = convert_nfctag_status(status, ST25DV_DRV_ERROR_FAILED_I2C);

   if(IS_OK(result) && (session != ST25DV_SESSION_OPEN))
   {
      status = ST25DV_PresentI2CPassword(&self->_st_device, DEFAULT_I2C_PASSWORD);
      result = convert_nfctag_status(status, ST25DV_DRV_ERROR_FAILED_I2C_UNLOCK);

      if(IS_OK(result))
      {
         nrf_delay_ms(I2C_PASSWORD_DELAY_MS);
         status = ST25DV_ReadI2CSecuritySession_Dyn(&self->_st_device, &session);
         result = convert_nfctag_status(status, ST25DV_DRV_ERROR_FAILED_I2C);

         if(IS_OK(result) && (session != ST25DV_SESSION_OPEN))
         {
            SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C_UNLOCK);
            DEBUG_ERROR("NFC driver: failed to unlock config.");
         }
      }
   }
   if(IS_ERR(result))
   {
      DEBUG_ERROR("I2C session unlock failed, error=%d", GET_ERR_CODE(result));
   }
   return result;
}

/**
 * @brief Dummy init hook needed by the ST driver.
 *
 * The higher layers already own the I2C peripheral, so there is no additional work here.
 */
static int32_t st25dv_bus_init(void)
{
   return NFCTAG_OK;
}

/**
 * @brief Dummy deinit hook to satisfy ST's IO structure.
 */
static int32_t st25dv_bus_deinit(void)
{
   return NFCTAG_OK;
}

/**
 * @brief Bridge ST's block write API to the platform I2C driver.
 *
 * The ST driver expects to write arbitrary lengths and will call back multiple times. This
 * function chunks writes into sub-255 byte frames (register + data) because our I2C layer
 * accepts a uint8_t length. Each chunk is prefixed with the target register address.
 */
static int32_t st25dv_bus_write(uint16_t dev_addr, uint16_t mem_addr, const uint8_t *data, uint16_t size)
{
   RETURN_VAL_IF_TRUE((size > 0) && (NULL == data), NFCTAG_ERROR);
   RETURN_VAL_IF_TRUE((NULL == mp_self) || (NULL == mp_self->_i2c_interface), NFCTAG_ERROR);
   RETURN_VAL_IF_TRUE(0 == size, NFCTAG_OK);

   // The ST driver now expects the upper layer to handle chunking. This function simply transmits the frame as
   // requested.

   if(size > ST25DV_I2C_MAX_WRITE_CHUNK)
   {
      DEBUG_ERROR("Write size %d exceeds max chunk %d", size, ST25DV_I2C_MAX_WRITE_CHUNK);
      return NFCTAG_ERROR;
   }

   const i2c_driver_interface_t *i2c = mp_self->_i2c_interface;
   uint8_t device_address = (uint8_t)(dev_addr >> 1u);
   uint8_t frame[ST25DV_I2C_MAX_WRITE_FRAME] = {0};
   frame[0u] = (uint8_t)(mem_addr >> 8u);
   frame[1u] = (uint8_t)(mem_addr & 0xFF);
   memcpy(&frame[2u], data, size);
   const uint8_t tx_len = (uint8_t)(size + sizeof(uint16_t));

   result_t result = i2c->transmit(i2c, device_address, frame, tx_len, I2C_TIMEOUT_US);

   (void)mp_self->_systick_ifc->get_time_ms(mp_self->_systick_ifc, &m_end_of_last_write_ms);

   if(IS_OK(result) && m_write_eeprom)
   {
      m_write_eeprom = false;
   }

   int32_t ret_result = NFCTAG_ERROR;
   if(IS_OK(result))
   {
      ret_result = NFCTAG_OK;
   }
   else
   {
      DEBUG_ERROR("I2C transmit failed, error=%d", GET_ERR_CODE(result));
   }

   return ret_result;
}

/**
 * @brief Bridge ST's block read API to the platform I2C driver.
 *
 * Reads are performed as register writes (for the address) followed by a receive. This helper
 * loops until the requested data length is satisfied, honoring the max chunk size the I2C
 * layer can fetch per call.
 */
static int32_t st25dv_bus_read(uint16_t dev_addr, uint16_t mem_addr, uint8_t *data, uint16_t size)
{
   RETURN_VAL_IF_TRUE(((size > 0u) && (NULL == data)), NFCTAG_ERROR);
   RETURN_VAL_IF_TRUE((NULL == mp_self) || (NULL == mp_self->_i2c_interface), NFCTAG_ERROR);
   RETURN_VAL_IF_TRUE(0 == size, NFCTAG_OK);

   const i2c_driver_interface_t *i2c = mp_self->_i2c_interface;
   uint16_t remaining = size;
   uint16_t current_addr = mem_addr;
   uint8_t *current = data;
   uint8_t device_address = (uint8_t)(dev_addr >> 1u);
   uint8_t addr_buf[sizeof(uint16_t)];
   result_t result = RESULT_OK;
   bool use_txrx = (NULL != i2c->transmit_receive);

   while(remaining > 0u)
   {
      uint16_t chunk = remaining;
      if(chunk > ST25DV_I2C_MAX_READ_CHUNK)
      {
         chunk = ST25DV_I2C_MAX_READ_CHUNK;
      }

      addr_buf[0u] = (uint8_t)(current_addr >> 8u);
      addr_buf[1u] = (uint8_t)(current_addr & 0xFFu);

      if(use_txrx)
      {
         result = i2c->transmit_receive(
            i2c, device_address, addr_buf, sizeof(addr_buf), current, (uint8_t)chunk, I2C_TIMEOUT_US);
      }
      else
      {
         result = i2c->transmit(i2c, device_address, addr_buf, sizeof(addr_buf), I2C_TIMEOUT_US);

         if(IS_OK(result))
         {
            result = i2c->receive(i2c, device_address, current, (uint8_t)chunk, I2C_TIMEOUT_US);
         }
      }

      if(IS_OK(result)
         || ((GET_ERR_UNIT(result) == SW_UNIT_ID_TWI_DRV)
             && ((GET_ERR_CODE(result) == TWI_DRV_ERROR_ANAK) || (GET_ERR_CODE(result) == TWI_DRV_ERROR_DNAK))))
      {
         // Do not consider NACK as an error here, as it may happen during ACK polling after writes
         current += chunk;
         current_addr += chunk;
         remaining -= chunk;
      }
      else
      {
         DEBUG_ERROR("I2C read failed at addr=%d, error=%d", current_addr, GET_ERR_CODE(result));
         break;
      }
   }

   (void)mp_self->_systick_ifc->get_time_ms(
      mp_self->_systick_ifc,
      &m_end_of_last_write_ms); // Cast to void to not overwrite the result of the I2C operation

   int32_t ret_result = NFCTAG_ERROR;

   if(IS_OK(result))
   {
      ret_result = NFCTAG_OK;
   }

   return ret_result;
}

/**
 * @brief Poll the tag until it replies or the trial count expires.
 *
 * ST uses this callback while waiting for EEPROM writes to complete. We simply issue small
 * reads; any ACK indicates the device is ready for the next command.
 */
static int32_t st25dv_bus_is_ready(uint16_t dev_addr, uint32_t trials)
{
   uint8_t dummy = 0u;
   int32_t res = NFCTAG_ERROR;

   for(uint32_t attempt = 0; attempt < trials; attempt++)
   {
      if(NFCTAG_OK == st25dv_bus_read(dev_addr, 0, &dummy, 1u))
      {
         res = NFCTAG_OK;
         break;
      }
      else
      {
         nrf_delay_ms(EEPROM_READY_POLL_INTERVAL_MS);
      }
   }

   return res;
}

static int32_t st25dv_bus_get_tick(void)
{
   if((NULL == mp_self) || (NULL == mp_self->_systick_ifc))
   {
      return 0;
   }

   uint64_t current_ms = 0;
   (void)mp_self->_systick_ifc->get_time_ms(mp_self->_systick_ifc, &current_ms);
   // The ST driver only uses this callback to measure short deltas (write timeout <= 320 ms).
   // Even if the value wraps around, any delta computed within 320 ms remains correct
   // when interpreted modulo 2^32. Therefore the cast is safe despite the 64-bit system counter. See st25dv.c
   return (int32_t)current_ms;
}

static void event_handler(nrf_drv_gpiote_pin_t pin,
                          nrf_gpiote_polarity_t action) // NOSONAR: action required by SDK
{
   UNUSED_PARAMETER(action);

   if(pin == m_interrupt_pin)
   {
      m_gpo_detected = true;
   }
}

static result_t verify_eeprom_write(st25dv_driver_t *self, uint16_t address, const uint8_t *data, uint16_t length)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_OK_IF_TRUE(0u == length);

   // Limit verification footprint to keep stack usage modest.
   enum
   {
      VERIFY_WINDOW = 128u
   };

   uint8_t verify_buf[VERIFY_WINDOW] = {0};
   uint16_t verify_len = length;
   uint16_t verify_addr = address;
   bool check_tail = false;

   if(length > VERIFY_WINDOW)
   {
      verify_len = VERIFY_WINDOW;
      check_tail = true;
   }

   result_t result = RESULT_OK;
   int32_t nfctag_status = ST25DV_ReadData(&self->_st_device, verify_buf, verify_addr, verify_len);
   result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

   if(IS_OK(result) && (0 != memcmp(data, verify_buf, verify_len)))
   {
      SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
      DEBUG_ERROR("EEPROM verify mismatch at addr=%d len=%d (head)", address, verify_len);
   }

   if(IS_OK(result) && check_tail)
   {
      // Check the last window as well.
      verify_addr = (uint16_t)(address + length - VERIFY_WINDOW);
      nfctag_status = ST25DV_ReadData(&self->_st_device, verify_buf, verify_addr, VERIFY_WINDOW);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

      if(IS_OK(result) && (0 != memcmp(&data[length - VERIFY_WINDOW], verify_buf, VERIFY_WINDOW)))
      {
         SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         DEBUG_ERROR("EEPROM verify mismatch at addr=%d len=%d (tail)", verify_addr, VERIFY_WINDOW);
      }
   }

   if(IS_ERR(result))
   {
      DEBUG_ERROR("EEPROM verify failed at addr=%d len=%d", address, length);
   }

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t get_interrupt_status(const nfc_tag_driver_interface_t *const interface, bool *interrupt_fired)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ST25DV_DRV_ERROR_INIT);
   RETURN_ERR_IF_NULL(interrupt_fired, ST25DV_DRV_ERROR_INIT);
   const st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, ST25DV_DRV_ERROR_UNIT_UNINITIALIZED);

   *interrupt_fired = m_gpo_detected;
   m_gpo_detected = false; // Reset status

   return RESULT_OK;
}

static result_t get_status(const nfc_tag_driver_interface_t *const interface, st25dv_mb_status_t *status)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, ST25DV_DRV_ERROR_INIT);
   RETURN_ERR_IF_NULL(status, ST25DV_DRV_ERROR_INIT);
   const st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, ST25DV_DRV_ERROR_UNIT_UNINITIALIZED);

   uint8_t raw_ctrl = 0u;
   uint8_t lenm1 = 0u; // Length - 1
   int32_t nfctag_status = ST25DV_GetMB_CTRL_DYN_ALL(&self->_st_device.Ctx, &raw_ctrl);
   result_t result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

   if(IS_OK(result))
   {
      status->raw_ctrl = raw_ctrl;
      status->raw_len_m1 = 0u;
      status->msg_len = 0u;
      status->enabled = ((raw_ctrl & ST25DV_MB_CTRL_DYN_MBEN_MASK) != 0u);
      status->host_put_msg = ((raw_ctrl & ST25DV_MB_CTRL_DYN_HOSTPUTMSG_MASK) != 0u);
      status->rf_put_msg = ((raw_ctrl & ST25DV_MB_CTRL_DYN_RFPUTMSG_MASK) != 0u);
   }

   // MB_LEN_Dyn is only meaningful when RF has put a message in the mailbox.
   if(IS_OK(result) && status->rf_put_msg)
   {
      nfctag_status = ST25DV_ReadMBLength_Dyn(&self->_st_device, &lenm1);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);

      if(IS_OK(result))
      {
         status->raw_len_m1 = lenm1;
         status->msg_len = (uint16_t)lenm1 + 1u;
      }
   }

   return result;
}

static result_t get_packet(const comms_driver_interface_t *const interface, uint8_t *buf, uint16_t max_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(buf, COMMS_DRIVER_ERROR_PTR_NULL);
   const st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, COMMS_DRIVER_ERROR_UNINITIALIZED);

   uint64_t current_ms = 0;

   // Casting result to void since this time is only used for tracing purposes and a failure here shouldn't affect the
   // flow of the rest of the code.
   (void)self->_systick_ifc->get_time_ms(self->_systick_ifc, &current_ms);

   // Check if the MB interrupt fired. NOTE: This depends on the GPO pin being connected and configured correctly.
   // Specifically, it requires the MB_CTRL_Dyn.RF_PUT_MSG bit to be set to '1' to trigger the GPO to enable interrupts
   // when RF puts a message in the MB. From testing, the interrupt seems to be a more reliable way to detect new
   // messages than polling the MB status.rf_put_msg bit.
   bool interrupt_fired = false;
   result_t result = get_interrupt_status(&self->interface, &interrupt_fired);
   if(IS_ERR(result))
   {
      SET_ERR(result, COMMS_DRIVER_ERROR_COMM_RX);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get interrupt status. unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   st25dv_mb_status_t status = {0};
   if(IS_OK(result) && interrupt_fired)
   {
      result = map_mailbox_result_to_comms(get_status(&self->interface, &status), COMMS_DRIVER_ERROR_COMM_RX);
      if(IS_ERR(result))
      {
         if(GET_ERR_CODE(result) != COMMS_DRIVER_ERROR_BUSY)
         {
            ON_ERR_DEBUG_WARNING(
               result, "Failed to get mailbox status. unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         }
      }
   }

   uint16_t length = status.msg_len;
   bool msg_avail = status.rf_put_msg && (length > 0u);

   if(IS_OK(result) && msg_avail)
   {
      if(length > max_len)
      {
         // Clamp length to caller’s buffer size
         length = max_len;
      }
      if(length > NFC_MAILBOX_BUFFER_SIZE)
      {
         length = NFC_MAILBOX_BUFFER_SIZE;
      }
      // Read exactly the received message length number of bytes. Reading the last byte frees the mailbox.
      int32_t nfctag_status = ST25DV_ReadMailboxData(&self->_st_device, buf, 0u, length);
      result = map_mailbox_result_to_comms(convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C),
                                           COMMS_DRIVER_ERROR_COMM_RX);
   }
   else if(IS_OK(result) && interrupt_fired && !msg_avail)
   {
      // RF->I2C message was consumed/cleared between interrupt and status read.
      SET_ERR(result, COMMS_DRIVER_ERROR_BUSY);
   }

   return result;
}

static result_t send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, COMMS_DRIVER_ERROR_PTR_NULL);
   const st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, COMMS_DRIVER_ERROR_UNINITIALIZED);
   RETURN_ERR_IF_TRUE(len > MAILBOX_BUFFER_SIZE, COMMS_DRIVER_ERROR_OVERFLOW);

   st25dv_mb_status_t status = {0};
   result_t result = map_mailbox_result_to_comms(get_status(&self->interface, &status), COMMS_DRIVER_ERROR_COMM_TX);

   // Mailbox free only when neither side has a message pending
   if(IS_OK(result) && (status.host_put_msg || status.rf_put_msg))
   {
      SET_ERR(result, COMMS_DRIVER_ERROR_BUSY);
   }

   if(IS_OK(result))
   {
      int32_t nfctag_status = ST25DV_WriteMailboxData(&self->_st_device, data, len);
      result = map_mailbox_result_to_comms(convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C),
                                           COMMS_DRIVER_ERROR_COMM_TX);
   }

   return result;
}

static result_t get_max_packet_length(const comms_driver_interface_t *const interface, uint16_t *max_packet_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, COMMS_DRIVER_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(max_packet_len, COMMS_DRIVER_ERROR_PTR_NULL);
   const st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, COMMS_DRIVER_ERROR_UNINITIALIZED);

   *max_packet_len = MAILBOX_BUFFER_SIZE;

   return RESULT_OK;
}

static result_t read_eeprom_blocking(const nfc_tag_driver_interface_t *const interface,
                                     uint16_t address,
                                     uint8_t *buffer,
                                     uint16_t length)
{
   // Expose the vendor EEPROM read helper via the generic NFC interface.
   // The Message/Comms layers can call this without knowing which tag IC is present.
   RETURN_ERR_IF_INTERFACE_NULL(interface, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(buffer, ST25DV_DRV_ERROR_PTR_NULL);
   st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, ST25DV_DRV_ERROR_UNIT_UNINITIALIZED);
   RETURN_OK_IF_TRUE(0u == length);

   result_t result = ensure_i2c_session_open(self);

   if(IS_OK(result) && (0u == self->_total_eeprom_bytes))
   {
      result = update_mem_size(self);
   }

   if(IS_OK(result))
   {
      result = validate_bounds(self, address, length);
   }

   if(IS_OK(result))
   {
      int32_t nfctag_status = ST25DV_ReadData(&self->_st_device, buffer, address, length);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
   }

   return result;
}

static result_t write_eeprom_blocking(const nfc_tag_driver_interface_t *const interface,
                                      uint16_t address,
                                      const uint8_t *data,
                                      uint16_t length)
{
   // Writes follow the same flow as reads: validate interface, then forward to ST's driver. Area 1 (first 32 bytes) is
   // reserved and cannot be written via this API.
   RETURN_ERR_IF_INTERFACE_NULL(interface, ST25DV_DRV_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(data, ST25DV_DRV_ERROR_PTR_NULL);
   st25dv_driver_t *self = interface->parent;
   RETURN_ERR_IF_TRUE(!self->_initialized, ST25DV_DRV_ERROR_UNIT_UNINITIALIZED);
   RETURN_OK_IF_TRUE(0u == length);

   result_t result = ensure_i2c_session_open(self);

   if(IS_OK(result) && (0u == self->_total_eeprom_bytes))
   {
      result = update_mem_size(self);
   }

   if(IS_OK(result))
   {
      result = validate_bounds(self, address, length);
   }

   if(IS_OK(result))
   {
      uint32_t area1_end = 0u;
      result = get_area_end_byte(self, ST25DV_AREA_END1_IDX, &area1_end);
      if(IS_OK(result) && (address <= area1_end))
      {
         SET_ERR(result, ST25DV_DRV_ERROR_OUT_OF_RANGE);
         DEBUG_ERROR("Writes to Area 1 are not permitted. Area 1 end address %d, received addr=%d", area1_end, address);
      }
   }

   if(IS_OK(result))
   {
      bool mailbox_enabled = false;
      // Disable mailbox (fast transfer mode) per datasheet requirement before EEPROM writes.
      result = get_mailbox_enabled(self, &mailbox_enabled);

      if(IS_OK(result) && mailbox_enabled)
      {
         result = set_mailbox_enabled(self, false);
      }

      if(IS_OK(result))
      {
         m_write_eeprom = true;
         // Chunking logic: ensure each call to ST25DV_WriteData uses <= ST25DV_I2C_MAX_WRITE_CHUNK bytes
         uint16_t remaining = length;
         uint16_t current_addr = address;
         const uint8_t *current = data;
         while(remaining > 0)
         {
            uint16_t chunk = remaining;
            if(chunk > ST25DV_I2C_MAX_WRITE_CHUNK)
            {
               chunk = ST25DV_I2C_MAX_WRITE_CHUNK;
            }
            int32_t nfctag_status = ST25DV_WriteData(&self->_st_device, current, current_addr, chunk);
            result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
            if(IS_ERR(result))
            {
               DEBUG_ERROR("ST25DV_WriteData failed at addr=%d, error=%d", current_addr, GET_ERR_CODE(result));
               break;
            }
            current += chunk;
            current_addr += chunk;
            remaining -= chunk;
         }
      }

      // Restore mailbox state to avoid breaking comms clients.
      if(mailbox_enabled)
      {
         (void)set_mailbox_enabled(self, true);
      }
   }

   if(IS_OK(result))
   {
      result = verify_eeprom_write(self, address, data, length);
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t st25dv_driver_init(st25dv_driver_t *const self,
                            const i2c_driver_interface_t *i2c_interface,
                            uint8_t interrupt_pin,
                            const system_time_interface_t *systick_ifc)
{
   RETURN_ERR_IF_NULL(self, ST25DV_DRV_ERROR_INIT);
   RETURN_ERR_IF_INTERFACE_NULL(i2c_interface, ST25DV_DRV_ERROR_INIT);

   self->_initialized = false;
   self->interface.parent = self;
   self->data_ifc.parent = self;
   self->_total_eeprom_bytes = 0u;
   memset(self->_area_end_bytes, 0, sizeof(self->_area_end_bytes));

   m_interrupt_pin = interrupt_pin;

   self->data_ifc.get_packet = get_packet;
   self->interface.get_status = get_status;
   self->data_ifc.send_packet = send_packet;
   self->data_ifc.get_max_packet_length = get_max_packet_length;
   self->interface.get_interrupt_status = get_interrupt_status;
   self->interface.read_eeprom_blocking = read_eeprom_blocking;
   self->interface.write_eeprom_blocking = write_eeprom_blocking;
   self->_i2c_interface = i2c_interface;
   self->_systick_ifc = systick_ifc;

   memset(&self->_st_device, 0, sizeof(self->_st_device));
   self->_st_bus_io.Init = st25dv_bus_init;
   self->_st_bus_io.DeInit = st25dv_bus_deinit;
   self->_st_bus_io.Write = st25dv_bus_write;
   self->_st_bus_io.Read = st25dv_bus_read;
   self->_st_bus_io.IsReady = st25dv_bus_is_ready;
   self->_st_bus_io.GetTick = st25dv_bus_get_tick;

   mp_self = self;

   int32_t nfctag_status = ST25DV_RegisterBusIO(&self->_st_device, &self->_st_bus_io);
   result_t result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_INIT);

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_Init(&self->_st_device);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_INIT);
   }

   if(IS_OK(result))
   {
      result = ensure_i2c_session_open(self);
      nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS);
   }

   if(IS_OK(result))
   {
      result = update_mem_size(self);
   }

   if(IS_OK(result))
   {
      result = configure_eeprom_protection(self);
   }

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_WriteMBMode(&self->_st_device, ST25DV_ENABLE);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      if(IS_OK(result))
      {
         nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS);
         ST25DV_EN_STATUS mode = ST25DV_DISABLE;
         nfctag_status = ST25DV_ReadMBMode(&self->_st_device, &mode);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
         if(IS_OK(result) && (mode != ST25DV_ENABLE))
         {
            SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         }
      }
   }

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_WriteMBWDG(&self->_st_device, MB_WDG_TIME_30_MS);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      if(IS_OK(result))
      {
         nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS);
         uint8_t watchdog = 0;
         nfctag_status = ST25DV_ReadMBWDG(&self->_st_device, &watchdog);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
         if(IS_OK(result) && (watchdog != MB_WDG_TIME_30_MS))
         {
            SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         }
      }
   }

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_SetMBEN_Dyn(&self->_st_device);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      if(IS_OK(result))
      {
         nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS);
         ST25DV_EN_STATUS enabled = ST25DV_DISABLE;
         nfctag_status = ST25DV_GetMBEN_Dyn(&self->_st_device, &enabled);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
         if(IS_OK(result) && (enabled != ST25DV_ENABLE))
         {
            SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         }
      }
   }

   if(IS_OK(result))
   {
      nfctag_status = ST25DV_WriteITPulse(&self->_st_device, ST25DV_188_US);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      if(IS_OK(result))
      {
         nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS);
         ST25DV_PULSE_DURATION pulse = ST25DV_302_US;
         nfctag_status = ST25DV_ReadITPulse(&self->_st_device, &pulse);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
         if(IS_OK(result) && (pulse != ST25DV_188_US))
         {
            SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         }
      }
   }

   if(IS_OK(result))
   {
      const uint16_t gpo_mask = ST25DV_GPO_RFPUTMSG_MASK | ST25DV_GPO_ENABLE_MASK;
      nfctag_status = ST25DV_ConfigureGPO(&self->_st_device, gpo_mask);
      result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
      if(IS_OK(result))
      {
         nrf_delay_ms(I2C_INTER_WRITE_DELAY_MS);
         uint16_t gpo_status = 0u;
         nfctag_status = ST25DV_GetGPOStatus(&self->_st_device, &gpo_status);
         result = convert_nfctag_status(nfctag_status, ST25DV_DRV_ERROR_FAILED_I2C);
         if(IS_OK(result) && ((gpo_status & gpo_mask) != gpo_mask))
         {
            SET_ERR(result, ST25DV_DRV_ERROR_FAILED_I2C);
         }
      }
   }

   // Configure interrupts
   ret_code_t err = NRF_SUCCESS;
   if(!nrf_drv_gpiote_is_init() && IS_OK(result))
   {
      err = nrf_drv_gpiote_init();
   }

   if((NRF_SUCCESS == err) && IS_OK(result))
   {
      nrf_drv_gpiote_in_config_t config
         = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false); // Set to low accuracy to conserve power (low accuracy == false)
      err = nrf_drv_gpiote_in_init(m_interrupt_pin, &config, event_handler);
   }

   if((NRF_SUCCESS == err) && IS_OK(result))
   {
      nrf_drv_gpiote_in_event_enable(m_interrupt_pin, true);
   }

   if(NRF_SUCCESS != err)
   {
      SET_ERR(result, ST25DV_DRV_ERROR_INIT);
   }

   if(IS_OK(result))
   {
      self->_initialized = true;
   }
   else
   {
      DEBUG_ERROR("NFC tag driver: Failed to initialize the driver.");
   }

   return result;
}
