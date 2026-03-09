/* Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @ingroup common
 * @brief Defines for common functionality across the application.
 * @details
 *
 * @file common.h
 * @ingroup common
 * @brief
 */

#ifndef COMMON_H_
#define COMMON_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "nordic_common.h"
#include <app_util.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "result.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define NFC_TAG_MAX_UID_SIZE (10u)

#define I2C_ADDRESS_MAX (127u)

#define COMMON_1K_FACTOR (1000u) /**< Factor for converting to/from kilo-units */

#define COMMON_BITS_PER_BYTE (8u)
#define COMMON_BIT_MASK(pos) (1U << (pos))

#define NFC_MAILBOX_BUFFER_SIZE (255u)

// Dose schedule parameters
#define DOSE_SCHEDULE_MAX_DOSES_PER_DAY        (10u)    /**< Maximum number of dose events per day */
#define DOSE_SCHEDULE_MAX_DOSAGE_MG            (10000u) /**< Maximum dosage amount in milligrams */
#define DOSE_SCHEDULE_MAX_TEMP_THRESHOLD_DEG_C (60)     /**< Maximum temperature threshold in degrees Celsius */
#define DOSE_SCHEDULE_MIN_TEMP_THRESHOLD_DEG_C (0)      /**< Minimum temperature threshold in degrees Celsius */
#define DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC  (3600u)  /**< Maximum temperature averaging window duration in seconds */
#define DOSE_SCHEDULE_MAX_DOSE_WINDOW_MINUTES  (60u)    /**< Maximum dose window duration in minutes */

#define COMMON_SECONDS_IN_MINUTE           (60u)
#define DOSE_DETECTION_MAX_DOSE_DURATION_S (4u * COMMON_SECONDS_IN_MINUTE) /**< 4 minutes in seconds */

#ifdef UNIT_TEST
#   define PRIVATE
#else
#   define PRIVATE static
#endif

#define COMMON_1K_CST          (1000u)
#define MAX_ERROR_ARGS         (6u)  /**< Maximum number of arguments for error logs */
#define MAC_ADDRESS_SIZE_BYTES (10u) /**< Size of a MAC address in bytes */

/**
 * @brief Macro to calculate the length of a static array.
 *
 * This macro computes the number of elements in a static array by dividing the total size of the array by the size of
 * its first element. The denominator is a compile-time check to ensure that the input is indeed an array.
 *
 * @param _x_ The static array whose length is to be calculated.
 *
 * @note The use of `0[_x_]` is intentional. It should not be rewritten. Doing so may defeat the intended
 * compile-time diagnostics in some toolchains.
 */
#define ARRAY_LEN(_x_) ((sizeof(_x_) / sizeof(0 [_x_])) / ((size_t)(!(sizeof(_x_) % sizeof(0 [_x_])))))

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef enum
{
   PPI_TYPE_RQ,
   PPI_TYPE_RE,
   PPI_TYPE_PUSH,
   PPI_TYPE_MAX,
} PPI_TYPE;
STATIC_ASSERT(PPI_TYPE_MAX <= UINT8_MAX, "PPI_TYPE must fit in uint8_t");
// Static assert to ensure enum fits in uint8_t - as used in message_protocol_packet_t

typedef enum
{
   BONDING_STATE_UNBONDED = 0,
   BONDING_STATE_BONDING,
   BONDING_STATE_BONDED,
   BONDING_STATE_MAX
} BONDING_STATE;

typedef enum
{
   CALIBRATION_STATUS_DISABLED = 0,
   CALIBRATION_STATUS_MAX
} CALIBRATION_STATUS;

typedef enum
{
   DOCKED_STATUS_UNDOCKED = 0,
   DOCKED_STATUS_DOCKED,
   DOCKED_STATUS_MAX,
} DOCKED_STATUS;

typedef enum
{
   BATTERY_STATE_SOC_GOOD = 0,
   BATTERY_STATE_SOC_LOW,
   BATTERY_STATE_CHARGING,
   BATTERY_STATE_CHARGING_COMPLETED,
   BATTERY_STATE_ERROR,
   BATTERY_STATE_MAX
} BATTERY_STATE;

typedef enum
{
   DEVICE_BLE_EVT_NONE = 0,
   DEVICE_BLE_EVT_CONNECTED,
   DEVICE_BLE_EVT_DISCONNECTED,
   DEVICE_BLE_EVT_TIMEOUT,
   DEVICE_BLE_EVT_MAX,
} DEVICE_BLE_EVT;

