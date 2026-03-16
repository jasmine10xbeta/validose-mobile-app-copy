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

// NRF SDK includes
#include "../../bsp/project.h"
#include "../../bsp/sdk_config.h"
#include "app_error.h"
#include "app_timer.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"
#include "nrf_crypto.h"
#include "nrf_crypto_error.h"
#include "nrf_delay.h"
#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_wdt.h"
#include "nrf_fstorage.h"
#include "nrf_fstorage_sd.h"
#include "nrf_gpio.h"
#include "nrf_power.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_saadc.h"
#include "nrf_sdm.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"
#include "nrfx_saadc.h"

// Custom includes
#include "SEGGER_RTT.h"
#include "ble_control_ring.h"
#include "bq25150_driver.h" // PMIC
#include "cap_detection.h"
#include "common.h"
#include "debug.h"
#include "dock_manager.h"
#include "dose_detection.h"
#include "fds_manager.h"
#include "general_control.h"
#include "i2c_driver.h"
#include "imu.h"
#include "queue.h"
#include "ring_battery_manager.h"
#include "ring_data_manager.h"
#include "ring_sources.h"
#include "st25dv_mb_driver.h"
#include "system_time.h"
#include "tilt_detection.h"
#include "tmd2635_driver.h"
#include "uart_driver.h"

#include "SEGGER_RTT.h"

#include "nfc_tag_driver_interface.h"
#include "nfc_tag_eeprom_queue.h"
#include "nfc_tag_eeprom_queue_interface.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_GENERAL_CONTROL_RING;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define GRAVITY_EARTH                  (9.80665f)
#define UPRIGHT_THRESHOLD              (8825) // 8825 mm/s^2 -> 0.9g
#define SIDEWAYS_THRESHOLD             (980)  // 980 mm/s^2 -> 0.1g
#define SYSTICK_BLE_PAIRING_TIMEOUT_MS (1000u * 60 * 2)
#define BLE_PAIR_PRIMING_TIMEOUT_MS    (3000u)
#define INIT_SYSTICK_PERIOD_MS         (20u) // Initial systick period during initialization.
#define MAX_CAP_OFF_PERIOD_MS          (1000u * 60 * 10u)

/**
 * Systick period and sleep periods: To conserve power, the device will generally use a lower systick tick rate and
 * longer sleep time (time between implementation_control_loop calls). When the container cap is removed, the device
 * switches to a faster tick rate and shorter sleep time to be more responsive during active dose detection.
 */
#define SLEEP_PERIOD_IDLE_MS        (50u)   // Bottle cap ON
#define SYSTICK_PERIOD_IDLE_MS      (25u)   // Bottle cap ON
#define SYSTICK_PERIOD_LOW_POWER_MS (1000u) // Bottle cap ON
#define SLEEP_PERIOD_ACTIVE_MS      (50u)   // Bottle cap OFF
#define SYSTICK_PERIOD_ACTIVE_MS    (25u)   // Bottle cap OFF

// Low voltage handling defines
#define BATTERY_DIFF_REPORT_THRESHOLD     (5) // percentage. 'u' suffix left out on purpose for comparison to abs() -> int
#define POWER_CHECK_VBAT_SAMPLES          (3u)
#define POWER_CHECK_VBAT_MAX_ATTEMPTS     (9u)
#define BATTERY_STALE_DATA_RETRY_DELAY_MS (1100u)
#define MINIMUM_BATTERY_VOLTAGE_MV        (3300u)
#define SHIP_MODE_1000_MS_DELAY           (1000u)
#define SHIP_MODE_ERROR_BLINKS            (4u)
#define SHIP_MODE_BLINK_DELAY_MS          (150u)
#define CHARGING_LED_BLINK_INTERVAL_MS    (1000u)

// Device primary key and BLE passkey flash addresses
#define APP_DATA_ADDR_DEVICE_PRIMARY_KEY     (0xFD000)
#define APP_DATA_FLASH_SIZE                  (0x1000)
#define APP_DATA_ADDR_DEVICE_PRIMARY_KEY_LEN (32u)
#define APP_DATA_ADDR_DEVICE_BT_PASSKEY      (0xFD020)
#define APP_DATA_ADDR_DEVICE_BT_PASSKEY_LEN  (32u)

// Data path defines
#define BATTERY_QUEUE_DEPTH (200u)
#define ERROR_QUEUE_DEPTH   (250u)
#define DOSE_QUEUE_SLOTS    (4u) // Number of wear levelling slots for the dose queue metadata

#define DEFAULT_CAP_DETECTION_HYSTERESIS (150u)  // Default hysteresis value for cap detection
#define DEFAULT_CAP_DETECTION_THRESHOLD  (5000u) // Default threshold for cap detection

/**********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct
{
   bool pairing_gesture_detected; /**< Gesture to put the device in BLE pairing mode */
   bool is_pairing;               /**< Indicates if BLE pairing is active or not */
   bool is_cap_removed;           /**< Used to detect when to change to active state for dose detection */
   bool is_in_low_power_mode;     /**< True if the device is in low power mode */
   bool ble_connected;
   bool ble_bonded;
} flag_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
static void wdt_event_handler(void);
static void wdt_init(void);
static void feed_watchdog(void);
static result_t debug_log_init(void);
static result_t load_ble_passkey(char *passkey_text);

