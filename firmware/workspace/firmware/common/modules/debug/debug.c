/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include "nrf_mtx.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "crc16.h"

// Custom includes
#include "../../common.h"
#include "SEGGER_RTT.h"
#include "debug.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DEBUG;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define FRAMING_CHAR         0x7Eu
#define ESCAPE_CHAR          0x7Du
#define ESCAPED_FRAMING_CHAR 0x5Eu
#define ESCAPED_ESCAPE_CHAR  0x5Du

#define MSG_DATA_LEN_SET_DEBUG_LEVEL (3u)
#define SET_DEBUG_LEVEL_UNIT_ID_IDX  (1u)
#define SET_DEBUG_LEVEL_LEVEL_IDX    (2u)

#define MSG_DATA_LEN_SET_DATASTREAM_ENABLED (3u)
#define SET_DATASTREAM_ENABLED_DATA_ID_IDX  (1u)
#define SET_DATASTREAM_ENABLED_STATE_IDX    (2u)
#define SET_BIT_IN_BYTE(W, B)               ((W) |= (uint8_t)(1U << (B)))
#define CLR_BIT_IN_BYTE(W, B)               ((W) &= (uint8_t)(~(1U << (B))))

#define HOT_LOG_SUMMARY_TIME_MS               (3000u)
#define DEBUG_LOG_LEVEL_TRACE_MIN_INTERVAL_MS (0u)
#define DEBUG_LOG_LEVEL_DEBUG_MIN_INTERVAL_MS (0u)
#define DEBUG_LOG_LEVEL_INFO_MIN_INTERVAL_MS  (0u)
#define DEBUG_LOG_LEVEL_WARN_MIN_INTERVAL_MS                                                                           \
   (0u) // The WARNING level timeout controls the summary rate limiting as well
#define DEBUG_LOG_LEVEL_ERROR_MIN_INTERVAL_MS    (0u)
#define DEBUG_LOG_LEVEL_CRITICAL_MIN_INTERVAL_MS (0u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

static inline uint32_t make_hot_log_key(uint8_t module_id, uint16_t line_number);
static int32_t find_hot_log_entry(uint32_t key);
PRIVATE uint16_t allocate_hot_log_table_index(void);
static result_t should_emit_log(DEBUG_LEVEL level, uint8_t module_id, uint16_t line, bool *emit_log);
static result_t should_emit_existing_hot_log(uint32_t hot_log_entry_index, bool *emit_log);
static inline result_t reset_tx_buffer(debug_buffer_t *buf);
static inline result_t append_tx_buffer_raw(debug_buffer_t *buf, uint8_t data_byte);
static inline result_t append_tx_buffer(debug_buffer_t *buf, uint8_t data_byte, bool is_update_crc);
static inline result_t append_tx_buffer_u32_le(debug_buffer_t *data_buf, uint32_t data_word, bool is_update_crc);
static inline result_t append_crc(debug_buffer_t *buf);
static inline void buffer_rx_byte(uint8_t rx_byte);
static inline void buffer_rx_clear(void);
static inline void handle_rx_frame(void);
static inline void process_framing_char(void);
static inline void process_rx_byte(uint8_t rx_byte);
static result_t get_unit_debug_level_for_endpoint(const uint8_t unit_id, DEBUG_LEVEL *level, uint8_t endpoint);
static result_t send_via_debug_buffer(uint8_t msg_id,
                                      uint8_t msg_sub_id,
                                      bool is_sub_id_encoded,
                                      bool is_msg_len_encoded,
                                      uint8_t data_len,
                                      const uint8_t *data_buf);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

nrf_mtx_t debug_tx_mutex;

static debug_msg_t received_msg;
static bool is_rx_frame_open = false;
static bool is_next_byte_escaped = false;
static const queue_interface_t *m_rx_cmd_queue = NULL;
static const system_time_interface_t *m_system_time = NULL;

static uint8_t debug_enable_bitmap[DEBUG_MAX_TX_ENDPOINTS][DEBUG_UNITS_ENABLED_BITMAP_SIZE];
static uint8_t datastream_enable_bitmap[DEBUG_DATASTREAM_ENABLED_BITMAP_SIZE];

static endpoint_t tx_handlers[DEBUG_MAX_TX_ENDPOINTS];

static uint8_t m_debug_tx_buffer_bytes[DEBUG_TX_BUFFER_SIZE];
static debug_buffer_t m_debug_buffer = {
   .size = DEBUG_TX_BUFFER_SIZE,
   .idx = 0u,
   .p_data = m_debug_tx_buffer_bytes,
   .crc = 0u,
};

static uint8_t m_data_tx_buffer_bytes[DEBUG_DATA_TX_BUFFER_SIZE];
static debug_buffer_t m_data_buffer = {
   .size = DEBUG_DATA_TX_BUFFER_SIZE,
   .idx = 0u,
   .p_data = m_data_tx_buffer_bytes,
   .crc = 0u,
};

PRIVATE hot_log_t m_hot_log_table[HOT_LOG_TABLE_SIZE] = {0};

