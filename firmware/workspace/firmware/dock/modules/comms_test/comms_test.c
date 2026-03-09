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
 * @todo: General reactor of the general controller module: VOD-1671
 * @todo Remove LIMP mode after verifying that it is no longer needed.
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
#include "nrf.h"
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
#include "battery_manager.h"
#include "ble_control.h"
#include "ble_control_interface.h"
#include "button_driver.h"
#include "buzzer.h"
#include "common.h"
#include "comms_driver_interface.h"
#include "comms_test.h"
#include "debug.h"
#include "dock_data_manager.h"
#include "dose_detection_dock.h"
#include "dose_scheduler.h"
#include "i2c_driver.h"
#include "imu.h"
#include "npm1300_driver.h"
#include "npm1300_driver_interface.h"
#include "nrfx_saadc.h"
#include "queue.h"
#include "ring_manager.h"
#include "rtc_system_time.h"
#include "spi_driver.h"
#include "sts30_dis_temp_sensor.h"
#include "system_time.h"
#include "uart_driver.h"
#include "version.h"

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

#define GRAVITY_EARTH                  (9.80665f)
#define INIT_SYSTICK_PERIOD_MS         (1u) // Initial systick period during initialization.
#define SYSTICK_BLE_PAIRING_TIMEOUT_MS (1000u * 60u)
#define TEMPERATURE_UPDATE_INTERVAL_MS (1000u * 60u * 5u) // NOTE: Should be multiples of 1000ms
STATIC_ASSERT(TEMPERATURE_UPDATE_INTERVAL_MS % 1000u == 0,
              "TEMPERATURE_UPDATE_INTERVAL_MS must be a multiple of 1000ms");
#define TEMPERATURE_OVER_THRESHOLD_REPORT_INTERVAL_MS  (1000u * 60u * 10u)
#define BLE_PARAM_REPORT_INTERVAL_MS                   (1000u)
#define CHECK_RING_STATUS_INTERVAL_MS                  (1000u * 1u * 4u)
#define NOTIFICATION_QUEUE_LEN                         (20u) // Max number of notifications in the queue.
#define INVALID_DOSE_SCHEDULE_REMINDER_INTERVAL_MS     (1000u * 60u * 5u)
#define DOSE_DUE_REMINDER_INTERVAL_MS                  (1000u * 60u * 5u)
#define DOSE_WINDOW_TIMEOUT_MS                         (1000u * 60u * 180u)
#define BATTERY_LEVEL_MIN_REPORT_INTERVAL_MS           (1000u * 60u * 5u)
#define INVALID_DOSE_INFO_ERR_REPORTING_INTERVAL_MS    (1000u * 60u)
#define INVALID_DOSE_INFO_BLE_REQUEST_INTERVAL_MS      (1000u * 10u)
#define APP_BASESLINING_FEEDBACK_REPORTING_INTERVAL_MS (1000u * 1u)
#define DOCK_STATUS_REPORTING_INTERVAL_MS              (1000u * 60u * 5u)
#define BATTERY_STATUS_ERROR_LOG_RATE_LIMIT_MS                                                                         \
   (1000u * 60u * 5u) // Rate limit for logging unrecognized battery states to prevent log spamming

#define MOVING_AVERAGE_TEMPERATURE_WINDOW_SIZE                                                                         \
   ((uint16_t)(DOSE_SCHEDULE_MAX_TEMP_AVG_WINDOW_SEC / (TEMPERATURE_UPDATE_INTERVAL_MS / 1000u)))

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
#define NFC_LOWPOWER_POLL_ON_TIME_MS  (500u * 1u)   // Duration to keep NFC on while polling for activity
#define NFC_LOWPOWER_ACTIVE_HOLD_MS   (1000u * 2u)  // Hold time after last activity before powering down

/**
 * Minimum time for received unix time (used for validating incoming time). This timestamp was recorded during
 * development. Any received time should have progressed from this point and the unix time value should therefore be
 * larger than this.
 */
