/*
 * Copyright (C) {Company} - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file general_control.c
 * @ingroup gc_module
 * @brief
 */

/**
 * @bug Possible bug: The dose scheduler might be getting stuck in a dose window over night which requires  intervention
 * outside the module to correct. Added a timeout in the active state in GC for now.
 *
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// NRF SDK includesFfilter
#include "../dock/bsp/project.h"
#include "../dock/bsp/sdk_config.h"
#include "SEGGER_RTT.h" // Testing only
#include "app_error.h"
#include "app_timer.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"
#include "nrf_crypto.h"
#include "nrf_crypto_error.h"
#include "nrf_delay.h"
#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_wdt.h"
#include "nrf_fstorage.h"
#include "nrf_fstorage_sd.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_power.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdm.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"

// Custom includes
#include "IS31FL3206_driver.h" // LED driver
#include "app_manager.h"
#include "baselining_fsm.h"
#include "battery_manager.h"
#include "ble_control.h"
#include "ble_control_interface.h"
#include "button_driver.h"
#include "buzzer.h"
#include "calibration_fsm.h"
#include "common.h"
#include "comms_driver_interface.h"
#include "debug.h"
#include "dock_data_manager.h"

#include "dose_scheduler.h"
#include "dose_size_detection.h"
#include "general_control.h"
#include "i2c_driver.h"
#include "imu.h"
#include "notifications_module.h"
#include "npm1300_driver.h"
#include "npm1300_driver_interface.h"
#include "nrfx_saadc.h"
#include "queue.h"
#include "ring_manager.h"
#include "rtc_system_time.h"
#include "spi_driver.h"
#include "sts30_dis_temp_sensor.h"
#include "system_fsm.h"
#include "system_time.h"
#include "uart_driver.h"
#include "version.h"
#include "weight_sensor.h"

// NFC
#include "rfal_analogConfig.h"
#include "rfal_nfc.h"
#include "rfal_nfcv.h"
#include "rfal_nrf5_port.h"
#include "rfal_platform.h"
#include "rfal_rf.h"
#include "rfal_st25xv.h"
#include "st25r3916.h"
#include "st25r3916_aat.h"
#include "st25r3916_com.h"
#include "st25r3916_irq.h"

#include "nfc_reader_driver.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_GENERAL_CONTROL_DOCK;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// #define DEBUG_DISABLE_WDT (1u) // Testing only

#ifndef BLE_TX_THROUGHPUT_TEST_ENABLE
#   define BLE_TX_THROUGHPUT_TEST_ENABLE (0u)
#endif
#if((BLE_TX_THROUGHPUT_TEST_ENABLE != 0u) && (BLE_TX_THROUGHPUT_TEST_ENABLE != 1u))
#   error "BLE_TX_THROUGHPUT_TEST_ENABLE must be 0 or 1."
#endif

#if defined(DEBUG_DISABLE_WDT) || (BLE_TX_THROUGHPUT_TEST_ENABLE == 1u)
#   define GC_WATCHDOG_ENABLED (0u)
#else
#   define GC_WATCHDOG_ENABLED (1u)
#endif

#define GRAVITY_EARTH                  (9.80665f)
#define INIT_SYSTICK_PERIOD_MS         (1u) // Initial systick period during initialization.
#define SYSTICK_BLE_PAIRING_TIMEOUT_MS (1000u * 60u)
#define TEMPERATURE_UPDATE_INTERVAL_MS (1000u * 60u * 5u) // NOTE: Should be multiples of 1000ms
STATIC_ASSERT(TEMPERATURE_UPDATE_INTERVAL_MS % 1000u == 0,
              "TEMPERATURE_UPDATE_INTERVAL_MS must be a multiple of 1000ms");
#define TEMPERATURE_OVER_THRESHOLD_REPORT_INTERVAL_MS (1000u * 60u * 10u)
#define BLE_PARAM_REPORT_INTERVAL_MS                  (1000u)
#define CHECK_RING_STATUS_INTERVAL_MS                 (1000u * 1u * 4u)
#define NOTIFICATION_QUEUE_LEN                        (20u) // Max number of notifications in the queue.
#define INVALID_DOSE_SCHEDULE_REMINDER_INTERVAL_MS    (1000u * 60u * 5u)
#define DOSE_DUE_REMINDER_INTERVAL_MS                 (1000u * 60u * 5u)
#define DOSE_WINDOW_TIMEOUT_MS                        (1000u * 60u * 180u)
#define BATTERY_LEVEL_MIN_REPORT_INTERVAL_MS          (1000u * 60u * 5u)
#define INVALID_DOSE_INFO_ERR_REPORTING_INTERVAL_MS   (1000u * 60u)
#define INVALID_DOSE_INFO_BLE_REQUEST_INTERVAL_MS     (1000u * 10u)
#define DOCK_STATUS_REPORTING_INTERVAL_MS             (1000u * 60u * 5u)
#define BATTERY_STATUS_ERROR_LOG_RATE_LIMIT_MS                                                                         \
   (1000u * 60u * 5u) // Rate limit for logging unrecognized battery states to prevent log spamming

// Max time that the ring can be removed from the dock before the dock will notify the user to put the Ring back onto
// the Dock
#define RING_REMOVAL_MAX_TIME_MS                     (1000u * 60u * 5u)
#define RING_REMOVAL_NOTIFICATION_REPEAT_INTERVAL_MS (1000u * 60u * 5u)
#define MEDICATION_CAP_OFF_MAX_TIME_S                (10u * 60u)

#define MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE                                                                         \
   ((uint16_t)(DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC / (TEMPERATURE_UPDATE_INTERVAL_MS / COMMON_1K_FACTOR)))

// Event queues
#define DOSE_EVENT_QUEUE_MAX_ELEMENTS (20u) // Max number of elements that can be stored in the event queues
#define DOSE_EVENT_QUEUE_ELEMENT_SIZE (sizeof(dose_event_t))
#define DOSE_EVENT_QUEUE_SIZE         (DOSE_EVENT_QUEUE_ELEMENT_SIZE * DOSE_EVENT_QUEUE_MAX_ELEMENTS)
#define RING_ERROR_QUEUE_MAX_ELEMENTS (20u) // Max number of elements that can be stored in the event queues
#define RING_ERROR_QUEUE_ELEMENT_SIZE (sizeof(result_t))
#define RING_ERROR_QUEUE_SIZE         (RING_ERROR_QUEUE_ELEMENT_SIZE * RING_ERROR_QUEUE_MAX_ELEMENTS)

// Threshold below which the Dock will charge the ring from the Dock's battery.
#define RING_BATTERY_LOW_THRESHOLD_MV        (4000u)
#define RING_BATTERY_CHARGING_THRESHOLD_MV   (3840u)
#define RING_BATTERY_FULL_MV                 (4150u) // mV
#define RING_MAX_TIME_DIFF_THRESHOLD_SECONDS (60)    // Max time difference between the ring and dock
// NFC power saving
#define NFC_LOWPOWER_POLL_INTERVAL_MS (1000u * 20u) // Off time between low-power poll windows
#define NFC_LOWPOWER_POLL_ON_TIME_MS  (1000u)       // Duration to keep NFC on while polling for activity
#define NFC_LOWPOWER_ACTIVE_HOLD_MS   (1000u * 2u)  // Hold time after last activity before powering down

// Todo: tune this to be as low as possibe and still reliably read the medication tag UID
#define NFC_MEDICATION_UID_TIMEOUT_MS  (1000u)
#define NFC_MEDICATION_UID_POLL_MS     (20u)
#define NFC_MEDICATION_UID_MAX_RETRIES (3u)
// Require consecutive sampled absences before committing present -> not present.
#define NFC_RING_PRESENCE_FALLING_CONFIRM_SAMPLES (4u)
// Only count one "absent" sample per this interval to avoid debouncing on repeated stale reads.
#define NFC_RING_ABSENCE_DEBOUNCE_MIN_INTERVAL_MS (250u) // 500u
// Revalidation uses the same cadence as normal debounce, but poll entry primes the timer so startup transients are
// not counted immediately after wake.
#define NFC_RING_ABSENCE_REVALIDATE_MIN_INTERVAL_MS (NFC_RING_ABSENCE_DEBOUNCE_MIN_INTERVAL_MS)
// Minimum on-time while validating a previously-present ring after waking from low-power OFF.
#define NFC_RING_PRESENCE_REVALIDATE_ON_TIME_MS                                                                        \
   (NFC_RING_ABSENCE_REVALIDATE_MIN_INTERVAL_MS * (NFC_RING_PRESENCE_FALLING_CONFIRM_SAMPLES + 1u))

/**
 * Minimum time for received unix time (used for validating incoming time). This timestamp was recorded during
 * development. Any received time should have progressed from this point and the unix time value should therefore be
 * larger than this.
 */
#define UNIX_TIME_MINIMUM_VAL (1751545437u)

// Low voltage handling defines
#define POWER_CHECK_VBAT_SAMPLES          (3u)
#define POWER_CHECK_VBAT_MAX_ATTEMPTS     (9u)
#define BATTERY_STALE_DATA_RETRY_DELAY_MS (1100u)
#define ERROR_NOTIFICATION_DURATION_MS    (10000)
#define MINIMUM_BATTERY_VOLTAGE_MV        (3300u)
#define LOG_FLUSH_LOOP_DELAY_MS           (100u)
#define SHIP_MODE_ERROR_BLINKS            (4u)
#define SHIP_MODE_BLINK_DELAY_MS          (150u)
#define DELAY_BEFORE_RESET_MS             (1000u)
#define AUTO_MEASURE_OFF_IDLE_LOOPS                                                                                    \
   (5u) // Number of Control loops that auto battery measurement will remain disabled for. Tuned for the the best
        // balance between power savings and responsiveness to battery changes.
#define PMIC_RESET_DELAY_MS (5000u)

// Device primary key and BLE passkey flash addresses
#define APP_DATA_ADDR_DEVICE_PRIMARY_KEY     (0xFD000)
#define APP_DATA_FLASH_SIZE                  (0x1000)
#define APP_DATA_ADDR_DEVICE_PRIMARY_KEY_LEN (32u)
#define APP_DATA_ADDR_DEVICE_BT_PASSKEY      (0xFD020)
#define APP_DATA_ADDR_DEVICE_BT_PASSKEY_LEN  (32u)

// Temperature sensor
#define TEMP_READ_FREQ_MS (100u)

#define APP_MANAGER_RX_QUEUE_NUM_ELEMENTS (5u)
#define APP_MANAGER_TX_QUEUE_NUM_ELEMENTS (5u)

#define BASELINING_SET_NVM_MAX_RETRIES  (3u) // Retries for storage data in non-volatile memory
#define CALIBRATION_SET_NVM_MAX_RETRIES (3u) // Retries for storage data in non-volatile memory

/**********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct
{
   bool pairing_gesture_detected; /**< Gesture to put the device in BLE pairing mode */
   bool is_dose_window_active;
} flag_t;

typedef enum
{
   NFC_POWER_STATE_UNKNOWN = 0,
   NFC_POWER_STATE_CHARGER_HIGH,
   NFC_POWER_STATE_CHARGER_LOW,
   NFC_POWER_STATE_LOWPOWER_OFF,
   NFC_POWER_STATE_LOWPOWER_POLL,
   NFC_POWER_STATE_LOWPOWER_ACTIVE,
   NFC_POWER_STATE_MAX
} NFC_POWER_STATE;

typedef struct
{
   NFC_POWER_STATE nfc_state;
   uint64_t poll_start_ms;
   uint64_t next_poll_ms;
   uint64_t last_activity_ms;
   bool ring_presence_valid;
   bool last_ring_present;
   uint8_t last_good_ring_uid[NFC_TAG_MAX_UID_SIZE];
   uint8_t ring_absence_sample_count;
   uint64_t ring_absence_last_count_ms;
   bool ring_battery_full;
   bool medication_read_pending;
   bool medication_read_in_progress;
   uint8_t medication_read_retry_count;
   uint64_t medication_read_start_ms;
   uint64_t medication_next_poll_ms;
   uint8_t last_good_medication_uid[NFC_TAG_MAX_UID_SIZE];
   bool medication_uid_stale;
   status_update_t prev_ring_status;
} nfc_runtime_state_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
#if(GC_WATCHDOG_ENABLED == 1u)
static void wdt_event_handler(void);
static void wdt_init(void);
#endif
static void feed_watchdog(void);

static void button_event_handler(nrf_drv_gpiote_pin_t pin,
                                 nrf_gpiote_polarity_t action); // NOSONAR: action required by SDK

static result_t load_ble_passkey(char *passkey_text);
static result_t debug_log_init(void);

//  static void on_ble_connection_status_update(DEVICE_BLE_EVT event);
//  static void on_ble_dose_schedule_update(const dose_schedule_t *schedule);
//  static void on_ble_time_update(const uint32_t unix_time_s);
//  static void on_ble_calibration_update(const uint8_t calibration);
//  static void on_ble_device_command_update(uint32_t command);

/**
 * @brief Change the active moving-average window length.
 *
 * @param len  Desired window length (0 … MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE).
 *             Values > MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE clamp to MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE.
 *
 * Behaviour:
 *  - When shrinking the window, the sum is recomputed so that only the newest @p len samples remain in the
 * calculation.
 *  - When growing the window, existing history is retained; new slots start empty until filled by subsequent calls
 * to moving_average_add().
 *  - Setting @p len to 0 clears all history and disables averaging.
 */
static void moving_average_set_length(uint16_t len);

/**
 * @brief Fixed-window moving average with runtime-selectable window length The data buffer is allocated once at
 * build-time (MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE). At runtime you may shrink or grow the active window length with
 * @p moving_average_set_length(). If the window length is set to **0**, @p moving_average_add() becomes a pure
 * pass-through and simply echoes the incoming sample.
 *
 * @note All arithmetic is integer-only.
 *
 * @param sample New input value.
 *
 * @return uint16_t Current average (truncated toward zero).
 */
static int16_t moving_average_add(int16_t sample);

static result_t init_buttons(void);

/**
 * @brief Puts the IMU into an active state if it's currently in a dormant state. This function also resets the
 * gesture detection and significant motion flags if the IMU sleep mode changes. This function does nothing if called
 * while the IMU is already active.
 *
 * @return result_t
 */
static result_t turn_on_imu(void);

/**
 * @brief Puts the IMU into a the dormant state if it's currently in the active state. This function also resets the
 * gesture detection and significant motion flags if the IMU sleep mode changes. This function does nothing if called
 * while the IMU is already dormant.
 *
 * @return result_t
 */
static result_t turn_off_imu(void);

/**
 * @brief System timer event handler.
 *
 * This function is called when the system timer expires. It increments the systick time by the set period,
 * starts the systick timer again, and handles any potential errors.
 *
 * @param[in] p_context Unused parameter required by the SDK.
 */
static void app_timer_handler_systick(void *p_context);

/**
 * @brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static result_t application_timer_init(void);

/**
 * @brief Initializes the system tick timer.
 *
 * This function creates and starts a single-shot timer using the NRF5 SDK's app_timer module.
 * The timer is set to call the app_timer_handler_systick function every m_systick_period_ms milliseconds. This
 * allows for dynamically changing the systick period based on the current system state which is useful for power
 * management. E.g. make the systick period short when a lot of active processing or monitoring is required and long
 * in states where less responsiveness is required to conserve battery life.
 *
 * @return A result_t indicating success or failure.
 * @retval RESULT_OK If the timer was successfully initialized and started.
 * @retval GC_ERROR_SYSTICK_INIT If there was an error initializing or starting the timer.
 */
static result_t systick_timer_init(void);

// Control related functions
static void implement_control_loop(void);

/**
 * @brief Unbonded state is equivalent to the storage state where the device has not been used before and
 * should be in the lowest possible power state. Nothing happens in the state.
 */
static void handle_sm_unbonded_state(const battery_status_t *battery_status);

/**
 * @brief Idle state: The device is not connected via BLE and ready to perform BLE operations
 * (Pairing/bonding or connecting to bonded device). Nothing happens in the state other than BLE
 * operations.
 */
static void handle_sm_idle_state(void);

static void handle_sm_baselining_state(bool is_ring_present,
                                       uint8_t *medication_uid,
                                       uint8_t medication_uid_size,
                                       bool medication_uid_stale);
static void report_baselining_feedback(const baselining_feedback_t *feedback);

/**
 * @brief BLE pairing state: Handles BLE pairing / bonding related tasks. The state will transition based
 * on the charger connected status or on the BONDING_STATE being either BONDED or UNBONDED.
 */
static void handle_sm_pairing_state(void);
static void
   handle_sm_calibration_state(bool is_ring_present, uint32_t rx_calibration_weight_mg, bool is_weight_present);
static void handle_sm_active_state(void);
static void handle_sm_invalid_dose_info_state(void);

static void fstorage_evt_handler(nrf_fstorage_evt_t *p_evt);
static void idle_state_handle(void);

/**
 * @brief Checks BLE bonding status and manages advertising accordingly.
 *
 * - This function queries the BLE state from @ref BLE Control and updates @ref m_ble_status accordingly.
 *
 * - If the device is bonded and not currently connected, starts advertising with a whitelist containing bonded device
 * credentials.
 *
 * - It updates the state machine bonding state and ensures advertising is only started once per session.
 *
 * - Errors in querying BLE status or starting advertising are logged for debugging.
 *
 * Typical usage: Call at the start of the main control loop to ensure the @ref m_ble_status is up to date.
 */
static void update_ble_state(void);

/**
 * @brief Periodically reads dock temperature via the IMU, checks thresholds, and reports to the data manager.
 *
 * Behavior:
 *
 * - Every TEMPERATURE_UPDATE_INTERVAL_MS, ensures the IMU is active, then reads temperature.
 *
 * - Valid readings are fed into a moving average; if a valid dose schedule is present and the average exceeds the
 *   threshold, an error log is emitted.
 *
 * - For valid readings, the latest temperature is timestamped with RTC time and enqueued to the data manager.
 *
 * - Internal flags ensure averaging and reporting are performed once per valid reading.
 */
static void handle_temperature_reading(void);

/**
 * @brief Update HMI notification events based on the current battery state.
 *
 * This helper sets or clears battery-related notification events according to the battery state (good, low, charging,
 * charge complete). If an unrecognized state is detected, it rate-limits error logging and asserts a generic error
 * notification while clearing other battery indicators.
 *
 * @param[in] battery_status Snapshot of the current battery status/state.
 */
static void handle_battery_hmi_notifications(battery_status_t battery_status);

static const char *nfc_power_state_to_str(NFC_POWER_STATE state);

/**
 * @brief Handle battery status sampling, safety checks, and UI/data reporting.
 *
 * This routine reads the current battery status from the battery interface, updates system state-machine inputs
 * (charger connected), enforces an undervoltage safety action (enter ship mode), handles PMIC reset detection with a
 * delayed reboot timer, and rate-limits battery level reporting and HMI notifications.
 *
 * Reporting behavior:
 *
 * - Battery level is rounded down to the nearest @c BATTERY_LEVEL_INCREMENT.
 *
 * - Reporting is rate-limited by @c BATTERY_LEVEL_MIN_REPORT_INTERVAL_MS.
 *
 * - When reporting, the level is enqueued to the dock data manager with a timestamp.
 *
 * - HMI notifications are updated based on the current battery state.
 *
 * @param[out] battery_status Pointer to the caller-owned status structure to receive
 *                            the latest battery status snapshot.
 */
static void handle_battery(battery_status_t *battery_status);

static void enter_ship_mode(void);

static inline bool is_ble_valid(void);

/**
 * @brief Parses the dose schedule from the given data
 *
 * This function parses the dose schedule from the given data and populates the dose_schedule_t structure.
 * It performs basic length checks before validating and returning the resulting structure.
 *
 * @param[in] p_data Pointer to the data containing the dose schedule
 * @param[in] length Length of the data
 * @param[out] p_sched Pointer to the dose_schedule_t structure to populate
 *
 * @return Result of the operation
 */
static result_t parse_dose_schedule(const uint8_t *p_data, uint16_t length, dose_schedule_t *p_sched);

/**
 * @brief Pushes the current dock charge status to the data manager for reporting, upon battery state change only.
 *
 * @param[in] battery_status Pointer to the current battery status
 *
 * @return Result of the operation
 */
static result_t push_dock_charge_status(BATTERY_STATE battery_status);

/***********************************************************************************************************************
 * Global function declarations
 **********************************************************************************************************************/
result_t debug_uart_tx_handler(const uint8_t *p_data, size_t length);
void debug_uart_rx_handler(const uint8_t *p_data, size_t length);
result_t debug_data_manager_tx_handler(const uint8_t *p_data, size_t length);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
// I2C setup
static nrf_drv_twi_t m_twi_instance_0 = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_0);
static nrf_drv_twi_t m_twi_instance_1 = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_1);
const nrf_drv_twi_config_t m_i2c_config_1 = {.scl = I2C_SCL_PIN,
                                             .sda = I2C_SDA_PIN,
                                             .frequency = NRF_DRV_TWI_FREQ_400K,
                                             .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
                                             .clear_bus_init = false};

const nrf_drv_twi_config_t m_i2c_config_0 = {.scl = I2C_SCL_B_PIN,
                                             .sda = I2C_SDA_B_PIN,
                                             .frequency = NRF_DRV_TWI_FREQ_400K,
                                             .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
                                             .clear_bus_init = false};
static i2c_driver_t m_i2c_driver;
static i2c_driver_t m_i2c_driver_b;

NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) = {
   .evt_handler = fstorage_evt_handler,
   .start_addr = APP_DATA_ADDR_DEVICE_PRIMARY_KEY,
   .end_addr = APP_DATA_ADDR_DEVICE_PRIMARY_KEY + APP_DATA_FLASH_SIZE,
}; // Storage for passkey. Addresses need to correspond to `APP_DATA` section in linker script

// Application timers
APP_TIMER_DEF(m_systick_timer);
APP_TIMER_DEF(m_timer_pmic_reset_expire);

// Application unit instances
static system_time_t m_systick = {0};
static uint16_t m_systick_period_ms = INIT_SYSTICK_PERIOD_MS;
static imu_t m_imu = {0};

static statemachine_t m_system_sm = {0};
static system_fsm_inputs_t m_system_sm_inputs = {0};
static system_fsm_outputs_t m_system_sm_outputs = {.changed = false,
                                                   .current_state_loop_period_ms = SLEEP_PERIOD_STORAGE_MS,
                                                   .current_state_systick_period_ms = SYSTICK_PERIOD_STORAGE_MS};
static statemachine_t m_baseline_sm = {0};
static baselining_fsm_inputs_t m_baseline_sm_inputs = {0};
static baselining_fsm_outputs_t m_baseline_sm_outputs = {.changed = false};

static statemachine_t m_calibrate_sm = {0};
static calibration_fsm_inputs_t m_calibrate_sm_inputs = {0};
static calibration_fsm_outputs_t m_calibrate_sm_outputs = {.changed = false};

static queue_t m_ble_notification_queue = {0};

// Other
static ble_control_t m_ble_control = {0};
static ble_control_status_t m_ble_status = {0};
static uint8_t m_ble_notification_queue_data[NOTIFICATION_QUEUE_LEN * sizeof(result_t)];
#if(GC_WATCHDOG_ENABLED == 1u)
static nrf_drv_wdt_channel_id m_channel_id; // Watchdog timer channel
#endif
static battery_manager_t m_battery = {0};
static npm1300_driver_t m_pmic = {0}; // Battery management IC driver
static volatile flag_t m_flag;        // System flags
// static ble_control_evt_handlers_t m_ble_evt_handlers = {0};
static rtc_system_time_t m_rtc;
static dose_scheduler_t m_dose_scheduler;
static ring_manager_t m_ring;
static nfc_driver_t m_nfc;
static app_manager_t m_app_manager;
static message_protocol_t m_msg_prot_ble; // App manager message protocol instance
static message_protocol_t m_msg_prot_nfc; // NFC message protocol instance
static queue_t m_queue_ble_rx;
static queue_t m_queue_app_manager_rx;
static queue_t m_queue_app_manager_tx;
static queue_interface_t *m_queue_app_manger_tx_ifx = &m_queue_app_manager_tx.interface;
static uint8_t m_buffer_ble_rx[BLE_RX_PACKET_QUEUE_SIZE] = {0};
static uint8_t m_buffer_app_manager_rx[APP_MANAGER_RX_QUEUE_NUM_ELEMENTS
                                       * (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)]
   = {0};
static uint8_t m_buffer_app_manager_tx[APP_MANAGER_TX_QUEUE_NUM_ELEMENTS
                                       * (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)]
   = {0};