static uint16_t m_hot_log_min_intervals_ms[DEBUG_LEVEL_MAX] = {
   [DEBUG_LEVEL_TRACE] = DEBUG_LOG_LEVEL_TRACE_MIN_INTERVAL_MS,
   [DEBUG_LEVEL_DEBUG] = DEBUG_LOG_LEVEL_DEBUG_MIN_INTERVAL_MS,
   [DEBUG_LEVEL_INFO] = DEBUG_LOG_LEVEL_INFO_MIN_INTERVAL_MS,
   [DEBUG_LEVEL_WARN] = DEBUG_LOG_LEVEL_WARN_MIN_INTERVAL_MS,
   [DEBUG_LEVEL_ERROR] = DEBUG_LOG_LEVEL_ERROR_MIN_INTERVAL_MS,
   [DEBUG_LEVEL_CRITICAL] = DEBUG_LOG_LEVEL_CRITICAL_MIN_INTERVAL_MS,
};

static bool m_debug_module_initialized = false;
static uint64_t m_last_summary_process_time_ms = 0u;
PRIVATE uint8_t m_summary_process_index = 0u;

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/

static inline uint32_t make_hot_log_key(uint8_t module_id, uint16_t line_number)
{
   return (((uint32_t)module_id << 16u) | ((uint32_t)line_number));
}

static int32_t find_hot_log_entry(uint32_t key)
{
   int32_t result = -1;

   for(uint8_t idx = 0u; idx < HOT_LOG_TABLE_SIZE; idx++)
   {
      if(m_hot_log_table[idx].in_use && (m_hot_log_table[idx].key == key))
      {
         result = (int32_t)idx;
         break;
      }
   }
   return result;
}

PRIVATE uint16_t allocate_hot_log_table_index(void)
{
   uint8_t result = HOT_LOG_TABLE_SIZE; // Invalid index

   for(uint8_t idx = 0u; idx < HOT_LOG_TABLE_SIZE; idx++)
   {
      if(false == m_hot_log_table[idx].in_use)
      {
         result = idx;
         break;
      }
   }

   if(HOT_LOG_TABLE_SIZE == result)
   {
      // No free entries, find the least recently used entry
      uint64_t oldest_time_ms = UINT64_MAX;
      for(uint8_t idx = 0u; idx < HOT_LOG_TABLE_SIZE; idx++)
      {
         if(m_hot_log_table[idx].last_used_time_ms < oldest_time_ms)
         {
            oldest_time_ms = m_hot_log_table[idx].last_used_time_ms;
            result = idx;
         }
      }
   }
   return result;
}

static result_t should_emit_existing_hot_log(uint32_t hot_log_entry_index, bool *emit_log)
{
   RETURN_ERR_IF_NULL(emit_log, DEBUG_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(hot_log_entry_index >= HOT_LOG_TABLE_SIZE, DEBUG_ERROR_INV_ARG);

   bool emit_log_local = false;

   uint64_t current_time_ms = 0u;
   result_t result = m_system_time->get_time_ms(m_system_time, &current_time_ms);

   m_hot_log_table[hot_log_entry_index].last_used_time_ms = current_time_ms;

   if(IS_OK(result))
   {
      if(current_time_ms >= m_hot_log_table[hot_log_entry_index].next_allowed_time_ms)
      {
         emit_log_local = true;
         // Update next allowed time based on level
         m_hot_log_table[hot_log_entry_index].next_allowed_time_ms
            = current_time_ms + m_hot_log_min_intervals_ms[m_hot_log_table[hot_log_entry_index].level];
      }
      else
      {
         emit_log_local = false;
         m_hot_log_table[hot_log_entry_index].suppress_count++;
         m_hot_log_table[hot_log_entry_index].updated_since_last_summary = true;
      }
   }

   *emit_log = emit_log_local;

   return result;
}

static result_t should_emit_log(DEBUG_LEVEL level, uint8_t module_id, uint16_t line, bool *emit_log)
{
   RETURN_ERR_IF_NULL(emit_log, DEBUG_ERROR_PTR_NULL);

   result_t result = RESULT_OK;
   // Here we will have to check whether the entry is already in the hot log table
   // If yes, we must check whether it is allowed to be logged again else make a new entry
   uint32_t hot_log_key = make_hot_log_key(module_id, line);
   int32_t hot_log_entry_index = find_hot_log_entry(hot_log_key);

   if(0 > hot_log_entry_index)
   {
      // This means the entry is not in the table yet. Find an empty slot, create new entry.
      uint32_t new_index = allocate_hot_log_table_index();

      // Reset the entry
      memset(&m_hot_log_table[new_index], 0, sizeof(hot_log_t));
      m_hot_log_table[new_index].in_use = true;
      m_hot_log_table[new_index].key = hot_log_key;
      m_hot_log_table[new_index].module_id = module_id;
      m_hot_log_table[new_index].line = line;
      m_hot_log_table[new_index].level = (uint8_t)level;
      uint64_t current_time_ms = 0u;

      result = m_system_time->get_time_ms(m_system_time, &current_time_ms);

      if(IS_OK(result))
      {
         m_hot_log_table[new_index].first_logged_time_ms = current_time_ms;
         m_hot_log_table[new_index].last_used_time_ms = current_time_ms;
         m_hot_log_table[new_index].next_allowed_time_ms
            = current_time_ms + m_hot_log_min_intervals_ms[m_hot_log_table[new_index].level];
      }
      *emit_log = true;
   }
   else
   {
      // This cast is safe because we have already checked the index above
      uint32_t hot_log_entry_index_u32 = (uint32_t)hot_log_entry_index;
      // This means the entry is already in the table. Check whether it can be logged again.
      result = should_emit_existing_hot_log(hot_log_entry_index_u32, emit_log);
   }

   return result;
}

static inline result_t reset_tx_buffer(debug_buffer_t *buf)
{
   RETURN_ERR_IF_NULL(buf, DEBUG_ERROR_PTR_NULL);
   buf->idx = 0u;
   buf->crc = 0u;
   return RESULT_OK;
}

static inline result_t append_tx_buffer_raw(debug_buffer_t *buf, uint8_t data_byte)
{
   RETURN_ERR_IF_NULL(buf, DEBUG_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   if(buf->idx + 1u >= buf->size)
   {
      SET_ERR(result, DEBUG_ERROR_BUF_OVERFLOW);
   }
   else
   {
      buf->p_data[buf->idx] = data_byte;
      buf->idx++;
   }

   return result;
}

static inline result_t append_tx_buffer(debug_buffer_t *buf, uint8_t data_byte, bool is_update_crc)
{
   RETURN_ERR_IF_NULL(buf, DEBUG_ERROR_PTR_NULL);
   result_t result = RESULT_THIS_UNIT_ERROR(DEBUG_ERROR_MAX);

   if(FRAMING_CHAR == data_byte)
   {
      result = append_tx_buffer_raw(buf, ESCAPE_CHAR);
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, ESCAPED_FRAMING_CHAR));
   }
   else if(ESCAPE_CHAR == data_byte)
   {
      result = append_tx_buffer_raw(buf, ESCAPE_CHAR);
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, ESCAPED_ESCAPE_CHAR));
   }
   else
   {
      result = append_tx_buffer_raw(buf, data_byte);
   }

   if(is_update_crc && (!IS_ERR(result)))
   {
      buf->crc = crc16_update(&data_byte, 1, &(buf->crc));
   }

   return result;
}