/**
 * @brief Structure representing the calibration record for the weight stack that should be stored in NVM. This
 * calibration data is used by the Weight sensing module to convert raw ADC values to weight measurements in milligrams.
 */
typedef struct
{
   int32_t zero_offset;
   int32_t calibration_factor;
   uint32_t full_assembly_weight_mg;
} weight_stack_calibration_record_t;

/**
 * @brief Medication type
 *
 * @todo Reserved for future use. Implement additional medication types once needed.
 */
typedef enum
{
   MEDICATION_TYPE_UNDEFINED = 0,

   MEDICATION_TYPE_MAX /**< Sentinel value */
} MEDICATION_TYPE;

#define DOSE_SCHEDULE_FIXED_SIZE_BYTES         (10u)
#define DOSE_SCHEDULE_WINDOWS_ARRAY_SIZE_BYTES (DOSE_SCHEDULE_MAX_DOSES_PER_DAY * sizeof(uint16_t))
#define DOSE_SCHEDULE_SIZE_BYTES               (DOSE_SCHEDULE_FIXED_SIZE_BYTES + DOSE_SCHEDULE_WINDOWS_ARRAY_SIZE_BYTES)

/**
 * @brief Dose schedule
 *
 * @note Packed to minimize memory usage and ensure consistency over communication interfaces.
 */
typedef struct __attribute__((packed))
{
   uint8_t medication_type; /**< Medication type. Reserved for future use. See @ref MEDICATION_TYPE */
   uint16_t dosage_mg;      /**< Dosage amount in milligrams.*/

   int8_t temp_upper_limit_deg_c;         /**< Upper limit for temperature in degrees Celsius */
   int8_t temp_lower_limit_deg_c;         /**< Lower limit for temperature in degrees Celsius */
   uint16_t temp_avg_window_duration_sec; /**< Duration over which temperature is averaged in seconds */

   /**
    * @brief Bitfield used to determine the days on which doses are scheduled.
    *
    * The bitfield works as follows:
    *
    * - If `b[7] = 1`, `b[6:0]` represent number of days to skip between dose days.
    *   E.g., `0b10000010` = dose every 3rd day (i.e., dose day, skip 2 days, dose day, etc.).
    *   The first dose day is determined by the `active_since_unix` timestamp.
    *
    * - If `b[7] = 0`, `b[6:0]` represent days of the week for dosing where `b[0] = Monday` and `b[6] = Sunday`.
    *   E.g., `0b00010110` = dose on Tuesdays, Wednesdays, and Fridays.
    */
   uint8_t dose_days_bitfield;
   uint8_t dose_window_duration_minutes; /**< Duration of each dose window in minutes */
   uint8_t dose_window_count;            /**< Number of dose windows defined in the schedule */
   uint16_t dose_window_start_times_minutes[DOSE_SCHEDULE_MAX_DOSES_PER_DAY]; /**< Start times of dose windows in
                                                                                 minutes from midnight */
} dose_schedule_t;

STATIC_ASSERT(sizeof(dose_schedule_t) == DOSE_SCHEDULE_SIZE_BYTES,
              "Size of dose_schedule_t does not match expected size of 45 bytes");

/**
 * @brief Dose schedule record
 *
 * A dose schedule and related metadata that should be stored in NVM.
 *
 * @note Packed to minimize memory usage and ensure consistency over communication interfaces.
 */
typedef struct __attribute__((packed))
{
   dose_schedule_t dose_schedule;    /**< Dose schedule data */
   uint32_t start_date_unix_seconds; /**< Start date of the schedule in UNIX epoch format */
} dose_schedule_record_t;

/**
 * @brief Event ID structure, contains days since epoch and event counter. The event counter is incremented each time a
 * new dose event is recorded and reset at midnight daily.
 * This structure is packed to minimize memory usage.
 *
 * This structure ensures a unique identifier for each dose event while keeping a low memory footprint.
 */
#define EVENT_ID_T_SIZE_BYTES (3u) /**< Size of event_id_t in bytes */
typedef struct __attribute__((packed))
{
   uint16_t days_since_epoch; /**< 0-65535*/
   uint8_t event_ctr;         /**< Event counter (0-255). Resets at midnight every day.*/
} event_id_t;
STATIC_ASSERT(EVENT_ID_T_SIZE_BYTES == sizeof(event_id_t),
              "Size of event_id_t does not match defined EVENT_ID_T_SIZE_BYTES");

