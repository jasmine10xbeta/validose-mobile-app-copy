/* Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @ingroup common
 * @brief Defines PPIs for communication between the app and the dock
 * @details
 *
 * @file app_dock_ppi.h
 * @ingroup common
 * @brief
 */

#ifndef APP_DOCK_PPI_H_
#define APP_DOCK_PPI_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @enum PPI_AD
 * @brief PPIs for BLE Comms between App and Dock.
 *
 * @par Command Details
 * - 0: PPI_AD_DOSE_SCHEDULE
 *   - Type: @ref PPI_TYPE_RQ, Payload: None
 *   - Type: @ref PPI_TYPE_RE, Payload: @ref dose_schedule_t
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref dose_schedule_t
 * - 1: PPI_AD_DOCK_STATUS
 *   - Type: @ref PPI_TYPE_RQ, Payload: None
 *   - Type: @ref PPI_TYPE_RE, Payload: @ref dock_status_t
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref dock_status_t
 * - 2: PPI_AD_RING_STATUS
 *   - Type: @ref PPI_TYPE_RQ, Payload: None
 *   - Type: @ref PPI_TYPE_RE, Payload: @ref ring_status_t
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref ring_status_t
 * - 3: PPI_AD_TIME
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref uint32_t
 * - 4: PPI_AD_WEIGHT_MEAS_RATE
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 5: PPI_AD_IMU_SAMPLE_RATE
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 6: PPI_AD_DOCK_BATT_SAMPLE_RATE
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 7: PPI_AD_RING_BATT_SAMPLE_RATE
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 8: PPI_AD_DOCK_TEMP_SAMPLE_RATE
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 9: PPI_AD_DOSE_EVENT_REPORT
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref dose_event_t
 * - 10: PPI_AD_DOSE_DATA_POINT
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 11: PPI_AD_DOCK_TEMP_LOG
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref temperature_log_t
 * - 12: PPI_AD_DOCK_WEIGHT_LOG
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref dock_weight_measurement_t
 * - 13: PPI_AD_DOCK_CHARGE_STATUS
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref dock_charge_status_t
 * - 14: PPI_AD_RING_DOCKED_STATUS
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref ring_docked_status_t
 * - 15: PPI_AD_BL_STATUS
 *   - Type: @ref PPI_TYPE_PUSH, Payload: None
 * - 16: PPI_AD_DOCK_BATT_LEVEL_LOG
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref battery_level_t
 * - 17: PPI_AD_RING_BATT_LEVEL_LOG
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref battery_level_t
 * - 18: PPI_AD_DOCK_DEBUG_LOG
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref raw_debug_log_t
 * - 19: PPI_AD_RING_DEBUG_LOG
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref raw_debug_log_t
 *
 * Baselining Flow Commands
 * - 20: PPI_AD_START_BASELINING
 *   - Type: @ref PPI_TYPE_RQ, Payload: None
 *   - Type: @ref PPI_TYPE_RE, Payload: Bool
 * - 21: PPI_AD_BASELINING_FEEDBACK
 *   - Type: @ref PPI_TYPE_PUSH, Payload: @ref baselining_feedback_t
 * - 22: PPI_AD_VALIDATE_MED
 *   - Type: @ref PPI_TYPE_RQ, Payload: None
 *   - Type: @ref PPI_TYPE_RE, Payload: Bool
 *
 * @note See per-value comments for brief descriptions.
 */
typedef enum
{
   PPI_AD_DOSE_SCHEDULE = 0u,
   PPI_AD_DOCK_STATUS,
   PPI_AD_RING_STATUS,
   PPI_AD_TIME,
   PPI_AD_WEIGHT_MEAS_RATE,
   PPI_AD_IMU_SAMPLE_RATE,
   PPI_AD_DOCK_BATT_SAMPLE_RATE,
   PPI_AD_RING_BATT_SAMPLE_RATE,
   PPI_AD_DOCK_TEMP_SAMPLE_RATE,
   PPI_AD_DOSE_EVENT_REPORT,
   PPI_AD_DOSE_DATA_POINT,
   PPI_AD_DOCK_TEMP_LOG,
   PPI_AD_DOCK_WEIGHT_LOG,
   PPI_AD_DOCK_CHARGE_STATUS,
   PPI_AD_RING_DOCKED_STATUS,
   PPI_AD_BL_STATUS,
   PPI_AD_DOCK_BATT_LEVEL_LOG,
   PPI_AD_RING_BATT_LEVEL_LOG,
   PPI_AD_DOCK_DEBUG_LOG,
   PPI_AD_RING_DEBUG_LOG,

   // Baselining PPIs
   PPI_AD_START_BASELINING,
   PPI_AD_BASELINING_FEEDBACK,
   PPI_AD_VALIDATE_MED,

   // Calibration PPIs
   PPI_AD_START_CALIBRATION,
   PPI_AD_CALIBRATION_DATA,
   PPI_AD_CALIBRATION_WEIGHT_PRESENT,
   PPI_AD_CALIBRATION_FEEDBACK,

   // Development Commands
   PPI_AD_DEVELOPMENT_CMD,

   PPI_AD_MAX,
} PPI_AD;

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @note If this enum is updated, make sure to update m_state_timeouts in baselining_fsm.c and baselining_fsm.h
 */