static inline result_t append_tx_buffer_unencoded_u32_le(debug_buffer_t *data_buf, uint32_t data_word)
{
   RETURN_ERR_IF_NULL(data_buf, DEBUG_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   for(uint32_t byte_idx = 0u; byte_idx < sizeof(uint32_t); byte_idx++)
   {
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(data_buf, (uint8_t)(data_word & 0xFFu)));
      data_word = data_word >> 8u;
   }
   return result;
}

static inline result_t append_tx_buffer_u32_le(debug_buffer_t *data_buf, uint32_t data_word, bool is_update_crc)
{
   RETURN_ERR_IF_NULL(data_buf, DEBUG_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   for(uint32_t byte_idx = 0u; byte_idx < sizeof(uint32_t); byte_idx++)
   {
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(data_buf, (uint8_t)(data_word & 0xFFu), is_update_crc));
      data_word = data_word >> 8u;
   }
   return result;
}

static inline result_t append_crc(debug_buffer_t *buf)
{
   RETURN_ERR_IF_NULL(buf, DEBUG_ERROR_PTR_NULL);

   result_t result = append_tx_buffer(buf, (uint8_t)((buf->crc >> 8u) & 0xFFu), false);
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, (uint8_t)(buf->crc & 0xFFu), false));

   return result;
}

static inline void buffer_rx_byte(uint8_t rx_byte)
{
   if(received_msg.packet_data_len < DEBUG_BUFFER_DATA_SIZE)
   {
      received_msg.packet_data[received_msg.packet_data_len] = rx_byte;
      received_msg.packet_data_len++;
   }
}

static inline void buffer_rx_clear(void)
{
   received_msg.packet_data_len = 0u;
}