/**
 * @brief Structure representing a dose event.
 *
 * @details This structure contains information about a dose event, including the start time, duration, and an
 * indication of whether the dose was completed withing the permitted time
 *
 * @note The structure is packed to minimize memory usage.
 */
#define DOSE_EVENT_T_SIZE_BYTES (12u)
typedef struct __attribute__((packed))
{
   event_id_t event_id;             /**< Unique identifier for the dose event.*/
   uint32_t start_timestamp_unix_s; /**< Start (Cap off) timestamp of the dose event in unix format. */
   uint16_t duration_s;             /**< End (Cap on) timestamp - Time since dose start in seconds */
   uint8_t dose_completed_in_time;  /**< Boolean indicating if the dose was completed in the allowed time limit.*/
   uint16_t tilt_count;             /**< Number of tilts detected during the dose event. */
} dose_event_t;
STATIC_ASSERT(DOSE_EVENT_T_SIZE_BYTES == sizeof(dose_event_t),
              "Size of dose_event_t does not match defined DOSE_EVENT_T_SIZE_BYTES");

#define DOSE_DATA_T_SIZE_BYTES (19u)
typedef struct __attribute__((packed))
{
   event_id_t event_id;            /**< Unique identifier for the dose event.*/
   uint32_t relative_timestamp_ms; /**< Timestamp of the dose event relative to start of dose event. */
   int16_t accel_x;                /**< X-axis acceleration in m/s². */
   int16_t accel_y;                /**< Y-axis acceleration in m/s². */
   int16_t accel_z;                /**< Z-axis acceleration in m/s². */
   int16_t angular_accel_x;        /**< X-axis rotation in deg/s². */
   int16_t angular_accel_y;        /**< Y-axis rotation in deg/s². */
   int16_t angular_accel_z;        /**< Z-axis rotation in deg/s². */
} dose_data_t;
STATIC_ASSERT(DOSE_DATA_T_SIZE_BYTES == sizeof(dose_data_t),
              "Size of dose_data_t does not match defined DOSE_DATA_T_SIZE_BYTES");

#define SEMANTIC_VERSION_T_SIZE_BYTES (3u)
typedef struct __attribute__((packed)) semantic_version
{
   uint8_t major; /**< Major version number. */
   uint8_t minor; /**< Minor version number. */
   uint8_t patch; /**< Patch version number. */
} semantic_version_t;
STATIC_ASSERT(SEMANTIC_VERSION_T_SIZE_BYTES == sizeof(semantic_version_t),
              "Size of semantic_version_t does not match defined SEMANTIC_VERSION_T_SIZE_BYTES");

#define RING_STATUS_T_SIZE_BYTES (47u)
typedef struct __attribute__((packed))
{
   semantic_version_t hardware_version;         /**< Hardware version of the ring. */
   semantic_version_t firmware_version;         /**< Firmware version of the ring. */
   uint8_t mac[10];                             /**< MAC address of the ring. */
   uint32_t timestamp_unix_s;                   /**< Current unix time in seconds. Set by the ring at transmission.*/
   uint32_t ship_mode_exit_timestamp_unix;      /**< Unix timestamp in seconds when the ring exited ship mode. */
   uint8_t battery_charge_status;               /**< Battery charge status of the ring. */
   uint32_t uptime_s;                           /**< Uptime of the ring in seconds. */
   int16_t temperature_celsius;                 /**< Temperature of the ring in degrees Celsius. */
   uint8_t dose_fifo_used_percent;              /**< Percentage of dose FIFO used. */
   uint8_t battery_fifo_used_percent;           /**< Percentage of battery FIFO used. */
   uint8_t imu_fifo_used_percent;               /**< Percentage of IMU FIFO used. */
   uint8_t error_fifo_used_percent;             /**< Percentage of error FIFO used. */
   uint8_t dose_fifo_used_percent_watermark;    /**< Watermark percentage of dose FIFO used. */
   uint8_t battery_fifo_used_percent_watermark; /**< Watermark percentage of battery FIFO used. */
   uint8_t imu_fifo_used_percent_watermark;     /**< Watermark percentage of IMU FIFO used. */
   uint8_t error_fifo_used_percent_watermark;   /**< Watermark percentage of error FIFO used. */
   uint16_t battery_sample_frequency_millihz;   /**< Battery sample frequency in millihertz. */
   uint16_t cap_on;
   uint16_t cap_off;
   uint16_t current_prox;
} ring_status_t;
STATIC_ASSERT(RING_STATUS_T_SIZE_BYTES == sizeof(ring_status_t),
              "Size of ring_status_t does not match defined RING_STATUS_T_SIZE_BYTES");