static weight_sensor_t m_weight_sensor = {0};

// Moving average for temperature measurements
static int16_t m_temperature_ma_buffer[MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE];
static int32_t m_temperature_ma_sum = 0;   // running sum of samples
static uint16_t m_temperature_ma_idx = 0u; // ring-buffer write index
static uint16_t m_temperature_ma_cnt = 0u; // #samples collected (<= MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE)
static uint16_t m_temperature_ma_len = MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE;

static notification_module_t m_hmi;

// Button
static button_driver_t m_button;
static uint8_t m_buttons_buf[BUTTONS_COUNT] = {BUTTON_PIN};
static button_status_t m_button_status[BUTTONS_COUNT] = {0};

static dock_data_manager_t m_dock_data_manager;

// SPI setup for ADC (ADS1220)
nrf_drv_spi_t m_ads_spi_instance = NRF_DRV_SPI_INSTANCE(SPI_ADC_INSTANCE);
static spi_driver_t m_ads_spi_driver;
static const nrf_drv_spi_config_t m_ads_spi_config = {
   .sck_pin = SPI_ADC_SCK_PIN,
   .mosi_pin = SPI_ADC_MOSI_PIN,
   .miso_pin = SPI_ADC_MISO_PIN,
   .ss_pin = SPI_ADC_CS_PIN,
   .orc = 0x00,
   .frequency = NRF_DRV_SPI_FREQ_4M,
   .mode = NRF_DRV_SPI_MODE_1,
   .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,
};

// Temperature sensor
static sts30_dis_temp_sensor_driver_t m_temp_sensor;

// Dose size detection
static dose_size_detection_t m_dsd;

// For detecting changes in battery charge status to trigger events like data reporting
static BATTERY_STATE m_prev_dock_charge_status = BATTERY_STATE_MAX;

// ============================ DEBUG/COMMAND UART ============================

#ifndef DEBUG_DISABLE_UART
NRF_LIBUARTE_ASYNC_DEFINE(debug_uart_instance,
                          DEBUG_UARTE_INSTANCE,
                          TIMER_INSTANCE_DEBUG_UARTE,
                          DEBUG_UARTE_RTC1_IDX,
                          NRF_LIBUARTE_PERIPHERAL_NOT_USED,
                          DEBUG_UARTE_RX_BUF_SIZE,
                          DEBUG_UARTE_RX_BUF_COUNT);
#endif

nrf_libuarte_async_config_t debug_uart_config = {.tx_pin = DEBUG_UARTE_TX_PIN,
                                                 .rx_pin = DEBUG_UARTE_RX_PIN,
                                                 .baudrate = NRF_UARTE_BAUDRATE_1000000,
                                                 .parity = NRF_UARTE_PARITY_EXCLUDED,
                                                 .hwfc = NRF_UARTE_HWFC_DISABLED,
                                                 .timeout_us = 100,
                                                 .int_prio = DEBUG_UARTE_IRQ_PRIORITY};

uint8_t uart_tx_queue_data[UART_TX_SW_QUEUE_LEN * sizeof(uart_tx_buffer_t)];
static queue_t m_uart_tx_queue;
uint8_t debug_rx_queue_data[DEBUG_RX_SW_QUEUE_LEN * sizeof(debug_msg_t)];
static queue_t m_debug_rx_queue;
static uart_driver_t m_debug_uart_driver;

#ifndef DEBUG_DISABLE_UART
static uart_driver_interface_t *m_debug_uart_driver_interface = NULL;
#endif

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/
result_t debug_uart_tx_handler(const uint8_t *p_data, size_t length)
{
   (void)(&(m_debug_uart_driver.interface))->transmit(&(m_debug_uart_driver.interface), p_data, length);
   return RESULT_OK;
}

result_t debug_data_manager_tx_handler(const uint8_t *p_data, size_t length)
{
   raw_debug_log_t log = {0};
   result_t result = debug_map_unencoded_log(&log, p_data, length);
   (void)result;

   // m_dock_data_manager.interface.enqueue(
   //    &m_dock_data_manager.interface, DATA_ID_DOCK_DEBUG_LOG, &log, sizeof(raw_debug_log_t), 1u);
   return RESULT_OK;
}

void debug_uart_rx_handler(const uint8_t *p_data, size_t length)
{
   (void)debug_on_data_rx(p_data, length);
}

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/
static void button_event_handler(nrf_drv_gpiote_pin_t pin,
                                 nrf_gpiote_polarity_t action) // NOSONAR: action required by SDK
{
   result_t result = RESULT_OK;
   UNUSED_PARAMETER(action);

   DEBUG_TRACE("Button press event detected");

   for(uint8_t button_number = 0u; button_number < BUTTONS_COUNT; button_number++)
   {
      // Todo: This needs to change. Cannot access module private member _buttons directly.
      if(pin == m_button._buttons[button_number].pin)
      {
         result = m_button.interface.handle_button_event(&m_button.interface, button_number);

         if(IS_ERR(result))
         {
            DEBUG_ERROR("Failed to handle button event");
         }
      }
   }
}

static result_t load_ble_passkey(char *passkey_text)
{
   result_t result = RESULT_OK;
   // Initialize flash storage, read and decrypt bluetooth passkey
   ret_code_t err_code = nrf_fstorage_init(&fstorage, &nrf_fstorage_sd, NULL);
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_FLASH_STORAGE_INIT);

   uint8_t device_root_key[APP_DATA_ADDR_DEVICE_PRIMARY_KEY_LEN] = {0};
   if(IS_OK(result))
   {
      err_code = nrf_fstorage_read(
         &fstorage, APP_DATA_ADDR_DEVICE_PRIMARY_KEY, &device_root_key, APP_DATA_ADDR_DEVICE_PRIMARY_KEY_LEN);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_FLASH_STORAGE_INIT);
   }

   uint8_t encrypted_passkey[APP_DATA_ADDR_DEVICE_BT_PASSKEY_LEN] = {0};
   if(IS_OK(result))
   {
      err_code = nrf_fstorage_read(
         &fstorage, APP_DATA_ADDR_DEVICE_BT_PASSKEY, &encrypted_passkey, APP_DATA_ADDR_DEVICE_BT_PASSKEY_LEN);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_FLASH_STORAGE_INIT);
   }

   if(IS_OK(result))
   {
      err_code = nrf_crypto_init();
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_CRYPTO_INIT);
   }

   uint8_t init_vector[APP_DATA_ADDR_DEVICE_PRIMARY_KEY_LEN];
   size_t len_out = APP_DATA_ADDR_DEVICE_BT_PASSKEY_LEN;
   static nrf_crypto_aes_context_t cbc_decrypt_ctx;

   if(IS_OK(result))
   {
      // Initialize AES decryption context
      err_code = nrf_crypto_aes_init(&cbc_decrypt_ctx, &g_nrf_crypto_aes_cbc_256_pad_pkcs7_info, NRF_CRYPTO_DECRYPT);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_PASSKEY_DECRYPT);
   }

   if(IS_OK(result))
   {
      // Set decryption key
      err_code = nrf_crypto_aes_key_set(&cbc_decrypt_ctx, device_root_key);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_PASSKEY_DECRYPT);
   }

   if(IS_OK(result))
   {
      // Set zero initialization vector
      memset(init_vector, 0, sizeof(init_vector));
      err_code = nrf_crypto_aes_iv_set(&cbc_decrypt_ctx, init_vector);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_PASSKEY_DECRYPT);
   }

   if(IS_OK(result))
   {
      // Decrypt passkey
      memset(passkey_text, 0, BLE_PASSKEY_LENGTH);
      len_out = 16u;
      err_code = nrf_crypto_aes_finalize(
         &cbc_decrypt_ctx, (uint8_t *)encrypted_passkey, len_out, (uint8_t *)passkey_text, &len_out);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_PASSKEY_DECRYPT);
   }

   if(IS_OK(result))
   {
      passkey_text[BLE_PASSKEY_LENGTH] = 0x00; // Ensure null-termination
   }

   return result;
}

static void fstorage_evt_handler(nrf_fstorage_evt_t *p_evt) // NOSONAR *p_evt required by SDK library
{
   if(p_evt->result != NRF_SUCCESS)
   {
      DEBUG_ERROR("Error while executing an fstorage operation.");
   }
   return;
}

/**
 * @todo The setup required below other than calling button_driver_init() should be moved into the button driver. Task
 * bookmarked for V1.1 of firmware
 */
static result_t init_buttons(void)
{
   result_t result
      = button_driver_init(&m_button,
                           BUTTONS_COUNT,
                           m_buttons_buf,
                           0,
                           NULL,
                           NRF_GPIO_PIN_NOPULL); // Todo: remove NRF_GPIO_PIN_NOPULL. See comment in button_driver.h
   ret_code_t err_code = NRF_SUCCESS;

   if(IS_OK(result))
   {
      memset(m_button_status, 0x00, BUTTONS_COUNT * sizeof(button_status_t));
   }

   if(IS_OK(result) && !nrf_drv_gpiote_is_init())
   {
      err_code = nrf_drv_gpiote_init();
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_BUTTON_INIT);
      if(NRF_SUCCESS != err_code)
      {
         DEBUG_ERROR("Failed to initialize GPIOTE. NRF_ERROR: %d", err_code);
      }
   }

   if(IS_OK(result))
   {
      for(uint8_t button_number = 0u; (button_number < BUTTONS_COUNT); button_number++)
      {
         // Configure GPIO interrupts
         nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
         config.pull = NRF_GPIO_PIN_NOPULL;

         err_code = nrf_drv_gpiote_in_init(m_button._buttons[button_number].pin, &config, button_event_handler);
         UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_BUTTON_INIT);
         if(NRF_SUCCESS != err_code)
         {
            DEBUG_ERROR(
               "Failed to initialize button interrupt. button_number: %u, NRF_ERROR: %d", button_number, err_code);
         }

         if(IS_OK(result))
         {
            nrf_drv_gpiote_in_event_enable(m_button._buttons[button_number].pin, true);
         }
      }
   }

   return result;
}

static result_t turn_on_imu(void)
{
   IMU_STATE imu_state;

   result_t result = m_imu.interface.get_state(&m_imu.interface, &imu_state);

   // Check if the hardware is off. If so, turn it on.
   if((IMU_STATE_ACTIVE != imu_state) && IS_OK(result))
   {
      result = m_imu.interface.set_state(&m_imu.interface, IMU_STATE_ACTIVE);

      // Reset interrupts
      imu_interrupt_map_t imu_interrupt_map = {false, false, false, false, false};
      m_imu.interface.get_interrupts(&m_imu.interface, &imu_interrupt_map);
   }

   return result;
}

static result_t turn_off_imu(void)
{
   IMU_STATE imu_state;

   result_t result = m_imu.interface.get_state(&m_imu.interface, &imu_state);

   // Check if the hardware is off. If not, turn it off.
   if((IMU_STATE_DORMANT != imu_state) && IS_OK(result))
   {
      // Reset interrupts
      imu_interrupt_map_t imu_interrupt_map = {false, false, false, false, false};
      m_imu.interface.get_interrupts(&m_imu.interface, &imu_interrupt_map);

      result = m_imu.interface.set_state(&m_imu.interface, IMU_STATE_DORMANT);
   }

   return result;
}

static result_t systick_timer_init(void)
{
   result_t result = RESULT_OK;
   ret_code_t err_code = app_timer_create(&m_systick_timer, APP_TIMER_MODE_SINGLE_SHOT, app_timer_handler_systick);
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_SYSTICK_INIT);

   if(IS_OK(result))
   {
      m_systick_period_ms = INIT_SYSTICK_PERIOD_MS;

      err_code = app_timer_start(m_systick_timer, APP_TIMER_TICKS(m_systick_period_ms), NULL);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_SYSTICK_INIT);
   }

   return result;
}

static void app_timer_handler_systick(void *p_context) // NOSONAR - p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   (void)m_systick.interface.inc_time_by_set_val_ms(&m_systick.interface, m_systick_period_ms);

   ret_code_t error = app_timer_start(m_systick_timer, APP_TIMER_TICKS(m_systick_period_ms), NULL);

   if(error != NRF_SUCCESS)
   {
      DEBUG_ERROR("Systick timer failed to start. NRF_ERROR: %d", error);
      nrf_delay_ms(1);
   }

   // Reset device if the systick timer failed.
   APP_ERROR_CHECK(error);
}

static result_t debug_log_init(void)
{
   // ************************************************************************
   // Initialize debug UART software buffers
   result_t result = queue_init(
      &m_uart_tx_queue, uart_tx_queue_data, UART_TX_SW_QUEUE_LEN * sizeof(uart_tx_buffer_t), sizeof(uart_tx_buffer_t));

   if(IS_OK(result))
   {
      // Initialize debug RX queue
      result = queue_init(
         &m_debug_rx_queue, debug_rx_queue_data, DEBUG_RX_SW_QUEUE_LEN * sizeof(debug_msg_t), sizeof(debug_msg_t));
   }

   if(IS_OK(result))
   {
      // Initialize debug and command unit
      result = debug_init(&(m_debug_rx_queue.interface), &(m_systick.interface), NULL);
   }

#ifndef DEBUG_DISABLE_UART
   if(IS_OK(result))
   {
      // Initialize debug UART
      result = uart_driver_init(&m_debug_uart_driver,
                                &debug_uart_instance,
                                &debug_uart_config,
                                debug_uart_rx_handler,
                                &(m_uart_tx_queue.interface));
   }

   if(IS_OK(result))
   {
      m_debug_uart_driver_interface = &(m_debug_uart_driver.interface);

      endpoint_t uart_debug_endpoint;
      uart_debug_endpoint.id = DEBUG_ENDPOINT_IDX_UART;
      uart_debug_endpoint.handler = debug_uart_tx_handler;
      uart_debug_endpoint.apply_encoding = true;
      uart_debug_endpoint.disable_auto_routing = false;
      uart_debug_endpoint.permit_bulk_data = true;
      IF_OK_RUN_AND_UPDATE(result, debug_register_endpoint(&uart_debug_endpoint));
   }
#endif

   if(IS_OK(result))
   {
      endpoint_t data_manager_endpoint;
      data_manager_endpoint.id = DEBUG_ENDPOINT_IDX_DATA_MANAGER;
      data_manager_endpoint.handler = debug_data_manager_tx_handler;
      data_manager_endpoint.apply_encoding = false;
      data_manager_endpoint.disable_auto_routing = false;
      data_manager_endpoint.permit_bulk_data = true;
      IF_OK_RUN_AND_UPDATE(result, debug_register_endpoint(&data_manager_endpoint));
      // This ensures that only ERROR level logs are sent to the dock data manager.
      IF_OK_RUN_AND_UPDATE(
         result, debug_set_all_unit_debug_level_for_endpoint(DEBUG_LEVEL_ERROR, DEBUG_ENDPOINT_IDX_DATA_MANAGER));
   }

   return result;
}

static result_t power_management_init(void)
{
   result_t result = RESULT_OK;
   ret_code_t err_code = nrf_pwr_mgmt_init();
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_POWER_MANAGEMENT_INIT);
   return result;
}

/** @brief Function for handling shutdown events.
 */
static bool shutdown_event_handler(nrf_pwr_mgmt_evt_t event)
{
   // If a DFU is pending, don't let anything delay the reset; reset immediately
   if(NRF_PWR_MGMT_EVT_PREPARE_DFU == event)
   {
      NVIC_SystemReset();
   }

   DEBUG_WARNING("Power management resetting....");
   return true;
}

/**@brief Register application shutdown handler with priority 0.
 */
NRF_PWR_MGMT_HANDLER_REGISTER(shutdown_event_handler, 0);

/** @brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
   nrf_pwr_mgmt_run();
}

static void timer_pmic_reset_expire_handler(void *p_context) // NOSONAR *p_context is required by the app_timer library
{
   UNUSED_PARAMETER(p_context);
   app_timer_stop(m_timer_pmic_reset_expire);
   NVIC_SystemReset();
}

static result_t pmic_reset_timer_init(void)
{
   result_t result = RESULT_OK;
   uint32_t err_code
      = app_timer_create(&m_timer_pmic_reset_expire, APP_TIMER_MODE_SINGLE_SHOT, timer_pmic_reset_expire_handler);
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_PMIC_RESET_TIMER_INIT);
   return result;
}

static result_t application_timer_init(void)
{
   result_t result = RESULT_OK;
   ret_code_t err_code = app_timer_init();
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_APPLICATION_TIMER_INIT);
   return result;
}

#if(GC_WATCHDOG_ENABLED == 1u)
static void wdt_event_handler(void)
{
   DEBUG_CRITICAL("Watchdog timer expired. Resetting device...");
   // @NOTE: The max amount of time we can spend in WDT interrupt is two cycles of 32768[Hz] clock - after that,
   // reset occurs
}

static void wdt_init(void)
{
   // The details of the WDT configuration is set in sdk_config.h. Time is set by WDT_CONFIG_RELOAD_VALUE
   nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;

   ret_code_t err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
   APP_ERROR_CHECK(err_code);

   err_code = nrf_drv_wdt_channel_alloc(&m_channel_id);
   APP_ERROR_CHECK(err_code);

   nrf_drv_wdt_enable();
}
#endif

static void feed_watchdog(void)
{
#if(GC_WATCHDOG_ENABLED == 1u)
   nrf_drv_wdt_channel_feed(m_channel_id);
#endif
}

static void moving_average_set_length(uint16_t len)
{
   // Clamp to legal range
   if(len > MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE)
   {
      len = MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE;
   }

   // Handle special case: disable averaging
   if(0 == len)
   {
      m_temperature_ma_len = 0u;
      m_temperature_ma_cnt = 0u;
      m_temperature_ma_sum = 0;
      m_temperature_ma_idx = 0u;
   }
   else if(len >= m_temperature_ma_cnt)
   {
      // If window grows, no immediate action is needed
      m_temperature_ma_len = len;
   }
   else if(len < m_temperature_ma_cnt)
   {
      // Window shrinks: recompute sum of newest <len> samples
      /* window is shrinking */
      uint16_t prev_window = m_temperature_ma_len;
      int32_t new_sum = 0;
      uint16_t oldest_pos = 0u; // will hold index of oldest kept

      for(uint16_t idx = 0u; idx < len; idx++)
      {
         int16_t pos = (int16_t)(m_temperature_ma_idx - 1u - idx);
         if(pos < 0)
         {
            pos += (int16_t)prev_window;
         }

         new_sum += m_temperature_ma_buffer[(uint16_t)pos];

         if(idx == len - 1) // last iteration → oldest sample
         {
            oldest_pos = (uint16_t)pos;
         }
      }

      m_temperature_ma_sum = new_sum;
      m_temperature_ma_cnt = len;
      m_temperature_ma_len = len;
      m_temperature_ma_idx = oldest_pos; // point to next slot to overwrite
   }
   else if(m_temperature_ma_idx >= m_temperature_ma_len)
   {
      // If the new window is smaller than the previous index, reset pointer
      m_temperature_ma_idx = 0u;
   }
}

static int16_t moving_average_add(int16_t sample)
{
   // Pass-through mode (guard-clause)
   if(0 == m_temperature_ma_len)
   {
      return sample;
   }

   // Subtract outgoing value once buffer is full
   if(m_temperature_ma_cnt == m_temperature_ma_len)
   {
      m_temperature_ma_sum -= m_temperature_ma_buffer[m_temperature_ma_idx];
   }
   else
   {
      m_temperature_ma_cnt++; // still filling
   }

   // Insert new sample
   m_temperature_ma_buffer[m_temperature_ma_idx] = sample;
   m_temperature_ma_sum += sample;

   // Advance circular index
   m_temperature_ma_idx++;
   if(m_temperature_ma_idx >= m_temperature_ma_len) // wrap inside active window
   {
      m_temperature_ma_idx = 0u;
   }

   return (int16_t)(m_temperature_ma_sum / m_temperature_ma_cnt);
}

static void handle_sm_unbonded_state(const battery_status_t *battery_status)
{
   if(!battery_status->charger_connected)
   {
      DEBUG_INFO("Device is Unbonded and charger not connected");
      DEBUG_INFO("ENTERING SHIP MODE.");

      enter_ship_mode();
   }
}

static void handle_sm_idle_state(void)
{
   result_t result = RESULT_OK;

   // Check for state first time running
   if(m_system_sm_outputs.changed)
   {
      // Clear Dose due event if active
      result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_DOSAGE_DOSE_DUE);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to clear notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
}

/**
 * @note The PPI_AD_START_BASELINING RQ messages from the App to start and stop baselining are handled in @ref
 * process_incoming_app_messages().
 *
 * @note m_baseline_sm_inputs are only set during normal operation in @ref process_incoming_app_messages() and in @ref
 * handle_sm_baselining_state().
 *
 * @note The baselinining state machine should always be left in either the BASELINING_STATE_COMPLETE or
 * BASELINING_STATE_ERROR state. Starting a new baselining process should always transition out of one of the previous
 * two states into BASELINING_STATE_WAIT_FOR_RING_REMOVAL state.
 *
 */