/**
 * @brief System timer event handler.
 *
 * This function is called when the system timer expires. It increments the systick time by the set period,
 * starts the systick timer again, and handles any potential errors.
 *
 * @param[in] p_context Unused parameter required by the SDK.
 */
static void app_timer_handler_systick(void *p_context);

static void app_timer_handler_testing(void *p_context);

/**
 * @brief RTC timer event handler.
 *
 * This function is called when the RTC timer expires. It increments the RTC time by the set period.
 *
 * @param[in] p_context Unused parameter required by the SDK.
 */
static void app_timer_handler_rtc(void *p_context);

/**
 * @brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static result_t application_timer_init(void);

/**
 * @brief Initializes the system tick timer and timer used to keep track of unix time for short periods.
 *
 * Timer 1: This function creates and starts a single-shot timer using the NRF5 SDK's app_timer module.
 * The timer is set to call the app_timer_handler_systick function every m_systick_period_ms milliseconds. This
 * allows for dynamically changing the systick period based on the current system state which is useful for power
 * management. E.g. make the systick period short when a lot of active processing or monitoring is required and long
 * in states where less responsiveness is required to conserve battery life.
 *
 * Timer 2: Simple App timer used to keep track of Unix time for short periods (< 1 day).
 *
 * @return A result_t indicating success or failure.
 * @retval RESULT_OK If the timer was successfully initialized and started.
 * @retval GC_ERROR_SYSTICK_INIT If there was an error initializing or starting the timer.
 */
static result_t timers_init(void);

static void ship_mode(void);

static void on_ble_connection_status_update(DEVICE_BLE_EVT event);
static void on_ble_device_commands_update(uint32_t command);
static void on_ble_nus_rx_data(const uint8_t *p_data, size_t length);

/**
 * @brief BLE pairing state: Handles BLE pairing / bonding related tasks.
 */
static void handle_ble_pairing(void);

static void fstorage_evt_handler(nrf_fstorage_evt_t *p_evt);

/** @brief Function for handling shutdown events.
 */
static bool shutdown_event_handler(nrf_pwr_mgmt_evt_t event);

static result_t power_management_init(void);

/** @brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void);

// Control related functions
static void implement_control_loop(void);

/***********************************************************************************************************************
 * Global function declarations
 **********************************************************************************************************************/
result_t debug_uart_tx_handler(const uint8_t *p_data, size_t length);
result_t debug_nfc_tx_handler(const uint8_t *p_data, size_t length);
void debug_uart_rx_handler(const uint8_t *p_data, size_t length);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
/**@brief Register application shutdown handler with priority 0.
 */
NRF_PWR_MGMT_HANDLER_REGISTER(shutdown_event_handler, 0);

// I2C setup
static nrf_drv_twi_t m_twi_instance_0 = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_0);
const nrf_drv_twi_config_t m_i2c_config_0 = {.scl = I2C0_SCL_PIN,
                                             .sda = I2C0_SDA_PIN,
                                             .frequency = NRF_DRV_TWI_FREQ_400K,
                                             .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
                                             .clear_bus_init = false};
static i2c_driver_t m_i2c_driver_0;

// I2C1 setup
static nrf_drv_twi_t m_twi_instance_1 = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_1);
const nrf_drv_twi_config_t m_i2c_config_1 = {.scl = I2C1_SCL_PIN,
                                             .sda = I2C1_SDA_PIN,
                                             .frequency = NRF_DRV_TWI_FREQ_400K,
                                             .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
                                             .clear_bus_init = false};
static i2c_driver_t m_i2c_driver_1;

NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) = {
   .evt_handler = fstorage_evt_handler,
   .start_addr = APP_DATA_ADDR_DEVICE_PRIMARY_KEY,
   .end_addr = APP_DATA_ADDR_DEVICE_PRIMARY_KEY + APP_DATA_FLASH_SIZE,
}; // Storage for passkey. Addresses need to correspond to `APP_DATA` section in linker script

// Application timers
APP_TIMER_DEF(m_systick_timer);
APP_TIMER_DEF(m_rtc_timer);
APP_TIMER_DEF(m_testing_timer); // Testing only. Used for heartbeat indications, etc.

// Application unit instances
static system_time_t m_systick = {0};
static system_time_t m_rtc = {0};
static uint16_t m_systick_period_ms = INIT_SYSTICK_PERIOD_MS;
static imu_t m_imu = {0};
static bq25150_driver_t m_pmic;

static nrf_drv_wdt_channel_id m_channel_id; // Watchdog timer channel
static volatile flag_t m_flag;              // System flags

static ble_control_evt_handlers_t m_ble_evt_handlers = {0};
static ring_battery_manager_t m_battery = {0};
static dose_detection_t m_dose_detect = {0};
static cap_detection_t m_cap_detect = {0};
static tilt_detection_t m_tilt_detect = {0};

static st25dv_driver_t m_nfc = {0};