static inline void handle_rx_frame(void)
{
   // Extract and check CRC
   uint16_t calc_rx_crc = 0u;
   uint16_t encoded_rx_crc_low_byte = 0u;
   uint16_t encoded_rx_crc_high_byte = 0u;

   if(received_msg.packet_data_len >= 2u)
   {
      encoded_rx_crc_low_byte = received_msg.packet_data[received_msg.packet_data_len - 1u];
      encoded_rx_crc_high_byte = received_msg.packet_data[received_msg.packet_data_len - 2u];
   }

   uint16_t encoded_rx_crc
      = ((uint16_t)(((uint16_t)(encoded_rx_crc_high_byte << 8u)) & 0xFF00u)) | encoded_rx_crc_low_byte;
   received_msg.packet_data_len -= 2;
   calc_rx_crc = crc16_update(received_msg.packet_data, received_msg.packet_data_len, &calc_rx_crc);

   if(calc_rx_crc == encoded_rx_crc)
   {
      // Is this a "set debug level" message?
      if((received_msg.packet_data[0u] == DEBUG_MSG_TYPE_SET_DEBUG_LEVEL)
         && (received_msg.packet_data_len == MSG_DATA_LEN_SET_DEBUG_LEVEL))
      {
         uint8_t unit_id = received_msg.packet_data[SET_DEBUG_LEVEL_UNIT_ID_IDX];
         DEBUG_LEVEL level = received_msg.packet_data[SET_DEBUG_LEVEL_LEVEL_IDX];
         (void)set_unit_debug_level_for_endpoint(unit_id, level, ENDPOINT_UART);
      }
      // Is this a "set datastream enabled" message?
      else if((received_msg.packet_data_len == MSG_DATA_LEN_SET_DATASTREAM_ENABLED)
              && (received_msg.packet_data[0u] == DEBUG_MSG_TYPE_DATASTREAM_SET_ENABLED))
      {
         uint8_t datastream_id = received_msg.packet_data[SET_DATASTREAM_ENABLED_DATA_ID_IDX];
         bool is_enabled = (0u != received_msg.packet_data[SET_DATASTREAM_ENABLED_STATE_IDX]);
         (void)debug_set_datastream_enabled(datastream_id, is_enabled);
      }
      // Is this an application-specific command message?
      else if(received_msg.packet_data[0u] == DEBUG_MSG_TYPE_CMD)
      {
         if(NULL != m_rx_cmd_queue)
         {
            (void)m_rx_cmd_queue->enqueue(m_rx_cmd_queue, (const void *)(&received_msg));
            DEBUG_DEBUG("RX command message of length: %u", received_msg.packet_data_len);
         }
         else
         {
            DEBUG_WARNING("Received command message, but RX queue is NULL!");
         }
      }
      else
      {
         DEBUG_WARNING("RX unknown debug message type %u", received_msg.packet_data[0u]);
      }
   }
   else
   {
      DEBUG_CRITICAL("RX CRC mismatch. Received: %u. Calculated: %u.", encoded_rx_crc, calc_rx_crc);
   }
}

static inline void process_framing_char(void)
{
   if(is_rx_frame_open)
   {
      is_rx_frame_open = false;

      // If the frame is empty, assume our framing is out of sync with reality and flip
      if(0u == received_msg.packet_data_len)
      {
         buffer_rx_clear();
         is_rx_frame_open = true;
      }
      else
      {
         handle_rx_frame();
      }
   }
   else
   {
      buffer_rx_clear();
      is_rx_frame_open = true;
   }
}

static inline void process_rx_byte(uint8_t rx_byte)
{
   if(is_next_byte_escaped)
   {
      if(is_rx_frame_open)
      {
         buffer_rx_byte(rx_byte ^ 0x20u);
      }
      is_next_byte_escaped = false;
   }
   else if(ESCAPE_CHAR == rx_byte)
   {
      is_next_byte_escaped = true;
   }
   else if(FRAMING_CHAR == rx_byte)
   {
      process_framing_char();
   }
   else
   {
      if(is_rx_frame_open)
      {
         buffer_rx_byte(rx_byte);
      }
   }
}