static void handle_sm_baselining_state(bool is_ring_present,
                                       uint8_t *medication_uid,
                                       uint8_t medication_uid_size,
                                       bool medication_uid_stale)
{
   if(NULL == medication_uid)
   {
      DEBUG_ERROR("NULL pointer received.");
      return;
   }
   if(medication_uid_size != NFC_TAG_MAX_UID_SIZE)
   {
      DEBUG_ERROR("Invalid medication UID size.");
      return;
   }

   result_t result = RESULT_OK;
   mp_packet_payload_t tx_msg = {0};
   MOTION_STATE motion_state = MOTION_STATE_UNSTABLE;
   static uint8_t new_medication_nfc_id[NFC_TAG_MAX_UID_SIZE] = {0}; /**< NFC ID of the medication. */
   static uint8_t med_uid_storage_retries_left = 0u;
   static uint8_t weight_calibration_storage_retries_left = 0u;
   static bool is_med_uid_storage_successful = false;
   static bool is_weight_calibration_storage_successful = false;
   static int32_t dock_weight_zero_offset = 0;
   static uint32_t dock_ring_full_med_weight = 0u;
   static const uint8_t invalid_medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};

   // Ensure there is an active BLE connection with the App, otherwise stop the process
   bool is_ble_active = is_ble_valid();

   if(!is_ble_active)
   {
      // Stop calibration procedure
      m_baseline_sm_inputs.start_baselining = false;
      m_baseline_sm_inputs.is_baselining_active = false;

      // Cancel any in-progress DSD-only baselining attempt when the overall baselining flow is stopped.
      result_t abort_result = m_dsd.interface.abort_baselining(&m_dsd.interface);
      ON_ERR_DEBUG_ERROR(abort_result,
                         "Failed to abort DSD baselining. Unit %d, Error: %d",
                         GET_ERR_UNIT(abort_result),
                         GET_ERR_CODE(abort_result));
   }

   bool is_stable = false;
   if(is_ble_active)
   {
      result = m_weight_sensor.interface.get_is_stable(&m_weight_sensor.interface, &is_stable);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to check if weight stable. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   weight_data_t weight_data = {0};
   bool _is_weight_data_stale = false;
   if(IS_OK(result) && is_ble_active)
   {
      if(is_stable)
      {
         motion_state = MOTION_STATE_STABLE;
      }
      else
      {
         motion_state = MOTION_STATE_UNSTABLE;
      }

      result
         = m_weight_sensor.interface.read_weight_data(&m_weight_sensor.interface, &weight_data, &_is_weight_data_stale);
   }

   // Update state machine inputs
   m_baseline_sm_inputs.is_ring_present = is_ring_present;

   // Update state machine
   m_baseline_sm_outputs.changed = false; // Clear output before updating state machine
   m_baseline_sm.update_statemachine(&m_baseline_sm, &m_baseline_sm_inputs, &m_baseline_sm_outputs);

   // DSD baselining is only valid during the SET_RING_DOCK_WEIGHT step.
   // Abort any ongoing attempt while outside that step (e.g., ring removed, cancel, timeout, backend failure).
   if(BASELINING_STATE_SET_RING_DOCK_WEIGHT != m_baseline_sm.current_state)
   {
      result_t abort_result = m_dsd.interface.abort_baselining(&m_dsd.interface);
      ON_ERR_DEBUG_ERROR(abort_result,
                         "Failed to abort DSD baselining. Unit %d, Error: %d",
                         GET_ERR_UNIT(abort_result),
                         GET_ERR_CODE(abort_result));
   }

   // Update feedback report
   baselining_feedback_t feedback = {.avg_weight_mg = weight_data.weight_mg,
                                     .current_state = (uint8_t)m_baseline_sm.current_state,
                                     .is_ring_present = m_baseline_sm_inputs.is_ring_present,
                                     .medication_nfc_id = {0},
                                     .motion_state = (uint8_t)motion_state,
                                     .std_dev = weight_data.stddev_mg};
   memcpy(feedback.medication_nfc_id, new_medication_nfc_id, sizeof(feedback.medication_nfc_id));

   switch(m_baseline_sm.current_state)
   {
      case BASELINING_STATE_WAIT_FOR_RING_REMOVAL:
         if(m_baseline_sm_outputs.changed)
         {
            report_baselining_feedback(&feedback);
         }
         break;
      case BASELINING_STATE_SET_DOCK_WEIGHT:
         if(m_baseline_sm_outputs.changed)
         {
            report_baselining_feedback(&feedback);
         }

         if(is_stable && !is_ring_present)
         {
            bool is_tare_successful = false;
            result = m_weight_sensor.interface.try_tare_if_stable(
               &m_weight_sensor.interface, is_ring_present, &is_tare_successful);
            ON_ERR_DEBUG_ERROR(
               result, "Failed to tare weight sensor. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

            if(is_tare_successful && IS_OK(result))
            {
               DEBUG_INFO("Tare successful. Zero offset set.");
               result = m_weight_sensor.interface.get_zero_offset(&m_weight_sensor.interface, &dock_weight_zero_offset);
               ON_ERR_DEBUG_ERROR(
                  result, "Failed to get zero offset. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
            }

            if(IS_OK(result) && is_tare_successful)
            {
               // Dock baseline successfully captured.
               m_baseline_sm_inputs.is_empty_dock_weight_set = true;
            }
         }
         break;
      case BASELINING_STATE_WAIT_FOR_RING:
         if(m_baseline_sm_outputs.changed)
         {
            report_baselining_feedback(&feedback);
         }

         // Wait for new medication UID.
         if(is_ring_present && !medication_uid_stale
            && (0 != memcmp(medication_uid, invalid_medication_uid, medication_uid_size)))
         {
            //  Lock in new medication ID
            memcpy(new_medication_nfc_id, medication_uid, medication_uid_size);
            // Update state machine inputs
            m_baseline_sm_inputs.is_ring_present = true;
            m_baseline_sm_inputs.is_medication_uid_avail = true;
         }
         break;
      case BASELINING_STATE_SET_RING_DOCK_WEIGHT:
         if(m_baseline_sm_outputs.changed)
         {
            report_baselining_feedback(&feedback);
         }

         if(is_stable && is_ring_present)
         {
            bool is_baseline_successful = false;
            result = m_dsd.interface.try_baseline_if_stable(&m_dsd.interface, is_ring_present, &is_baseline_successful);
            ON_ERR_DEBUG_ERROR(
               result, "Failed to establish baseline. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

            if(is_baseline_successful && IS_OK(result))
            {
               DEBUG_INFO("Full dock, ring, medication baseline setting successful.");
               result = m_dsd.interface.get_full_assembly_weight(&m_dsd.interface, &dock_ring_full_med_weight);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to get full assembly weight. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
            }

            if(IS_OK(result) && is_baseline_successful)
            {
               // Full assembly weight successfully captured.
               m_baseline_sm_inputs.is_ring_dock_full_med_weight_set = true;
            }
         }
         break;
      case BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION:
         if(m_baseline_sm_outputs.changed)
         {
            // First time running this state
            // Start with feedback update to ensure the newest state is reported
            report_baselining_feedback(&feedback);
            // Request backend validation result from App.
            tx_msg.type = (uint8_t)PPI_TYPE_RQ;
            tx_msg.ppi = (uint8_t)PPI_AD_VALIDATE_MED;
            result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
            ON_ERR_DEBUG_ERROR(
               result, "Failed to enqueue msg. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         }
         /**
          * @note @ref m_baseline_sm_inputs.backend_validation_status and @ref
          * backend_validation_status.dose_schedule_status are set in @ref process_incoming_app_messages()
          */
         break;
      case BASELINING_STATE_SET_LOCAL_MED_UID:
         // This state ensures data gets set in NVM
         if(m_baseline_sm_outputs.changed)
         {
            med_uid_storage_retries_left = BASELINING_SET_NVM_MAX_RETRIES;
            weight_calibration_storage_retries_left = BASELINING_SET_NVM_MAX_RETRIES;
            is_med_uid_storage_successful = false;
            is_weight_calibration_storage_successful = false;
         }

         // Store medication UID in NVM
         if((med_uid_storage_retries_left > 0u) && !is_med_uid_storage_successful)
         {
            // Set the new medication uid in the data manager
            result = m_dock_data_manager.interface.set_med_nfc_uid(
               &m_dock_data_manager.interface, new_medication_nfc_id, sizeof(new_medication_nfc_id));
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to update medication UID in data manager. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            if(IS_OK(result))
            {
               is_med_uid_storage_successful = true;
            }
            else
            {
               med_uid_storage_retries_left--;
               DEBUG_WARNING("Medication UID storage retries left: %d", med_uid_storage_retries_left);
            }
         }

         // Store weight baseline data in NVM
         if((weight_calibration_storage_retries_left > 0u) && !is_weight_calibration_storage_successful)
         {
            // Set the updated weight calibration record in the data manager
            weight_stack_calibration_record_t old_record = {0};
            weight_stack_calibration_record_t updated_record = {0};

            result = m_dock_data_manager.interface.get_weight_calibration_record(&m_dock_data_manager.interface,
                                                                                 &old_record);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to get calibration record from data manager. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            if(IS_OK(result))
            {
               updated_record.calibration_factor = old_record.calibration_factor;  // Unchanged
               updated_record.full_assembly_weight_mg = dock_ring_full_med_weight; // Updated
               updated_record.zero_offset = dock_weight_zero_offset;               // Updated
               result = m_dock_data_manager.interface.set_weight_calibration_record(&m_dock_data_manager.interface,
                                                                                    &updated_record);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to update weight calibration record in data manager. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
            }

            if(IS_OK(result))
            {
               is_weight_calibration_storage_successful = true;
            }
            else
            {
               weight_calibration_storage_retries_left--;
               DEBUG_WARNING("Weight calibration data storage retries left: %d",
                             weight_calibration_storage_retries_left);
            }
         }

         if(is_med_uid_storage_successful && is_weight_calibration_storage_successful)
         {
            // Move onto next state
            m_baseline_sm_inputs.med_update_status = MED_UID_STORAGE_SUCCESS;
         }

         if((0u == weight_calibration_storage_retries_left) || (0u == med_uid_storage_retries_left))
         {
            // Failed to store data in NVM
            m_baseline_sm_inputs.med_update_status = MED_UID_STORAGE_FAILED;
         }

         break;
      case BASELINING_STATE_COMPLETE:
         if(m_baseline_sm_outputs.changed)
         {
            // First time running this state
            report_baselining_feedback(&feedback);

            m_baseline_sm_inputs.is_baselining_active = false;
            // Update system state machine baselining active status
            m_system_sm_inputs.is_baselining_active = false;
         }
         break;
      case BASELINING_STATE_ERROR:
         if(m_baseline_sm_outputs.changed)
         {
            // First time running this state
            report_baselining_feedback(&feedback);
            // Update baselining state machine active status
            m_baseline_sm_inputs.is_baselining_active = false;
            // Update system state machine baselining active status
            m_system_sm_inputs.is_baselining_active = false;
         }
         break;
      default:
         break;
   }
}

static void report_baselining_feedback(const baselining_feedback_t *feedback)
{
   mp_packet_payload_t tx_msg = {0};
   tx_msg.type = (uint8_t)PPI_TYPE_PUSH;
   tx_msg.ppi = (uint8_t)PPI_AD_BASELINING_FEEDBACK;
   tx_msg.pkt_payload_len = BASELINING_FEEDBACK_T_SIZE_BYTES;

   uint32_t avg_weight_mg_u32 = (uint32_t)feedback->avg_weight_mg;
   uint16_t offset = 0u;
   tx_msg.payload[offset++] = (uint8_t)feedback->current_state;
   tx_msg.payload[offset++] = (uint8_t)feedback->motion_state;
   tx_msg.payload[offset++] = (uint8_t)(feedback->std_dev & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((feedback->std_dev >> 8u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)(avg_weight_mg_u32 & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((avg_weight_mg_u32 >> 8u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((avg_weight_mg_u32 >> 16u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((avg_weight_mg_u32 >> 24u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)feedback->is_ring_present;
   memcpy(&tx_msg.payload[offset], feedback->medication_nfc_id, sizeof(feedback->medication_nfc_id));
   offset += sizeof(feedback->medication_nfc_id);

   // Verify correct encoding
   if(tx_msg.pkt_payload_len == offset)
   {
      // Send feedback to App
      result_t result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to enqueue msg. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   else
   {
      DEBUG_ERROR("Bad encoding.");
   }
}

static void report_calibration_feedback(const calibration_feedback_t *feedback)
{
   mp_packet_payload_t tx_msg = {0};
   tx_msg.type = (uint8_t)PPI_TYPE_PUSH;
   tx_msg.ppi = (uint8_t)PPI_AD_CALIBRATION_FEEDBACK;
   tx_msg.pkt_payload_len = CALIBRATION_FEEDBACK_T_SIZE_BYTES;

   uint32_t avg_weight_mg_u32 = (uint32_t)feedback->avg_weight_mg;
   uint16_t offset = 0u;
   tx_msg.payload[offset++] = (uint8_t)feedback->current_state;
   tx_msg.payload[offset++] = (uint8_t)feedback->motion_state;
   tx_msg.payload[offset++] = (uint8_t)(feedback->std_dev & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((feedback->std_dev >> 8u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)(avg_weight_mg_u32 & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((avg_weight_mg_u32 >> 8u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((avg_weight_mg_u32 >> 16u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((avg_weight_mg_u32 >> 24u) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)(feedback->is_ring_present ? 1u : 0u);

   // Verify correct encoding
   if(tx_msg.pkt_payload_len == offset)
   {
      // Send feedback to App
      result_t result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to enqueue msg. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   else
   {
      DEBUG_ERROR("Bad encoding.");
   }
}

static void report_calibration_data(const weight_stack_calibration_record_t *record)
{
   mp_packet_payload_t tx_msg = {0};
   tx_msg.type = (uint8_t)PPI_TYPE_PUSH;
   tx_msg.ppi = (uint8_t)PPI_AD_CALIBRATION_DATA;
   tx_msg.pkt_payload_len = sizeof(weight_stack_calibration_record_t);

   size_t offset = 0u;
   uint32_t val = 0u;

   val = (uint32_t)record->zero_offset;
   tx_msg.payload[offset++] = (uint8_t)(val & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 8) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 16) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 24) & 0xFFu);

   val = (uint32_t)record->calibration_factor;
   tx_msg.payload[offset++] = (uint8_t)(val & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 8) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 16) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 24) & 0xFFu);

   val = record->full_assembly_weight_mg;
   tx_msg.payload[offset++] = (uint8_t)(val & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 8) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 16) & 0xFFu);
   tx_msg.payload[offset++] = (uint8_t)((val >> 24) & 0xFFu);

   result_t result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
   ON_ERR_DEBUG_ERROR(result, "Failed to enqueue msg. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
}

static void handle_sm_calibration_state(bool is_ring_present, uint32_t rx_calibration_weight_mg, bool is_weight_present)
{
   result_t result = RESULT_OK;
   MOTION_STATE motion_state = MOTION_STATE_UNSTABLE;

   static uint8_t weight_calibration_storage_retries_left = 0u;
   static int32_t calibration_factor = 0u;
   static int32_t dock_weight_zero_offset = 0;

   static weight_stack_calibration_record_t new_calibration_record = {0};

   // Ensure there is an active BLE connection with the App, otherwise stop the process
   bool is_ble_active = is_ble_valid();

   if(!is_ble_active)
   {
      // Stop calibration procedure
      m_calibrate_sm_inputs.start_calibration = false;
      m_calibrate_sm_inputs.is_calibration_active = false;
   }

   bool is_stable = false;
   if(is_ble_active)
   {
      result = m_weight_sensor.interface.get_is_stable(&m_weight_sensor.interface, &is_stable);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to check if weight stable. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   weight_data_t weight_data = {0};
   bool _is_weight_data_stale = false;
   if(IS_OK(result) && is_ble_active)
   {
      if(is_stable)
      {
         motion_state = MOTION_STATE_STABLE;
      }
      else
      {
         motion_state = MOTION_STATE_UNSTABLE;
      }

      result
         = m_weight_sensor.interface.read_weight_data(&m_weight_sensor.interface, &weight_data, &_is_weight_data_stale);
   }

   // Update state machine inputs
   m_calibrate_sm_inputs.is_ring_present = is_ring_present;

   // Update state machine
   m_calibrate_sm_outputs.changed = false; // Clear output before updating state machine
   m_calibrate_sm.update_statemachine(&m_calibrate_sm, &m_calibrate_sm_inputs, &m_calibrate_sm_outputs);

   // Update feedback report
   calibration_feedback_t feedback = {.avg_weight_mg = weight_data.weight_mg,
                                      .current_state = (uint8_t)m_calibrate_sm.current_state,
                                      .is_ring_present = is_ring_present,
                                      .motion_state = (uint8_t)motion_state,
                                      .std_dev = weight_data.stddev_mg};

   switch(m_calibrate_sm.current_state)
   {
      case CALIBRATION_STATE_WAIT_FOR_RING_REMOVAL:
         if(m_calibrate_sm_outputs.changed)
         {
            report_calibration_feedback(&feedback);
            // Clear calibration record
            new_calibration_record.calibration_factor = 0;
            new_calibration_record.full_assembly_weight_mg = 0u;
            new_calibration_record.zero_offset = 0;
         }
         break;
      case CALIBRATION_STATE_SET_DOCK_WEIGHT:
         if(m_calibrate_sm_outputs.changed)
         {
            report_calibration_feedback(&feedback);
         }

         if(is_stable && !is_ring_present)
         {
            bool is_tare_successful = false;
            result = m_weight_sensor.interface.try_tare_if_stable(
               &m_weight_sensor.interface, is_ring_present, &is_tare_successful);
            ON_ERR_DEBUG_ERROR(
               result, "Failed to tare weight sensor. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

            if(is_tare_successful && IS_OK(result))
            {
               DEBUG_INFO("Tare successful. Zero offset set.");
               result = m_weight_sensor.interface.get_zero_offset(&m_weight_sensor.interface, &dock_weight_zero_offset);
               ON_ERR_DEBUG_ERROR(
                  result, "Failed to get zero offset. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
            }

            if(IS_OK(result) && is_tare_successful)
            {
               // Dock baseline successfully captured.
               m_calibrate_sm_inputs.is_empty_dock_weight_set = true;
            }
         }
         break;
      case CALIBRATION_STATE_WAIT_FOR_CALIBRATION_WEIGHT:
         if(m_calibrate_sm_outputs.changed)
         {
            report_calibration_feedback(&feedback);
         }
         // Wait for calibration weight to be added
         if(is_weight_present && !is_ring_present)
         {
            m_calibrate_sm_inputs.is_calibration_weight_present = true;
         }
         break;
      case CALIBRATION_STATE_CALIBRATING:
         if(m_calibrate_sm_outputs.changed)
         {
            report_calibration_feedback(&feedback);
         }

         if(is_stable && !is_ring_present)
         {
            bool is_calibration_successful = false;
            bool is_weight_detected = false;
            result = m_weight_sensor.interface.try_calibrate_if_stable(&m_weight_sensor.interface,
                                                                       rx_calibration_weight_mg,
                                                                       is_ring_present,
                                                                       &is_weight_detected,
                                                                       &is_calibration_successful);
            ON_ERR_DEBUG_ERROR(
               result, "Failed to tare weight sensor. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

            if(is_calibration_successful && IS_OK(result))
            {
               result
                  = m_weight_sensor.interface.get_calibration_factor(&m_weight_sensor.interface, &calibration_factor);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to get full assembly weight. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
            }

            if(IS_OK(result) && is_calibration_successful)
            {
               // Successfully calibrated the weight sensor and retrieved the calibration factor
               m_calibrate_sm_inputs.is_calibration_weight_set = true;
            }
         }
         break;
      case CALIBRATION_STATE_STORE_UPDATED_PARAM:
         // This state ensures data gets set in NVM
         if(m_calibrate_sm_outputs.changed)
         {
            weight_calibration_storage_retries_left = CALIBRATION_SET_NVM_MAX_RETRIES;
         }

         // Store weight baseline data in NVM
         if(weight_calibration_storage_retries_left > 0u)
         {
            // Set the updated weight calibration record in the data manager
            weight_stack_calibration_record_t old_record = {0};

            result = m_dock_data_manager.interface.get_weight_calibration_record(&m_dock_data_manager.interface,
                                                                                 &old_record);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to get calibration record from data manager. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            if(IS_OK(result))
            {
               new_calibration_record.calibration_factor = calibration_factor;                      // Updated
               new_calibration_record.full_assembly_weight_mg = old_record.full_assembly_weight_mg; // Unchanged
               new_calibration_record.zero_offset = dock_weight_zero_offset;                        // Updated
               result = m_dock_data_manager.interface.set_weight_calibration_record(&m_dock_data_manager.interface,
                                                                                    &new_calibration_record);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to update weight calibration record in data manager. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
            }

            if(IS_OK(result))
            {
               // Success: Move onto next state
               m_calibrate_sm_inputs.data_update_status = DATA_STORAGE_SUCCESS;
            }
            else
            {
               weight_calibration_storage_retries_left--;
               DEBUG_WARNING("Weight calibration data storage retries left: %d",
                             weight_calibration_storage_retries_left);
            }
         }
         else
         {
            // Failed to store data in NVM
            m_calibrate_sm_inputs.data_update_status = DATA_STORAGE_FAILED;
         }

         break;
      case CALIBRATION_STATE_COMPLETE:
         if(m_calibrate_sm_outputs.changed)
         {
            // First time running this state
            report_calibration_feedback(&feedback);
            report_calibration_data(&new_calibration_record);

            m_calibrate_sm_inputs.is_calibration_active = false;
            // Update system state machine baselining active status
            m_system_sm_inputs.is_calibration_active = false;
         }
         break;
      case CALIBRATION_STATE_ERROR:
         if(m_calibrate_sm_outputs.changed)
         {
            // First time running this state
            report_calibration_feedback(&feedback);
            // Update baselining state machine active status
            m_calibrate_sm_inputs.is_calibration_active = false;
            // Update system state machine baselining active status
            m_system_sm_inputs.is_calibration_active = false;
         }
         break;
      default:
         break;
   }
}

static void handle_sm_pairing_state()
{
   result_t result = RESULT_OK;
   static uint64_t pairing_state_start_time_ms = 0u;
   uint64_t current_systick_time_ms = 0u;

   // Set LED to indicate pairing mode is active
   result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BLE_PAIRING);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to set notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(m_system_sm_outputs.changed)
   {
      // Pairing state first run, get timestamp
      result = m_systick.interface.get_time_ms(&m_systick.interface, &pairing_state_start_time_ms);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get systick time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      // Load BLE passkey
      char decrypted_passkey_text[64] = {0x00};
      if(IS_OK(result))
      {
         result = load_ble_passkey(decrypted_passkey_text);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to load BLE passkey. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }

      // Start advertising without whitelist (allow new bonds)
      if(IS_OK(result))
      {
         result = m_ble_control.interface.start_advertising(&m_ble_control.interface, true, decrypted_passkey_text);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to start advertising. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         memset(decrypted_passkey_text, 0, 64);
      }
   }

   if(!m_system_sm_outputs.changed && m_ble_status.is_bonded && !m_ble_status.allow_new_bond)
   {
      // New bond formed
      m_system_sm_inputs.bonding_state = BONDING_STATE_BONDED;
      DEBUG_INFO("Bonded successfully, restarting advertising with whitelist");
      result = m_ble_control.interface.start_advertising(&m_ble_control.interface, false, NULL);
      // Stop HMI notification for pairing
      if(IS_OK(result))
      {
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BLE_PAIRING);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to clear notification event. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
      }
   }

   // Get the current timestamp
   result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_time_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // Check for pairing state timeout if no new bond has been formed
   if((current_systick_time_ms - pairing_state_start_time_ms > SYSTICK_BLE_PAIRING_TIMEOUT_MS)
      && (m_system_sm_inputs.bonding_state == BONDING_STATE_BONDING) && IS_OK(result))
   {
      DEBUG_INFO("Pairing timeout.");

      // Pairing timeout, leave state
      if(m_ble_status.is_bonded)
      {
         DEBUG_INFO("Retained pre-existing bonding credentials. Restarting advertising with whitelist.");
         m_system_sm_inputs.bonding_state = BONDING_STATE_BONDED;

         // Start whitelist advertising
         result = m_ble_control.interface.start_advertising(&m_ble_control.interface, false, NULL);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to start advertising. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
      else
      {
         DEBUG_INFO("No pre-existing bonding credentials. Device is unbonded.");
         m_system_sm_inputs.bonding_state = BONDING_STATE_UNBONDED;
         // Stop advertising - no bonding credentials
         m_ble_control.interface.stop_advertising(&m_ble_control.interface);
      }

      // Clear HMI notification for pairing
      result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BLE_PAIRING);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to clear notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
}

// Active dose window state.
static void handle_sm_active_state(void)
{
   uint64_t current_time_ms = 0u;
   static uint64_t prev_time_ms = 0u;
   result_t result = m_systick.interface.get_time_ms(&m_systick.interface, &current_time_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(m_system_sm_outputs.changed)
   {
      // First time running in this state.
      prev_time_ms = current_time_ms; // Reset timout
      result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_DOSAGE_DOSE_DUE);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to set notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   // Reimplement the dose due notification:
   result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_DOSAGE_DOSE_DUE);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to set notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // Hard limit the time in this state to a predefined max dose window time. This is a safety measure in case the dose
   // window close event is missed for any reason - we don't want to be stuck in the dose due state indefinitely.
   // The dose window should be opened and closed under normal circumstances in the @ref implement_control_loop()
   // function.
   if(current_time_ms - prev_time_ms > DOSE_WINDOW_TIMEOUT_MS)
   {
      prev_time_ms = current_time_ms;
      result = m_dose_scheduler.interface.close_dose_window(&m_dose_scheduler.interface);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to close dose window. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
}

static void handle_sm_invalid_dose_info_state(void)
{
   result_t result = RESULT_OK;
   static bool have_not_reported_invalid_dose = false;

   if(m_system_sm_outputs.changed)
   {
      have_not_reported_invalid_dose = true;
      // First time running in this state.
      DEBUG_DEBUG("Invalid dose info detected.");
      // result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR); // Todo re-enable
      // error notification for production after testing
      ON_ERR_DEBUG_ERROR(
         result, "Failed to set notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   uint64_t current_time_ms = 0u;
   static uint64_t prev_error_time_ms = 0u;
   result = m_systick.interface.get_time_ms(&m_systick.interface, &current_time_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(current_time_ms - prev_error_time_ms > INVALID_DOSE_INFO_ERR_REPORTING_INTERVAL_MS)
   {
      prev_error_time_ms = current_time_ms;
      // Reimplement the error notification:
      DEBUG_DEBUG("Invalid dose info detected.");
      // result = m_hmi.interface.set_notification_event(
      //    &m_hmi.interface, NOTIFICATION_EVENT_ERROR); // Todo re-enable error notification for production after
      //    testing
      ON_ERR_DEBUG_ERROR(
         result, "Failed to set notification event. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   if(have_not_reported_invalid_dose)
   {
      mp_packet_payload_t tx_msg = {0};
      tx_msg.type = (uint8_t)PPI_TYPE_RQ;
      tx_msg.ppi = (uint8_t)PPI_AD_DOSE_SCHEDULE;
      tx_msg.pkt_payload_len = 0u; // No payload needed for this request

      result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &tx_msg);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to enqueue tx message. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      if(IS_OK(result))
      {
         have_not_reported_invalid_dose = false; // Only report once until we get a response from the App
      }
   }
}

static void update_ble_state(void)
{
   result_t result = RESULT_OK;
   bool is_whitelist_advertising = false;
   bool should_start_adv = false;
   ble_control_status_t status = {0};

   // Update BLE status
   result = m_ble_control.interface.get_ble_status(&m_ble_control.interface, &status);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get BLE status. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result))
   {
      memcpy(&m_ble_status, &status, sizeof(ble_control_status_t));

      // Update state machine input
      if(m_ble_status.is_bonded && (BONDING_STATE_UNBONDED == m_system_sm_inputs.bonding_state))
      {
         DEBUG_DEBUG("Device is bonded. Updating bonding state input to BONDED.");
         m_system_sm_inputs.bonding_state = BONDING_STATE_BONDED;
      }

      is_whitelist_advertising = m_ble_status.is_advertising && (!m_ble_status.allow_new_bond);
      should_start_adv = (!is_whitelist_advertising) && (!m_ble_status.is_connected) && m_ble_status.is_bonded
                         && (BONDING_STATE_BONDING != m_system_sm_inputs.bonding_state);
   }

   // Stop advertising if unbonded
   if(IS_OK(result) && m_ble_status.is_advertising && !m_ble_status.is_bonded
      && (BONDING_STATE_BONDING != m_system_sm_inputs.bonding_state))
   {
      DEBUG_DEBUG("Device is unbonded, not currently bonding and is advertising . Stopping advertising.");
      result = m_ble_control.interface.stop_advertising(&m_ble_control.interface);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to stop advertising. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   if(should_start_adv && IS_OK(result))
   {
      // Start advertising with whitelist
      DEBUG_DEBUG("Device is bonded but not currently advertising or connected. Starting whitelist advertising.");
      result = m_ble_control.interface.start_advertising(&m_ble_control.interface, false, NULL);
      ON_ERR_DEBUG_ERROR(result,
                         "Failed to start advertising with whitelist. Unit %d, Error: %d",
                         GET_ERR_UNIT(result),
                         GET_ERR_CODE(result));
   }
}

static const char *nfc_power_state_to_str(NFC_POWER_STATE state)
{
   const char *state_str = "UNKNOWN";

   switch(state)
   {
      case NFC_POWER_STATE_CHARGER_HIGH:
         state_str = "CHARGER_HIGH";
         break;
      case NFC_POWER_STATE_CHARGER_LOW:
         state_str = "CHARGER_LOW";
         break;
      case NFC_POWER_STATE_LOWPOWER_OFF:
         state_str = "LOWPOWER_OFF";
         break;
      case NFC_POWER_STATE_LOWPOWER_POLL:
         state_str = "LOWPOWER_POLL";
         break;
      case NFC_POWER_STATE_LOWPOWER_ACTIVE:
         state_str = "LOWPOWER_ACTIVE";
         break;
      case NFC_POWER_STATE_UNKNOWN:
      // Fall through
      case NFC_POWER_STATE_MAX:
      // Fall through
      default:
         break;
   }

   return state_str;
}

static void handle_battery_hmi_notifications(battery_status_t battery_status)
{
   result_t result = RESULT_OK;
   static uint64_t prev_reported_battery_status_error_time_ms = 0u;

   uint64_t current_time_ms = 0u;

   result = m_systick.interface.get_time_ms(&m_systick.interface, &current_time_ms);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get systick time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   switch(battery_status.battery_state)
   {
      case BATTERY_STATE_SOC_GOOD:
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CHARGING);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result
            = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         break;
      case BATTERY_STATE_SOC_LOW:
         result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CHARGING);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result
            = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         break;
      case BATTERY_STATE_CHARGING:
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CHARGING);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result
            = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         break;
      case BATTERY_STATE_CHARGING_COMPLETED:
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CHARGING);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         result
            = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update HMI notification. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         break;
      case BATTERY_STATE_ERROR:
      // Fall through
      case BATTERY_STATE_MAX:
         // Fall through
      default:
         // Rate limit this error log to avoid flooding in case of an issue with the battery status reporting
         if(current_time_ms - prev_reported_battery_status_error_time_ms > BATTERY_STATUS_ERROR_LOG_RATE_LIMIT_MS)
         {
            prev_reported_battery_status_error_time_ms = current_time_ms;
            // Unrecognized battery state, log error and clear other HMI battery notifications to be safe
            DEBUG_ERROR("Unrecognized battery state: %d", battery_status.battery_state);
            result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to update HMI notification. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to update HMI notification. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            result
               = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BAT_MANAGER_CHARGING);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to update HMI notification. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            result = m_hmi.interface.clear_notification_event(&m_hmi.interface,
                                                              NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to update HMI notification. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
         }
         break;
   }
}

static void handle_battery(battery_status_t *battery_status)
{
   if(NULL == battery_status)
   {
      DEBUG_ERROR("Invalid NULL pointer for battery status");
      return;
   }

   // Get battery status
   battery_status_t battery = {0};
   result_t result = m_battery.interface.get_battery_status(&m_battery.interface, &battery);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get battery status. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // Check under voltage: If below the minimum threshold, put device in SHIP MODE to protect battery from damage.
   if((MINIMUM_BATTERY_VOLTAGE_MV > battery.battery_voltage_mv) && IS_OK(result))
   {
      DEBUG_WARNING("Battery below minimum operational voltage. Putting device in SHIP MODE.");
      enter_ship_mode();
   }

   // Update system state machine inputs
   if(IS_OK(result))
   {
      m_system_sm_inputs.battery_charger_connected = battery.charger_connected;
   }

   // Check for PMIC errors and handle accordingly
   // Restart firmware if PMIC reports reset
   static const uint16_t pmic_reset_delay_ms = PMIC_RESET_DELAY_MS;
   static bool start_pmic_reset_timer_once = false;

   // Ensure timer is not started multiple times
   if((false == start_pmic_reset_timer_once) && battery.is_pmic_reset_detected && IS_OK(result))
   {
      DEBUG_CRITICAL("PMIC reset detected. Application firmware will restart in %u ms...", pmic_reset_delay_ms);
      app_timer_start(m_timer_pmic_reset_expire, APP_TIMER_TICKS(pmic_reset_delay_ms), NULL);
      start_pmic_reset_timer_once = true;
   }

   // Handle battery related HMI notifications and report battery status to data manager
   // Only report battery level in increments of @p BATTERY_LEVEL_INCREMENT percent, rounding down.
   static uint8_t prev_reported_battery_level = 0u;
   uint8_t new_battery_level
      = BATTERY_LEVEL_INCREMENT * (battery.battery_level / BATTERY_LEVEL_INCREMENT); // Discard remainder.

   uint64_t current_time_ms = 0u;
   static uint64_t prev_reported_battery_level_time_ms = 0u;
   if(IS_OK(result))
   {
      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_time_ms);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get systick time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   // Rate limit and only report battery level changes of at least BATTERY_LEVEL_INCREMENT percent to save bandwidth,
   // and only if there are no errors in getting battery status or time.
   if((new_battery_level != prev_reported_battery_level)
      && (current_time_ms - prev_reported_battery_level_time_ms > BATTERY_LEVEL_MIN_REPORT_INTERVAL_MS)
      && IS_OK(result))
   {
      // Note: Setting the updated time immediately instead of after success of enqueuing the data automatically rate
      // limits potential errors from flooding the error handling.
      prev_reported_battery_level_time_ms = current_time_ms;
      // Report new battery status to data manager
      uint32_t current_unix_time_s = 0u;
      result = m_rtc.interface.get_time_unix(&m_rtc.interface, &current_unix_time_s);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get unix time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      if(IS_OK(result))
      {
         battery_level_t level = {.timestamp_unix_s = current_unix_time_s, .battery_level = new_battery_level};
         prev_reported_battery_level = new_battery_level;
         result = m_dock_data_manager.interface.enqueue(
            &m_dock_data_manager.interface, DATA_ID_DOCK_BATTERY_LEVEL, &level, sizeof(level), 1u);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to update battery level in data manager. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
      }
   }

   // Update HMI w.r.t. battery status and charger connection status
   if(IS_ERR(result))
   {
      // Invalid battery status. Set battery state to error to trigger appropriate HMI notifications.
      battery.battery_state = BATTERY_STATE_ERROR;
   }

   handle_battery_hmi_notifications(battery);

   if(IS_OK(result))
   {
      // Update caller's battery status struct with the latest data
      *battery_status = battery;
   }
}

static void handle_temperature_reading(void)
{
   static uint64_t prev_reported_temperature_time = 0u;
   uint64_t current_systic_time_ms = 0u;
   int16_t temperature_celsius = 0;
   result_t result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systic_time_ms);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get systic time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // 1. Get temperature value

   // The below implementation ensures that the IMU is turned on and given time to stabilize before attempting to read
   // the temperature value. The stabilization time is not explicitly defined but is implicitly handled by waiting until
   // the next temperature update cycle to read the value after turning on the IMU. I.e. the stabilization time is tied
   // to the m_system_sm_outputs.current_state_loop_period_ms

   // Is it time to get a new temperature reading?
   static bool is_temp_value_valid = false;
   static bool is_temp_value_avg_checked = false;
   if((current_systic_time_ms - prev_reported_temperature_time > TEMPERATURE_UPDATE_INTERVAL_MS) && IS_OK(result))
   {
      IMU_STATE imu_state = 0;
      result = m_imu.interface.get_state(&m_imu.interface, &imu_state);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get IMU state. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      // Is the IMU on yet?
      if(IS_OK(result) && (IMU_STATE_ACTIVE == imu_state))
      {
         // IMU is already on, get temperature.
         result = m_imu.interface.get_temperature_celsius(&m_imu.interface, &temperature_celsius);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to get IMU temperature. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

         // Validate temperature reading. Retry on next cycle if invalid
         if((temperature_celsius < INT8_MAX) && (temperature_celsius > -30) && IS_OK(result))
         {
            // Valid value received, reset timer.
            prev_reported_temperature_time = current_systic_time_ms;
            is_temp_value_valid = true;
            is_temp_value_avg_checked = false; // Reset flag
         }
         else if(IS_OK(result))
         {
            // Invalid temperature. Timer not reset, will retry on next cycle
            is_temp_value_valid = false;
            DEBUG_WARNING("Invalid IMU temperature reading: %d deg C. Will retry on next cycle.", temperature_celsius);
         }
      }
      else if(IS_OK(result) && (IMU_STATE_DORMANT == imu_state))
      {
         // IMU is off, turn it on
         result = turn_on_imu();
         ON_ERR_DEBUG_ERROR(
            result, "Failed to turn on IMU. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
   }

   // 2. Get moving average of temperature readings and report error if over threshold defined in dose schedule.
   if(is_temp_value_valid && !is_temp_value_avg_checked && IS_OK(result))
   {
      int16_t avg_temp_celsius = moving_average_add(temperature_celsius);
      dose_schedule_record_t schedule_record = {0};

      result = m_dock_data_manager.interface.get_dose_schedule_record(&m_dock_data_manager.interface, &schedule_record);
      ON_ERR_DEBUG_ERROR(result,
                         "Failed to get dose schedule from data manager. Unit %d, Error: %d",
                         GET_ERR_UNIT(result),
                         GET_ERR_CODE(result));
      // Only report temperature out of range if a valid dose schedule is detected
      if((avg_temp_celsius > schedule_record.dose_schedule.temp_upper_limit_deg_c)
         && m_system_sm_inputs.is_valid_dose_schedule_detected && IS_OK(result))
      {
         static uint64_t last_overtemp_report_time_ms = 0u;

         // Check if it's time to report another over-temperature event. This prevents flooding the HMI with error
         // notifications. It determines how often the same over-temperature error is reported.
         if(IS_OK(result)
            && (current_systic_time_ms - last_overtemp_report_time_ms >= TEMPERATURE_OVER_THRESHOLD_REPORT_INTERVAL_MS))
         {
            DEBUG_ERROR("Temperature over threshold. Medication might be exposed to exessive heat. "
                        "temp_avg_window_duration_sec = %d; temp_upper_limit_deg_c = %d; average temperature "
                        "over window (celsius) = %d",
                        schedule_record.dose_schedule.temp_avg_window_duration_sec,
                        schedule_record.dose_schedule.temp_upper_limit_deg_c,
                        avg_temp_celsius);

            // Set HMI error notification
            result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR);
            ON_ERR_DEBUG_ERROR(result,
                               "Failed to set HMI error notification. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));

            last_overtemp_report_time_ms = current_systic_time_ms;
         }
      }
      is_temp_value_avg_checked = true; // Ensures this check is only done once per valid temperature reading
   }

   // 3. Periodically report dock temp to the data manager.
   if(is_temp_value_valid && IS_OK(result))
   {
      uint32_t current_unix_time = 0u;
      result = m_rtc.interface.get_time_unix(&m_rtc.interface, &current_unix_time);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get RTC unix time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      if(IS_OK(result))
      {
         temperature_log_t temp_log = {.temperature_deg_c = temperature_celsius, .timestamp_unix_s = current_unix_time};

         result = m_dock_data_manager.interface.enqueue(
            &m_dock_data_manager.interface, DATA_ID_TEMPERATURE_LOG, &temp_log, sizeof(temperature_log_t), 1u);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to enqueue temperature reading to data manager. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
      }
      if(IS_OK(result))
      {
         // Successfully reported temperature, reset flags
         is_temp_value_valid = false; // Reset flag
      }
   }
}

static void handle_button(bool *is_pairing_gesture_detected)
{
   if(NULL == is_pairing_gesture_detected)
   {
      DEBUG_ERROR("Invalid NULL pointer for is_pairing_gesture_detected");
      return;
   }

   button_status_t button_status[BUTTONS_COUNT] = {0};
   result_t result = m_button.interface.get_button_events(&m_button.interface, button_status, NULL);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get button events. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result) && (BUTTON_PRESS_LONG == button_status[0].button_event))
   {
      *is_pairing_gesture_detected = true;
      DEBUG_INFO("Pairing gesture detected.");
   }
}

/**
 * @brief Invalidate cached ring-presence sample while preserving last value.
 *
 * The next successful presence sample is treated as fresh, but the previous
 * sampled value is kept so callers continue seeing the last known state while
 * NFC is intentionally powered down.
 *
 * @param[out] state Pointer to NFC runtime state.
 */
static void nfc_reset_ring_presence(nfc_runtime_state_t *state)
{
   state->ring_presence_valid = false;
}

/**
 * @brief Check if a ring status snapshot reports pending ring activity.
 *
 * Activity is defined as non-zero usage in any ring FIFO.
 *
 * @param[in] ring_status Latest ring status snapshot.
 *
 * @retval true  At least one FIFO contains pending data.
 * @retval false No FIFO activity is present.
 */
static bool nfc_ring_status_has_activity(const status_update_t *ring_status)
{
   return (ring_status->ring_status.dose_fifo_used_percent > 0u)
          || (ring_status->ring_status.battery_fifo_used_percent > 0u)
          || (ring_status->ring_status.error_fifo_used_percent > 0u);
}

/**
 * @brief Power up NFC hardware and select Type-V detection.
 *
 * This helper performs the standard wake sequence used for ring communication:
 * chip power-up followed by Type-V detection selection.
 *
 * @return result_t
 * @retval RESULT_OK Reader is powered and configured for Type-V.
 * @retval Any propagated NFC driver error if setup fails.
 */
static result_t nfc_power_up_and_select_type_v(uint8_t power_level)
{
   result_t result = m_nfc.interface.chip_power_up(&m_nfc.interface);
   if(NFC_R_ERROR_WRONG_STATE == GET_ERR_CODE(result))
   {
      // Not an error, device is already on.
      CLEAR_ERR(result);
   }
   ON_ERR_DEBUG_ERROR(
      result, "Failed to power up NFC chip. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result))
   {
      result = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_V, power_level);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to set NFC tag type. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   return result;
}

/**
 * @brief Enter low-power poll mode for Type-V processing.
 *
 * This powers/configures the reader for Type-V, sets low RF output power, and
 * records poll timing/state metadata.
 *
 * @param[out] state Pointer to NFC runtime state.
 * @param[in] now_ms Current systick time in milliseconds.
 *
 * @return result_t
 * @retval RESULT_OK Poll window entered and state updated.
 * @retval Any propagated NFC driver error if setup fails.
 */
static result_t nfc_enter_lowpower_poll_window(nfc_runtime_state_t *state, uint64_t now_ms)
{
   result_t result = nfc_power_up_and_select_type_v(NFC_POWER_LEVEL_LOW);

   if(IS_OK(result))
   {
      state->nfc_state = NFC_POWER_STATE_LOWPOWER_POLL;
      state->poll_start_ms = now_ms;

      // Keep the first absence sample at least one debounce interval after wake-up/reconfigure.
      if(state->last_ring_present && !state->ring_presence_valid)
      {
         state->ring_absence_last_count_ms = now_ms;
      }
   }

   return result;
}

/**
 * @brief Refresh cached ring presence and detect ring-presence rising edge.
 *
 * Reads current tag presence from the NFC driver and updates
 * @p state->ring_presence_valid / @p state->last_ring_present. If the ring
 * transitioned from "not present or unknown" to "present", the medication-read
 * trigger flag is set.
 *
 * The ring UID cache is refreshed only on a true presence rising edge
 * (not present -> present) to avoid unnecessary NFC transactions.
 *
 * Falling edges (present -> not present) are confirmed with consecutive sampled
 * "absent" results before committing removal.
 *
 * @param[in,out] state Pointer to NFC runtime state.
 * @param[in] now_ms Current systick time in milliseconds.
 * @param[out] trigger_medication_read Set true on ring-presence rising edge.
 *
 * @return result_t
 * @retval RESULT_OK Presence was sampled and cache updated.
 * @retval Any propagated NFC driver error from presence check.
 */
static result_t nfc_update_ring_presence(nfc_runtime_state_t *state, uint64_t now_ms, bool *trigger_medication_read)
{
   RETURN_ERR_IF_NULL(state, GC_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(trigger_medication_read, GC_ERROR_PTR_NULL);

   bool is_ring_present_local = false;
   result_t result = m_nfc.interface.is_ring_present(&m_nfc.interface, &is_ring_present_local);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get ring presence status. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result))
   {
      if(is_ring_present_local)
      {
         // Rising edge (false -> true):
         // Commit immediately for responsiveness.
         bool ring_rising_edge = !state->last_ring_present;
         state->last_ring_present = true;
         state->ring_presence_valid = true;
         state->ring_absence_sample_count = 0u;
         state->ring_absence_last_count_ms = 0u;

         if(ring_rising_edge)
         {
            // A confirmed ring placement is treated as a "new session" for medication UID acquisition.
            state->medication_read_retry_count = 0u;
            *trigger_medication_read = true;

            uint8_t ring_uid_local[NFC_TAG_MAX_UID_SIZE] = {0};
            uint8_t ring_uid_len = NFC_TAG_MAX_UID_SIZE;
            static const uint8_t invalid_ring_uid[NFC_TAG_MAX_UID_SIZE] = {0};

            // UID read is intentionally done only on confirmed rising edges to avoid repeated NFC transactions.
            result_t uid_result = m_nfc.interface.get_tag_uid(&m_nfc.interface, ring_uid_local, &ring_uid_len);
            ON_ERR_DEBUG_ERROR(uid_result,
                               "Failed to get ring NFC-V UID. Unit %d, Error: %d",
                               GET_ERR_UNIT(uid_result),
                               GET_ERR_CODE(uid_result));

            if(IS_OK(uid_result) && (ring_uid_len > 0u) && (ring_uid_len <= NFC_TAG_MAX_UID_SIZE)
               && (0 != memcmp(ring_uid_local, invalid_ring_uid, ring_uid_len)))
            {
               // Store a full-width buffer with trailing zero padding for stable consumers.
               memset(state->last_good_ring_uid, 0, NFC_TAG_MAX_UID_SIZE);
               memcpy(state->last_good_ring_uid, ring_uid_local, ring_uid_len);
            }
         }
      }
      else
      {
         // Falling edge candidate (true -> false):
         // Commit only after consecutive sampled absences while the reader is ON.
         if(state->last_ring_present)
         {
            uint32_t absence_min_interval_ms = state->ring_presence_valid ? NFC_RING_ABSENCE_DEBOUNCE_MIN_INTERVAL_MS :
                                                                            NFC_RING_ABSENCE_REVALIDATE_MIN_INTERVAL_MS;
            bool can_count_absence = (0u == state->ring_absence_last_count_ms)
                                     || (now_ms < state->ring_absence_last_count_ms)
                                     || ((now_ms - state->ring_absence_last_count_ms) >= absence_min_interval_ms);
            if(can_count_absence)
            {
               if(state->ring_absence_sample_count < UINT8_MAX)
               {
                  state->ring_absence_sample_count++;
               }
               state->ring_absence_last_count_ms = now_ms;
            }

            if(state->ring_absence_sample_count >= NFC_RING_PRESENCE_FALLING_CONFIRM_SAMPLES)
            {
               state->last_ring_present = false;
               state->ring_presence_valid = true;
               state->ring_absence_sample_count = 0u;
               state->ring_absence_last_count_ms = 0u;
            }
         }
         else
         {
            state->ring_presence_valid = true;
            state->ring_absence_sample_count = 0u;
            state->ring_absence_last_count_ms = 0u;
         }
      }
   }

   return result;
}

/**
 * @brief Advance the non-blocking medication Type-A UID read state machine.
 *
 * Per invocation this helper:
 * - Starts a queued Type-A read when eligible.
 * - Polls for medication tag presence/UID at
 *   @ref NFC_MEDICATION_UID_POLL_MS cadence.
 * - Completes on UID read success, timeout, or error.
 * - Restores Type-V detection and clears read flags when done.
 *
 * @param[in,out] state Pointer to NFC runtime state.
 * @param[in] now_ms Current systick time in milliseconds.
 */
static void nfc_process_medication_read_state_machine(nfc_runtime_state_t *state, uint64_t now_ms)
{
   RETURN_VOID_IF_NULL(state);

   uint8_t nfc_power_before_type_a_read = NFC_POWER_LEVEL_LOW;
   static const uint8_t invalid_medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};

   // A queued read is only valid while the ring is still present.
   if(state->medication_read_pending && !state->medication_read_in_progress && !state->last_ring_present)
   {
      state->medication_read_pending = false;
      state->medication_uid_stale = true;
      state->medication_read_retry_count = 0u;
   }

   // Start a non-blocking Type-A read attempt.
   if(state->medication_read_pending && !state->medication_read_in_progress && state->last_ring_present)
   {
      result_t medication_result = m_nfc.interface.get_output_power(&m_nfc.interface, &nfc_power_before_type_a_read);
      ON_ERR_DEBUG_ERROR(medication_result,
                         "Failed to get NFC power. Unit %d, Error: %d",
                         GET_ERR_UNIT(medication_result),
                         GET_ERR_CODE(medication_result));

      if(IS_OK(medication_result))
      {
         medication_result = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_A, NFC_POWER_LEVEL_HIGH);
         ON_ERR_DEBUG_ERROR(medication_result,
                            "Failed to set NFC tag type A for medication read. Unit %d, Error: %d",
                            GET_ERR_UNIT(medication_result),
                            GET_ERR_CODE(medication_result));
      }

      if(IS_OK(medication_result))
      {
         state->medication_read_in_progress = true;
         // A fresh medication UID is only available once this read finishes successfully.
         state->medication_uid_stale = true;
         state->medication_read_start_ms = now_ms;
         state->medication_next_poll_ms = now_ms;
      }
      else
      {
         state->medication_read_pending = false;
         state->medication_uid_stale = true;
      }
   }

   if(state->medication_read_in_progress && (now_ms >= state->medication_next_poll_ms))
   {
      bool medication_done = false;
      bool should_retry = false;
      bool medication_tag_present = false;
      uint8_t medication_uid_len = NFC_TAG_MAX_UID_SIZE;
      uint8_t medication_uid_local[NFC_TAG_MAX_UID_SIZE] = {0};

      result_t medication_result = m_nfc.interface.is_ring_present(&m_nfc.interface, &medication_tag_present);
      ON_ERR_DEBUG_ERROR(medication_result,
                         "Failed to get NFC-A medication presence. Unit %d, Error: %d",
                         GET_ERR_UNIT(medication_result),
                         GET_ERR_CODE(medication_result));

      if(IS_OK(medication_result) && medication_tag_present)
      {
         medication_result = m_nfc.interface.get_tag_uid(&m_nfc.interface, medication_uid_local, &medication_uid_len);
         ON_ERR_DEBUG_ERROR(medication_result,
                            "Failed to get medication NFC-A UID. Unit %d, Error: %d",
                            GET_ERR_UNIT(medication_result),
                            GET_ERR_CODE(medication_result));

         if(IS_OK(medication_result) && (medication_uid_len > 0u) && (medication_uid_len <= NFC_TAG_MAX_UID_SIZE)
            && (0 != memcmp(medication_uid_local, invalid_medication_uid, medication_uid_len)))
         {
            memset(state->last_good_medication_uid, 0, NFC_TAG_MAX_UID_SIZE);
            memcpy(state->last_good_medication_uid, medication_uid_local, medication_uid_len);
            state->medication_uid_stale = false;
            state->medication_read_retry_count = 0u;
            medication_done = true;
         }
      }

      if(!medication_done && ((now_ms - state->medication_read_start_ms) >= NFC_MEDICATION_UID_TIMEOUT_MS))
      {
         if(state->last_ring_present && (state->medication_read_retry_count < NFC_MEDICATION_UID_MAX_RETRIES))
         {
            state->medication_read_retry_count++;
            should_retry = true;
            DEBUG_WARNING("Timed out waiting for NFC-A medication tag UID, retrying (%u/%u)",
                          (unsigned int)state->medication_read_retry_count,
                          (unsigned int)NFC_MEDICATION_UID_MAX_RETRIES);
         }
         else
         {
            DEBUG_WARNING("Timed out waiting for NFC-A medication tag UID");
            state->medication_read_retry_count = 0u;
         }
         state->medication_uid_stale = true;
         medication_done = true;
      }

      if(IS_ERR(medication_result) && (!medication_done))
      {
         state->medication_uid_stale = true;
         if(state->last_ring_present && (state->medication_read_retry_count < NFC_MEDICATION_UID_MAX_RETRIES))
         {
            state->medication_read_retry_count++;
            should_retry = true;
         }
         else
         {
            state->medication_read_retry_count = 0u;
         }
         medication_done = true;
      }

      if(IS_ERR(medication_result) || medication_done)
      {
         result_t restore_result
            = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_V, nfc_power_before_type_a_read);
         ON_ERR_DEBUG_ERROR(restore_result,
                            "Failed to restore NFC tag type V after medication read. Unit %d, Error: %d",
                            GET_ERR_UNIT(restore_result),
                            GET_ERR_CODE(restore_result));

         state->medication_read_in_progress = false;
         state->medication_read_pending = should_retry;
      }
      else
      {
         state->medication_next_poll_ms = now_ms + NFC_MEDICATION_UID_POLL_MS;
      }
   }
}

/**
 * @brief Transition NFC reader into the low-power OFF state.
 *
 * Restores Type-V detection (best effort), turns RF field off, powers down the
 * chip, and updates runtime scheduling/flags for next poll window.
 *
 * @param[in,out] state Pointer to NFC runtime state.
 * @param[in] now_ms Current systick time in milliseconds.
 *
 * @return result_t
 * @retval RESULT_OK Reader transitioned to @ref NFC_POWER_STATE_LOWPOWER_OFF.
 * @retval Any propagated NFC driver error during shutdown.
 */
static result_t nfc_shutdown_to_lowpower_off(nfc_runtime_state_t *state, uint64_t now_ms)
{
   result_t restore_result = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_V, NFC_POWER_LEVEL_LOW);
   ON_ERR_DEBUG_ERROR(restore_result,
                      "Failed to restore NFC tag type V before low-power shutdown. Unit %d, Error: %d",
                      GET_ERR_UNIT(restore_result),
                      GET_ERR_CODE(restore_result));

   result_t result = m_nfc.interface.field_off(&m_nfc.interface);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to turn NFC field off. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result))
   {
      result = m_nfc.interface.chip_power_down(&m_nfc.interface);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to power down NFC chip. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   if(IS_OK(result))
   {
      state->nfc_state = NFC_POWER_STATE_LOWPOWER_OFF;
      state->next_poll_ms = now_ms + NFC_LOWPOWER_POLL_INTERVAL_MS;
      nfc_reset_ring_presence(state);
      state->medication_read_pending = false;
      state->medication_read_in_progress = false;
      state->medication_read_retry_count = 0u;
   }

   return result;
}

/**
 * @brief Manage NFC power policy, ring presence detection, and medication UID reads.
 *
 * This routine is called every control loop and coordinates two cooperating state machines:
 *
 * - NFC power-state machine (@ref NFC_POWER_STATE):
 *
 *   - @ref NFC_POWER_STATE_UNKNOWN: Startup/invalid state. The function normalizes into charger or low-power flow.
 *
 *   - @ref NFC_POWER_STATE_CHARGER_HIGH: Charger connected, Type-V enabled, high RF power selected for charging.
 *
 *   - @ref NFC_POWER_STATE_CHARGER_LOW: Charger connected, Type-V enabled, low RF power selected (ring battery full).
 *
 *   - @ref NFC_POWER_STATE_LOWPOWER_OFF: NFC chip and field are off. Wait until next poll window.
 *
 *   - @ref NFC_POWER_STATE_LOWPOWER_POLL: Short low-power on-window used to detect ring/activity.
 *
 *   - @ref NFC_POWER_STATE_LOWPOWER_ACTIVE: Extended low-power on-window while ring-manager activity is present.
 *
 * - Medication UID read sub-state machine:
 *
 *   - A ring-presence rising edge queues a medication read request.
 *
 *   - The reader switches from Type-V to Type-A and polls non-blocking for a medication tag UID.
 *
 *   - On success, timeout, or read error, the reader switches back to Type-V and the sub-state resets.
 *
 * High-level behavior:
 *
 * - If charger is connected, keep NFC active and set RF power based on ring presence and ring battery fullness.
 *
 * - If charger is disconnected and @p allow_power_saving is true, periodically wake NFC, process Type-V traffic, and
 *   power back down after poll/hold windows when no activity is detected.
 *
 * - If charger is disconnected and @p allow_power_saving is false, keep the NFC reader/field on continuously for
 *   maximum responsiveness (no low-power OFF/POLL power cycling).
 *
 * - Low-power shutdown is deferred while a medication Type-A read is in progress.
 *
 * - Ring presence output is sticky across intentional NFC power cycles: while the reader is off,
 *   this routine continues reporting the last known good ring-presence state until a fresh valid
 *   Type-V sample is taken after NFC powers back on.
 *
 * @param[out] is_ring_present Last known good Type-V ring presence state, preserved across intentional
 * power-down windows until a new valid Type-V sample is obtained.
 * @param[out] ring_uid Buffer updated with the latest ring Type-V UID, refreshed only on ring-presence rising edges.
 * @param[out] medication_uid Buffer updated with latest medication Type-A UID when available.
 * @param[out] medication_uid_stale True if the last medication UID read failed, false if the last read succeeded.
 * @param[in] battery_status Current dock battery/charger snapshot.
 * @param[in] ring_status Latest ring status snapshot used for activity and battery-fullness decisions.
 * @param[in] allow_power_saving True to allow charger-disconnected low-power NFC cycling, false to keep NFC on.
 */
static void handle_nfc(bool *is_ring_present,
                       uint8_t *ring_uid,
                       uint8_t *medication_uid,
                       bool *medication_uid_stale,
                       battery_status_t battery_status,
                       status_update_t ring_status,
                       bool allow_power_saving)
{
   if(NULL == is_ring_present || NULL == ring_uid || NULL == medication_uid || NULL == medication_uid_stale)
   {
      DEBUG_ERROR("Invalid NULL pointer in handle_nfc arguments");
      return;
   }

   result_t result = RESULT_OK;
   uint64_t now_ms = 0u;
   bool is_fresh_ring_status = false;
   bool trigger_medication_read = false;
   bool nfc_should_process = false; // True when the reader is on and m_nfc.interface.process() ran this loop.
   bool lowpower_should_process
      = false; // True when low-power NFC is currently ON and should continue low-power state-machine processing.
   bool request_lowpower_shutdown = false;
   bool charger_connected = battery_status.charger_connected;

   // Persist NFC runtime state across control-loop iterations.
   static nfc_runtime_state_t nfc_runtime = {.nfc_state = NFC_POWER_STATE_UNKNOWN, .medication_uid_stale = true};

   if(ring_status.update_time_unix_s > nfc_runtime.prev_ring_status.update_time_unix_s)
   {
      // Fresh ring status available, update runtime state and make decisions based on it.
      is_fresh_ring_status = true;
      memcpy(&nfc_runtime.prev_ring_status, &ring_status, sizeof(status_update_t));
      nfc_runtime.ring_battery_full
         = (BATTERY_STATE_CHARGING_COMPLETED == ring_status.ring_status.battery_charge_status);
   }

   result = m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // For tracing
   static NFC_POWER_STATE prev_state = NFC_POWER_STATE_UNKNOWN;
   if(nfc_runtime.nfc_state != prev_state)
   {
      // State changed. Log a trace
      DEBUG_TRACE("NFC power state: %s (%d) systick (ms): %d",
                  nfc_power_state_to_str(nfc_runtime.nfc_state),
                  nfc_runtime.nfc_state,
                  (uint32_t)now_ms);
      prev_state = nfc_runtime.nfc_state;
   }

   // Continuous-reader mode:
   // - Charger connected (existing behavior for charging control), or
   // - Charger disconnected but caller requests NFC responsiveness (power saving disabled).
   if(IS_OK(result) && (charger_connected || !allow_power_saving))
   {
      bool should_wake_reader = (NFC_POWER_STATE_LOWPOWER_OFF == nfc_runtime.nfc_state)
                                || (NFC_POWER_STATE_UNKNOWN == nfc_runtime.nfc_state);

      if(should_wake_reader)
      {
         result = nfc_power_up_and_select_type_v(NFC_POWER_LEVEL_LOW);
      }

      if(IS_OK(result))
      {
         result = m_nfc.interface.process(&m_nfc.interface, true);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to process NFC. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         if(IS_OK(result))
         {
            nfc_should_process = true;
         }
      }

      if(IS_OK(result) && !nfc_runtime.medication_read_in_progress)
      {
         result = nfc_update_ring_presence(&nfc_runtime, now_ms, &trigger_medication_read);
      }

      if(IS_OK(result))
      {
         uint8_t target_power = NFC_POWER_LEVEL_LOW;
         if(nfc_runtime.medication_read_in_progress)
         {
            // Keep RF power high during Type-A medication UID reads.
            target_power = NFC_POWER_LEVEL_HIGH;
         }
         else if(charger_connected && nfc_runtime.last_ring_present)
         {
            target_power = nfc_runtime.ring_battery_full ? NFC_POWER_LEVEL_LOW : NFC_POWER_LEVEL_MED;
         }

         result = m_nfc.interface.set_output_power(&m_nfc.interface, target_power);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to set NFC output power. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

         if(IS_OK(result))
         {
            if(charger_connected)
            {
               nfc_runtime.nfc_state
                  = nfc_runtime.ring_battery_full ? NFC_POWER_STATE_CHARGER_LOW : NFC_POWER_STATE_CHARGER_HIGH;
            }
            else
            {
               // Keep reader marked as active while power saving is disabled.
               nfc_runtime.nfc_state = NFC_POWER_STATE_LOWPOWER_ACTIVE;
            }
         }
      }
   }

   // Charger-disconnected mode (power-saving enabled): run low-power poll/active/off state machine.
   if(IS_OK(result) && !charger_connected && allow_power_saving)
   {
      bool entered_from_charger_state = (NFC_POWER_STATE_CHARGER_HIGH == nfc_runtime.nfc_state)
                                        || (NFC_POWER_STATE_CHARGER_LOW == nfc_runtime.nfc_state)
                                        || (NFC_POWER_STATE_UNKNOWN == nfc_runtime.nfc_state);
      if(entered_from_charger_state)
      {
         result = m_nfc.interface.set_output_power(&m_nfc.interface, NFC_POWER_LEVEL_LOW);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to set NFC output power. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

         if(IS_OK(result))
         {
            nfc_runtime.nfc_state = NFC_POWER_STATE_LOWPOWER_POLL;
            nfc_runtime.poll_start_ms = now_ms;
         }
      }

      if(IS_OK(result))
      {
         switch(nfc_runtime.nfc_state)
         {
            case NFC_POWER_STATE_LOWPOWER_OFF:
               if(now_ms >= nfc_runtime.next_poll_ms)
               {
                  result = nfc_enter_lowpower_poll_window(&nfc_runtime, now_ms);
                  if(IS_OK(result))
                  {
                     lowpower_should_process = true;
                  }
               }
               break;

            case NFC_POWER_STATE_LOWPOWER_POLL:
            // Fall through to allow processing in both POLL and ACTIVE states. The difference is that in POLL state we
            // will transition to OFF after the poll window expires regardless of activity, whereas in ACTIVE state
            // we require inactivity to transition.
            case NFC_POWER_STATE_LOWPOWER_ACTIVE:
               lowpower_should_process = true;
               break;

            case NFC_POWER_STATE_UNKNOWN:
            // Fall through
            case NFC_POWER_STATE_CHARGER_HIGH:
            // Fall through
            case NFC_POWER_STATE_CHARGER_LOW:
            // Fall through
            case NFC_POWER_STATE_MAX:
            // Fall through
            default:
               nfc_runtime.nfc_state = NFC_POWER_STATE_LOWPOWER_OFF;
               nfc_runtime.next_poll_ms = now_ms + NFC_LOWPOWER_POLL_INTERVAL_MS;
               nfc_reset_ring_presence(&nfc_runtime);
               break;
         }
      }

      if(IS_OK(result) && lowpower_should_process)
      {
         result = m_nfc.interface.process(&m_nfc.interface, true);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to process NFC. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         if(IS_OK(result))
         {
            nfc_should_process = true;
         }

         if(IS_OK(result) && !nfc_runtime.medication_read_in_progress)
         {
            result = nfc_update_ring_presence(&nfc_runtime, now_ms, &trigger_medication_read);
         }

         bool has_activity = false;
         if(IS_OK(result) && is_fresh_ring_status)
         {
            has_activity = nfc_ring_status_has_activity(&ring_status);
            if(has_activity)
            {
               nfc_runtime.last_activity_ms = now_ms;
               if(NFC_POWER_STATE_LOWPOWER_POLL == nfc_runtime.nfc_state)
               {
                  nfc_runtime.nfc_state = NFC_POWER_STATE_LOWPOWER_ACTIVE;
               }
            }
         }

         if(IS_OK(result))
         {
            uint32_t lowpower_poll_min_on_time_ms = NFC_LOWPOWER_POLL_ON_TIME_MS;
            if((NFC_POWER_STATE_LOWPOWER_POLL == nfc_runtime.nfc_state) && nfc_runtime.last_ring_present
               && !nfc_runtime.ring_presence_valid)
            {
               lowpower_poll_min_on_time_ms = NFC_RING_PRESENCE_REVALIDATE_ON_TIME_MS;
            }

            if((NFC_POWER_STATE_LOWPOWER_POLL == nfc_runtime.nfc_state) && !has_activity
               && (now_ms - nfc_runtime.poll_start_ms >= lowpower_poll_min_on_time_ms))
            {
               request_lowpower_shutdown = true;
            }
            else if((NFC_POWER_STATE_LOWPOWER_ACTIVE == nfc_runtime.nfc_state) && !has_activity
                    && (now_ms - nfc_runtime.last_activity_ms >= NFC_LOWPOWER_ACTIVE_HOLD_MS))
            {
               request_lowpower_shutdown = true;
            }
         }
      }
   }

   if(trigger_medication_read)
   {
      nfc_runtime.medication_read_pending = true;
      nfc_runtime.medication_uid_stale = true;
   }

   if(IS_OK(result) && nfc_should_process)
   {
      nfc_process_medication_read_state_machine(&nfc_runtime, now_ms);
   }

   if(IS_OK(result) && !charger_connected && allow_power_saving && lowpower_should_process && request_lowpower_shutdown
      && !nfc_runtime.medication_read_in_progress)
   {
      (void)nfc_shutdown_to_lowpower_off(&nfc_runtime, now_ms);
   }

   memcpy(ring_uid, nfc_runtime.last_good_ring_uid, NFC_TAG_MAX_UID_SIZE);
   memcpy(medication_uid, nfc_runtime.last_good_medication_uid, NFC_TAG_MAX_UID_SIZE);
   *medication_uid_stale = nfc_runtime.medication_uid_stale;
   *is_ring_present = nfc_runtime.last_ring_present;
}

static void reset_baselining_state_machine_inputs(void)
{
   m_baseline_sm_inputs.start_baselining = false;
   m_baseline_sm_inputs.is_baselining_active = false;
   m_baseline_sm_inputs.is_ring_present = false;
   m_baseline_sm_inputs.is_medication_uid_avail = false;
   m_baseline_sm_inputs.is_empty_dock_weight_set = false;
   m_baseline_sm_inputs.is_ring_dock_full_med_weight_set = false;
   m_baseline_sm_inputs.med_update_status = MED_UID_STORAGE_NONE;
   m_baseline_sm_inputs.backend_validation_status = BACKEND_VAL_STATUS_NONE;
   m_baseline_sm_inputs.dose_schedule_status = DOSE_SCH_STATUS_NONE;
}

static void reset_calibration_state_machine_inputs(void)
{
   m_calibrate_sm_inputs.start_calibration = false;
   m_calibrate_sm_inputs.is_calibration_active = false;
   m_calibrate_sm_inputs.is_ring_present = false;
   m_calibrate_sm_inputs.data_update_status = DATA_STORAGE_NONE;
   m_calibrate_sm_inputs.is_empty_dock_weight_set = false;
   m_calibrate_sm_inputs.is_calibration_weight_set = false;
   m_calibrate_sm_inputs.is_calibration_weight_present = false;
}

static result_t get_dock_status(dock_status_t *dock_status)
{
   RETURN_ERR_IF_NULL(dock_status, GC_ERROR_PTR_NULL);

   ship_mode_t current_ship_mode = {0};
   uint32_t current_unix_time_s = 0u;
   uint64_t current_systic_time_ms = 0u;
   result_t result = RESULT_OK;

   // Get FICR->DEVICE_ID - 64 bit unique device identifier pre-programmed in factory and cannot be erased by the user.
   uint32_t dev_id0 = NRF_FICR->DEVICEID[0];
   uint32_t dev_id1 = NRF_FICR->DEVICEID[1];

   result = m_rtc.interface.get_time_unix(&m_rtc.interface, &current_unix_time_s);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get current unix time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result))
   {
      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systic_time_ms);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get current systick time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   if(IS_OK(result))
   {
      result = m_dock_data_manager.interface.get_ship_mode_data(&m_dock_data_manager.interface, &current_ship_mode);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to retrieve ship mode data. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   if(IS_OK(result))
   {
      // Compile status
      dock_status_t current_status = {0};
      version_parse_triplet_u8(DOCK_APP_VERSION,
                               &current_status.firmware_version.major,
                               &current_status.firmware_version.minor,
                               &current_status.firmware_version.patch);

      current_status.hardware_version.major
         = (uint8_t)NRF_DFU_HW_VERSION; // TODO Verify this. It needs to get this from the makefile.
      DEBUG_TRACE("NRF_DFU_HW_VERSION = %d", current_status.hardware_version.major);
      current_status.hardware_version.minor = 0u;
      current_status.hardware_version.patch = 0u;
      // Pack as little-endian bytes
      current_status.mac[0] = (uint8_t)(dev_id0 & 0xFF);
      current_status.mac[1] = (uint8_t)((dev_id0 >> 8) & 0xFF);
      current_status.mac[2] = (uint8_t)((dev_id0 >> 16) & 0xFF);
      current_status.mac[3] = (uint8_t)((dev_id0 >> 24) & 0xFF);
      current_status.mac[4] = (uint8_t)(dev_id1 & 0xFF);
      current_status.mac[5] = (uint8_t)((dev_id1 >> 8) & 0xFF);
      current_status.mac[6] = (uint8_t)((dev_id1 >> 16) & 0xFF);
      current_status.mac[7] = (uint8_t)((dev_id1 >> 24) & 0xFF);
      current_status.ship_mode_exit_timestamp_unix = current_ship_mode.exit_ship_mode_unix_s;
      current_status.timestamp_unix_s = current_unix_time_s;
      current_status.uptime_s = (uint32_t)(current_systic_time_ms / 1000u);
      *dock_status = current_status;
   }

   return result;
}

static result_t push_dock_charge_status(BATTERY_STATE battery_status)
{
   RETURN_OK_IF_TRUE(battery_status == m_prev_dock_charge_status);
   result_t result = RESULT_OK;
   uint32_t unix_time_s = 0u;
   dock_charge_status_t charge_status = {0};

   // Update current charge status
   charge_status.charge_status = battery_status;

   result = m_rtc.interface.get_time_unix(&m_rtc.interface, &unix_time_s);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get unix time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // Update dock charge status with unix timestamp of when the change was observed
   if(IS_OK(result))
   {
      charge_status.timestamp_unix_s = unix_time_s;
   }

   // enqueue to dock manager
   IF_OK_RUN_AND_UPDATE(result,
                        m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                              DATA_ID_DOCK_CHARGE_STATUS,
                                                              &charge_status,
                                                              DOCK_CHARGE_STATUS_T_SIZE_BYTES,
                                                              1u));

   // Update previous charge status only if enqueue succeeded
   if(IS_OK(result))
   {
      m_prev_dock_charge_status = battery_status;
   }

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue dock charge status message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));

   return result;
}

#ifdef DEBUG
static void clear_app_manager_in_flight_tx_for_hil_seed(void)
{
   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_MAX;
   result_t tx_status_result = m_msg_prot_ble.interface.get_tx_packet_status(&m_msg_prot_ble.interface, &tx_status);

   if(IS_OK(tx_status_result))
   {
      DEBUG_INFO("[HIL seed] Pre-seed TX ownership: app_mgr_active=%u data_id=%u ppi=%u type=%u mp_tx_status=%u",
                 (uint32_t)m_app_manager._current_tx_transaction.active,
                 (uint32_t)m_app_manager._current_tx_transaction.data_id,
                 (uint32_t)m_app_manager._current_tx_transaction.ppi,
                 (uint32_t)m_app_manager._current_tx_transaction.type,
                 (uint32_t)tx_status);
   }
   else
   {
      DEBUG_ERROR("[HIL seed] Failed to read message-protocol TX status. Unit %d, Error: %d",
                  GET_ERR_UNIT(tx_status_result),
                  GET_ERR_CODE(tx_status_result));
   }

   // Prevent stale completion callbacks from popping freshly seeded queue entries.
   m_app_manager._current_tx_transaction.active = false;
   m_app_manager._current_tx_transaction.data_id = DATA_ID_MAX;
   m_app_manager._current_tx_transaction.ppi = PPI_AD_MAX;
   m_app_manager._current_tx_transaction.type = PPI_TYPE_MAX;

   DEBUG_INFO("[HIL seed] Cleared app-manager TX ownership before seeding.");
}

// This function clears ALL data queues in the dock data manager. It is intended for testing purposes only to ensure a
// clean slate of data for test scenarios, and should be used with caution.
static void dequeue_all_dock_push_data(void)
{
   result_t result = RESULT_OK;

   for(size_t data_id = 0u; data_id < DATA_ID_MAX; data_id++)
   {
      size_t count = 0u;
      result = m_dock_data_manager.interface.get_element_count(&(m_dock_data_manager.interface), data_id, &count);

      if(IS_OK(result) && (count > 0u))
      {
         result = m_dock_data_manager.interface.pop(&(m_dock_data_manager.interface), data_id, count);

         ON_ERR_DEBUG_ERROR(result,
                            "Failed to pop existing data ID %d entries from dock data manager. Unit %d, Error: %d",
                            (int)data_id,
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
      }
   }

   // Send response back to app manager indicating success of the dequeue operation so that tests can verify this before
   // proceeding.
   if(IS_OK(result))
   {
      // Construct response payload
      mp_packet_payload_t tx_msg = {0};
      tx_msg.type = PPI_TYPE_RE;
      tx_msg.ppi = PPI_AD_DEVELOPMENT_CMD;
      tx_msg.payload[0] = 1u; // Just indicates success
      tx_msg.pkt_payload_len = 1u;
      result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &tx_msg);
      ON_ERR_DEBUG_ERROR(result,
                         "Failed to enqueue dev cmd success response to app manager. Unit %d, Error: %d",
                         GET_ERR_UNIT(result),
                         GET_ERR_CODE(result));
   }
}

static void enqueue_hil_test_dose_event(void)
{
   dose_event_t hil_test_dose_event = {.event_id = {.days_since_epoch = 12345u, .event_ctr = 20u},
                                       .start_timestamp_unix_s = 1697049600u, // Oct 11, 2023 @ 12:00:00 PM UTC
                                       .duration_s = 240u,                    // 4 minutes
                                       .dose_completed_in_time = 1u,          // true
                                       .tilt_count = 3u};

   result_t result = m_dock_data_manager.interface.enqueue(
      &m_dock_data_manager.interface, DATA_ID_DOSE_EVENT, &hil_test_dose_event, sizeof(dose_event_t), 1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test dose event message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_ring_docked_status(void)
{
   ring_docked_status_t hil_test_ring_docked_status = {
      .timestamp_unix_s = 1697049600u, // Oct 11, 2023 @ 12:00:00 PM UTC
      .docked_status = DOCKED_STATUS_DOCKED,
      .ring_nfc_id = {0xDEu, 0xADu, 0xBEu, 0xEFu, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u},
      .medication_nfc_id = {0xBAu, 0xADu, 0xF0u, 0x0Du, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u},
   };

   result_t result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                           DATA_ID_RING_DOCKED_STATUS,
                                                           &hil_test_ring_docked_status,
                                                           sizeof(ring_docked_status_t),
                                                           1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test ring docked status message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_weight_measurement(void)
{
   dock_weight_measurement_t hil_test_weight_measurement = {
      .weight_mg = 50000,              // 50 grams
      .total_dispensed_mg = 10u,       // 10 mgrams
      .std_dev = 100u,                 // Example standard deviation
      .temperature_deg_c_div10 = 250,  // 25.0 degrees Celsius
      .timestamp_unix_s = 1697049600u, // Oct 11, 2023 @ 12:00:00 PM UTC
   };

   result_t result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                           DATA_ID_WEIGHT_MEASUREMENT,
                                                           &hil_test_weight_measurement,
                                                           sizeof(dock_weight_measurement_t),
                                                           1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test weight measurement message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_temperature_log(void)
{
   temperature_log_t hil_test_temperature_log = {
      .temperature_deg_c = 25,         // 25.0 degrees Celsius
      .timestamp_unix_s = 1697049600u, // Oct 11, 2023 @ 12:00:00 PM UTC
   };

   result_t result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                           DATA_ID_TEMPERATURE_LOG,
                                                           &hil_test_temperature_log,
                                                           sizeof(temperature_log_t),
                                                           1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test temperature log message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_ring_battery_level(void)
{
   battery_level_t hil_test_ring_battery_level = {
      .timestamp_unix_s = 1697049600u, // Oct 11, 2023 @ 12:00:00 PM UTC
      .battery_level = 85u,            // 85% battery
   };

   result_t result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                           DATA_ID_RING_BATTERY_LEVEL,
                                                           &hil_test_ring_battery_level,
                                                           sizeof(battery_level_t),
                                                           1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test ring battery level message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_dock_battery_level(void)
{
   battery_level_t hil_test_dock_battery_level = {
      .timestamp_unix_s = 1697049600u, // Oct 11, 2023 @ 12:00:00 PM UTC
      .battery_level = 70u,            // 70% battery
   };

   result_t result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                           DATA_ID_DOCK_BATTERY_LEVEL,
                                                           &hil_test_dock_battery_level,
                                                           sizeof(battery_level_t),
                                                           1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test dock battery level message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_dock_charge_status(void)
{
   dock_charge_status_t hil_test_dock_charge_status = {
      .timestamp_unix_s = 1772616072u,
      .charge_status = (uint8_t)BATTERY_STATE_CHARGING, // Charge status of the dock battery. maps to BATTERY_STATE enum
   };

   result_t result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                           DATA_ID_DOCK_CHARGE_STATUS,
                                                           &hil_test_dock_charge_status,
                                                           sizeof(dock_charge_status_t),
                                                           1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test dock charge status message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_ring_status(void)
{
   ring_status_t hil_test_ring_status = {
      .hardware_version = {.major = 1u, .minor = 0u, .patch = 1u},
      .firmware_version = {.major = 1u, .minor = 0u, .patch = 1u},
      .mac = {0xDEu, 0xADu, 0xBEu, 0xEFu, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u},
      .timestamp_unix_s = 1772616496u,
      .ship_mode_exit_timestamp_unix = 1697049600u,
      .battery_charge_status = (uint8_t)BATTERY_STATE_CHARGING,
      .uptime_s = 3600u,
      .temperature_celsius = 25,
      .dose_fifo_used_percent = 50u,
      .battery_fifo_used_percent = 30u,
      .imu_fifo_used_percent = 20u,
      .error_fifo_used_percent = 10u,
      .dose_fifo_used_percent_watermark = 80u,
      .battery_fifo_used_percent_watermark = 60u,
      .imu_fifo_used_percent_watermark = 40u,
      .error_fifo_used_percent_watermark = 20u,
      .battery_sample_frequency_millihz = 1000u,
   };

   result_t result = m_dock_data_manager.interface.enqueue(
      &m_dock_data_manager.interface, DATA_ID_RING_STATUS, &hil_test_ring_status, sizeof(ring_status_t), 1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test ring status message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_dock_status(void)
{
   dock_status_t hil_test_dock_status = {
      .firmware_version = {.major = 1u, .minor = 0u, .patch = 1u},
      .hardware_version = {.major = 1u, .minor = 0u, .patch = 1u},
      .mac = {0xDEu, 0xADu, 0xBEu, 0xEFu, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u},
      .ship_mode_exit_timestamp_unix = 1697049600u,
      .timestamp_unix_s = 1772616496u,
      .uptime_s = 3600u,
   };

   result_t result = m_dock_data_manager.interface.enqueue(
      &m_dock_data_manager.interface, DATA_ID_DOCK_STATUS, &hil_test_dock_status, sizeof(dock_status_t), 1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test dock status message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_dock_debug_log(void)
{
   raw_debug_log_t hil_test_dock_debug_log = {
      .level = (uint8_t)DEBUG_LEVEL_INFO,
      .timestamp = 1772616496u,
      .module_id = (uint8_t)SW_UNIT_ID_GENERAL_CONTROL_DOCK,
      .line = 3358u,
      .arg_values = {0xDEADBEEFu, 0xCAFEBABEu, 0xFEEDFACEu, 0xBAADF00Du, 0x0D15EA5Eu, 0xC001D00Du},

   };

   result_t result = m_dock_data_manager.interface.enqueue(
      &m_dock_data_manager.interface, DATA_ID_DOCK_DEBUG_LOG, &hil_test_dock_debug_log, sizeof(raw_debug_log_t), 1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test dock debug log message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

static void enqueue_hil_test_ring_debug_log(void)
{
   raw_debug_log_t hil_test_ring_debug_log = {
      .level = (uint8_t)DEBUG_LEVEL_INFO,
      .timestamp = 1772616496u,
      .module_id = (uint8_t)SW_UNIT_ID_GENERAL_CONTROL_RING,
      .line = 3358u,
      .arg_values = {0xDEADBEEFu, 0xCAFEBABEu, 0xFEEDFACEu, 0xBAADF00Du, 0x0D15EA5Eu, 0xC001D00Du},

   };

   result_t result = m_dock_data_manager.interface.enqueue(
      &m_dock_data_manager.interface, DATA_ID_RING_DEBUG_LOG, &hil_test_ring_debug_log, sizeof(raw_debug_log_t), 1u);

   ON_ERR_DEBUG_ERROR(result,
                      "Failed to enqueue HIL test ring debug log message. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}
#endif

/**
 * @note This function is for testing only. It allows commands to be sent to the device to fasciliate testing.
 */
static void handle_testing_commands_from_app(APP2DOCK_COMMANDS command)
{
   switch(command)
   {
      case APP2DOCK_COMMANDS_NONE:
         // Nothing to do
         break;

      case APP2DOCK_COMMANDS_ENTER_SHIP_MODE:
         enter_ship_mode();
         break;

      case APP2DOCK_COMMANDS_ENABLE_DFU:
         // Nothing to do
         break;

#ifdef DEBUG
      case APP2DOCK_COMMANDS_RESET_DOCK:
         NVIC_SystemReset();
         break;

      case APP2DOCK_COMMANDS_POPULATE_DOCK_PUSH_DATA:
      {
         DEBUG_INFO("Populating dock with push data for testing.");
         clear_app_manager_in_flight_tx_for_hil_seed();
         (void)dequeue_all_dock_push_data();
         (void)enqueue_hil_test_dose_event();
         (void)enqueue_hil_test_ring_docked_status();
         (void)enqueue_hil_test_weight_measurement();
         (void)enqueue_hil_test_temperature_log();
         (void)enqueue_hil_test_ring_battery_level();
         (void)enqueue_hil_test_dock_battery_level();
         (void)enqueue_hil_test_dock_charge_status();
         (void)enqueue_hil_test_ring_status();
         (void)enqueue_hil_test_dock_status();
         (void)enqueue_hil_test_dock_debug_log();
         (void)enqueue_hil_test_ring_debug_log();
         break;
      }
#endif

      case APP2DOCK_COMMANDS_MAX:
         break;

      default:
         DEBUG_ERROR("Received unknown command from app: %d", command);
         break;
   }
}

// Checks for a normal, active BLE connection with a bonded peer and isn't currently in BLE pairing mode. This function
// is used to check if there is a valid BLE connection to allow processing of PPIs.
static inline bool is_ble_valid(void)
{
   return (m_ble_status.is_bonded && m_ble_status.is_connected && !m_ble_status.allow_new_bond);
}

static result_t parse_dose_schedule(const uint8_t *p_data, uint16_t length, dose_schedule_t *p_sched)
{
   RETURN_ERR_IF_NULL(p_data, GC_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(p_sched, GC_ERROR_PTR_NULL);

   if(length != sizeof(dose_schedule_t))
   {
      DEBUG_ERROR("Invalid input length: %d, expected: %d", length, sizeof(dose_schedule_t));
      RETURN_ERR(GC_ERROR_INVALID_DOSE_SCHEDULE_RECEIVED);
   }

   dose_schedule_t temp_schedule = {0};

   // Copy dose schedule
   // Note: Dose schedule is packed and aligned to 1-byte boundaries and already in little-endian format.
   // We can therefore do a direct memory copy.
   memcpy(&temp_schedule, p_data, sizeof(dose_schedule_t));

   // Validate schedule
   result_t result = validate_dose_schedule(&temp_schedule);

   // Copy to output
   if(IS_OK(result))
   {
      memcpy(p_sched, &temp_schedule, sizeof(dose_schedule_t));
   }

   return result;
}

static void process_incoming_app_messages(uint32_t *rx_calibration_weight_mg, bool *is_weight_present)
{
   if((NULL == rx_calibration_weight_mg) || (NULL == is_weight_present))
   {
      DEBUG_ERROR("NULL ptr received.");
      return;
   }

   result_t result = RESULT_OK;
   mp_packet_payload_t rx_msg = {0};
   size_t rx_count = 0u;
   bool valid_ble_connection = false;

   // Check if there are any incoming messages from the app manager
   result = m_queue_app_manager_rx.interface.get_count(&m_queue_app_manager_rx.interface, &rx_count);

   if(IS_OK(result) && (rx_count > 0))
   {
      DEBUG_TRACE("Processing %u incoming messages from app manager.", rx_count);

      for(uint16_t idx = 0u; idx < rx_count; idx++)
      {
         // Pop message from queue
         result = m_queue_app_manager_rx.interface.dequeue(&m_queue_app_manager_rx.interface, &rx_msg);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to dequeue message from app manager. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));

         /**
          * Check for valid BLE connection before processing PPI. Discard PPI message if no valid BLE connection. The
          * check for a valid BLE connection is intentionally done here to allow reporting which PPI was received during
          * a bad BLE connection to assist debugging efforts.
          */
         if(IS_OK(result))
         {
            if(is_ble_valid())
            {
               valid_ble_connection = true;
            }
            else
            {
               DEBUG_WARNING(
                  "No valid BLE connection, discarding rx PPI message. PPI %d, type %d", rx_msg.ppi, rx_msg.type);
            }
         }

         if(IS_OK(result) && valid_ble_connection)
         {
            // Process the message
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_START_BASELINING == rx_msg.ppi))
            {
               // Parse message payload
               bool start_baselining_request = false;
               if(sizeof(uint8_t) == rx_msg.pkt_payload_len)
               {
                  start_baselining_request = (rx_msg.payload[0u] >= 1u) ? true : false;

                  // Handle baselining request
                  if(start_baselining_request)
                  {
                     // let App know the baselining process has started.
                     mp_packet_payload_t tx_msg = {0};
                     tx_msg.type = (uint8_t)PPI_TYPE_RE;
                     tx_msg.ppi = (uint8_t)PPI_AD_START_BASELINING;
                     tx_msg.payload[0] = (uint8_t)true;
                     tx_msg.pkt_payload_len = sizeof(uint8_t);
                     result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
                     ON_ERR_DEBUG_ERROR(result,
                                        "Failed to enqueue msg. Unit %d, Error: %d",
                                        GET_ERR_UNIT(result),
                                        GET_ERR_CODE(result));

                     // Start baselining flow
                     DEBUG_TRACE("Received baselining START request from App. Starting baselining flow.");
                     m_system_sm_inputs.is_baselining_active = true;

                     // Init baselining state inputs
                     // Reset baselining state inputs
                     reset_baselining_state_machine_inputs();
                     m_baseline_sm_inputs.start_baselining = true;
                     m_baseline_sm_inputs.is_baselining_active = true;
                  }
                  else
                  {
                     // Stop baselining flow.
                     // Do NOT set the m_system_sm_inputs.is_baselining_active to false here because
                     // we want the system state machine to continue to execute the baselining state logic until the
                     // baselining state machine has completed all necessary cleanup in the baselining state. The
                     // m_system_sm_inputs.is_baselining_active will be set to false in the baselining state once the
                     // baselining state machine signals that it has completed cleanup and is ready to exit the
                     // baselining state.

                     DEBUG_TRACE("Received baselining STOP request from App. Stopping baselining flow.");
                     m_baseline_sm_inputs.is_baselining_active = false;

                     // Ensure DSD-only baselining state is rolled back immediately when the flow is canceled.
                     result_t abort_result = m_dsd.interface.abort_baselining(&m_dsd.interface);
                     ON_ERR_DEBUG_ERROR(abort_result,
                                        "Failed to abort DSD baselining. Unit %d, Error: %d",
                                        GET_ERR_UNIT(abort_result),
                                        GET_ERR_CODE(abort_result));

                     // Reset baselining state inputs
                     reset_baselining_state_machine_inputs();
                  }
               }
               else
               {
                  DEBUG_ERROR(
                     "Invalid payload length for baselining request message. Expected %d, got %d. Ignoring message.",
                     sizeof(uint8_t),
                     rx_msg.pkt_payload_len);
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RE == rx_msg.type) && (PPI_AD_VALIDATE_MED == rx_msg.ppi))
            {
               // Parse message payload
               if(sizeof(uint8_t) == rx_msg.pkt_payload_len)
               {
                  if(rx_msg.payload[0u] >= 1u)
                  {
                     m_baseline_sm_inputs.backend_validation_status = BACKEND_VAL_STATUS_SUCCESS;
                  }
                  else
                  {
                     m_baseline_sm_inputs.backend_validation_status = BACKEND_VAL_STATUS_UNSUCCESSFUL;
                  }
               }
               else
               {
                  DEBUG_ERROR(
                     "Invalid payload length for baselining request message. Expected %d, got %d. Ignoring message.",
                     sizeof(uint8_t),
                     rx_msg.pkt_payload_len);
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_PUSH == rx_msg.type) && (PPI_AD_DOSE_SCHEDULE == rx_msg.ppi))
            {
               // Parse message payload
               if(DOSE_SCHEDULE_SIZE_BYTES == rx_msg.pkt_payload_len)
               {
                  dose_schedule_record_t schedule_record = {0};

                  result = parse_dose_schedule(rx_msg.payload, rx_msg.pkt_payload_len, &schedule_record.dose_schedule);

                  if(IS_OK(result))
                  {
                     uint32_t now_unix_seconds = 0u;
                     result = m_rtc.interface.get_time_unix(&m_rtc.interface, &now_unix_seconds);
                     ON_ERR_DEBUG_ERROR(result,
                                        "Failed to get current unix time. Unit %d, Error: %d",
                                        GET_ERR_UNIT(result),
                                        GET_ERR_CODE(result));

                     schedule_record.start_date_unix_seconds = STRIP_TIME_UNIX32(now_unix_seconds);
                  }

                  IF_OK_RUN_AND_UPDATE(
                     result,
                     m_dose_scheduler.interface.load_schedule(&m_dose_scheduler.interface,
                                                              &schedule_record.dose_schedule,
                                                              schedule_record.start_date_unix_seconds));

                  if(IS_OK(result))
                  {
                     result = m_dock_data_manager.interface.set_dose_schedule_record(&m_dock_data_manager.interface,
                                                                                     &schedule_record);
                     ON_ERR_DEBUG_ERROR(result,
                                        "Failed to store dose schedule. Unit %d, Error: %d",
                                        GET_ERR_UNIT(result),
                                        GET_ERR_CODE(result));
                  }

                  if(IS_OK(result))
                  {
                     // Successfully updated dose schedule.
                     /**
                      * Note: m_system_sm_inputs.is_valid_dose_schedule_detected can be set here regardless of
                      * whether or not the system is in the baselining state since the
                      * m_system_sm_inputs.is_valid_dose_schedule_detected gets automatically gets reset when the
                      * baselining process is started. So a async dose schedule push unrelated to the baselining will
                      * not affect the baselining state machine.
                      */

                     m_system_sm_inputs.is_valid_dose_schedule_detected = DOSE_SCH_STATUS_VALID;

                     uint16_t avg_window_size
                        = (uint16_t)((uint32_t)(schedule_record.dose_schedule.temp_avg_window_duration_sec)
                                     / (TEMPERATURE_UPDATE_INTERVAL_MS / COMMON_1K_FACTOR));
                     moving_average_set_length(avg_window_size);

                     // If the baselining state machine is waiting for backend validation and a dose schedule
                     if(BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION == m_baseline_sm.current_state)
                     {
                        m_baseline_sm_inputs.dose_schedule_status = DOSE_SCH_STATUS_VALID;
                     }
                  }
                  else
                  {
                     // Bad dose schedule received.
                     /**
                      * @note Do not set the m_system_sm_inputs.is_valid_dose_schedule_detected = false here. The
                      * device could have a valid dose schedule already loaded and this new invalid schedule should
                      * not invalidate the existing valid schedule.
                      */
                     DEBUG_ERROR("Failed to load new dose schedule. Unit %d, Error: %d",
                                 GET_ERR_UNIT(result),
                                 GET_ERR_CODE(result));

                     // If the baselining state machine is waiting for backend validation and a dose schedule
                     if(BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION == m_baseline_sm.current_state)
                     {
                        m_baseline_sm_inputs.dose_schedule_status = DOSE_SCH_STATUS_INVALID;
                     }
                  }
               }
               else
               {
                  DEBUG_ERROR(
                     "Invalid payload length for baselining request message. Expected %d, got %d. Ignoring message.",
                     sizeof(uint8_t),
                     rx_msg.pkt_payload_len);
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_DOSE_SCHEDULE == rx_msg.ppi))
            {
               // App requested the current dose schedule.

               // Get schedule
               dose_schedule_record_t schedule_record = {0};
               result = m_dock_data_manager.interface.get_dose_schedule_record(&m_dock_data_manager.interface,
                                                                               &schedule_record);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to get current dose schedule. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
               if(IS_OK(result))
               {
                  // Construct payload (LE)
                  mp_packet_payload_t tx_msg = {0};
                  tx_msg.type = PPI_TYPE_RE;
                  tx_msg.ppi = PPI_AD_DOSE_SCHEDULE;

                  // Note: Dose schedule is packed and aligned to 1-byte boundaries and already in little-endian format.
                  // We can therefore do a direct memory copy.
                  memcpy(tx_msg.payload, &schedule_record.dose_schedule, sizeof(dose_schedule_t));

                  tx_msg.pkt_payload_len = sizeof(dose_schedule_t);

                  if(DOSE_SCHEDULE_SIZE_BYTES == tx_msg.pkt_payload_len)
                  {
                     // send response message (if active connection, otherwise drop it)
                     result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &tx_msg);
                     ON_ERR_DEBUG_ERROR(
                        result,
                        "Failed to enqueue dose schedule response message to app manager. Unit %d, Error: %d",
                        GET_ERR_UNIT(result),
                        GET_ERR_CODE(result));
                  }
                  else
                  {
                     DEBUG_ERROR("Bad encoding");
                  }
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_DOCK_STATUS == rx_msg.ppi))
            {
               // App requested the current dock status.

               dock_status_t current_status = {0};
               result = get_dock_status(&current_status);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to get current dock status. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
               if(IS_OK(result))
               {
                  // Construct payload (LE)
                  mp_packet_payload_t tx_msg = {0};
                  tx_msg.type = PPI_TYPE_RE;
                  tx_msg.ppi = PPI_AD_DOCK_STATUS;
                  size_t offset = 0u;
                  tx_msg.payload[offset++] = current_status.hardware_version.major;
                  tx_msg.payload[offset++] = current_status.hardware_version.minor;
                  tx_msg.payload[offset++] = current_status.hardware_version.patch;
                  tx_msg.payload[offset++] = current_status.firmware_version.major;
                  tx_msg.payload[offset++] = current_status.firmware_version.minor;
                  tx_msg.payload[offset++] = current_status.firmware_version.patch;
                  memcpy(&tx_msg.payload[offset], current_status.mac, MAC_ADDRESS_SIZE_BYTES);
                  offset += MAC_ADDRESS_SIZE_BYTES;
                  tx_msg.payload[offset++] = (uint8_t)(current_status.timestamp_unix_s & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.timestamp_unix_s >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.timestamp_unix_s >> 16u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.timestamp_unix_s >> 24u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)(current_status.ship_mode_exit_timestamp_unix & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.ship_mode_exit_timestamp_unix >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.ship_mode_exit_timestamp_unix >> 16u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.ship_mode_exit_timestamp_unix >> 24u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)(current_status.uptime_s & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.uptime_s >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.uptime_s >> 16u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((current_status.uptime_s >> 24u) & 0xFFu);
                  tx_msg.pkt_payload_len = (uint16_t)offset;

                  // send response message (if active connection, otherwise drop it)
                  result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &tx_msg);
                  ON_ERR_DEBUG_ERROR(
                     result,
                     "Failed to enqueue dock status response message to app manager. Unit %d, Error: %d",
                     GET_ERR_UNIT(result),
                     GET_ERR_CODE(result));
               }

            } ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_RING_STATUS == rx_msg.ppi))
            {
               // App requested the current ring status.

               status_update_t current_status = {0};
               const ring_status_t *ring_status = &current_status.ring_status;

               result = m_ring.interface.get_ring_status(&m_ring.interface, &current_status);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to get current ring status. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
               if(IS_OK(result))
               {
                  // Construct payload (LE)
                  mp_packet_payload_t tx_msg = {0};
                  tx_msg.type = PPI_TYPE_RE;
                  tx_msg.ppi = PPI_AD_RING_STATUS;
                  size_t offset = 0u;
                  tx_msg.payload[offset++] = ring_status->hardware_version.major;
                  tx_msg.payload[offset++] = ring_status->hardware_version.minor;
                  tx_msg.payload[offset++] = ring_status->hardware_version.patch;
                  tx_msg.payload[offset++] = ring_status->firmware_version.major;
                  tx_msg.payload[offset++] = ring_status->firmware_version.minor;
                  tx_msg.payload[offset++] = ring_status->firmware_version.patch;
                  memcpy(&tx_msg.payload[offset], ring_status->mac, MAC_ADDRESS_SIZE_BYTES);
                  offset += MAC_ADDRESS_SIZE_BYTES;
                  tx_msg.payload[offset++] = (uint8_t)(ring_status->timestamp_unix_s & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->timestamp_unix_s >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->timestamp_unix_s >> 16u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->timestamp_unix_s >> 24u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)(ring_status->ship_mode_exit_timestamp_unix & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->ship_mode_exit_timestamp_unix >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->ship_mode_exit_timestamp_unix >> 16u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->ship_mode_exit_timestamp_unix >> 24u) & 0xFFu);
                  tx_msg.payload[offset++] = ring_status->battery_charge_status;
                  tx_msg.payload[offset++] = (uint8_t)(ring_status->uptime_s & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->uptime_s >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->uptime_s >> 16u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->uptime_s >> 24u) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((uint16_t)ring_status->temperature_celsius & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)(((uint16_t)ring_status->temperature_celsius >> 8u) & 0xFFu);
                  tx_msg.payload[offset++] = ring_status->dose_fifo_used_percent;
                  tx_msg.payload[offset++] = ring_status->battery_fifo_used_percent;
                  tx_msg.payload[offset++] = ring_status->imu_fifo_used_percent;
                  tx_msg.payload[offset++] = ring_status->error_fifo_used_percent;
                  tx_msg.payload[offset++] = ring_status->dose_fifo_used_percent_watermark;
                  tx_msg.payload[offset++] = ring_status->battery_fifo_used_percent_watermark;
                  tx_msg.payload[offset++] = ring_status->imu_fifo_used_percent_watermark;
                  tx_msg.payload[offset++] = ring_status->error_fifo_used_percent_watermark;
                  tx_msg.payload[offset++] = (uint8_t)(ring_status->battery_sample_frequency_millihz & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((ring_status->battery_sample_frequency_millihz >> 8u) & 0xFFu);
                  tx_msg.pkt_payload_len = (uint16_t)offset;

                  // send response message (if active connection, otherwise drop it)
                  result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &tx_msg);
                  ON_ERR_DEBUG_ERROR(
                     result,
                     "Failed to enqueue ring status response message to app manager. Unit %d, Error: %d",
                     GET_ERR_UNIT(result),
                     GET_ERR_CODE(result));
               }

            } ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_TIME == rx_msg.ppi))
            {
               if(0u != rx_msg.pkt_payload_len)
               {
                  DEBUG_ERROR("Time RQ message should have 0 length payload. Got payload length %d.",
                              rx_msg.pkt_payload_len);
                  SET_ERR(result, GC_ERROR_INVALID_PARAM);
               }
               else
               {
                  // App has requested current Unix time from Dock
                  uint32_t current_unix_time_s = 0u;
                  result = m_rtc.interface.get_time_unix(&m_rtc.interface, &current_unix_time_s);
                  ON_ERR_DEBUG_ERROR(result,
                                     "Failed to get current unix time. Unit %d, Error: %d",
                                     GET_ERR_UNIT(result),
                                     GET_ERR_CODE(result));

                  if(IS_OK(result))
                  {
                     mp_packet_payload_t tx_msg = {0};
                     tx_msg.type = PPI_TYPE_RE;
                     tx_msg.ppi = PPI_AD_TIME;
                     tx_msg.payload[0u] = (uint8_t)(current_unix_time_s & 0xFFu);
                     tx_msg.payload[1u] = (uint8_t)((current_unix_time_s >> 8u) & 0xFFu);
                     tx_msg.payload[2u] = (uint8_t)((current_unix_time_s >> 16u) & 0xFFu);
                     tx_msg.payload[3u] = (uint8_t)((current_unix_time_s >> 24u) & 0xFFu);
                     tx_msg.pkt_payload_len = sizeof(uint32_t);

                     // send response message
                     result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &tx_msg);
                     ON_ERR_DEBUG_ERROR(
                        result,
                        "Failed to enqueue current time response message to app manager. Unit %d, Error: %d",
                        GET_ERR_UNIT(result),
                        GET_ERR_CODE(result));
                  }
               }
            }
            else if((PPI_TYPE_PUSH == rx_msg.type) && (PPI_AD_TIME == rx_msg.ppi))
            {
               // Parse payload
               if(sizeof(uint32_t) == rx_msg.pkt_payload_len)
               {
                  uint32_t new_time_unix_s = (uint32_t)rx_msg.payload[0u] //
                                             | ((uint32_t)rx_msg.payload[1u] << 8u)
                                             | ((uint32_t)rx_msg.payload[2u] << 16u)
                                             | ((uint32_t)rx_msg.payload[3u] << 24u);
                  // Validate time
                  if(new_time_unix_s > UNIX_TIME_MINIMUM_VAL)
                  {
                     // Set RTC time
                     result = m_rtc.interface.set_time_unix(&m_rtc.interface, new_time_unix_s);
                     ON_ERR_DEBUG_ERROR(result,
                                        "Failed to set RTC time. Unit %d, Error: %d",
                                        GET_ERR_UNIT(result),
                                        GET_ERR_CODE(result));
                  }
                  else
                  {
                     SET_ERR(result, GC_ERROR_INVALID_PARAM);
                     DEBUG_WARNING("Received invalid unix time. Received unix time: %d", new_time_unix_s);
                  }
               }
               else
               {
                  DEBUG_ERROR("Invalid payload length for time sync message. Expected %d, got %d.",
                              sizeof(uint32_t),
                              rx_msg.pkt_payload_len);
                  SET_ERR(result, GC_ERROR_INVALID_PARAM);
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_PUSH == rx_msg.type) && (PPI_AD_START_CAP_DETECTION_CALIBRATION_INTERVAL == rx_msg.ppi))
            {
               UPDATE_ERR_IF_TRUE(result, 0u != rx_msg.pkt_payload_len, GC_ERROR_INVALID_PARAM);
               ON_ERR_DEBUG_ERROR(
                  result,
                  "Invalid payload length for cap detection report status message. Expected %d, got %d.",
                  0u,
                  rx_msg.pkt_payload_len);

               if(IS_OK(result))
               {
                  // Start high rate sample for calibration period
                  result = m_ring.interface.start_cap_detection_monitoring_interval(&m_ring.interface);
                  ON_ERR_DEBUG_ERROR(result,
                                     "Failed to start cap detection monitoring interval. Unit %d, Error: %d",
                                     GET_ERR_UNIT(result),
                                     GET_ERR_CODE(result));
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_PUSH == rx_msg.type) && (PPI_AD_CAP_DETECTION_CONFIG == rx_msg.ppi))
            {
               UPDATE_ERR_IF_TRUE(
                  result, sizeof(cap_detection_cfg_t) != rx_msg.pkt_payload_len, GC_ERROR_INVALID_PARAM);
               ON_ERR_DEBUG_ERROR(result,
                                  "Invalid payload length for cap detection config message. Expected %d, got %d.",
                                  sizeof(cap_detection_cfg_t),
                                  rx_msg.pkt_payload_len);

               if(IS_OK(result))
               {
                  // Parse payload
                  cap_detection_cfg_t new_config = {0};
                  new_config.threshold
                     = (uint16_t)((uint16_t)rx_msg.payload[0u] | ((uint16_t)rx_msg.payload[1u] << 8u));
                  new_config.hysteresis
                     = (uint16_t)((uint16_t)rx_msg.payload[2u] | ((uint16_t)rx_msg.payload[3u] << 8u));

                  // Set new config
                  result = m_ring.interface.send_cap_detection_config_update(&m_ring.interface, new_config);
                  ON_ERR_DEBUG_ERROR(result,
                                     "Failed to set new cap detection config. Unit %d, Error: %d",
                                     GET_ERR_UNIT(result),
                                     GET_ERR_CODE(result));
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_START_CALIBRATION == rx_msg.ppi))
            {
               // Parse message payload
               bool start_calibration_request = false;
               uint32_t local_calibration_weight_mg = 0u;
               if(sizeof(start_calibration_param_t) == rx_msg.pkt_payload_len)
               {
                  // Parse payload (see start_calibration_param_t)
                  start_calibration_request = (rx_msg.payload[0] > 0u) ? true : false;
                  local_calibration_weight_mg = (uint32_t)rx_msg.payload[1u] //
                                                | ((uint32_t)rx_msg.payload[2u] << 8u)
                                                | ((uint32_t)rx_msg.payload[3u] << 16u)
                                                | ((uint32_t)rx_msg.payload[4u] << 24u);

                  // Handle start calibration request
                  if(start_calibration_request)
                  {
                     // let App know the calibration process has started.
                     mp_packet_payload_t tx_msg = {0};
                     tx_msg.type = (uint8_t)PPI_TYPE_RE;
                     tx_msg.ppi = (uint8_t)PPI_AD_START_CALIBRATION;
                     tx_msg.payload[0] = (uint8_t)true;
                     tx_msg.pkt_payload_len = sizeof(uint8_t);
                     result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
                     ON_ERR_DEBUG_ERROR(result,
                                        "Failed to enqueue msg. Unit %d, Error: %d",
                                        GET_ERR_UNIT(result),
                                        GET_ERR_CODE(result));

                     // Start calibration flow
                     DEBUG_TRACE("Received calibration START request from App. Starting calibration flow.");
                     m_system_sm_inputs.is_calibration_active = true;

                     // Init calibration state inputs
                     reset_calibration_state_machine_inputs();
                     m_calibrate_sm_inputs.start_calibration = true;
                     m_calibrate_sm_inputs.is_calibration_active = true;

                     // Report received calibration weight size
                     *rx_calibration_weight_mg = local_calibration_weight_mg;
                  }
                  else
                  {
                     // Stop calibration flow.
                     // Do NOT set the m_system_sm_inputs.is_calibration_active to false here because
                     // we want the system state machine to continue to execute the calibration state logic until the
                     // calibration state machine has completed all necessary cleanup in the calibration state. The
                     // m_system_sm_inputs.is_calibration_active will be set to false in the calibration state once the
                     // calibration state machine signals that it has completed cleanup and is ready to exit the
                     // calibration state.

                     DEBUG_TRACE("Received calibration STOP request from App. Stopping calibration flow.");
                     m_calibrate_sm_inputs.is_calibration_active = false;

                     // Reset calibration state inputs
                     reset_calibration_state_machine_inputs();
                  }
               }
               else
               {
                  DEBUG_ERROR("Invalid payload length for start calibration request message. Expected %d, got %d. "
                              "Ignoring message.",
                              sizeof(start_calibration_param_t),
                              rx_msg.pkt_payload_len);
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_CALIBRATION_DATA == rx_msg.ppi))
            {
               // No payload expected for this PPI

               // Repond to PPI by sending the requested data
               weight_stack_calibration_record_t record = {0};
               result = m_dock_data_manager.interface.get_weight_calibration_record(&m_dock_data_manager.interface,
                                                                                    &record);
               ON_ERR_DEBUG_ERROR(result,
                                  "Failed to get weight_stack_calibration_record_t. Unit %d, Error: %d",
                                  GET_ERR_UNIT(result),
                                  GET_ERR_CODE(result));
               if(IS_OK(result))
               {
                  mp_packet_payload_t tx_msg = {0};
                  tx_msg.type = (uint8_t)PPI_TYPE_RE;
                  tx_msg.ppi = (uint8_t)PPI_AD_CALIBRATION_DATA;
                  tx_msg.pkt_payload_len = sizeof(weight_stack_calibration_record_t);

                  size_t offset = 0u;
                  uint32_t val = 0u;

                  val = (uint32_t)record.zero_offset;
                  tx_msg.payload[offset++] = (uint8_t)(val & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 8) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 16) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 24) & 0xFFu);

                  val = (uint32_t)record.calibration_factor;
                  tx_msg.payload[offset++] = (uint8_t)(val & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 8) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 16) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 24) & 0xFFu);

                  val = record.full_assembly_weight_mg;
                  tx_msg.payload[offset++] = (uint8_t)(val & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 8) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 16) & 0xFFu);
                  tx_msg.payload[offset++] = (uint8_t)((val >> 24) & 0xFFu);

                  result = m_queue_app_manger_tx_ifx->enqueue(m_queue_app_manger_tx_ifx, &tx_msg);
                  ON_ERR_DEBUG_ERROR(
                     result, "Failed to enqueue msg. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
               }
               // Intentionally ignore the message if the getting data from the data manager failed. The error is logged
               // via the debug module
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_PUSH == rx_msg.type) && (PPI_AD_CALIBRATION_WEIGHT_PRESENT == rx_msg.ppi))
            {
               if(sizeof(uint8_t) == rx_msg.pkt_payload_len)
               {
                  // Parse payload
                  *is_weight_present = (rx_msg.payload[0] > 0u) ? true : false;
               }
               else
               {
                  DEBUG_ERROR("Invalid payload length for PPI_AD_CALIBRATION_WEIGHT_PRESENT. Expected %d, got %d. "
                              "Ignoring message.",
                              sizeof(uint8_t),
                              rx_msg.pkt_payload_len);
               }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if((PPI_TYPE_RQ == rx_msg.type) && (PPI_AD_DEVELOPMENT_CMD == rx_msg.ppi))
            {
               if(sizeof(uint8_t) == rx_msg.pkt_payload_len)
               {
                  // Parse payload
                  APP2DOCK_COMMANDS command = (APP2DOCK_COMMANDS)rx_msg.payload[0];
                  handle_testing_commands_from_app(command);
               }
               else
               {
                  DEBUG_ERROR("Invalid payload length for PPI_AD_DEVELOPMENT_CMD. Expected %d, got %d. "
                              "Ignoring message.",
                              sizeof(uint8_t),
                              rx_msg.pkt_payload_len);
               }
            }
            else
            {
               DEBUG_WARNING("Received unknown message from app manager. Type: %d, PPI: %d. Ignoring message.",
                             rx_msg.type,
                             rx_msg.ppi);
            }
         }
      }
   }
}

/**
 * @brief Detect prolonged "cap open" conditions using only fresh ring status samples.
 *
 * This function intentionally advances timeout tracking only when a new ring status update is received.
 * The ring manager returns its latest snapshot each loop, so duplicate timestamps are considered stale samples and are
 * ignored to avoid false alarms when communication pauses.
 *
 * Behavior:
 * - Cap open is detected when @p status_update->ring_status.cap_detection_status equals
 *   @ref CAP_STATE_OPEN.
 *
 * - Freshness and elapsed time tracking are based on @p status_update->update_time_unix_s (time the dock received the
 *   status update).
 *
 * - If cap-open duration exceeds @ref MEDICATION_CAP_OFF_MAX_TIME_S, a generic HMI error notification is raised once
 * along with an appropriate DEBUG_ERROR log.
 *
 * - Timer and notification latch are reset when the cap is no longer open.
 *
 * @param[in] status_update Latest ring status update snapshot.
 */
static void handle_medication_cap_off_for_too_long(const status_update_t *status_update)
{
   if(NULL == status_update)
   {
      DEBUG_ERROR("NULL paramater received.");
      return;
   }

   static uint32_t last_processed_timestamp_s = 0u;
   static uint32_t cap_open_start_timestamp_s = 0u;
   static bool has_reported_cap_off_too_long = false;

   uint32_t sample_timestamp_s = status_update->update_time_unix_s; // Timestamp when current status was received
   bool should_process_sample = true;
   bool should_check_cap_open_duration = false;

   if((0u == sample_timestamp_s) || (sample_timestamp_s == last_processed_timestamp_s))
   {
      // Ignore stale samples: only react to newly received timestamps.
      should_process_sample = false;
   }

   if(should_process_sample)
   {
      if(sample_timestamp_s < last_processed_timestamp_s)
      {
         // Update timestamp restarted or moved backwards. Reset local tracking and continue from this sample.
         cap_open_start_timestamp_s = 0u;
         has_reported_cap_off_too_long = false;
      }

      last_processed_timestamp_s = sample_timestamp_s;

      bool is_cap_open = (CAP_STATE_OPEN == (CAP_STATE)status_update->ring_status.cap_detection_status);
      if(!is_cap_open)
      {
         cap_open_start_timestamp_s = 0u;
         has_reported_cap_off_too_long = false;
      }
      else if(0u == cap_open_start_timestamp_s)
      {
         cap_open_start_timestamp_s = sample_timestamp_s;
      }
      else
      {
         should_check_cap_open_duration = true;
      }
   }

   if(should_check_cap_open_duration)
   {
      uint32_t cap_open_duration_s = sample_timestamp_s - cap_open_start_timestamp_s;
      if((cap_open_duration_s > MEDICATION_CAP_OFF_MAX_TIME_S) && !has_reported_cap_off_too_long)
      {
         DEBUG_ERROR("Medication cap open too long. Duration: %us, Threshold: %us.",
                     cap_open_duration_s,
                     MEDICATION_CAP_OFF_MAX_TIME_S);

         result_t result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to set HMI notification. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         if(IS_OK(result))
         {
            has_reported_cap_off_too_long = true;
         }
      }
   }
}

static void handle_ring_off_for_too_long(bool is_ring_present)
{
   uint64_t now_ms = 0u;
   static uint64_t time_at_ring_removal_ms = 0u;
   static uint64_t last_notification_time_ms = 0u;
   static bool has_seen_ring_since_boot = false;
   static bool prev_is_ring_present = false;

   result_t result = RESULT_OK;

   if(is_ring_present)
   {
      has_seen_ring_since_boot = true;
   }

   if(is_ring_present != prev_is_ring_present)
   {
      // Status changed
      if(!is_ring_present)
      {
         result = m_systick.interface.get_time_ms(&m_systick.interface, &time_at_ring_removal_ms);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to get systick time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }

      if(IS_OK(result))
      {
         // The IS_OK check forces a retry if the get_time_ms() call failed.
         prev_is_ring_present = is_ring_present;
      }
   }

   // Handle ring off for too long notification
   if(!is_ring_present && has_seen_ring_since_boot)
   {
      result = m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get systick time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      bool notify = false;
      if((now_ms - time_at_ring_removal_ms > RING_REMOVAL_MAX_TIME_MS) && IS_OK(result))
      {
         notify = true;
      }

      if(notify && (now_ms - last_notification_time_ms > RING_REMOVAL_NOTIFICATION_REPEAT_INTERVAL_MS) && IS_OK(result))
      {
         // Todo: consider making this event a unique one in stead of a generic error
         result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to set HMI notification. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         if(IS_OK(result))
         {
            last_notification_time_ms = now_ms;
         }
      }
   }
}

static void handle_medication_change_notification(uint8_t *medication_uid, uint8_t uid_size)
{
   if(NULL == medication_uid)
   {
      DEBUG_ERROR("NULL pointer received.");
      return;
   }
   if(NFC_TAG_MAX_UID_SIZE != uid_size)
   {
      DEBUG_ERROR("Incorrect UID size received.");
      return;
   }

   bool valid_uid = true; // Is this an actual UID or just unintialized array of zeros
   static const uint8_t invalid_medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};
   if(0 == memcmp(medication_uid, invalid_medication_uid, NFC_TAG_MAX_UID_SIZE))
   {
      valid_uid = false;
   }

   if(valid_uid)
   {
      uint8_t prev_medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};
      result_t result = m_dock_data_manager.interface.get_med_nfc_uid(
         &m_dock_data_manager.interface, prev_medication_uid, NFC_TAG_MAX_UID_SIZE);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get medication NFC UID. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      memcpy(prev_medication_uid,
             medication_uid,
             NFC_TAG_MAX_UID_SIZE); // Update the prev_medication_uid for next comparison

      if(IS_OK(result) && (0 != memcmp(medication_uid, prev_medication_uid, NFC_TAG_MAX_UID_SIZE)))
      {
         DEBUG_ERROR("Medication change detected!");
         result = m_hmi.interface.set_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_ERROR);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to set HMI notification. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
   }
}

static void enter_ship_mode(void)
{
   // Explicitly call debug processor to ensure debug logs get printed before shutdown
#ifndef DEBUG_DISABLE_UART
   if(NULL != m_debug_uart_driver_interface)
   {
      size_t pending_tx_count = 0u;

      (void)m_debug_uart_driver_interface->parent->_tx_queue->get_count(
         m_debug_uart_driver_interface->parent->_tx_queue, &pending_tx_count);

      for(size_t idx = 0u; idx < pending_tx_count; idx++)
      {
         nrf_delay_ms(LOG_FLUSH_LOOP_DELAY_MS);
         (void)m_debug_uart_driver_interface->process(&m_debug_uart_driver.interface);
         feed_watchdog();
      }
   }
#endif

   // Todo: do any cleanup necessary before shutting down
   uint32_t unix_timestamp_s = 0u;
   ship_mode_t ship = {0};
   result_t result = m_rtc.interface.get_time_unix(&m_rtc.interface, &unix_timestamp_s);
   if(IS_OK(result))
   {
      ship.enter_ship_mode_unix_s = unix_timestamp_s;
      ship.is_sleeping = true;

      (void)m_dock_data_manager.interface.set_ship_mode_data(&m_dock_data_manager.interface, &ship);
      nrf_delay_ms(DELAY_BEFORE_RESET_MS); // Ensures data manager has plenty of time to write the data
   }

   (void)m_battery.interface.enter_ship_mode(&m_battery.interface);

   while(true) // Infinite loop until watchdog resets in case ship mode entry fails
   {
      nrf_delay_ms(DELAY_BEFORE_RESET_MS);
   }
}

static void handle_dose_size_detection(bool *valid_weight_data, dock_weight_measurement_t *weight_data)
{
   const dose_size_detection_interface_t *p_dsd = &m_dsd.interface;

   DSD_DATA_STATE data_state = DSD_DATA_STATE_NO_DATA;
   static DSD_DATA_STATE prev_data_state = DSD_DATA_STATE_NO_DATA;
   bool data_state_changed = false;
   bool new_data_available = false;
   dock_weight_measurement_t local_weight_data = {0};
   *valid_weight_data = false;

   result_t result = p_dsd->get_dose_size_data_state(p_dsd, &data_state);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to get dose size data state. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   if(IS_OK(result))
   {
      data_state_changed = (data_state != prev_data_state);

      if((DSD_DATA_STATE_READY == data_state) && data_state_changed)
      {
         // Expect new weight data only after a ring off & on cycle.
         dose_size_data_out_t dsd_data = {0};
         result = p_dsd->fetch_dose_size_data(p_dsd, &dsd_data);
         if(IS_OK(result))
         {
            DEBUG_INFO("[DSD]: Dose size data ready! %d mg dispensed (%u mg total)",
                       dsd_data.dose_size_mg,
                       dsd_data.total_dispensed_mg);
            local_weight_data.std_dev = dsd_data.sigma_total_mg;
            local_weight_data.weight_mg = dsd_data.dose_size_mg;
            local_weight_data.total_dispensed_mg = dsd_data.total_dispensed_mg;
            new_data_available = true;
         }
         else
         {
            DEBUG_ERROR("Failed to fetch dose size data. result(%d,%d)", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         }
      }
      else if(DSD_DATA_STATE_BAD == data_state)
      {
         DSD_DATA_BAD_REASON bad_reason = DSD_DATA_BAD_REASON_NONE;
         result = p_dsd->get_dose_size_data_bad_reason(p_dsd, &bad_reason);
         ON_ERR_DEBUG_ERROR(result,
                            "Failed to get dose size data bad reason. Unit %d, Error: %d",
                            GET_ERR_UNIT(result),
                            GET_ERR_CODE(result));
         if(IS_OK(result))
         {
            DEBUG_WARNING("[DSD]: Dose size data is bad. Reason: %d", bad_reason);

            // Fetch and report bad data to preserve visibility and keep downstream totals aligned.
            dose_size_data_out_t dsd_data = {0};
            result = p_dsd->fetch_dose_size_data(p_dsd, &dsd_data);
            if((DOSE_SIZE_DETECTION_ERROR_DATA_BAD == GET_ERR_CODE(result))
               && (SW_UNIT_ID_DOSE_SIZE_DETECTION == GET_ERR_UNIT(result)))
            {
               CLEAR_ERR(result);
            }

            ON_ERR_DEBUG_ERROR(result,
                               "Failed to fetch bad dose size data. Unit %d, Error: %d",
                               GET_ERR_UNIT(result),
                               GET_ERR_CODE(result));
            if(IS_OK(result))
            {
               DEBUG_WARNING("[DSD]: Reporting bad dose size data. Dose: %d mg, Total: %u mg, Reason: %d",
                             dsd_data.dose_size_mg,
                             dsd_data.total_dispensed_mg,
                             bad_reason);

               local_weight_data.std_dev = dsd_data.sigma_total_mg;
               local_weight_data.weight_mg = dsd_data.dose_size_mg;
               local_weight_data.total_dispensed_mg = dsd_data.total_dispensed_mg;
               new_data_available = true;
            }
         }
      }
      prev_data_state = data_state;
   }

   if(new_data_available)
   {
      // Gather rest of data for dock_weight_measurement_t
      uint32_t unix_time_s = 0u;
      result = m_rtc.interface.get_time_unix(&m_rtc.interface, &unix_time_s);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get unix time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      int16_t temp_decidegc = 0;
      if(IS_OK(result))
      {
         local_weight_data.timestamp_unix_s = unix_time_s; // Get timestamp

         bool is_stale = false;
         result = m_temp_sensor.interface.get_temperature(&m_temp_sensor.interface, &temp_decidegc, &is_stale);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to get temperature. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }

      if(IS_OK(result))
      {
         // Todo: ignoring temp meas. staleness for now. Verify what that staleness means and whether or not it's
         // relevant to logging here.
         local_weight_data.temperature_deg_c_div10 = temp_decidegc; // Get loadcell temperature

         // Report weight data to rest of system
         memcpy(weight_data, &local_weight_data, sizeof(dock_weight_measurement_t));
         *valid_weight_data = true;

         // Store the data
         result = m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                        DATA_ID_WEIGHT_MEASUREMENT,
                                                        &local_weight_data,
                                                        sizeof(local_weight_data),
                                                        1u);
         ON_ERR_DEBUG_ERROR(
            result, "Failed to enqueue weight data. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }

      if(IS_OK(result))
      {
         // Store updated total dispensed weight in data manager for logging and baselining purposes. Note that this
         // value gets reset to 0 at the start of each baselining process.
         // TODO: NOTE: THIS LINE IS MEANT FOR PRODUCTION BUT IS COMMENTED OUT DURING TESTING TO PREVENT UNNECESSARY
         // WEAR ON THE MCU INTERNAL FLASH WHICH HAS LIMITED WRITE CYCLES. DO NOT UNCOMMENT THIS LINE BEFORE WE KNOW HOW
         // OFTEN THIS DATA GETS UPDATED! If the presence detection is iffy, it will trigger this writing often and
         // unnecessarily wear out the flash.

         // result = m_dock_data_manager.interface.set_total_weight_dispensed_mg(&m_dock_data_manager.interface,
         //                                                                      local_weight_data.total_dispensed_mg);
         // ON_ERR_DEBUG_ERROR(
         //    result, "Failed to store weight data. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
   }
}

static void implement_control_loop(void)
{
   result_t result = RESULT_OK;
   static battery_status_t battery_status = {0};
   static status_update_t ring_status = {0};
   static ring_docked_status_t ring_docked_status = {0};
   static uint32_t rx_calibration_weight = 0u; // Calibration weight size received for the calibration procedure
   static bool is_weight_present = false;      // Used during weight calibration process

   update_ble_state();

   result = m_app_manager.interface.set_comms_link_status(&m_app_manager.interface, is_ble_valid());
   ON_ERR_DEBUG_ERROR(
      result, "Failed to set comms link status. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   bool is_pairing_gesture_detected = false;
   handle_button(&is_pairing_gesture_detected);

   if(is_pairing_gesture_detected && battery_status.charger_connected)
   {
      // Change to BLE pairing state.
      m_system_sm_inputs.bonding_state = BONDING_STATE_BONDING;
      DEBUG_DEBUG("Pairing gesture detected while charger is connected. Transitioning to BLE pairing state.");
   }

   IF_OK_RUN_AND_UPDATE(result, m_msg_prot_ble.interface.process(&m_msg_prot_ble.interface));
   IF_OK_RUN_AND_UPDATE(result, m_app_manager.interface.process(&m_app_manager.interface));

   // Get battery data
   handle_battery(&battery_status);

   // Update state machine dose window input
   result = m_dose_scheduler.interface.check_dose_window(&m_dose_scheduler.interface,
                                                         &m_system_sm_inputs.is_dose_window_active);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to check dose window. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   bool is_ring_present = false;
   static uint8_t ring_uid[NFC_TAG_MAX_UID_SIZE] = {0};
   static uint8_t medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};
   bool medication_uid_stale = false;

   // Calibration/baselining states need responsive NFC, so disable low-power power cycling in those states.
   bool keep_nfc_responsive = m_system_sm_inputs.is_calibration_active || m_system_sm_inputs.is_baselining_active
                              || (STATEMACHINE_STATE_CALIBRATION == m_system_sm.current_state)
                              || (STATEMACHINE_STATE_BASELINING == m_system_sm.current_state);

   bool allow_nfc_power_saving = !keep_nfc_responsive;
   handle_nfc(&is_ring_present,
              ring_uid,
              medication_uid,
              &medication_uid_stale,
              battery_status,
              ring_status,
              allow_nfc_power_saving);

   if(is_ring_present != ring_docked_status.docked_status)
   {
      uint32_t unix_time_s = 0u;
      result = m_rtc.interface.get_time_unix(&m_rtc.interface, &unix_time_s);
      ON_ERR_DEBUG_ERROR(
         result, "Failed to get unix time. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      if(IS_OK(result))
      {
         ring_docked_status.timestamp_unix_s = (IS_OK(result)) ? unix_time_s : 0u;
         ring_docked_status.docked_status = is_ring_present;

         memcpy(ring_docked_status.ring_nfc_id, ring_uid, NFC_TAG_MAX_UID_SIZE);
         memcpy(ring_docked_status.medication_nfc_id, medication_uid, NFC_TAG_MAX_UID_SIZE);
      }

      IF_OK_RUN_AND_UPDATE(result,
                           m_dock_data_manager.interface.enqueue(&m_dock_data_manager.interface,
                                                                 DATA_ID_RING_DOCKED_STATUS,
                                                                 &ring_docked_status,
                                                                 sizeof(ring_docked_status),
                                                                 1u));
      ON_ERR_DEBUG_ERROR(result,
                         "Failed to enqueue ring docked status. Unit %d, Error: %d",
                         GET_ERR_UNIT(result),
                         GET_ERR_CODE(result));
   }

   bool is_weight_valid = false;
   dock_weight_measurement_t weight_data = {0};
   handle_dose_size_detection(&is_weight_valid, &weight_data);

   process_incoming_app_messages(&rx_calibration_weight, &is_weight_present);

   // Update state machine
   m_system_sm_outputs.changed = false; // Clear output before updating state machine
   m_system_sm.update_statemachine(&m_system_sm, &m_system_sm_inputs, &m_system_sm_outputs);

   if(STATEMACHINE_STATE_PAIRING_BLE != m_system_sm.current_state)
   {
      result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BLE_PAIRING);
      ON_ERR_DEBUG_ERROR(result,
                         "Failed to clear BLE pairing HMI notification. Unit %d, Error: %d",
                         GET_ERR_UNIT(result),
                         GET_ERR_CODE(result));
   }

   /**
    * @note The states below depend on the statemachine being updated BEFORE the states are executed. Do NOT
    * change this order without updating the state executions.
    */

   switch(m_system_sm.current_state)
   {
      case STATEMACHINE_STATE_UNBONDED:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: UNBONDED");
         }
         handle_sm_unbonded_state(&battery_status);
         break;
      case STATEMACHINE_STATE_IDLE:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: IDLE");
         }
         // DEBUG_TRACE("Battery voltage (mv): %d", battery_status.battery_voltage_mv);

         handle_sm_idle_state();
         break;
      case STATEMACHINE_STATE_ACTIVE:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: ACTIVE");
         }
         handle_sm_active_state();
         break;
      case STATEMACHINE_STATE_INVALID_DOSE_INFO:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: INVALID_DOSE_INFO");
         }
         handle_sm_invalid_dose_info_state();
         break;
      case STATEMACHINE_STATE_PAIRING_BLE:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: PAIRING_BLE");
         }
         handle_sm_pairing_state();
         break;
      case STATEMACHINE_STATE_CALIBRATION:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: CALIBRATION");
         }
         handle_sm_calibration_state(is_ring_present, rx_calibration_weight, is_weight_present);
         break;
      case STATEMACHINE_STATE_BASELINING:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: BASELINING");
         }
         handle_sm_baselining_state(is_ring_present, medication_uid, sizeof(medication_uid), medication_uid_stale);
         break;
      default:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: UNKNOWN");
         }
         break;
   }

   // Service notification module
   result = m_hmi.interface.process(&m_hmi.interface);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to process HMI notifications. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   result = m_msg_prot_nfc.interface.process(&m_msg_prot_nfc.interface);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to process NFC message protocol. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   result = m_ring.interface.process(&m_ring.interface, is_ring_present);
   ON_ERR_DEBUG_ERROR(result, "Failed ring process. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   result = m_ring.interface.get_ring_status(&m_ring.interface, &ring_status);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to process ring manager. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   handle_temperature_reading();

   handle_ring_off_for_too_long(is_ring_present);

   handle_medication_cap_off_for_too_long(&ring_status);

   if(STATEMACHINE_STATE_BASELINING != m_system_sm.current_state)
   {
      // Ignore medication changes during the baselining state since it's goal is to lock in a new medication UID
      handle_medication_change_notification(medication_uid, sizeof(medication_uid));
   }

   if(STATEMACHINE_STATE_CALIBRATION != m_system_sm.current_state)
   {
      // Explicitly clear these if not in calibration state. This ensures fresh values when starting a new calibration
      rx_calibration_weight = false;
      is_weight_present = false;
   }

   // Handle weight related tasks
   result = m_temp_sensor.interface.temp_sensor_process(&m_temp_sensor.interface, TEMP_READ_FREQ_MS);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to process temperature sensor. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   result = m_weight_sensor.interface.process(&m_weight_sensor.interface, is_ring_present);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to process weight sensor. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   static bool has_logged_dsd_error = false; // Used to prevent log flooding in case of persistent DSD errors
   result = m_dsd.interface.process(&m_dsd.interface, is_ring_present);
   if((GET_ERR_CODE(result) == DOSE_SIZE_DETECTION_ERROR_NOT_BASELINED
       || GET_ERR_CODE(result) == DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED)
      && GET_ERR_UNIT(result) == SW_UNIT_ID_DOSE_SIZE_DETECTION)
   {
      if(!has_logged_dsd_error)
      {
         DEBUG_ERROR("Dose size detection either not baselined or scale not calibrated. Code: %d, Unit: %d",
                     GET_ERR_CODE(result),
                     GET_ERR_UNIT(result));
         has_logged_dsd_error = true;
      }
      CLEAR_ERR(result);
   }
   ON_ERR_DEBUG_ERROR(result, "Failed to process DSD. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   // Push dock charge status on change
   result = push_dock_charge_status(battery_status.battery_state);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to push dock charge status. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t general_control_run(void)
{
   result_t result = RESULT_OK;
   static uint64_t prev_main_loop_timestamp_ms = 0u;
   DEBUG_INFO("Starting main control loop");

   // Main application loop
   for(;;)
   {
// Debug / Command comms
#ifndef DEBUG_DISABLE_UART
      if(NULL != m_debug_uart_driver_interface)
      {
         (void)m_debug_uart_driver_interface->process(&m_debug_uart_driver.interface);
      }
#endif
      if(m_system_sm_outputs.changed)
      {
         //  State changed. Update systick period.
         m_systick_period_ms = m_system_sm_outputs.current_state_systick_period_ms;
      }

      uint64_t current_systick_time_ms = 0u;
      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_time_ms);
      BREAK_ON_ERR(result);

      if(current_systick_time_ms - prev_main_loop_timestamp_ms >= m_system_sm_outputs.current_state_loop_period_ms)
      {
         prev_main_loop_timestamp_ms = current_systick_time_ms;
         implement_control_loop();
      }

      feed_watchdog();
      idle_state_handle();
   }

   return result;
}

result_t general_control_init(void)
{
   result_t result = RESULT_OK;

   DEBUG_INFO("general_control_init start.");

#if(GC_WATCHDOG_ENABLED == 1u)
   DEBUG_INFO("wdt_init");
   wdt_init();
#else
   DEBUG_INFO("wdt_init skipped.");
#endif

   // Initialize the async SVCI interface to bootloader before any interrupts are enabled.
   uint32_t err_code = ble_dfu_buttonless_async_svci_init();
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_NRF_ERR_CHECK_DFU);

   IF_OK_RUN_AND_UPDATE(result, system_time_init(&m_systick));

   IF_OK_RUN_AND_UPDATE(result, application_timer_init());

   IF_OK_RUN_AND_UPDATE(result, power_management_init());

   IF_OK_RUN_AND_UPDATE(result, i2c_driver_init(&m_i2c_driver_b, &m_twi_instance_0, &m_i2c_config_0)); // I2C bus B

   IF_OK_RUN_AND_UPDATE(result, i2c_driver_init(&m_i2c_driver, &m_twi_instance_1, &m_i2c_config_1));

   if(IS_OK(result))
   {
      result = npm1300_driver_init(&m_pmic, &m_i2c_driver.interface, PMIC_I2C_ADDRESS);
   }

   IF_OK_RUN_AND_UPDATE(result, battery_manager_init(&m_battery, &m_pmic.interface));

   // NOTE: This needs to happen before debug_log_init, as that is where the dock_data_manager endpoint is
   // registered.
   IF_OK_RUN_AND_UPDATE(result, dock_data_manager_init(&m_dock_data_manager));

   IF_OK_RUN_AND_UPDATE(result, debug_log_init());

   // Init BLE Rx queue
   IF_OK_RUN_AND_UPDATE(
      result, queue_init(&m_queue_ble_rx, m_buffer_ble_rx, sizeof(m_buffer_ble_rx), BLE_RX_PACKET_ELEMENT_SIZE));

   if(IS_OK(result))
   {
      // Init BLE control event handler instance and pass it to the ble_control module via ble_control_init().
      // m_ble_evt_handlers.on_ble_connection_status_update = on_ble_connection_status_update;

      // IF_OK_RUN_AND_UPDATE(result, ble_control_init(&m_ble_control, &m_ble_evt_handlers,
      // &m_queue_ble_rx.interface));
      IF_OK_RUN_AND_UPDATE(
         result, ble_control_init(&m_ble_control, NULL, &m_queue_ble_rx.interface)); // No event handlers for now
   }

   IF_OK_RUN_AND_UPDATE(result, systick_timer_init());

   // Init queues

   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_queue_app_manager_rx,
                                   m_buffer_app_manager_rx,
                                   sizeof(m_buffer_app_manager_rx),
                                   (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)));

   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_queue_app_manager_tx,
                                   m_buffer_app_manager_tx,
                                   sizeof(m_buffer_app_manager_tx),
                                   (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)));

   IF_OK_RUN_AND_UPDATE(
      result,
      message_protocol_init(
         &m_msg_prot_ble, &m_systick.interface, &m_ble_control.data_ifc, false, 1000u, NULL, NULL, NULL, NULL));

   IF_OK_RUN_AND_UPDATE(result,
                        app_manager_init(&m_app_manager,
                                         &m_msg_prot_ble.interface,
                                         &m_queue_app_manager_rx.interface,
                                         &m_queue_app_manager_tx.interface,
                                         &m_ble_control.data_ifc,
                                         &m_systick.interface,
                                         &m_dock_data_manager.interface));

   // Dock data manager and debug log already initialized above.

   IF_OK_RUN_AND_UPDATE(result, imu_x_init(&m_imu, &m_i2c_driver.interface));

   IF_OK_RUN_AND_UPDATE(result, turn_off_imu());

   IF_OK_RUN_AND_UPDATE(result, pmic_reset_timer_init());

   if(IS_OK(result))
   {
      sys_time_t init_time = {.day = 1,
                              .month = 0,
                              .year = 25,
                              .hours = 12,
                              .minutes = 0,
                              .seconds = 0,
                              .weekday = SYS_TIME_WEEKDAY_WEDNESDAY};
      result = rtc_system_time_init(&m_rtc, RTC_I2C_ADDRESS, &m_i2c_driver.interface, init_time);
   }

   ship_mode_t ship = {0};
   if(IS_OK(result))
   {
      result = m_dock_data_manager.interface.get_ship_mode_data(&m_dock_data_manager.interface, &ship);
   }

   uint32_t unix_time_s = 0u;
   if(IS_OK(result) && ship.is_sleeping)
   {
      result = m_rtc.interface.get_time_unix(&m_rtc.interface, &unix_time_s);
   }

   if(IS_OK(result) && ship.is_sleeping)
   {
      ship.is_sleeping = false;
      ship.exit_ship_mode_unix_s = unix_time_s;
      result = m_dock_data_manager.interface.set_ship_mode_data(&m_dock_data_manager.interface, &ship);
   }

   IF_OK_RUN_AND_UPDATE(result, system_fsm_init(&m_system_sm));
   IF_OK_RUN_AND_UPDATE(result, baselining_fsm_init(&m_baseline_sm, &m_systick.interface));
   IF_OK_RUN_AND_UPDATE(result, calibration_fsm_init(&m_calibrate_sm, &m_systick.interface));

   if(IS_OK(result))
   {
      // Init state machine inputs/outputs
      m_system_sm_inputs.battery_charger_connected = false;
      m_system_sm_inputs.bonding_state = BONDING_STATE_UNBONDED;
      m_system_sm_inputs.is_dose_window_active = false;
      m_system_sm_inputs.is_valid_dose_schedule_detected = false;
      m_system_sm_inputs.is_baselining_active = false;
      m_system_sm_inputs.is_calibration_active = false;

      m_system_sm_outputs.changed = false;
      m_system_sm_outputs.current_state_loop_period_ms = SLEEP_PERIOD_ACTIVE_MS;
      m_system_sm_outputs.current_state_systick_period_ms = SYSTICK_PERIOD_ACTIVE_MS;

      m_baseline_sm_inputs.start_baselining = false;
      m_baseline_sm_inputs.is_baselining_active = false;
      m_baseline_sm_inputs.is_ring_present = false;
      m_baseline_sm_inputs.is_medication_uid_avail = false;
      m_baseline_sm_inputs.is_empty_dock_weight_set = false;
      m_baseline_sm_inputs.is_ring_dock_full_med_weight_set = false;
      m_baseline_sm_inputs.med_update_status = MED_UID_STORAGE_NONE;
      m_baseline_sm_inputs.backend_validation_status = BACKEND_VAL_STATUS_NONE;
      m_baseline_sm_inputs.dose_schedule_status = DOSE_SCH_STATUS_NONE;

      m_calibrate_sm_inputs.start_calibration = false;
      m_calibrate_sm_inputs.is_calibration_active = false;
      m_calibrate_sm_inputs.is_ring_present = false;
      m_calibrate_sm_inputs.data_update_status = DATA_STORAGE_NONE;
      m_calibrate_sm_inputs.is_empty_dock_weight_set = false;
      m_calibrate_sm_inputs.is_calibration_weight_set = false;
      m_calibrate_sm_inputs.is_calibration_weight_present = false;
   }

   // Check BLE bonded status
   ble_control_status_t status = {0};
   if(IS_OK(result))
   {
      result = m_ble_control.interface.get_ble_status(&m_ble_control.interface, &status);
   }

   if(IS_OK(result))
   {
      memcpy(&m_ble_status, &status, sizeof(ble_control_status_t));
   }

   if(IS_OK(result))
   {
      // Init system flags
      /**
       * Explicitly cast away the volatile qualifier, since memset expects a void *, and casting a volatile pointer
       * directly to void * is considered discarding qualifiers. This is known to be safe in this instance since this
       * is during initialization, before anything has been set up to use @p m_flag.
       */
      memset((void *)(uintptr_t)&m_flag, 0, sizeof(flag_t));
   }

   // Init notification buffer
   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&m_ble_notification_queue,
                                   m_ble_notification_queue_data,
                                   NOTIFICATION_QUEUE_LEN * sizeof(result_t),
                                   sizeof(result_t)));

   IF_OK_RUN_AND_UPDATE(result, dose_scheduler_init(&m_dose_scheduler, &m_rtc.interface));

   // Load dose schedule from flash
   dose_schedule_record_t schedule_record = {0};
   if(IS_OK(result))
   {
      result = m_dock_data_manager.interface.get_dose_schedule_record(&m_dock_data_manager.interface, &schedule_record);
   }

   // Testing only - Start: TODO - Remove when we enable NVM storage during testing
   // Hard code a dose schedule
   schedule_record.start_date_unix_seconds = 948240000 - 1;
   schedule_record.dose_schedule.dose_days_bitfield = 0x01; // Mondays only
   schedule_record.dose_schedule.dose_window_count = 1;
   schedule_record.dose_schedule.dose_window_duration_minutes = 1;
   schedule_record.dose_schedule.dose_window_start_times_minutes[0] = 360;
   schedule_record.dose_schedule.temp_lower_limit_deg_c = 5;
   schedule_record.dose_schedule.temp_upper_limit_deg_c = 60;
   schedule_record.dose_schedule.dosage_mg = 80u;
   schedule_record.dose_schedule.temp_avg_window_duration_sec = 300;

   // Testing only - Stop

   // Check for valid schedule
   if(IS_OK(result))
   {
      result = m_dose_scheduler.interface.load_schedule(
         &m_dose_scheduler.interface, &schedule_record.dose_schedule, schedule_record.start_date_unix_seconds);
      if(IS_OK(result))
      {
         // Valid dose schedule detected.
         m_system_sm_inputs.is_valid_dose_schedule_detected = true;

         // Update temperature average window size based on the stored dose schedule
         uint16_t avg_window_size = (uint16_t)((uint32_t)(schedule_record.dose_schedule.temp_avg_window_duration_sec)
                                               / (TEMPERATURE_UPDATE_INTERVAL_MS / COMMON_1K_FACTOR));
         moving_average_set_length(avg_window_size);
      }
      else if(IS_ERR(result)
              && ((GET_ERR_CODE(result) != DOSE_SCHEDULER_ERROR_NULL)
                  && (GET_ERR_CODE(result) != DOSE_SCHEDULER_ERROR_UNINITIALIZED)))
      {
         DEBUG_ERROR("Failed to load dose schedule. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         m_system_sm_inputs.is_valid_dose_schedule_detected = false;
         result = RESULT_OK; // Clear error since we want to continue running even if dose schedule is invalid. The
                             // state machine will handle the invalid dose schedule case with appropriate
                             // notifications to the user.
      }
   }

   // Weight stack init - start
   nrf_drv_spi_uninit(&m_ads_spi_instance);
   IF_OK_RUN_AND_UPDATE(result, spi_driver_init(&m_ads_spi_driver, &m_ads_spi_instance, &m_ads_spi_config));
   nrf_delay_ms(50);
   IF_OK_RUN_AND_UPDATE(
      result,
      sts30_dis_temp_sensor_init(&m_temp_sensor, &m_i2c_driver_b.interface, &m_systick.interface, STS30_I2C_ADDRESS));
   IF_OK_RUN_AND_UPDATE(
      result,
      weight_sensor_init(
         &m_weight_sensor, &m_systick.interface, &(m_ads_spi_driver.interface), &m_temp_sensor.interface));

   weight_stack_calibration_record_t record = {0};
   IF_OK_RUN_AND_UPDATE(
      result, m_dock_data_manager.interface.get_weight_calibration_record(&m_dock_data_manager.interface, &record));

   uint32_t total_weight_dispensed_mg = 0u;
   IF_OK_RUN_AND_UPDATE(result,
                        m_dock_data_manager.interface.get_total_weight_dispensed_mg(&m_dock_data_manager.interface,
                                                                                    &total_weight_dispensed_mg));

   IF_OK_RUN_AND_UPDATE(result,
                        dose_size_detection_init(&m_dsd,
                                                 &m_weight_sensor.interface,
                                                 &m_systick.interface,
                                                 record.full_assembly_weight_mg,
                                                 total_weight_dispensed_mg));

   // Load calibration data
   if(IS_OK(result))
   {
      result = m_weight_sensor.interface.set_zero_offset(&m_weight_sensor.interface, record.zero_offset);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("Failed to set weight sensor zero offset!"
                     " result(%d,%d)",
                     GET_ERR_UNIT(result),
                     GET_ERR_CODE(result));
         CLEAR_ERR(result); // Clear error to allow calibration procedure to run if the issue is with the current zero
                            // offset value. Todo: verify this.
      }
   }

   if(IS_OK(result))
   {
      result = m_weight_sensor.interface.set_calibration_factor(&m_weight_sensor.interface, record.calibration_factor);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("Failed to set weight sensor calibration factor!"
                     " result(%d,%d)",
                     GET_ERR_UNIT(result),
                     GET_ERR_CODE(result));
         CLEAR_ERR(result); // Clear error to allow calibration procedure to run if the issue is with the current
                            // calibration factor value. Todo: verify this.
      }
   }
   // Weight stack init - end

   IF_OK_RUN_AND_UPDATE(result, notifications_module_init(&m_hmi, &m_i2c_driver_b.interface, &m_systick.interface));

   IF_OK_RUN_AND_UPDATE(result, nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface));

   IF_OK_RUN_AND_UPDATE(
      result,
      message_protocol_init(&m_msg_prot_nfc, &m_systick.interface, &m_nfc.data_ifc, true, 50u, NULL, NULL, NULL, NULL));

   IF_OK_RUN_AND_UPDATE(
      result,
      ring_manager_init(
         &m_ring, &m_systick.interface, &m_msg_prot_nfc.interface, &m_dock_data_manager.interface, &m_rtc.interface));

   IF_OK_RUN_AND_UPDATE(result, init_buttons());

   feed_watchdog();

   if(IS_OK(result))
   {
      DEBUG_INFO("general_control_init completed.");
   }

   return result;
}