#define UNIX_TIME_MINIMUM_VAL (1751545437u)

// Low voltage handling defines
#define POWER_CHECK_VBAT_SAMPLES           (3u)
#define POWER_CHECK_VBAT_MAX_ATTEMPTS      (9u)
#define BATTERY_STALE_DATA_RETRY_DELAY_MS  (1100u)
#define ERROR_NOTIFICATION_DURATION_MS     (10000)
#define MINIMUM_BATTERY_VOLTAGE_MV         (3300u) // Voltage at which the device will enter Limp mode to prioritize charging
#define LIMP_MODE_ERROR_BLINKS             (9u)
#define LIMP_MODE_INDICATION_CHARGE_CYCLES (2u)
#define LIMP_MODE_CHARGE_DURATION_S        (15u)
#define SHIP_MODE_1000_MS_DELAY            (1000u)
#define LIMP_MODE_BLINK_DELAY_MS           (300u)
#define LIMP_MODE_CHARGE_HYSTERESIS_MV     (200u)
#define SHIP_MODE_ERROR_BLINKS             (4u)
#define SHIP_MODE_BLINK_DELAY_MS           (150u)
#define DELAY_BEFORE_RESET_MS              (1000u)
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

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
static void wdt_event_handler(void);
static void wdt_init(void);
static void feed_watchdog(void);

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

static void limp_mode(const pmic_status_t *pmic_status, uint16_t vbat);

static void fstorage_evt_handler(nrf_fstorage_evt_t *p_evt);
static void idle_state_handle(void);

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
static uint16_t m_systick_period_ms = 20u;
static imu_t m_imu = {0};

// Other
static nrf_drv_wdt_channel_id m_channel_id; // Watchdog timer channel
static battery_manager_t m_battery = {0};
static npm1300_driver_t m_pmic = {0}; // Battery management IC driver
static volatile flag_t m_flag;        // System flags

static rtc_system_time_t m_rtc;

static ring_manager_t m_ring;
static nfc_driver_t m_nfc = {0};
static message_protocol_t m_msg_prot_nfc = {0}; // NFC message protocol instance
static dock_data_manager_t m_dock_data_manager = {0};

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

   m_dock_data_manager.interface.enqueue(
      &m_dock_data_manager.interface, DATA_ID_DOCK_DEBUG_LOG, &log, sizeof(raw_debug_log_t), 1u);
   return RESULT_OK;
}

result_t debug_ble_tx_handler(const uint8_t *p_data, size_t length)
{
   // Todo
   (void)p_data;
   (void)length;
   return RESULT_OK;
}

void debug_uart_rx_handler(const uint8_t *p_data, size_t length)
{
   (void)debug_on_data_rx(p_data, length);
}

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/
static void fstorage_evt_handler(nrf_fstorage_evt_t *p_evt) // NOSONAR *p_evt required by SDK library
{
   if(p_evt->result != NRF_SUCCESS)
   {
      DEBUG_ERROR("Error while executing an fstorage operation.");
      return;
   }
}