static result_t get_unit_debug_level_for_endpoint(const uint8_t unit_id, DEBUG_LEVEL *level, uint8_t endpoint)
{
   RETURN_ERR_IF_NULL(level, DEBUG_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(endpoint >= ENDPOINT_MAX, DEBUG_ERROR_INV_ARG);

   result_t result = RESULT_OK;

   *level = DEBUG_LEVEL_TRACE;

   for(uint8_t idx = 0u; idx < DEBUG_LEVEL_BITMAP_BITS_PER_UNIT; idx++)
   {
      uint16_t byte_idx = DEBUG_BITMAP_BYTE_IDX(unit_id, idx);
      uint8_t bit_idx = DEBUG_BITMAP_BIT_IDX(unit_id, idx);

      if(IS_SET(debug_enable_bitmap[endpoint][byte_idx], bit_idx))
      {
         SET_BIT_IN_BYTE(*level, idx);
      }
   }

   if(*level > DEBUG_LEVEL_DISABLED)
   {
      *level = DEBUG_LEVEL_DISABLED;
      SET_ERR(result, DEBUG_ERROR_SET_DEBUG_LEVEL);
   }

   return result;
}

static result_t send_via_debug_buffer(uint8_t msg_id,
                                      uint8_t msg_sub_id,
                                      bool is_sub_id_encoded,
                                      bool is_msg_len_encoded,
                                      uint8_t data_len,
                                      const uint8_t *data_buf)
{
   RETURN_ERR_IF_NULL(data_buf, DEBUG_ERROR_PTR_NULL);
   reset_tx_buffer(&m_debug_buffer);

   result_t result = append_tx_buffer_raw(&m_debug_buffer, FRAMING_CHAR);

   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(&m_debug_buffer, msg_id, true));

   if(IS_OK(result) && is_sub_id_encoded)
   {
      result = append_tx_buffer(&m_debug_buffer, msg_sub_id, true);
   }

   if(IS_OK(result) && is_msg_len_encoded)
   {
      result = append_tx_buffer(&m_debug_buffer, data_len, true);
   }

   for(size_t idx = 0; idx < data_len; idx++)
   {
      if(IS_OK(result))
      {
         result = append_tx_buffer(&m_debug_buffer, data_buf[idx], true);
      }
      else
      {
         break;
      }
   }

   IF_OK_RUN_AND_UPDATE(result, append_crc(&m_debug_buffer));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(&m_debug_buffer, FRAMING_CHAR));

   // Only send via primary endpoint
   if(IS_OK(result) && (NULL != tx_handlers[PRIMARY_ENDPOINT_INDEX].handler))
   {
      tx_handlers[PRIMARY_ENDPOINT_INDEX].handler(m_debug_buffer.p_data, m_debug_buffer.idx);
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t debug_map_unencoded_log(raw_debug_log_t *log, const uint8_t *p_data, size_t length)
{
   RETURN_ERR_IF_NULL(log, DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_NULL(p_data, DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_TRUE(length < RAW_DEBUG_LOG_T_METADATA_SIZE, DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_TRUE(length > (RAW_DEBUG_LOG_T_METADATA_SIZE + RAW_DEBUG_LOG_T_ARGS_SIZE), DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_TRUE((false == m_debug_module_initialized), DEBUG_ERROR_NOT_INITIALIZED);

   log->level = p_data[0u];
   log->timestamp = ((uint32_t)p_data[1u] << 0u) |  //
                    ((uint32_t)p_data[2u] << 8u) |  //
                    ((uint32_t)p_data[3u] << 16u) | //
                    ((uint32_t)p_data[4u] << 24u);
   log->module_id = p_data[5u];
   log->line = (uint16_t)(((uint16_t)p_data[6u] << 0u) | //
                          ((uint16_t)p_data[7u] << 8u));

   memcpy(log->arg_values, &p_data[8u], length - RAW_DEBUG_LOG_T_METADATA_SIZE);

   return RESULT_OK;
}

result_t set_unit_debug_level_for_endpoint(const uint8_t unit_id, DEBUG_LEVEL level, uint8_t endpoint)
{
   RETURN_ERR_IF_TRUE(level > DEBUG_LEVEL_DISABLED, DEBUG_ERROR_SET_DEBUG_LEVEL);

   for(uint8_t idx = 0u; idx < DEBUG_LEVEL_BITMAP_BITS_PER_UNIT; idx++)
   {
      uint16_t byte_idx = DEBUG_BITMAP_BYTE_IDX(unit_id, idx);
      uint8_t bit_idx = DEBUG_BITMAP_BIT_IDX(unit_id, idx);

      if(IS_SET(level, idx))
      {
         SET_BIT_IN_BYTE(debug_enable_bitmap[endpoint][byte_idx], bit_idx);
      }
      else
      {
         CLR_BIT_IN_BYTE(debug_enable_bitmap[endpoint][byte_idx], bit_idx);
      }
   }
   return RESULT_OK;
}

result_t debug_set_all_unit_debug_level_for_endpoint(DEBUG_LEVEL level, ENDPOINT_ID endpoint)
{
   RETURN_ERR_IF_TRUE(level > DEBUG_LEVEL_DISABLED, DEBUG_ERROR_SET_DEBUG_LEVEL);
   RETURN_ERR_IF_TRUE(endpoint >= ENDPOINT_MAX, DEBUG_ERROR_SET_DEBUG_LEVEL);

   result_t result = RESULT_OK;

   for(uint8_t unit = 0u; (unit < DEBUG_LEVEL_BITMAP_MAX_UNIT_COUNT) && IS_OK(result); unit++)
   {
      result = set_unit_debug_level_for_endpoint(unit, level, endpoint);
   }
   return result;
}

result_t debug_set_datastream_enabled(const uint8_t datastream_id, bool is_enabled)
{
   uint16_t byte_idx = DEBUG_BITMAP_BYTE_IDX(datastream_id, 0u);
   uint8_t bit_idx = DEBUG_BITMAP_BIT_IDX(datastream_id, 0u);

   if(is_enabled)
   {
      SET_BIT_IN_BYTE(datastream_enable_bitmap[byte_idx], bit_idx);
   }
   else
   {
      CLR_BIT_IN_BYTE(datastream_enable_bitmap[byte_idx], bit_idx);
   }

   return RESULT_OK;
}

result_t debug_get_datastream_enabled(const uint8_t datastream_id, bool *is_enabled)
{
   *is_enabled = false;

   uint16_t byte_idx = DEBUG_BITMAP_BYTE_IDX(datastream_id, 0u);
   uint8_t bit_idx = DEBUG_BITMAP_BIT_IDX(datastream_id, 0u);

   if(IS_SET(datastream_enable_bitmap[byte_idx], bit_idx))
   {
      *is_enabled = true;
   }

   return RESULT_OK;
}

result_t debug_send_command_response(uint8_t original_msg_id, uint8_t data_len, const uint8_t *data_buf)
{
   return send_via_debug_buffer((uint8_t)DEBUG_MSG_TYPE_CMD_RESPONSE, original_msg_id, true, true, data_len, data_buf);
}

result_t debug_on_data_rx(const uint8_t *p_data, size_t length)
{
   RETURN_ERR_IF_NULL(p_data, DEBUG_ERROR_PTR_NULL);

   for(size_t idx = 0; idx < length; idx++)
   {
      process_rx_byte(p_data[idx]);
   }
   return RESULT_OK;
}

result_t debug_get_num_rx_msgs(size_t *rx_count)
{
   RETURN_ERR_IF_NULL(rx_count, DEBUG_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(m_rx_cmd_queue, DEBUG_ERROR_PTR_NULL);

   return m_rx_cmd_queue->get_count(m_rx_cmd_queue, rx_count);
}

result_t debug_dequeue_message(debug_msg_t *msg)
{
   RETURN_ERR_IF_NULL(msg, DEBUG_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(m_rx_cmd_queue, DEBUG_ERROR_PTR_NULL);

   return m_rx_cmd_queue->dequeue(m_rx_cmd_queue, (void *)msg);
}

result_t debug_register_endpoint(endpoint_t *endpoint_spec)
{
   RETURN_ERR_IF_NULL(endpoint_spec->handler, DEBUG_ERROR_REGISTER_HANDLER);
   RETURN_ERR_IF_TRUE(endpoint_spec->id >= DEBUG_MAX_TX_ENDPOINTS, DEBUG_ERROR_REGISTER_HANDLER);

   memcpy(&(tx_handlers[endpoint_spec->id]), endpoint_spec, sizeof(endpoint_t));

   return RESULT_OK;
}

result_t initialize_tx_packet(debug_buffer_t *buf, DEBUG_LEVEL level, uint8_t module_id, uint16_t line)
{
   RETURN_ERR_IF_NULL(buf, DEBUG_ERROR_PTR_NULL);

   result_t result = append_tx_buffer_raw(buf, FRAMING_CHAR);

   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, (uint8_t)level, true));

   uint8_t timestamp_bytes[4u] = {0u};

   // If system time is available, include it in the debug message.
   if(IS_OK(result))
   {
      uint64_t system_time_ms = 0u;
      result = m_system_time->get_time_ms(m_system_time, &system_time_ms);
      if(IS_OK(result))
      {
         uint32_t system_time_s = (uint32_t)(system_time_ms / COMMON_1K_CST);
         timestamp_bytes[3u] = (uint8_t)((system_time_s >> 24u) & 0xFFu);
         timestamp_bytes[2u] = (uint8_t)((system_time_s >> 16u) & 0xFFu);
         timestamp_bytes[1u] = (uint8_t)((system_time_s >> 8u) & 0xFFu);
         timestamp_bytes[0u] = (uint8_t)(system_time_s & 0xFFu);
      }
   }

   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, timestamp_bytes[0u], true));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, timestamp_bytes[1u], true));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, timestamp_bytes[2u], true));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, timestamp_bytes[3u], true));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, module_id, true));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, (uint8_t)(line & 0xFFu), true));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(buf, (uint8_t)((line >> 8u) & 0xFFu), true));

   return result;
}