#define DOCK_STATUS_T_SIZE_BYTES (28u)
typedef struct __attribute__((packed))
{
   semantic_version_t hardware_version;    /**< Hardware version of the dock. */
   semantic_version_t firmware_version;    /**< Firmware version of the dock. */
   uint8_t mac[MAC_ADDRESS_SIZE_BYTES];    /**< MAC address of the dock. */
   uint32_t timestamp_unix_s;              /**< Current unix time in seconds. Set by the dock at transmission.*/
   uint32_t ship_mode_exit_timestamp_unix; /**< Unix timestamp in seconds when the dock exited ship mode. */
   uint32_t uptime_s;                      /**< Uptime of the dock in seconds. */
} dock_status_t;
STATIC_ASSERT(DOCK_STATUS_T_SIZE_BYTES == sizeof(dock_status_t),
              "Size of dock_status_t does not match defined DOCK_STATUS_T_SIZE_BYTES");

#define TEMPERATURE_LOG_T_SIZE_BYTES (6u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Unix timestamp in seconds */
   int16_t temperature_deg_c; /**< Temperature in degrees Celsius. */
} temperature_log_t;
STATIC_ASSERT(TEMPERATURE_LOG_T_SIZE_BYTES == sizeof(temperature_log_t),
              "Size of temperature_log_t does not match defined TEMPERATURE_LOG_T_SIZE_BYTES");

#define DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES (16u)
typedef struct __attribute__((packed))
{
   int32_t weight_mg;           /**< Weight measurement in milligrams. */
   uint32_t total_dispensed_mg; /**< Total dispensed weight since start of previous baselining (i.e. new medication). */
   uint16_t std_dev;            /**< Standard deviation for all raw ADC data used for averaged weight sample. */
   int16_t temperature_deg_c_div10; /**< Load Cell Temperature in degrees Celsius / 10 */
   uint32_t timestamp_unix_s;       /**< Unix timestamp in seconds */
} dock_weight_measurement_t;
STATIC_ASSERT(DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES == sizeof(dock_weight_measurement_t),
              "Size of dock_weight_measurement_t does not match defined DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES");

#define DOCK_CHARGE_STATUS_T_SIZE_BYTES (5u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Unix timestamp in seconds */
   uint8_t charge_status;     /**< Charge status of the dock battery. maps to BATTERY_STATE enum*/
} dock_charge_status_t;
STATIC_ASSERT(DOCK_CHARGE_STATUS_T_SIZE_BYTES == sizeof(dock_charge_status_t),
              "Size of dock_charge_status_t does not match defined DOCK_CHARGE_STATUS_T_SIZE_BYTES");

#define RING_DOCKED_STATUS_T_SIZE_BYTES (25u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s;      /**< Unix timestamp in seconds */
   uint8_t docked_status;          /**< Docked status of the ring. Maps to docked status enum */
   uint8_t ring_nfc_id[10u];       /**< NFC ID of the ring. */
   uint8_t medication_nfc_id[10u]; /**< NFC ID of the medication. */
} ring_docked_status_t;
STATIC_ASSERT(RING_DOCKED_STATUS_T_SIZE_BYTES == sizeof(ring_docked_status_t),
              "Size of ring_docked_status_t does not match defined RING_DOCKED_STATUS_T_SIZE_BYTES");

#define BLUETOOTH_STATUS_T_SIZE_BYTES (5u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Unix timestamp in seconds */
   uint8_t bluetooth_status;  /**< Bluetooth status of the device. Maps to typed enum BLUETOOTH_STATUS */
} bluetooth_status_t;
STATIC_ASSERT(BLUETOOTH_STATUS_T_SIZE_BYTES == sizeof(bluetooth_status_t),
              "Size of bluetooth_status_t does not match defined BLUETOOTH_STATUS_T_SIZE_BYTES");