// Datapath instances
static dock_manager_t m_dock_manager = {0};
static ring_data_manager_t m_data_manager = {0};
static ring_sources_t m_ring_sources = {0};
static fds_manager_t m_fds_manager = {0};

static nfc_tag_eeprom_queue_t m_dose_queue;
static queue_t battery_queue;
static uint8_t battery_queue_storage[BATTERY_QUEUE_DEPTH * sizeof(ring_battery_data_t)];
static queue_t error_queue;
static uint8_t error_queue_storage[ERROR_QUEUE_DEPTH * sizeof(raw_debug_log_t)];

static message_protocol_t m_message_protocol = {0};

/* --- IR Proximity Sensor --- */
static tmd2635_driver_t m_prox = {0};

static const tmd2635_config_t m_prox_cfg = {
   .i2c_addr = TMD26353_I2C_ADDRESS,
   .photodiode = TMD2635_PD_NEAR,      // Near photodiode for close range detection
   .gain = TMD2635_GAIN_2X,            // 2x gain to improve sensitivity and put measurements in mid-range
   .max_pulses = 8u,                   // Balanced between power consumption and measurement quality
   .pulse_len = TMD2635_PULSE_LEN_1US, // Shortest pulse length to avoid saturation at close range
   .drive_current = TMD2635_DRIVE_7MA, // Lowest drive current to save power and reduce variation
   .hw_avg = TMD2635_HWAVG_16,         // Good balance between measurement quality and time/power consumption
   .moving_avg = TMD2635_MAVG_4        // Moderate moving average to reduce noise and bouncing
};

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
   return m_debug_uart_driver.interface.transmit(&(m_debug_uart_driver.interface), p_data, length);
}

result_t debug_nfc_tx_handler(const uint8_t *p_data, size_t length)
{
   (void)p_data;
   (void)length;
   return RESULT_OK;

   // raw_debug_log_t log = {0};
   // (void)debug_map_unencoded_log(&log, p_data, length);
   // return m_data_manager.interface.enqueue_error(&m_data_manager.interface, &log);
}

void debug_uart_rx_handler(const uint8_t *p_data, size_t length)
{
   (void)debug_on_data_rx(p_data, length);
}

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/

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
      passkey_text[BLE_PASSKEY_LENGTH - 1u] = 0x00; // Ensure null-termination
   }

   return result;
}

static result_t timers_init(void)
{
   result_t result = RESULT_OK;
   // Systick timer
   ret_code_t err_code = app_timer_create(&m_systick_timer, APP_TIMER_MODE_SINGLE_SHOT, app_timer_handler_systick);
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_SYSTICK_INIT);

   if(IS_OK(result))
   {
      m_systick_period_ms = INIT_SYSTICK_PERIOD_MS;

      err_code = app_timer_start(m_systick_timer, APP_TIMER_TICKS(m_systick_period_ms), NULL);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_SYSTICK_INIT);
   }

   // RTC timer
   if(IS_OK(result))
   {
      err_code = app_timer_create(&m_rtc_timer, APP_TIMER_MODE_REPEATED, app_timer_handler_rtc);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_RTC_TIMER_INIT);
   }
   if(IS_OK(result))
   {
      err_code
         = app_timer_start(m_rtc_timer, APP_TIMER_TICKS(1000u), NULL); // 1 second timer to keep track of Unix time
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_RTC_TIMER_INIT);
   }
   // testing timer
   if(IS_OK(result))
   {
      err_code = app_timer_create(&m_testing_timer, APP_TIMER_MODE_SINGLE_SHOT, app_timer_handler_testing);
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_RTC_TIMER_INIT);
   }
   return result;
}

static void app_timer_handler_testing(void *p_context) // NOSONAR - p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   nrf_gpio_pin_clear(LED_GREEN);
}

static void app_timer_handler_rtc(void *p_context) // NOSONAR - p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   (void)m_rtc.interface.inc_time_by_1000_ms(&m_rtc.interface);
}

static void app_timer_handler_systick(void *p_context) // NOSONAR - p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   ret_code_t error = app_timer_start(m_systick_timer, APP_TIMER_TICKS(m_systick_period_ms), NULL);

   if(error != NRF_SUCCESS)
   {
      DEBUG_ERROR("Systick timer failed to start. NRF_ERROR: %d", error);
      nrf_delay_ms(1);
   }

   // Reset device if the systick timer failed.
   APP_ERROR_CHECK(error);

   (void)m_systick.interface.inc_time_by_set_val_ms(&m_systick.interface, m_systick_period_ms);
}

static void ship_mode(void)
{
   DEBUG_INFO("SHIP MODE. Charger not connected and battery low.");

   // Blink error indication
   for(uint16_t idx = 0u; idx < SHIP_MODE_ERROR_BLINKS; idx++)
   {
      feed_watchdog();
      nrf_gpio_pin_toggle(LED_RED);
      nrf_delay_ms(SHIP_MODE_BLINK_DELAY_MS);
      nrf_gpio_pin_toggle(LED_RED);
      nrf_delay_ms(SHIP_MODE_BLINK_DELAY_MS);
   }

   feed_watchdog();
   (void)m_battery.interface.enter_ship_mode(&m_battery.interface);

   while(true) // Infinite loop until watchdog resets in case ship mode entry fails
   {
      SEGGER_RTT_printf(0, "Ship mode mode!\n");
      nrf_delay_ms(SHIP_MODE_1000_MS_DELAY);
   }
}