typedef enum
{
   BASELINING_STATE_WAIT_FOR_RING_REMOVAL = 0u,
   BASELINING_STATE_SET_DOCK_WEIGHT,
   BASELINING_STATE_WAIT_FOR_RING,
   BASELINING_STATE_SET_RING_DOCK_WEIGHT,
   BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION,
   BASELINING_STATE_SET_LOCAL_MED_UID, // After medication validation in backend, save the approved medication UID in
                                       // dock manager. Once saved successfully, progress to COMPLETE state.
   BASELINING_STATE_COMPLETE,
   BASELINING_STATE_ERROR, /**< An error occurred during baselining -> If the App receives this, baselining process must
                              be restarted. */
   BASELINING_STATE_MAX
} BASELINING_STATE;

/**
 * @brief Enum representing the state of the calibration process
 *
 * Current state of the calibration process. Used to report to calibration progress to the app.
 */
typedef enum
{
   // Normal flow states
   CALIBRATION_STATE_WAIT_FOR_RING_REMOVAL = 0u, /**< Waiting for the ring to be removed */
   CALIBRATION_STATE_SET_DOCK_WEIGHT, /**< Waiting to stabilize before setting the tare weight / zero offset */
   CALIBRATION_STATE_WAIT_FOR_CALIBRATION_WEIGHT, /**< Waiting for the calibration weight to be placed */
   CALIBRATION_STATE_CALIBRATING,                 /**< Waiting to stabilize before setting the calibration factor */
   CALIBRATION_STATE_STORE_UPDATED_PARAM,
   CALIBRATION_STATE_COMPLETE, /**< Calibration process is complete */

   // Error states
   CALIBRATION_STATE_ERROR, /**< Catch all error state */

   CALIBRATION_STATE_MAX /**< Sentinel value */
} CALIBRATION_STATE;

typedef enum
{
   MOTION_STATE_STABLE = 0,
   MOTION_STATE_UNSTABLE,
   MOTION_STATE_MAX
} MOTION_STATE;

typedef enum
{
   APP2DOCK_COMMANDS_NONE = 0,
   APP2DOCK_COMMANDS_ENTER_SHIP_MODE,
   APP2DOCK_COMMANDS_ENABLE_DFU, // TODO
   APP2DOCK_COMMANDS_MAX
} APP2DOCK_COMMANDS;

#define BASELINING_FEEDBACK_T_SIZE_BYTES (19u)
typedef struct __attribute__((packed))
{
   uint8_t current_state; /**< Current state of baselining process - see @ref BASELINING_STATE enum*/
   uint8_t motion_state;  /**< Current state of motion during baselining - currently just stable/unstable but declaring
                             uint8_t for future head-room. See @ref MOTION_STATE enum */
   uint16_t std_dev;      /**< Standard deviation */
   int32_t avg_weight_mg; /**< Average weight measurement */
   bool is_ring_present;  /**< Boolean indicating whether a ring is present on the dock. */
   uint8_t medication_nfc_id[10u]; /**< NFC ID of the medication. */
} baselining_feedback_t;
STATIC_ASSERT(sizeof(baselining_feedback_t) == BASELINING_FEEDBACK_T_SIZE_BYTES,
              "Size of baselining_feedback_t does not match expected size of 21 bytes");

#define CALIBRATION_FEEDBACK_T_SIZE_BYTES (9u)
typedef struct __attribute__((packed))
{
   uint8_t current_state; /**< Current state of baselining process - see @ref CALIBRATION_STATE enum*/
   uint8_t motion_state;  /**< Current state of motion during baselining - currently just stable/unstable but declaring
                             uint8_t for future head-room. See @ref MOTION_STATE enum */
   uint16_t std_dev;      /**< Standard deviation */
   int32_t avg_weight_mg; /**< Average weight measurement */
   bool is_ring_present;  /**< Boolean indicating whether a ring is present on the dock. */
} calibration_feedback_t;
STATIC_ASSERT(sizeof(calibration_feedback_t) == CALIBRATION_FEEDBACK_T_SIZE_BYTES,
              "Size of calibration_feedback_t does not match expected size of 11 bytes");

#define START_CALIBRATION_PARAM_T_SIZE_BYTES (5u)
typedef struct __attribute__((packed))
{
   bool start;                     /**< Used to indicate whether calibration process should start or stop */
   uint32_t calibration_weight_mg; /**< The weight to be used for calibration in mg. */
} start_calibration_param_t;
STATIC_ASSERT(sizeof(start_calibration_param_t) == START_CALIBRATION_PARAM_T_SIZE_BYTES,
              "Size of start_calibration_param_t does not match expected size of 5 byte");

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // APP_DOCK_PPI_H_