static void limp_mode(const pmic_status_t *pmic_status, uint16_t vbat)
{
   // Charger is connected and battery voltage is low. Enter Limp or Trickle-Charge mode.
   DEBUG_WARNING("LIMP MODE");
   DEBUG_WARNING("Battery voltage low. Trickle charging.");
   DEBUG_WARNING("Battery voltage = %d mV", vbat);
   DEBUG_WARNING("PMIC charger connected = %d", pmic_status->charger_connected);
   DEBUG_WARNING("Battery connected = %d", pmic_status->battery_detected);
   DEBUG_WARNING("Charging stopped due to temp = %d", pmic_status->charging_stopped_die_temp);
   DEBUG_WARNING("Charging = %d", pmic_status->charging);

   for(uint16_t outer = 0u; outer < LIMP_MODE_INDICATION_CHARGE_CYCLES; outer++)
   {
      // Blink error indication
      for(uint16_t idx = 0u; idx < LIMP_MODE_ERROR_BLINKS; idx++)
      {
         feed_watchdog();
         nrf_gpio_pin_toggle(LED_RED);
         nrf_delay_ms(LIMP_MODE_BLINK_DELAY_MS);
         nrf_gpio_pin_toggle(LED_RED);
         nrf_delay_ms(LIMP_MODE_BLINK_DELAY_MS);
      }

      // Charge delay - Allow 15 seconds of uninterrupted time for charging
      for(uint16_t idx = 0; idx < LIMP_MODE_CHARGE_DURATION_S; idx++)
      {
         feed_watchdog();
         nrf_delay_ms(SHIP_MODE_1000_MS_DELAY);
      }
   }

   // Charge until watchdog triggers a reset.
   while(true) // Infinite loop until watchdog resets in case ship mode entry fails
   {
      nrf_delay_ms(SHIP_MODE_1000_MS_DELAY);
   }
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
   SEGGER_RTT_printf(0, "Systick init\n");

   result_t result = RESULT_OK;
   ret_code_t err_code = app_timer_create(&m_systick_timer, APP_TIMER_MODE_SINGLE_SHOT, app_timer_handler_systick);
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_SYSTICK_INIT);

   if(IS_OK(result))
   {
      m_systick_period_ms = 20u;

      err_code = app_timer_start(m_systick_timer, APP_TIMER_TICKS(m_systick_period_ms), NULL);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_SYSTICK_INIT);
   }

   if(IS_ERR(result))
   {
      DEBUG_ERROR("Failed to initialize systick timer. NRF error code: %d", err_code);
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

static void feed_watchdog(void)
{
   nrf_drv_wdt_channel_feed(m_channel_id);
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t general_control_run(void)
{
   result_t result = RESULT_OK;
   static uint64_t prev_main_loop_timestamp_ms = 0;
   DEBUG_INFO("Starting main control loop");

   // Main application loop
   for(;;)
   {
      uint64_t current_systick_time_ms = 0;
      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_time_ms);
      BREAK_ON_ERR(result);

      if(current_systick_time_ms - prev_main_loop_timestamp_ms >= 50u)
      {
         prev_main_loop_timestamp_ms = current_systick_time_ms;
         implement_control_loop();
      }

      feed_watchdog();
      idle_state_handle();
   }

   return result;
}

static void implement_control_loop(void)
{
   result_t result = RESULT_OK;

   uint32_t dev_id0 = NRF_FICR->DEVICEID[0];
   uint32_t dev_id1 = NRF_FICR->DEVICEID[1];
   uint64_t dev_id = (((uint64_t)dev_id1) << 32) | ((uint64_t)dev_id0);

   uint64_t current_systick_ms = 0;
   result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick time for control loop!");

   bool is_ring_present = false;
   result = m_nfc.interface.is_ring_present(&m_nfc.interface, &is_ring_present);
   ON_ERR_DEBUG_ERROR(result, "Failed to get ring presence status for control loop!");

   result = m_nfc.interface.process(&m_nfc.interface, true);
   ON_ERR_DEBUG_ERROR(result, "Failed to process NFC events in control loop!");

   uint32_t current_time_ms = 0;
   result = m_rtc.interface.get_time_unix(&m_rtc.interface, &current_time_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get RTC time for control loop!");

   result = m_ring.interface.process(&m_ring.interface, true);
   ON_ERR_DEBUG_ERROR(result, "Failed to process NFC events in control loop!");
   result = m_msg_prot_nfc.interface.process(&m_msg_prot_nfc.interface);
   ON_ERR_DEBUG_ERROR(result, "Failed to process NFC message protocol events in control loop!");

   size_t ring_battery_level_count = 0;
   m_dock_data_manager.interface.get_element_count(
      &m_dock_data_manager.interface, DATA_ID_RING_BATTERY_LEVEL, &ring_battery_level_count);

   size_t ring_status_count = 0;
   m_dock_data_manager.interface.get_element_count(
      &m_dock_data_manager.interface, DATA_ID_RING_STATUS, &ring_status_count);

   static uint64_t last_status_report_ms = 0;
   if((current_systick_ms - last_status_report_ms > 1000u))
   {
      last_status_report_ms = current_systick_ms;

      SEGGER_RTT_SetTerminal(3);
      SEGGER_RTT_printf(0, "\033[2J\033[;H");
      SEGGER_RTT_printf(0, "---------------------------\n");
      SEGGER_RTT_printf(0, "-- CAP AND CHARGE STATUS --\n");
      SEGGER_RTT_printf(0, "---------------------------\n");
      SEGGER_RTT_printf(0, "ID: 0x%08lx", dev_id1);
      SEGGER_RTT_printf(0, "%08lx\n", dev_id0);
      SEGGER_RTT_printf(0, "Time: %us\n", current_time_ms);
      SEGGER_RTT_printf(0, "Systick: %lums\n", current_systick_ms);
      SEGGER_RTT_printf(0, "Ring battery level count: %d\n", ring_battery_level_count);
      SEGGER_RTT_printf(0, "Ring status count: %d\n", ring_status_count);
      is_ring_present ? SEGGER_RTT_printf(0, "Ring is present\n") : SEGGER_RTT_printf(0, "Ring is NOT present\n");

      SEGGER_RTT_SetTerminal(0);
   }
}

result_t general_control_init(void)
{
   result_t result = RESULT_OK;

   DEBUG_INFO("general_control_init start.");

   DEBUG_INFO("wdt_init");
   wdt_init();
   nrfx_clock_lfclk_start();

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

      // I2C read error - possibly due to battery being too low - enter limp mode as a safety mechanism
      if(IS_ERR(result) && (NPM1300_DRV_ERROR_FAILED_I2C == GET_ERR_CODE(result)))
      {
         DEBUG_WARNING("I2C read error during init. VBAT assumed to be low, entering limp mode");
         pmic_status_t status = {0};
         limp_mode(&status, 0);
      }
   }

   IF_OK_RUN_AND_UPDATE(result, battery_manager_init(&m_battery, &m_pmic.interface));

   // NOTE: This needs to happen before debug_log_init, as that is where the dock_data_manager endpoint is
   // registered.
   IF_OK_RUN_AND_UPDATE(result, dock_data_manager_init(&m_dock_data_manager));
   IF_OK_RUN_AND_UPDATE(result, debug_log_init());
   IF_OK_RUN_AND_UPDATE(result, systick_timer_init());
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

   m_flag = (flag_t){0};

   IF_OK_RUN_AND_UPDATE(result, nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface));
   if(IS_ERR(result))
   {
      SEGGER_RTT_printf(0, "Failed to initialize NFC\n");
      nrf_delay_ms(100);
   }

   IF_OK_RUN_AND_UPDATE(
      result,
      message_protocol_init(&m_msg_prot_nfc, &m_systick.interface, &m_nfc.data_ifc, true, NULL, NULL, NULL, NULL));
   if(IS_ERR(result))
   {
      SEGGER_RTT_printf(0, "Failed to init MSG PROT\n");
      nrf_delay_ms(100);
   }

   IF_OK_RUN_AND_UPDATE(
      result,
      ring_manager_init(
         &m_ring, &m_systick.interface, &m_msg_prot_nfc.interface, &m_dock_data_manager.interface, &m_rtc.interface));
   if(IS_ERR(result))
   {
      SEGGER_RTT_printf(0, "Failed to init Ring manager\n");
      nrf_delay_ms(100);
   }

   while(IS_ERR(result))
   {
      feed_watchdog();
      SEGGER_RTT_printf(0, "Initialization completed with errors. Halting...\n");
      SEGGER_RTT_printf(0, "ERR UNIT: %d ERR CODE: %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      nrf_delay_ms(1000);
   }
   SEGGER_RTT_printf(0, "general_control_init completed.\n");

   return result;
}