static result_t debug_log_init(void)
{
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
      result = debug_init(&(m_debug_rx_queue.interface), &m_systick.interface, NULL);
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
      ON_ERR_DEBUG_ERROR(result, "Failed to initialize debug UART driver");
   }

   if(IS_OK(result))
   {
      m_debug_uart_driver_interface = &(m_debug_uart_driver.interface);

      endpoint_t uart_endpoint = {0};
      uart_endpoint.apply_encoding = true;
      uart_endpoint.disable_auto_routing = true;
      uart_endpoint.handler = debug_uart_tx_handler;
      uart_endpoint.permit_bulk_data = false;
      uart_endpoint.id = ENDPOINT_UART;

      result = debug_register_endpoint(&uart_endpoint);
      ON_ERR_DEBUG_ERROR(result, "Failed to register UART endpoint");
   }
#endif /* DEBUG_DISABLE_UART */

   if(IS_OK(result))
   {
      endpoint_t nfc_endpoint = {0};
      nfc_endpoint.apply_encoding = false;
      nfc_endpoint.disable_auto_routing = false;
      nfc_endpoint.handler = debug_nfc_tx_handler;
      nfc_endpoint.permit_bulk_data = false;
      nfc_endpoint.id = ENDPOINT_USER_DEFINED_1;

      result = debug_register_endpoint(&nfc_endpoint);
      ON_ERR_DEBUG_ERROR(result, "Failed to register NFC debug endpoint");

      IF_OK_RUN_AND_UPDATE(result, debug_set_all_unit_debug_level_for_endpoint(DEBUG_LEVEL_ERROR, nfc_endpoint.id));
      ON_ERR_DEBUG_ERROR(result, "Failed to set debug level for NFC debug endpoint");
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

static void idle_state_handle(void)
{
   nrf_pwr_mgmt_run();
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

static void on_ble_connection_status_update(DEVICE_BLE_EVT event)
{
   switch(event)
   {
      case DEVICE_BLE_EVT_CONNECTED:
      {
         DEBUG_INFO("BLE connected");
         m_flag.ble_connected = true;
      }
      break;

      case DEVICE_BLE_EVT_DISCONNECTED:
      {
         DEBUG_INFO("BLE disconnected");
         m_flag.ble_connected = false;
      }
      break;

      case DEVICE_BLE_EVT_NONE:
         // No action
         break;

      case DEVICE_BLE_EVT_TIMEOUT:
      {
         DEBUG_INFO("BLE timeout");
         m_flag.ble_connected = false;
      }
      break;

      case DEVICE_BLE_EVT_MAX:
         // Fall through
      default:
         m_flag.ble_connected = false;
         break;
   }
}

static void on_ble_nus_rx_data(const uint8_t *p_data, size_t length)
{
   (void)debug_on_data_rx(p_data, length);
}

static void on_ble_device_commands_update(uint32_t command)
{
   DEBUG_INFO("BLE test val received: %d", command);

   if(BLE_DEVICE_COMMAND_ENTER_SHIP_MODE == command)
   {
      (void)m_pmic.interface.enter_ship_mode(&m_pmic.interface);
   }
   else if(BLE_DEVICE_COMMAND_RESET_DEVICE == command)
   {
      // TODO: Store current dose events in flash

      // Reset the device
      SEGGER_RTT_printf(0, "\n\nRESETTING THE DEVICE....\n\n");
      nrf_delay_ms(10);
      sd_nvic_SystemReset();
   }
}

static void fstorage_evt_handler(nrf_fstorage_evt_t *p_evt) // NOSONAR *p_evt required by SDK library
{
   if(p_evt->result != NRF_SUCCESS)
   {
      DEBUG_ERROR("Error while executing an fstorage operation.");
      return;
   }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void handle_ble_pairing(void)
{
   if(!m_flag.is_pairing)
   {
      return;
   }

   result_t result = RESULT_OK;
   static uint64_t pairing_state_start_time_ms = 0u;
   uint64_t current_systick_time_ms = 0u;
   static bool is_pairing_starting = true;

   nrf_gpio_pin_set(LED_BLUE); // Set LED to indicate pairing mode is active

   // Check for pairing state first time running
   if(is_pairing_starting)
   {
      is_pairing_starting = false;
      // Pairing state first run, get timestamp
      result = m_systick.interface.get_time_ms(&m_systick.interface, &pairing_state_start_time_ms);
      ON_ERR_DEBUG_ERROR(result, "Failed to get systick time for pairing state start time!");

      // Load BLE passkey
      char decrypted_passkey_text[64] = {0x00};
      result = load_ble_passkey(decrypted_passkey_text);
      ON_ERR_DEBUG_ERROR(result, "Failed to load BLE passkey!");

      // Start advertising
      if(IS_OK(result))
      {
         ble_control_start_advertising(true, decrypted_passkey_text);
         memset(decrypted_passkey_text, 0, 64);
      }
   }
   else
   {
      // Not Pairing state first run - Check if bonded & not still permitting new bonds
      result = ble_control_get_bonded_status(&m_flag.ble_bonded);
      ON_ERR_DEBUG_ERROR(result, "Failed to get bonded status!");

      bool is_permitting_new_bonds = false;
      result = ble_control_is_permitting_new_bonds(&is_permitting_new_bonds);
      ON_ERR_DEBUG_ERROR(result, "Failed to get permitting new bonds status!");

      // Check if bonded successfully.
      if(m_flag.ble_bonded && (false == is_permitting_new_bonds))
      {
         is_pairing_starting = true; // Pairing concluded, setting status for next pairing gesture detected event
         m_flag.is_pairing = false;  // Pairing concluded, setting status for next pairing gesture detected event
         DEBUG_INFO("Bonded successfully, restarting advertising with whitelist");
         ble_control_start_advertising(false, NULL);

         nrf_gpio_pin_clear(LED_BLUE); // Stop HMI notification for pairing
      }
   }

   // Get the current timestamp
   result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_time_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick time for pairing state current time!");

   // Check for pairing state timeout
   if(current_systick_time_ms - pairing_state_start_time_ms > SYSTICK_BLE_PAIRING_TIMEOUT_MS)
   {
      DEBUG_INFO("Pairing timeout.");

      // Pairing timeout, leave state
      result = ble_control_get_bonded_status(&m_flag.ble_bonded);
      ON_ERR_DEBUG_ERROR(result, "Failed to get bonded status!");
      nrf_gpio_pin_clear(LED_BLUE);

      if(m_flag.ble_bonded)
      {
         DEBUG_INFO("Retained pre-existing bonding credentials. Restarting advertising with whitelist.");
         is_pairing_starting = true; // Pairing concluded, setting status for next pairing gesture detected event
         m_flag.is_pairing = false;  // Pairing concluded, setting status for next pairing gesture detected event
         ble_control_start_advertising(false, NULL);
      }
      else
      {
         DEBUG_INFO("No pre-existing bonding credentials. Device is unbonded.");
         is_pairing_starting = true; // Pairing concluded, setting status for next pairing gesture detected event
         m_flag.is_pairing = false;  // Pairing concluded, setting status for next pairing gesture detected event
         ble_control_stop_advertising();
      }
   }
}
#pragma GCC diagnostic pop

static void implement_control_loop(void)
{
   result_t result = RESULT_OK;

   uint64_t current_systick_ms = 0u;
   result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_ms);
   ON_ERR_DEBUG_ERROR(result, "Failed to get systick time.");
   uint32_t current_systick_s = (uint32_t)(current_systick_ms / COMMON_1K_CST);

   // Statuses that require I2C reads are rate limited
   // Static to keep the previous value if the data is stale.
   static battery_status_t battery = {0};
   static IMU_STATE imu_state = IMU_STATE_UNINITIALIZED;
   static CAP_STATE cap_state = CAP_STATE_UNKNOWN;
   static uint16_t prox_val = 0u;
   static uint8_t dose_queue_element_count = 0u;

   // Process functions
   result = m_data_manager.interface.process(&m_data_manager.interface);
   ON_ERR_DEBUG_ERROR(result, "Failed to process data manager!");
   result = m_message_protocol.interface.process(&m_message_protocol.interface);
   result = m_dock_manager.interface.process(&m_dock_manager.interface, true);
   ON_ERR_DEBUG_ERROR(result, "Failed to process dock manager!");

   static uint64_t last_rate_limited_status_query = 0u;
   if(current_systick_ms - last_rate_limited_status_query > 500u)
   {
      last_rate_limited_status_query = current_systick_ms;

      // Update the cap status
      result = m_cap_detect.interface.get_cap_status(&m_cap_detect.interface, &cap_state, &prox_val);
      m_flag.is_cap_removed = (cap_state == CAP_STATE_OPEN);
      ON_ERR_DEBUG_ERROR(result, "Failed to get cap status!");

      // Raise an error once if the cap is off for too long
      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_ms);
      ON_ERR_DEBUG_ERROR(result, "Failed to get systick time!");

      static bool has_raised_cap_error = false;
      static uint64_t cap_removed_start_time_ms = 0u;
      if(m_flag.is_cap_removed && !has_raised_cap_error)
      {
         if(current_systick_ms - cap_removed_start_time_ms > MAX_CAP_OFF_PERIOD_MS)
         {
            DEBUG_ERROR("Cap has been removed for more than %d seconds. Raising cap error.",
                        MAX_CAP_OFF_PERIOD_MS / COMMON_1K_CST);

            has_raised_cap_error = true;
         }
      }
      else if(!m_flag.is_cap_removed)
      {
         has_raised_cap_error = false;
         cap_removed_start_time_ms = current_systick_ms;
      }

      // Update the battery status
      result = m_battery.interface.get_battery_status(&m_battery.interface, &battery);
      ON_ERR_DEBUG_ERROR(result, "Failed to get battery status!");

      result = m_imu.interface.get_state(&m_imu.interface, &imu_state);
      ON_ERR_DEBUG_ERROR(result, "Failed to get IMU state!");

      // Retrieve the number of elements in the dose queue
      result = m_dose_queue.interface.nfc_get_element_count(&m_dose_queue.interface, &dose_queue_element_count);
      ON_ERR_DEBUG_ERROR(result, "Failed to get dose queue element count!");
   }
   static uint64_t prev_blink_ms = 0u;

   if(battery.charger_connected)
   {
      if(current_systick_ms - prev_blink_ms > 1000u)
      {
         prev_blink_ms = current_systick_ms;
         nrf_gpio_pin_set(LED_GREEN);
         nrf_delay_ms(1);
         nrf_gpio_pin_clear(LED_GREEN);
      }
   }
   else
   {
      nrf_gpio_pin_clear(LED_GREEN);
   }

   static uint64_t last_status_report_ms = 0u;
   if((current_systick_ms - last_status_report_ms > 1000u))
   {
      last_status_report_ms = current_systick_ms;

      ring_status_t ring_status = {0};
      m_data_manager.interface.get_status(&m_data_manager.interface, &ring_status);

      uint16_t cap_detection_threshold = 0u;
      uint16_t cap_detection_hysteresis = 0u;

      m_cap_detect.interface.get_config(&m_cap_detect.interface, &cap_detection_threshold, &cap_detection_hysteresis);

      SEGGER_RTT_SetTerminal(1);
      SEGGER_RTT_printf(0, "\033[2J\033[;H");
      SEGGER_RTT_printf(0, "-- CAP AND CHARGE STATUS --\n");
      SEGGER_RTT_printf(0, "Systick: %us\n", current_systick_s);

      battery.charger_connected ? SEGGER_RTT_printf(0, "Charger: Connected\n") :
                                  SEGGER_RTT_printf(0, "Charger: Not Connected\n");
      battery.battery_state == BATTERY_STATE_CHARGING ? SEGGER_RTT_printf(0, "Battery: Charging\n") : 1;
      battery.battery_state == BATTERY_STATE_CHARGING_COMPLETED ? SEGGER_RTT_printf(0, "Battery: Charge Complete\n") :
                                                                  1;
      battery.battery_state == BATTERY_STATE_ERROR ? SEGGER_RTT_printf(0, "Battery: Error\n") : 1;
      battery.battery_state == BATTERY_STATE_SOC_GOOD ? SEGGER_RTT_printf(0, "Battery: SOC Good\n") : 1;
      battery.battery_state == BATTERY_STATE_SOC_LOW ? SEGGER_RTT_printf(0, "Battery: SOC Low\n") : 1;

      SEGGER_RTT_printf(0, "VBAT: %umV\n", battery.battery_voltage_mv);
      SEGGER_RTT_printf(0, "VIN: %umV\n", battery.vin_mv);
      SEGGER_RTT_printf(0, "IIN: %umA\n", battery.iin_ma);
      SEGGER_RTT_printf(0, "Charge: %u%%\n", battery.battery_level);

      m_flag.is_in_low_power_mode ? SEGGER_RTT_printf(0, "Power mode: LOW POWER\n") :
                                    SEGGER_RTT_printf(0, "Power mode: HIGH POWER\n");
      imu_state == IMU_STATE_ACTIVE ? SEGGER_RTT_printf(0, "IMU state: ACTIVE\n") :
                                      SEGGER_RTT_printf(0, "IMU state: DORMANT\n");

      // SEGGER_RTT_SetTerminal(2);
      // SEGGER_RTT_printf(0, "\033[2J\033[;H");
      // SEGGER_RTT_printf(0, "------- RING STATUS -------\n");
      // SEGGER_RTT_printf(0, "Systick: %lums\n", current_systick_ms);
      // SEGGER_RTT_printf(0, "Dose fifo elements: %u\n", dose_queue_element_count);
      // SEGGER_RTT_printf(0, "Dose fifo used: %u%%\n", ring_status.dose_fifo_used_percent);
      // SEGGER_RTT_printf(0, "Dose fifo watermark: %u%%\n", ring_status.dose_fifo_used_percent_watermark);
      // SEGGER_RTT_printf(0, "Battery fifo elements: %u\n", m_data_manager._battery_queue_ifc->parent->_element_count);
      // SEGGER_RTT_printf(0, "Battery fifo used: %u%%\n", ring_status.battery_fifo_used_percent);
      // SEGGER_RTT_printf(0, "Battery fifo watermark: %u%%\n", ring_status.battery_fifo_used_percent_watermark);
      // SEGGER_RTT_printf(0, "Error fifo used: %u%%\n", ring_status.error_fifo_used_percent);
      // SEGGER_RTT_printf(0, "Error fifo watermark: %u%%\n", ring_status.error_fifo_used_percent_watermark);
      // SEGGER_RTT_printf(0, "Battery sample frequency: %u milliHz\n", ring_status.battery_sample_frequency_millihz);

      // ring_status.battery_charge_status == BATTERY_STATE_CHARGING ?
      //    SEGGER_RTT_printf(0, "Battery charge status: Charging\n") :
      //    1;
      // ring_status.battery_charge_status == BATTERY_STATE_CHARGING_COMPLETED ?
      //    SEGGER_RTT_printf(0, "Battery charge status: Charge Complete\n") :
      //    1;
      // ring_status.battery_charge_status == BATTERY_STATE_SOC_GOOD ?
      //    SEGGER_RTT_printf(0, "Battery charge status: SOC Good\n") :
      //    1;
      // ring_status.battery_charge_status == BATTERY_STATE_SOC_LOW ?
      //    SEGGER_RTT_printf(0, "Battery charge status: SOC Low\n") :
      //    1;

      SEGGER_RTT_SetTerminal(3);
      SEGGER_RTT_printf(0, "\033[2J\033[;H");
      SEGGER_RTT_printf(0, "----- DOSE DETECTION ------\n");
      SEGGER_RTT_printf(0, "Systick: %ums\n", current_systick_s);
      SEGGER_RTT_printf(0, "Doses detected: %d\n", m_dose_detect._dose_event_queue._element_count);
      cap_state == CAP_STATE_CLOSED ? SEGGER_RTT_printf(0, "Cap state: CLOSED\n") : 1;
      cap_state == CAP_STATE_OPEN ? SEGGER_RTT_printf(0, "Cap state: OPEN\n") : 1;
      cap_state == CAP_STATE_UNKNOWN ? SEGGER_RTT_printf(0, "Cap state: UNKNOWN\n") : 1;
      SEGGER_RTT_printf(0, "Prox value: %u\n", prox_val);
      SEGGER_RTT_printf(
         0, "Cap detection Config\nThreshold: %u\nHysteresis: %u\n", cap_detection_threshold, cap_detection_hysteresis);

      SEGGER_RTT_SetTerminal(0);
   }
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

      // Switch between power saving and active systick periods based on cap detection.
      m_systick_period_ms = m_flag.is_cap_removed ? SYSTICK_PERIOD_ACTIVE_MS : SYSTICK_PERIOD_IDLE_MS;
      uint64_t loop_period_ms = m_flag.is_cap_removed ? SLEEP_PERIOD_ACTIVE_MS : SLEEP_PERIOD_IDLE_MS;

      m_flag.is_in_low_power_mode = !m_flag.is_cap_removed;

      uint64_t current_systick_time_ms = 0u;
      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_time_ms);
      RETURN_ON_ERR(result);

      if(current_systick_time_ms - prev_main_loop_timestamp_ms >= loop_period_ms)
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
   DEBUG_INFO("general_control_init start.");

   result_t result = RESULT_OK;
   m_flag = (flag_t){0}; // Initialize flags

   DEBUG_INFO("wdt_init");
   wdt_init();

   // Initialize the async SVCI interface to bootloader before any interrupts are enabled.
   if(IS_OK(result))
   {
      uint32_t err_code = ble_dfu_buttonless_async_svci_init();
      UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_NRF_ERR_CHECK_DFU);
   }

   IF_OK_RUN_AND_UPDATE(result, system_time_init(&m_systick));
   IF_OK_RUN_AND_UPDATE(result, system_time_init(&m_rtc));
   IF_OK_RUN_AND_UPDATE(result, application_timer_init());
   IF_OK_RUN_AND_UPDATE(result, timers_init());
   IF_OK_RUN_AND_UPDATE(result, power_management_init());
   IF_OK_RUN_AND_UPDATE(result, i2c_driver_init(&m_i2c_driver_0, &m_twi_instance_0, &m_i2c_config_0));
   IF_OK_RUN_AND_UPDATE(result, i2c_driver_init(&m_i2c_driver_1, &m_twi_instance_1, &m_i2c_config_1));
   IF_OK_RUN_AND_UPDATE(result,
                        bq25150_driver_init(&m_pmic, &m_i2c_driver_1.interface, PMIC_I2C_ADDRESS, BM_LP, BM_CE));

   // Explicitly initialise the cryto module
   ret_code_t err_code = nrf_crypto_init();
   UPDATE_IF_NRF_ERR(err_code, result, GC_ERROR_CRYPTO_INIT);

   // I2C read error - possibly due to battery being too low - enter limp mode as a safety mechanism
   if(IS_ERR(result) && (BQ25150_DRV_ERROR_FAILED_I2C == GET_ERR_CODE(result)))
   {
      DEBUG_WARNING("I2C read error during init. VBAT assumed to be low, entering ship mode");
      ship_mode();
   }
   IF_OK_RUN_AND_UPDATE(result, ring_battery_manager_init(&m_battery, &m_pmic.interface));
   IF_OK_RUN_AND_UPDATE(result, debug_log_init());

   // Init BLE control event handler instance and pass it to the ble_control module via ble_control_init().
   if(IS_OK(result))
   {
      m_ble_evt_handlers.on_ble_connection_status_update = on_ble_connection_status_update;
      m_ble_evt_handlers.on_ble_device_commands_update = on_ble_device_commands_update;
      m_ble_evt_handlers.on_ble_nus_rx_data = on_ble_nus_rx_data;
   }

   IF_OK_RUN_AND_UPDATE(result, ble_control_init(&m_ble_evt_handlers));
   IF_OK_RUN_AND_UPDATE(result, imu_x_init(&m_imu, &m_i2c_driver_0.interface));

   // Check BLE bonded status
   IF_OK_RUN_AND_UPDATE(result, ble_control_get_bonded_status(&m_flag.ble_bonded));
   IF_OK_RUN_AND_UPDATE(result, tmd2635_driver_init(&m_prox, &m_prox_cfg, &m_i2c_driver_0.interface));
   IF_OK_RUN_AND_UPDATE(result, tilt_detection_init(&m_tilt_detect, &m_systick.interface, &m_imu.interface));
   IF_OK_RUN_AND_UPDATE(result, fds_manager_init(&m_fds_manager));

   uint16_t stored_threshold = 0u;
   uint16_t stored_hysteresis = 0u;