result_t initialize_unencoded_tx_packet(debug_buffer_t *buf, DEBUG_LEVEL level, uint8_t module_id, uint16_t line)
{
   RETURN_ERR_IF_NULL(buf, DEBUG_ERROR_PTR_NULL);

   result_t result = append_tx_buffer_raw(buf, (uint8_t)level);

   uint8_t timestamp_bytes[4u] = {0u};

   if(IS_OK(result))
   {
      uint64_t system_time_ms = 0u;
      result = m_system_time->get_time_ms(m_system_time, &system_time_ms);
      if(IS_OK(result))
      {
         uint32_t system_time_s = (uint32_t)(system_time_ms / COMMON_1K_CST);
         timestamp_bytes[3u] = (uint8_t)((system_time_s >> 24u) & 0xFFu);
         timestamp_bytes[2u] = (uint8_t)((system_time_s >> 16u) & 0xFFu);
         timestamp_bytes[1u] = (uint8_t)((system_time_s >> 8u) & 0xFFu);
         timestamp_bytes[0u] = (uint8_t)(system_time_s & 0xFFu);
      }
   }

   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, timestamp_bytes[0u]));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, timestamp_bytes[1u]));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, timestamp_bytes[2u]));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, timestamp_bytes[3u]));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, module_id));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, (uint8_t)(line & 0xFFu)));
   IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(buf, (uint8_t)((line >> 8u) & 0xFFu)));

   return result;
}