#define BATTERY_LEVEL_T_SIZE_BYTES (5u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Unix timestamp in seconds */
   uint8_t battery_level;     /**< Battery level percentage (0-100%). */
} battery_level_t;
STATIC_ASSERT(BATTERY_LEVEL_T_SIZE_BYTES == sizeof(battery_level_t),
              "Size of battery_level_t does not match defined BATTERY_LEVEL_T_SIZE_BYTES");

#define RING_BATTERY_DATA_T_SIZE_BYTES (5u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Timestamp of the battery data in epoch format. */
   uint8_t charge_percent;    /**< State of Charge in percent (0-100%). */
} ring_battery_data_t;
STATIC_ASSERT(RING_BATTERY_DATA_T_SIZE_BYTES == sizeof(ring_battery_data_t),
              "Size of ring_battery_data_t does not match defined RING_BATTERY_DATA_T_SIZE_BYTES");

#define RING_IMU_DATA_T_SIZE_BYTES (16u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Timestamp of the IMU data in epoch format. */
   int16_t accel_x_mg;        /**< Acceleration in X axis in milli-g. */
   int16_t accel_y_mg;        /**< Acceleration in Y axis in milli-g. */
   int16_t accel_z_mg;        /**< Acceleration in Z axis in milli-g. */
   int16_t gyro_x_mdps;       /**< Gyroscope data in X axis in milli-degrees per second. */
   int16_t gyro_y_mdps;       /**< Gyroscope data in Y axis in milli-degrees per second. */
   int16_t gyro_z_mdps;       /**< Gyroscope data in Z axis in milli-degrees per second. */
} ring_imu_data_t;
STATIC_ASSERT(RING_IMU_DATA_T_SIZE_BYTES == sizeof(ring_imu_data_t),
              "Size of ring_imu_data_t does not match defined RING_IMU_DATA_T_SIZE_BYTES");

// Type for error logs
#define ERROR_LOG_T_SIZE_BYTES (28u)
typedef struct __attribute__((packed))
{
   uint8_t level;                 /**< Error level from DEBUG_LEVEL enum. */
   uint8_t module_id;             /**< Module ID where the error originated. */
   uint16_t line;                 /**< Line number in the source code where the error was logged. */
   uint32_t args[MAX_ERROR_ARGS]; /**< Variable arguments associated with the error log. */
} error_log_t;
STATIC_ASSERT(ERROR_LOG_T_SIZE_BYTES == sizeof(error_log_t),
              "Size of error_log_t does not match defined ERROR_LOG_T_SIZE_BYTES");

#define RING_ERROR_T_SIZE_BYTES (32u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Timestamp of the error in epoch format. */
   error_log_t error;         /**< Error log associated with the ring error. */
} ring_error_t;
STATIC_ASSERT(RING_ERROR_T_SIZE_BYTES == sizeof(ring_error_t),
              "Size of ring_error_t does not match defined RING_ERROR_T_SIZE_BYTES");

#define RING_DOCKING_EVENT_T_SIZE_BYTES (5u)
typedef struct __attribute__((packed))
{
   uint32_t timestamp_unix_s; /**< Timestamp of the docking event in epoch format. */
   uint8_t status;            /**< Ring docked status(e.g., docked or undocked). Values from DOCKED_STATUS enum */
} ring_docking_event_t;
STATIC_ASSERT(RING_DOCKING_EVENT_T_SIZE_BYTES == sizeof(ring_docking_event_t),
              "Size of ring_docking_event_t does not match defined RING_DOCKING_EVENT_T_SIZE_BYTES");

#define SHIP_MODE_T_SIZE_BYTES (9u)
typedef struct __attribute__((packed))
{
   uint32_t enter_ship_mode_unix_s; /**< Timestamp of the ship mode entry event in epoch format. */
   uint32_t exit_ship_mode_unix_s;  /**< Timestamp of the ship mode exit event in epoch format. */
   bool is_sleeping; /**< Boolean set to false upon first wake after setting the device into ship mode. */
} ship_mode_t;
STATIC_ASSERT(SHIP_MODE_T_SIZE_BYTES == sizeof(ship_mode_t),
              "Size of ship_mode_t does not match defined SHIP_MODE_T_SIZE_BYTES");

typedef struct __attribute__((packed))
{
   uint16_t cap_on;
   uint16_t cap_off;
} prox_data_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // COMMON_H_