// #define TEST_HARDCODE_CAP_DETECTION_CONFIG
#ifdef TEST_HARDCODE_CAP_DETECTION_CONFIG
   stored_threshold = 7000u;
   stored_hysteresis = 1000u;
#else

   IF_OK_RUN_AND_UPDATE(result,
                        m_fds_manager.interface.retrieve_uint16_t(
                           &m_fds_manager.interface, RECORD_ID_CAP_DETECTION_THRESHOLD, &stored_threshold));
   IF_OK_RUN_AND_UPDATE(result,
                        m_fds_manager.interface.retrieve_uint16_t(
                           &m_fds_manager.interface, RECORD_ID_CAP_DETECTION_HYSTERESIS, &stored_hysteresis));
#endif

   // @todo add input validation for retrieved values and use defaults if invalid
   if(stored_threshold == 0u)
   {
      stored_threshold = DEFAULT_CAP_DETECTION_THRESHOLD;
      IF_OK_RUN_AND_UPDATE(result,
                           m_fds_manager.interface.store_uint16_t(
                              &m_fds_manager.interface, RECORD_ID_CAP_DETECTION_THRESHOLD, stored_threshold));
   }

   if((stored_hysteresis > stored_threshold) || (stored_hysteresis == 0u))
   {
      stored_hysteresis = DEFAULT_CAP_DETECTION_HYSTERESIS;
      IF_OK_RUN_AND_UPDATE(result,
                           m_fds_manager.interface.store_uint16_t(
                              &m_fds_manager.interface, RECORD_ID_CAP_DETECTION_HYSTERESIS, stored_hysteresis));
   }

   IF_OK_RUN_AND_UPDATE(result,
                        cap_detection_init(&m_cap_detect, &(m_prox.interface), stored_threshold, stored_hysteresis));
   IF_OK_RUN_AND_UPDATE(
      result, dose_detection_init(&m_dose_detect, &m_rtc.interface, &m_cap_detect.interface, &m_tilt_detect.interface));
   IF_OK_RUN_AND_UPDATE(result, st25dv_driver_init(&m_nfc, &m_i2c_driver_1.interface, NFC_GPO, &m_systick.interface));

   IF_OK_RUN_AND_UPDATE(
      result,
      nfc_tag_eeprom_queue_init(&m_dose_queue, sizeof(dose_event_t), DOSE_QUEUE_SLOTS, &m_nfc.interface, false));

   IF_OK_RUN_AND_UPDATE(result,
                        queue_init(&battery_queue,
                                   battery_queue_storage,
                                   BATTERY_QUEUE_DEPTH * sizeof(ring_battery_data_t),
                                   sizeof(ring_battery_data_t)));

   IF_OK_RUN_AND_UPDATE(
      result,
      queue_init(
         &error_queue, error_queue_storage, ERROR_QUEUE_DEPTH * sizeof(raw_debug_log_t), sizeof(raw_debug_log_t)));

   IF_OK_RUN_AND_UPDATE(result,
                        init_ring_data_manager(&m_data_manager,
                                               &m_rtc.interface,
                                               &m_systick.interface,
                                               &m_fds_manager.interface,
                                               &m_imu.interface,
                                               &m_battery.interface,
                                               &m_dose_detect.interface,
                                               &m_cap_detect.interface,
                                               &m_dose_queue.interface,
                                               &battery_queue.interface,
                                               &error_queue.interface));

   // Default polling the battery level once every 30 seconds.
   // The call to update the sample rate every boot is not a problem for write endurance because the FDS manager will
   // not overwrite the entry if the value is the same as the existing value.
   uint16_t battery_sample_rate_ms = 30u;
   m_data_manager.interface.update_battery_sample_rate(&m_data_manager.interface, battery_sample_rate_ms);

   IF_OK_RUN_AND_UPDATE(result,
                        ring_sources_init(&m_ring_sources,
                                          &m_data_manager.interface,
                                          &m_dose_queue.interface,
                                          &error_queue.interface,
                                          &battery_queue.interface));

   IF_OK_RUN_AND_UPDATE(
      result,
      message_protocol_init(
         &m_message_protocol, &m_systick.interface, &m_nfc.data_ifc, false, 50u, NULL, NULL, NULL, NULL));

   IF_OK_RUN_AND_UPDATE(
      result,
      dock_manager_init(
         &m_dock_manager, &m_message_protocol.interface, &m_data_manager.interface, &m_ring_sources.interface));

   DEBUG_INFO("general_control_init completed.");

   return result;
}