result_t route_debug_log_to_endpoint(
   uint8_t endpoint_idx, DEBUG_LEVEL level, uint8_t module_id, uint16_t line, const uint32_t *args, uint32_t nargs)
{
   RETURN_ERR_IF_TRUE(MAX_SUPPORTED_VARARGS < nargs, DEBUG_ERROR_TX_MAX_ARGS);
   RETURN_ERR_IF_TRUE((NULL == args) && (nargs != 0u), DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_TRUE(endpoint_idx >= ENDPOINT_MAX, DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_NULL(tx_handlers[endpoint_idx].handler, DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_TRUE((false == m_debug_module_initialized), DEBUG_ERROR_NOT_INITIALIZED);

   DEBUG_LEVEL unit_level = DEBUG_LEVEL_DISABLED;

   // Skip all logic if endpoint is not registered, debug level is lower, or result is error
   result_t result = get_unit_debug_level_for_endpoint(module_id, &unit_level, endpoint_idx);

   // Early return for efficiency if level is below logging level threshold
   if(IS_OK(result))
   {
      RETURN_OK_IF_TRUE(level < unit_level);
   }

   bool is_lock_acquired = false;

   if(IS_OK(result))
   {
      is_lock_acquired = nrf_mtx_trylock(&debug_tx_mutex);
   }

   if(IS_OK(result) && !is_lock_acquired)
   {
      SET_ERR(result, DEBUG_ERROR_TX_MUTEX);
   }

   IF_OK_RUN_AND_UPDATE(result, reset_tx_buffer(&m_debug_buffer));

   if(IS_OK(result))
   {
      if(tx_handlers[endpoint_idx].apply_encoding)
      {
         IF_OK_RUN_AND_UPDATE(result, initialize_tx_packet(&m_debug_buffer, level, module_id, line));

         for(uint8_t arg_idx = 0u; arg_idx < nargs; arg_idx++)
         {
            IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_u32_le(&m_debug_buffer, args[arg_idx], true));
         }
         IF_OK_RUN_AND_UPDATE(result, append_crc(&m_debug_buffer));
         IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(&m_debug_buffer, FRAMING_CHAR));
      }
      else
      {
         IF_OK_RUN_AND_UPDATE(result, initialize_unencoded_tx_packet(&m_debug_buffer, level, module_id, line));

         for(uint8_t arg_idx = 0u; arg_idx < nargs; arg_idx++)
         {
            IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_unencoded_u32_le(&m_debug_buffer, args[arg_idx]));
         }
      }
   }

   IF_OK_RUN_AND_UPDATE(result, tx_handlers[endpoint_idx].handler(m_debug_buffer.p_data, m_debug_buffer.idx));

   if(is_lock_acquired)
   {
      nrf_mtx_unlock(&debug_tx_mutex);
   }

   return result;
}

result_t debug_send_log_n(DEBUG_LEVEL level, uint8_t module_id, uint16_t line, const uint32_t *args, uint32_t nargs)
{
   RETURN_ERR_IF_TRUE(MAX_SUPPORTED_VARARGS < nargs, DEBUG_ERROR_TX_MAX_ARGS);
   RETURN_ERR_IF_TRUE((NULL == args) && (nargs != 0u), DEBUG_ERROR_INV_ARG);
   RETURN_ERR_IF_TRUE((false == m_debug_module_initialized), DEBUG_ERROR_NOT_INITIALIZED);

   bool emit_log = true;
   result_t result = should_emit_log(level, module_id, line, &emit_log);

   if(IS_OK(result) && (true == emit_log))
   {
      for(uint8_t endpoint_idx = 0u; endpoint_idx < ENDPOINT_MAX; endpoint_idx++)
      {
         // Auto-route debug to non-NULL handlers for which auto-routing is enabled
         if((NULL != tx_handlers[endpoint_idx].handler) && (false == tx_handlers[endpoint_idx].disable_auto_routing))
         {
            (void)route_debug_log_to_endpoint(endpoint_idx, level, module_id, line, args, nargs);
         }
      }
   }

   return result;
}

result_t debug_send_data(uint8_t data_id, uint8_t data_len, const uint8_t *data_buf)
{
   RETURN_ERR_IF_NULL(data_buf, DEBUG_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE((false == m_debug_module_initialized), DEBUG_ERROR_NOT_INITIALIZED);

   bool is_datastream_enabled = false;
   result_t result = debug_get_datastream_enabled(data_id, &is_datastream_enabled);

   if(is_datastream_enabled && IS_OK(result))
   {
      if(data_len > DEBUG_DATA_BUFFER_DATA_SIZE)
      {
         data_len = DEBUG_DATA_BUFFER_DATA_SIZE;
      }

      (void)reset_tx_buffer(&m_data_buffer);
      result = append_tx_buffer_raw(&m_data_buffer, FRAMING_CHAR);

      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(&m_data_buffer, (uint8_t)DEBUG_MSG_TYPE_DATA, true));
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(&m_data_buffer, data_id, true));
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(&m_data_buffer, data_len, true));

      for(size_t idx = 0; idx < data_len; idx++)
      {
         IF_OK_RUN_AND_UPDATE(result, append_tx_buffer(&m_data_buffer, data_buf[idx], true));
         BREAK_ON_ERR(result);
      }

      IF_OK_RUN_AND_UPDATE(result, append_crc(&m_data_buffer));
      IF_OK_RUN_AND_UPDATE(result, append_tx_buffer_raw(&m_data_buffer, FRAMING_CHAR));

      for(int endpoint_idx = 0; endpoint_idx < ENDPOINT_MAX; endpoint_idx++)
      {
         if((NULL != tx_handlers[endpoint_idx].handler) && tx_handlers[endpoint_idx].permit_bulk_data)
         {
            tx_handlers[endpoint_idx].handler(m_data_buffer.p_data, m_data_buffer.idx);
         }
      }
   }
   return result;
}

