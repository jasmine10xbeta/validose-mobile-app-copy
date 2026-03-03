/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file debug.h
 * @brief Debug and test command unit
 *
 * Implements compressed debug output, and receipt of test commands via UART
 */

#ifndef DEBUG_H_
#define DEBUG_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SEGGER_RTT.h"

// Custom includes
#include "../../common.h"
#include "../../utils/queue/queue_interface.h"
#include "../system_time/system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#ifndef DEBUG_INITIAL_LEVEL
#   define DEBUG_INITIAL_LEVEL DEBUG_LEVEL_INFO
#endif

#ifdef __GNUC__
/**
 * @brief Count the number of arguments in a variable arguments list.
 *
 * @note WARNING: This macro were tested for safety under GCC only. They are likely portable, but not guaranteed to be.
 *
 * @param[in] _VA_ARGS_ Variable arguments list.
 */
#   define NUMARGS(...) (sizeof((uint32_t[]){0, ##__VA_ARGS__}) / sizeof(uint32_t) - 1)

#else
#   error Define the NUMARGS macro for your target environment in debug.h
#endif

// ================================== SETTINGS. CHANGE BASED ON PROJECT REQUIREMENTS =================================//

#define PRIMARY_ENDPOINT_INDEX (0) /**< Only send commands and data via this endpoint */
#define MAX_SUPPORTED_VARARGS  (6) /**< Max supported number of variable arguments to a debug print statement. */
#define DEBUG_BUFFER_DATA_SIZE                                                                                         \
   (64u) /**< Max size of a single debug message. Change based on use case and `MAX_SUPPORTED_VARARGS`. */
#define DEBUG_DATA_BUFFER_DATA_SIZE (128u) /**< Max size of a single data message. Change based on use case. */
#define DEBUG_MAX_TX_ENDPOINTS      (5u) /**< Max number of debug endpoints (e.g. UART, flash). Change based on use case.*/

