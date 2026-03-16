/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup ring_manager Ring Manager
 * @ingroup modules
 * @brief This module provides a simple interface to the ring.
 * @details The unit interfaces with the message protocol to send and acquire data.
 *
 * @note This module is meant to be used as a singleton.
 *
 * @file ring_manager.h
 * @ingroup ring_manager
 * @brief
 */

#ifndef RING_MANAGER_H_
#define RING_MANAGER_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
#include "message_protocol.h"
#include "ring_dock_ppi.h"

#include "dock_data_manager_interface.h"
#include "ring_manager_interface.h"
#include "rtc_system_time_interface.h"
#include "system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
// Max size needed for the command data is for time update command (uint64_t).
#define MAX_PENDING_COMMAND_DATA_BYTES (sizeof(uint64_t))

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

// Error codes specific to the module.
typedef enum
{
   RING_MANAGER_ERROR_NONE = 0,
   RING_MANAGER_ERROR_NULL_PTR,
   RING_MANAGER_ERROR_NULL_INTERFACE_PTR,
   RING_MANAGER_ERROR_INVALID_DATA_LENGTH,
   RING_MANAGER_ERROR_INCORRECT_PPI_FOR_RX,
   RING_MANAGER_ERROR_UNKNOWN_PPI_FOR_RX,
   RING_MANAGER_ERROR_PROCESSING,
   RING_MANAGER_ERROR_INVALID_PAYLOAD_LENGTH,
   RING_MANAGER_ERROR_INCORRECT_RX_PACKET_TYPE,
   RING_MANAGER_ERROR_INVALID_RX_PACKET_TYPE,
   RING_MANAGER_ERROR_MAX,
} RING_MANAGER_ERROR;

typedef enum
{
   RING_MANAGER_DATA_ID_DOSE_EVENT = 0,
   RING_MANAGER_DATA_ID_RING_DEBUG_LOG_DATA,
   RING_MANAGER_DATA_ID_RING_BATTERY_LEVEL,
   RING_MANAGER_DATA_ID_RING_CAP_DETECTION_STATUS,
   RING_MANAGER_DATA_ID_MAX,
} RING_MANAGER_DATA_ID;

// Command IDs for tracking pending commands - In decreasing order of priority
typedef enum
{
   CMD_ID_UPDATE_TIME = 0,
   CMD_ID_REQ_CAP_DETECTION_STATUS_DATA,
   CMD_ID_UPDATE_BATTERY_SAMPLE_FREQ,
   CMD_ID_UPDATE_CAP_DETECTION_CONFIG,
   CMD_ID_REQ_STATUS_UPDATE,
   CMD_ID_REQ_DOSE_EVENT_DATA,
   CMD_ID_REQ_RING_DEBUG_LOG_DATA,
   CMD_ID_REQ_RING_BATTERY_LEVEL_DATA,
   CMD_ID_MAX,
} COMMAND_ID;

/**
 * @brief Abstraction of the specific destinations for data received from the ring as data_store_t.
 *
 * This allows the destination for data received from the ring to be abstracted and monitored so that
 * data is only requested from the ring when there is capacity in the corresponding storage to receive it.
 */
typedef struct data_store
{
   const DATA_ID id;                       /**< ID. Corresponds to one of the data types received from the ring. */
   const uint16_t element_size;            /**< Size of each element in the data store for the ID */
   const COMMAND_ID associated_command_id; /**< The command ID associated with the data in this store.
                                            Links the data destination to the command that used to get it */
   uint16_t free_bytes;                    /**< Number of free bytes in the data store. */
} data_store_t;

typedef struct pending_command
{
   bool pending;                                 /**< Indicates if the command is pending transmission */
   PPI_RD ppi;                                   /**< PPI associated with the command */
   PPI_TYPE type;                                /**< PPI type (request or response) */
   uint8_t data[MAX_PENDING_COMMAND_DATA_BYTES]; /**< Array to hold command data */
   uint16_t data_length;                         /**< Length of valid data in the data array */
} pending_command_t;

typedef struct ring_manager
{
   ring_manager_interface_t interface;                          /**< Interface for ring manager operations */
   const system_time_interface_t *_systick_ifc;                 /**< Pointer to the system time interface */
   const message_protocol_interface_t *_msg_prot_ifc;           /**< Pointer to the message protocol interface */
   const dock_data_manager_interface_t *_dock_data_manager_ifc; /**< Pointer to the dock data manager interface */
   const rtc_system_time_interface_t *_rtc_ifc;                 /**< Pointer to the RTC system time interface */
   pending_command_t _pending_commands[CMD_ID_MAX];             /**< Array to hold pending commands */
   data_store_t _data_store[RING_MANAGER_DATA_ID_MAX];          /**< Array to hold data stores */
   status_update_t _last_status_update;                         /**< Last status update received */
   ring_status_t _stored_status;                                /**< Last status update stored */
   uint64_t _last_status_request_systick_ms;                    /**< Timestamp of the last status request */
   uint64_t _last_status_store_systick_ms;                      /**< Timestamp of the last status store */
   cap_detection_status_t _last_cap_detection_status;           /**< Current cap detection status */
   uint64_t _last_cap_det_status_rq_systick_ms; /**< Timestamp of the last cap detection status update */
   uint64_t _cap_det_status_polling_timeout;    /**< Timestamp of the last cap detection status polling start */

} ring_manager_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_manager_init(ring_manager_t *const self,
                           const system_time_interface_t *systick_ifc,
                           const message_protocol_interface_t *msg_prot_ifc,
                           const dock_data_manager_interface_t *dock_data_manager_ifc,
                           const rtc_system_time_interface_t *rtc_ifc);

#endif // RING_MANAGER_H_