result_t debug_init(const queue_interface_t *rx_cmd_queue,
                    const system_time_interface_t *system_time,
                    const uint16_t *hot_log_min_intervals_ms)
{
   RETURN_ERR_IF_NULL(system_time, DEBUG_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(system_time->get_initialization_status, DEBUG_ERROR_PTR_NULL);

   bool is_system_time_initialized = false;
   result_t result = system_time->get_initialization_status(system_time, &is_system_time_initialized);
   RETURN_ERR_IF_TRUE((false == is_system_time_initialized), DEBUG_ERROR_NOT_INITIALIZED);

   if(NULL != hot_log_min_intervals_ms)
   {
      memcpy(m_hot_log_min_intervals_ms, hot_log_min_intervals_ms, DEBUG_LEVEL_MAX * sizeof(uint16_t));
   }
   else
   {
      m_hot_log_min_intervals_ms[DEBUG_LEVEL_TRACE] = DEBUG_LOG_LEVEL_TRACE_MIN_INTERVAL_MS;
      m_hot_log_min_intervals_ms[DEBUG_LEVEL_DEBUG] = DEBUG_LOG_LEVEL_DEBUG_MIN_INTERVAL_MS;
      m_hot_log_min_intervals_ms[DEBUG_LEVEL_INFO] = DEBUG_LOG_LEVEL_INFO_MIN_INTERVAL_MS;
      m_hot_log_min_intervals_ms[DEBUG_LEVEL_WARN] = DEBUG_LOG_LEVEL_WARN_MIN_INTERVAL_MS;
      m_hot_log_min_intervals_ms[DEBUG_LEVEL_ERROR] = DEBUG_LOG_LEVEL_ERROR_MIN_INTERVAL_MS;
      m_hot_log_min_intervals_ms[DEBUG_LEVEL_CRITICAL] = DEBUG_LOG_LEVEL_CRITICAL_MIN_INTERVAL_MS;
   }

   m_summary_process_index = 0u;
   m_last_summary_process_time_ms = 0u;
   memset(m_hot_log_table, 0, HOT_LOG_TABLE_SIZE * sizeof(hot_log_t));

   is_rx_frame_open = false;
   is_next_byte_escaped = false;
   memset(&received_msg, 0, sizeof(debug_msg_t));

   // This is permitted to be NULL. Do NOT NULL check this.
   m_rx_cmd_queue = rx_cmd_queue;
   memset(&tx_handlers, 0, DEBUG_MAX_TX_ENDPOINTS * sizeof(debug_tx_handler *));

   m_system_time = system_time;

   // Initialize debug log mutex
   nrf_mtx_init(&debug_tx_mutex);

   // Set default log level to INFO
   for(uint8_t endpoint = 0u; endpoint < ENDPOINT_MAX; endpoint++)
   {
      (void)debug_set_all_unit_debug_level_for_endpoint(DEBUG_INITIAL_LEVEL, endpoint);
   }

   // Disable all debug datastreams by default
   for(size_t idx = 0; idx < DEBUG_DATASTREAM_MAX_COUNT; idx++)
   {
      (void)debug_set_datastream_enabled((uint8_t)idx, false);
   }

   if(IS_OK(result))
   {
      m_debug_module_initialized = true;
   }

   return result;
}

result_t debug_process(void)
{
   RETURN_ERR_IF_TRUE((false == m_debug_module_initialized), DEBUG_ERROR_NOT_INITIALIZED);

   uint64_t current_time_ms = 0u;
   result_t result = m_system_time->get_time_ms(m_system_time, &current_time_ms);
   if(IS_OK(result)
      && (current_time_ms >= (m_last_summary_process_time_ms + m_hot_log_min_intervals_ms[(uint8_t)DEBUG_LEVEL_WARN])))
   {
      bool processed_entry = false;
      for(uint8_t index = m_summary_process_index; index < HOT_LOG_TABLE_SIZE; index++)
      {
         if(m_hot_log_table[index].in_use && m_hot_log_table[index].updated_since_last_summary
            && (current_time_ms >= (m_hot_log_table[index].last_summary_time_ms + HOT_LOG_SUMMARY_TIME_MS)))
         {
            DEBUG_WARNING("Suppressed log summary: Module ID: %u, Line: %u, Level: %u, Suppressed: %u times",
                          m_hot_log_table[index].module_id,
                          m_hot_log_table[index].line,
                          m_hot_log_table[index].level,
                          m_hot_log_table[index].suppress_count);

            // Reset summary tracking
            m_hot_log_table[index].last_summary_time_ms = current_time_ms;
            m_hot_log_table[index].updated_since_last_summary = false;
            m_hot_log_table[index].suppress_count = 0u;
            m_summary_process_index = index + 1u;
            if(m_summary_process_index >= HOT_LOG_TABLE_SIZE)
            {
               m_summary_process_index = 0u;
            }
            m_last_summary_process_time_ms = current_time_ms;
            processed_entry = true;
            break; // Only process one per call to avoid long blocking
         }
      }

      if(false == processed_entry)
      {
         // All entries processed, reset for next round
         m_summary_process_index = 0u;
      }
   }

   return result;
}