// Change this macro to match your platform default text debug interface (e.g. SEGGER_RTT_printf), printf, or blank
#define DEBUG_PLATFORM_DEFAULT(debug_string, ...)                                                                      \
   SEGGER_RTT_printf(0, " [%s:%u] " debug_string "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define HOT_LOG_TABLE_SIZE (64u)
STATIC_ASSERT((HOT_LOG_TABLE_SIZE < UINT8_MAX), "HOT_LOG_TABLE_SIZE must be less than UINT8_MAX");

// =================== CONSTANT DEFINITIONS ANS CALCULATED BUFFER SIZES - SHOULD NOT NEED TO CHANGE ==================//

#define DEBUG_UNIT_ID_LEN   (1)
#define DEBUG_LINE_LEN      (2)
#define DEBUG_MAX_VAR_ARGS  (DEBUG_BUFFER_DATA_SIZE / 4)
#define DEBUG_FRAMING_LEN   (2)
#define DEBUG_CRC_LEN       (2)
#define DEBUG_ID_LEN        (1)
#define DEBUG_DATA_ID_LEN   (1)
#define DEBUG_DATA_SIZE_LEN (1)

#define DEBUG_RX_BUFFER_SIZE                                                                                           \
   (DEBUG_ID_LEN + DEBUG_UNIT_ID_LEN + DEBUG_LINE_LEN + DEBUG_BUFFER_DATA_SIZE + DEBUG_CRC_LEN)
#define DEBUG_TX_BUFFER_SIZE                                                                                           \
   (DEBUG_FRAMING_LEN + DEBUG_ID_LEN + DEBUG_UNIT_ID_LEN + DEBUG_LINE_LEN + DEBUG_BUFFER_DATA_SIZE                     \
    + DEBUG_BUFFER_DATA_SIZE + DEBUG_CRC_LEN)
#define DEBUG_DATA_TX_BUFFER_SIZE                                                                                      \
   (DEBUG_FRAMING_LEN + DEBUG_DATA_ID_LEN + DEBUG_DATA_SIZE_LEN + DEBUG_LINE_LEN + DEBUG_DATA_BUFFER_DATA_SIZE         \
    + DEBUG_DATA_BUFFER_DATA_SIZE + DEBUG_CRC_LEN)

/**
 * @brief Write varargs string to debug interface with `DEBUG_LEVEL_TRACE`.
 *
 * The string argument is intentionally discarded and the called `DEBUG_LOG` macro
 * only logs the unit ID and line number, along with _VAR_ARGS_. The string can
 * be reconstructed by the 10XBeta debug engineering tool.
 *
 * @param[in] debug_string The format string to log. Discarded.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to `debug_string`.
 */
#define DEBUG_TRACE(debug_string, ...)                                                                                 \
   DEBUG_PLATFORM_DEFAULT("[TRACE] " debug_string, ##__VA_ARGS__);                                                     \
   DEBUG_LOG(DEBUG_LEVEL_TRACE, ##__VA_ARGS__)

/**
 * @brief Write varargs string to debug interface with `DEBUG_LEVEL_DEBUG`.
 *
 * @param[in] debug_string The format string to log. Discarded.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to `debug_string`.
 */
#define DEBUG_DEBUG(debug_string, ...)                                                                                 \
   DEBUG_PLATFORM_DEFAULT("[DEBUG] " debug_string, ##__VA_ARGS__);                                                     \
   DEBUG_LOG(DEBUG_LEVEL_DEBUG, ##__VA_ARGS__)

/**
 * @brief Write varargs string to debug interface with `DEBUG_LEVEL_INFO`.
 *
 * The string argument is intentionally discarded and the called `DEBUG_LOG` macro
 * only logs the unit ID and line number, along with _VAR_ARGS_. The string can
 * be reconstructed by the 10XBeta debug engineering tool.
 *
 * @param[in] debug_string The format string to log. Discarded.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to `debug_string`.
 */
#define DEBUG_INFO(debug_string, ...)                                                                                  \
   DEBUG_PLATFORM_DEFAULT("[INFO] " debug_string, ##__VA_ARGS__);                                                      \
   DEBUG_LOG(DEBUG_LEVEL_INFO, ##__VA_ARGS__)

/**
 * @brief Write varargs string to debug interface with `DEBUG_LEVEL_WARN`.
 *
 * The string argument is intentionally discarded and the called `DEBUG_LOG` macro
 * only logs the unit ID and line number, along with _VAR_ARGS_. The string can
 * be reconstructed by the 10XBeta debug engineering tool.
 *
 * @param[in] debug_string The format string to log. Discarded.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to `debug_string`.
 */
#define DEBUG_WARNING(debug_string, ...)                                                                               \
   DEBUG_PLATFORM_DEFAULT("[WARNING] " debug_string, ##__VA_ARGS__);                                                   \
   DEBUG_LOG(DEBUG_LEVEL_WARN, ##__VA_ARGS__)

/**
 * @brief Write varargs string to debug interface with `DEBUG_LEVEL_ERROR`.
 *
 * The string argument is intentionally discarded and the called `DEBUG_LOG` macro
 * only logs the unit ID and line number, along with _VAR_ARGS_. The string can
 * be reconstructed by the 10XBeta debug engineering tool.
 *
 * @param[in] debug_string The format string to log. Discarded.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to `debug_string`.
 */
#define DEBUG_ERROR(debug_string, ...)                                                                                 \
   DEBUG_PLATFORM_DEFAULT("[ERROR] " debug_string, ##__VA_ARGS__);                                                     \
   DEBUG_LOG(DEBUG_LEVEL_ERROR, ##__VA_ARGS__)

/**
 * @brief Write varargs string to debug interface with `DEBUG_LEVEL_CRITICAL`.
 *
 * The string argument is intentionally discarded and the called `DEBUG_LOG` macro
 * only logs the unit ID and line number, along with _VAR_ARGS_. The string can
 * be reconstructed by the 10XBeta debug engineering tool.
 *
 * @param[in] debug_string The format string to log.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to `debug_string`.
 */
#define DEBUG_CRITICAL(debug_string, ...)                                                                              \
   DEBUG_PLATFORM_DEFAULT("[CRITICAL] " debug_string, ##__VA_ARGS__);                                                  \
   DEBUG_LOG(DEBUG_LEVEL_CRITICAL, ##__VA_ARGS__)

/**
 * @brief Write varargs string to debug interface with variable debug level.
 *
 * Do not invoke this macro directly. It is intended to be called via the DEBUG_XXX macros.
 *
 * @param[in] level The debug level.
 * @param[in] _VA_ARGS_ Variable arguments corresponding to the format string in the calling macro. See
 * `debug_interface.h`
 */

#define GET_DEBUG_LOG_MACRO(_0, _1, _2, _3, _4, _5, _6, NAME, ...) NAME

#define DEBUG_LOG(level, ...)                                                                                          \
   GET_DEBUG_LOG_MACRO(                                                                                                \
      , ##__VA_ARGS__, DEBUG_LOG_6, DEBUG_LOG_5, DEBUG_LOG_4, DEBUG_LOG_3, DEBUG_LOG_2, DEBUG_LOG_1, DEBUG_LOG_0)      \
   ((level), ##__VA_ARGS__)

#define DEBUG_LOG_0(level) debug_send_log_n(level, THIS_UNIT_ID, (uint16_t)__LINE__, NULL, 0u)

#define DEBUG_LOG_1(level, arg1)                                                                                       \
   do                                                                                                                  \
   {                                                                                                                   \
      const uint32_t _args[] = {(uint32_t)(arg1)};                                                                     \
      debug_send_log_n((level), THIS_UNIT_ID, (uint16_t)__LINE__, _args, 1u);                                          \
   } while(0)

#define DEBUG_LOG_2(level, arg1, arg2)                                                                                 \
   do                                                                                                                  \
   {                                                                                                                   \
      const uint32_t _args[] = {(uint32_t)(arg1), (uint32_t)(arg2)};                                                   \
      debug_send_log_n((level), THIS_UNIT_ID, (uint16_t)__LINE__, _args, 2u);                                          \
   } while(0)

#define DEBUG_LOG_3(level, arg1, arg2, arg3)                                                                           \
   do                                                                                                                  \
   {                                                                                                                   \
      const uint32_t _args[] = {(uint32_t)(arg1), (uint32_t)(arg2), (uint32_t)(arg3)};                                 \
      debug_send_log_n((level), THIS_UNIT_ID, (uint16_t)__LINE__, _args, 3u);                                          \
   } while(0)

#define DEBUG_LOG_4(level, arg1, arg2, arg3, arg4)                                                                     \
   do                                                                                                                  \
   {                                                                                                                   \
      const uint32_t _args[] = {(uint32_t)(arg1), (uint32_t)(arg2), (uint32_t)(arg3), (uint32_t)(arg4)};               \
      debug_send_log_n((level), THIS_UNIT_ID, (uint16_t)__LINE__, _args, 4u);                                          \
   } while(0)

#define DEBUG_LOG_5(level, arg1, arg2, arg3, arg4, arg5)                                                               \
   do                                                                                                                  \
   {                                                                                                                   \
      const uint32_t _args[]                                                                                           \
         = {(uint32_t)(arg1), (uint32_t)(arg2), (uint32_t)(arg3), (uint32_t)(arg4), (uint32_t)(arg5)};                 \
      debug_send_log_n((level), THIS_UNIT_ID, (uint16_t)__LINE__, _args, 5u);                                          \
   } while(0)

#define DEBUG_LOG_6(level, arg1, arg2, arg3, arg4, arg5, arg6)                                                         \
   do                                                                                                                  \
   {                                                                                                                   \
      const uint32_t _args[] = {                                                                                       \
         (uint32_t)(arg1), (uint32_t)(arg2), (uint32_t)(arg3), (uint32_t)(arg4), (uint32_t)(arg5), (uint32_t)(arg6)};  \
      debug_send_log_n((level), THIS_UNIT_ID, (uint16_t)__LINE__, _args, 6u);                                          \
   } while(0)

/**
 * @brief Write data sample to debug interface.
 *
 * @param id Application-specific data type identifier
 * @param len Length of the data buffer in bytes
 * @param p_data The buffered data to send
 */
#define DEBUG_DATA(id, len, p_data) debug_send_data(id, len, p_data)

/**
 * @brief Write command response to debug interface.
 *
 * @param id Application-specific data type identifier
 * @param p_response The struct representing the data to send
 */
#define DEBUG_COMMAND_RESPONSE(id, p_response)                                                                         \
   debug_send_command_response(id, sizeof(p_response), (uint8_t *)&(p_response))

// Defines for debug unit enable/disable cache
#define DEBUG_LEVEL_BITMAP_MAX_UNIT_COUNT UINT16_C(255u)
#define DEBUG_LEVEL_BITMAP_BITS_PER_UNIT  UINT16_C(3u)
#define DEBUG_UNITS_ENABLED_BITMAP_SIZE                                                                                \
   (((DEBUG_LEVEL_BITMAP_BITS_PER_UNIT * DEBUG_LEVEL_BITMAP_MAX_UNIT_COUNT) / 8u) + 1u)

// Defines for data stream enable/disable cache
#define DEBUG_DATASTREAM_MAX_COUNT           ((uint16_t)(255u))
#define DEBUG_DATASTREAM_ENABLED_BITMAP_SIZE ((DEBUG_DATASTREAM_MAX_COUNT / 8u) + 1u)

// Debug unit and datasteram bitmap lookup helpers
#define DEBUG_BITMAP_BYTE_IDX(unit_id, bit_offset)                                                                     \
   (uint16_t)(((((uint16_t)unit_id) * DEBUG_LEVEL_BITMAP_BITS_PER_UNIT) + (uint16_t)bit_offset) / (uint16_t)8u)
#define DEBUG_BITMAP_BIT_IDX(unit_id, bit_offset)                                                                      \
   ((uint8_t)(((((uint16_t)unit_id) * DEBUG_LEVEL_BITMAP_BITS_PER_UNIT) + (uint16_t)bit_offset) % (uint16_t)8u))

// Change the first line in the if statement to match your platform default text debug interface
// (e.g. SEGGER_RTT_printf), printf, or blank
#define ON_ERR_DEBUG_ERROR(result, debug_string, ...)                                                                  \
   do                                                                                                                  \
   {                                                                                                                   \
      if(IS_ERR((result)))                                                                                             \
      {                                                                                                                \
         DEBUG_PLATFORM_DEFAULT("[ERROR] " debug_string, ##__VA_ARGS__);                                               \
         DEBUG_LOG(DEBUG_LEVEL_ERROR, ##__VA_ARGS__);                                                                  \
      }                                                                                                                \
   } while(0u)

#define ON_ERR_DEBUG_WARNING(result, debug_string, ...)                                                                \
   do                                                                                                                  \
   {                                                                                                                   \
      if(IS_ERR((result)))                                                                                             \
      {                                                                                                                \
         DEBUG_PLATFORM_DEFAULT("[WARNING] " debug_string, ##__VA_ARGS__);                                             \
         DEBUG_LOG(DEBUG_LEVEL_WARN, ##__VA_ARGS__);                                                                   \
      }                                                                                                                \
   } while(0u)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef enum
{
   DEBUG_ERROR_NONE = 0,
   DEBUG_ERROR_TX_MAX_ARGS,
   DEBUG_ERROR_TX_MUTEX,
   DEBUG_ERROR_PTR_NULL,
   DEBUG_ERROR_BUF_OVERFLOW,
   DEBUG_ERROR_GET_DEBUG_LEVEL,
   DEBUG_ERROR_SET_DEBUG_LEVEL,
   DEBUG_ERROR_REGISTER_HANDLER,
   DEBUG_ERROR_NO_ENDPOINTS,
   DEBUG_ERROR_DATASTREAM_DISABLED,
   DEBUG_ERROR_INV_ARG,
   DEBUG_ERROR_NOT_INITIALIZED,
   DEBUG_ERROR_MAX,
} DEBUG_ERROR;

typedef struct debug_buffer
{
   uint16_t size;
   uint16_t idx;
   uint8_t *p_data;
   uint16_t crc;
} debug_buffer_t;

/**
 * @brief Debug message frame. Used only for incoming messages.
 */
typedef struct __attribute__((packed, aligned(1)))
{
   uint8_t packet_data_len;
   uint8_t packet_data[DEBUG_RX_BUFFER_SIZE];
} debug_msg_t;

typedef enum
{
   DEBUG_MSG_TYPE_TRACE = 0,
   DEBUG_MSG_TYPE_DEBUG,
   DEBUG_MSG_TYPE_INFO,
   DEBUG_MSG_TYPE_WARN,
   DEBUG_MSG_TYPE_ERROR,
   DEBUG_MSG_TYPE_CRITICAL,
   DEBUG_MSG_TYPE_CMD,
   DEBUG_MSG_TYPE_CMD_RESPONSE,
   DEBUG_MSG_TYPE_SET_DEBUG_LEVEL,
   DEBUG_MSG_TYPE_DATA,
   DEBUG_MSG_TYPE_DATASTREAM_SET_ENABLED,
   DEBUG_MSG_TYPE_MAX,
} DEBUG_MSG_TYPE;

typedef enum
{
   DEBUG_LEVEL_TRACE = 0,
   DEBUG_LEVEL_DEBUG,
   DEBUG_LEVEL_INFO,
   DEBUG_LEVEL_WARN,
   DEBUG_LEVEL_ERROR,
   DEBUG_LEVEL_CRITICAL,
   DEBUG_LEVEL_DISABLED,
   DEBUG_LEVEL_MAX,
} DEBUG_LEVEL;

typedef enum
{
   ENDPOINT_UART = 0,
   ENDPOINT_USER_DEFINED_1,
   ENDPOINT_USER_DEFINED_2,
   ENDPOINT_USER_DEFINED_3,
   ENDPOINT_USER_DEFINED_4,
   ENDPOINT_MAX,
} ENDPOINT_ID;

typedef result_t (*debug_tx_handler)(const uint8_t *p_data, size_t length);

typedef struct
{
   ENDPOINT_ID id; /**< Unique index of this endpoint. */
   debug_tx_handler
      handler; /**< Handler function which pipes raw debug bytes to a physical output interface (e.g. UART, CAN).*/
   bool apply_encoding;       /**< If `true` apply standard HDLC-like framing. Otherwise, write as raw struct. */
   bool disable_auto_routing; /**< Set `true` only if messages will be manually routed to this endpoint via calls to
                                 `route_debug_log_to_endpoint` from user code. This is useful when forwarding logs
                                 between external nodes.*/
   bool permit_bulk_data;     /**< If true, this endpoint will receive bulk data via user-code calls to `DEBUG_DATA`.
                                 Otherwise, only log data will be routed. */
} endpoint_t;

#define RAW_DEBUG_LOG_T_METADATA_SIZE (8u)
#define RAW_DEBUG_LOG_T_ARGS_SIZE     (MAX_SUPPORTED_VARARGS * sizeof(uint32_t))
typedef struct __attribute__((packed))
{
   uint8_t level;
   uint32_t timestamp;
   uint8_t module_id;
   uint16_t line;
   uint32_t arg_values[MAX_SUPPORTED_VARARGS];
} raw_debug_log_t;
STATIC_ASSERT(sizeof(raw_debug_log_t) == (RAW_DEBUG_LOG_T_METADATA_SIZE + RAW_DEBUG_LOG_T_ARGS_SIZE),
              "Size of raw_debug_log_t does not match defined size");

typedef struct
{
   uint32_t key; // combination of module_id and line number to make lookup easier
   uint8_t module_id;
   uint16_t line;
   uint8_t level; // Used to determine which emission policy to apply

   uint64_t next_allowed_time_ms;
   uint64_t last_summary_time_ms;
   uint32_t suppress_count;
   uint64_t first_logged_time_ms;
   uint64_t last_used_time_ms;
   bool updated_since_last_summary;
   bool in_use;

} hot_log_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Set the debug level for all units for a specified endpoint
 *
 * @param log Pointer to a raw debug log type to have the data mapped to
 * @param p_data Data buffer containing the unencoded debug log
 * @param length data in bytes in p_data
 */
result_t debug_map_unencoded_log(raw_debug_log_t *log, const uint8_t *p_data, size_t length);

/**
 * @brief Set the debug level for a specific unit for a specified endpoint
 *
 * @param level Debug severity
 * @param endpoint The endpoint for which the debug level must be set
 * @param unit_id The id of the software unit for which to set the debug level
 */
result_t set_unit_debug_level_for_endpoint(const uint8_t unit_id, DEBUG_LEVEL level, uint8_t endpoint);

/**
 * @brief Set the debug level for all units for a specified endpoint
 *
 * @param level Debug severity
 * @param endpoint The endpoint for which the debug level must be set
 */
result_t debug_set_all_unit_debug_level_for_endpoint(DEBUG_LEVEL level, ENDPOINT_ID endpoint);

/**
 * @brief Get the debug level from a raw debug entry provided as a data buffer
 *
 * @param p_data Raw mbuf as a data buffer
 * @param size Size of the data buffer
 * @param debug_level pointer to a debug_level enum that will be set the the debug level of the mbuf
 */

result_t get_debug_level(const uint8_t *p_data, size_t size, DEBUG_LEVEL *debug_level);

/**
 * @brief Set whether datastream is enabled.
 *
 * @param datastream_id Unique ID of the datastream.
 * @param is_enabled Set to `true` if enabled; `false` otherwise.
 */
result_t debug_set_datastream_enabled(const uint8_t datastream_id, bool is_enabled);

/**
 * @brief Get whether datastream is enabled.
 *
 * @param datastream_id Unique ID of the datastream.
 * @param is_enabled Set to `true` if enabled; `false` otherwise.
 */
result_t debug_get_datastream_enabled(const uint8_t datastream_id, bool *is_enabled);

/**
 * @brief Buffer bytes from physical debug comms interface (e.g. UART).
 *
 * @param p_data Buffer containing received bytes.
 * @param length Number of valid bytes in `p_data`.
 */
result_t debug_on_data_rx(const uint8_t *p_data, size_t length);

/**
 * @brief Get the number of debug command messages buffered in the RX software queue.
 *
 * @param rx_count The number of avaialble debug command message.
 */
result_t debug_get_num_rx_msgs(size_t *rx_count);

/**
 * @brief Get the next incoming debug command message from the software queue.
 *
 * @param msg The next received debug command message.
 */
result_t debug_dequeue_message(debug_msg_t *msg);

/**
 * @brief Register a debug comms interface (e.g. UART, flash, SD card) where compressed debug data bytes must be sent.
 *
 * @note Handler function should not perform any long-running, blocking operations (e.g. blocking serial writes).
 *
 * @param endpoint_spec The configuration for the endpoint to register
 */
result_t debug_register_endpoint(endpoint_t *endpoint_spec);

/**
 * @brief Manually route debugging data. Perform nonblocking debug write with variable number of arguments to a specific
 * endpoint.
 *
 * @note This function is for specific use cases where debug from an external source needs to be routed to only specific
 * endpoints, e.g. when forwarding logs between external nodes. Only use this if you know you require this
 * functionality. For typical use cases, this should not be called at all by user code.
 *
 * @param endpoint_idx Index of the endpoint to which to route the
 * @param level Debug severity
 * @param module_id Unit ID of the unit reporting the debug entry
 * @param line Originating line number of the debug entry
 * @param nargs Number of arguments in following variable arguments list
 */
result_t route_debug_log_to_endpoint(
   uint8_t endpoint_idx, DEBUG_LEVEL level, uint8_t module_id, uint16_t line, const uint32_t *args, uint32_t nargs);

/**
 * @brief Perform nonblocking debug write with variable number of arguments
 *
 * @note This function must only be invoked via the provided `DEBUG_XXX` macros
 *
 * @param level Debug severity
 * @param module_id Unit ID of the unit reporting the debug entry
 * @param line Originating line number of the debug entry
 * @param nargs Number of arguments in following variable arguments list
 */
result_t debug_send_log_n(DEBUG_LEVEL level, uint8_t module_id, uint16_t line, const uint32_t *args, uint32_t nargs);

/**
 * @brief Perform nonblocking data stream write
 *
 * @note This function must only be invoked via the provided `DEBUG_DATA` macro
 *
 * @param data_id Unique data stream ID.
 * @param data_len Number of data bytes in `data_buf`.
 * @param data_buf The data record representing a single sample in the data stream.
 */
result_t debug_send_data(uint8_t data_id, uint8_t data_len, const uint8_t *data_buf);

/**
 * @brief Send response to debug command
 *
 * @note This function must only be invoked via the provided `DEBUG_COMMAND_RESPONSE` macro
 *
 * @param original_msg_id ID of the debug command message to which we are responding.
 * @param data_len Number of data bytes in `data_buf`.
 * @param data_buf Buffer containing the response data.
 */
result_t debug_send_command_response(uint8_t original_msg_id, uint8_t data_len, const uint8_t *data_buf);

/**
 * @brief Initialize the debug unit.
 *
 * @param rx_cmd_queue Queue in which incoming command messages are buffered.
 * @param system_time Reference to the system time unit instance.
 * @param hot_log_min_intervals_ms Pointer to an array of minimum intervals (in ms) between repeated log messages for
 * each hot log entry. If `NULL`, hot logging defaults will be used.
 */
result_t debug_init(const queue_interface_t *rx_cmd_queue,
                    const system_time_interface_t *system_time,
                    const uint16_t *hot_log_min_intervals_ms);

/**
 * @brief Process debug unit tasks. Must be called periodically from main loop.
 */
result_t debug_process(void);

#endif // DEBUG_H_
