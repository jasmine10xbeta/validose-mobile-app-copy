/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file notifications.h
 * @ingroup modules/notifications
 * @brief
 */

#ifndef NOTIFICATIONS_H_
#define NOTIFICATIONS_H_

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
// Custom includes
#include "notifications_module_interface.h"
/**********************************************************************************************************************
 * Definitions
 *********************************************************************************************************************/
#define ERROR_BLINK_INTERVAL_MS        (500u)
#define BLE_PAIRING_BLINK_INTERVAL_MS  (700u) // msec between on/ off blink
#define BAT_CHARGING_BLINK_INTERVAL_MS (700u)

#define CFG_ERROR_BLINK_INTERVAL_MS        APP_TIMER_TICKS(ERROR_BLINK_INTERVAL_MS)
#define CFG_BLE_PAIRING_BLINK_INTERVAL_MS  APP_TIMER_TICKS(BLE_PAIRING_BLINK_INTERVAL_MS)
#define CFG_BAT_CHARGING_BLINK_INTERVAL_MS APP_TIMER_TICKS(BAT_CHARGING_BLINK_INTERVAL_MS)

// Special Configuration - BATTERY LOW
#define BAT_LOW_BLINK_INTERVAL_MS        (2000u)
#define BAT_LOW_BLINK_ON_DURATION_MS     (10u)
#define CFG_BAT_LOW_BLINK_ON_DURATION_MS APP_TIMER_TICKS(BAT_LOW_BLINK_ON_DURATION_MS)
#define CFG_BAT_LOW_BLINK_INTERVAL_MS    APP_TIMER_TICKS(BAT_LOW_BLINK_INTERVAL_MS)
// Special Configuration - DOSE DUE
#define DOSE_DUE_BLINK_PULSE_INTERVAL_MS  (500u)
#define DOSE_DUE_BLINK_ON_DURATION_MS     (20u)
#define CFG_DOSE_DUE_BLINK_ON_DURATION_MS APP_TIMER_TICKS(DOSE_DUE_BLINK_ON_DURATION_MS)
#define DOSE_DUE_BLINK_INTERVAL_S         (10u)
#define CFG_DOSE_DUE_BLINKS_INTERVAL_MS   APP_TIMER_TICKS(DOSE_DUE_BLINK_PULSE_INTERVAL_MS)
#define DOSE_DUE_BLINK_COUNT              (2u)

#define MAX_MELODY_LENGTH_NUM_BEEPS (6u)

#define NUM_BEEPS_ERROR       (3u)
#define NUM_BEEPS_BATTERY_LOW (3u)
#define NUM_BEEPS_DOSE_DUE    (6u)
#define NUM_BLINKS_ERROR      (3u)

/**********************************************************************************************************************
 * Variables
 *********************************************************************************************************************/
// LED configurations
const led_config_t m_led_config_error = {.color = LED_COLOR_RED,
                                         .type = LED_TYPE_BLINK,
                                         .blink_interval_ms = CFG_ERROR_BLINK_INTERVAL_MS,
                                         .blink_pure_ms = ERROR_BLINK_INTERVAL_MS,
                                         .custom_pulse_count = 0,
                                         .custom_blink_on_duration_ms = 0,
                                         .pulse = false,
                                         .pulse_repeat_interval_ms = 0,
                                         .fade_interval_ms = 0,
                                         .brightness_pwm_r = 0xFF,
                                         .brightness_pwm_g = 0x00,
                                         .brightness_pwm_b = 0x00};
// Bluetooth pairing is occurring
// If not paired after count blinks, call this config again
const led_config_t m_led_config_ble_pairing = {.color = LED_COLOR_BLUE,
                                               .type = LED_TYPE_BLINK,
                                               .blink_interval_ms = CFG_BLE_PAIRING_BLINK_INTERVAL_MS,
                                               .blink_pure_ms = BLE_PAIRING_BLINK_INTERVAL_MS,
                                               .custom_pulse_count = 0,
                                               .custom_blink_on_duration_ms = 0,
                                               .pulse = false,
                                               .pulse_repeat_interval_ms = 0,
                                               .fade_interval_ms = 0,
                                               .brightness_pwm_r = 0x00,
                                               .brightness_pwm_g = 0x00,
                                               .brightness_pwm_b = 0xCC};
// Battery is charging
const led_config_t m_led_config_battery_charging = {.color = LED_COLOR_GREEN,
                                                    .type = LED_TYPE_BLINK,
                                                    .blink_interval_ms = CFG_BAT_CHARGING_BLINK_INTERVAL_MS,
                                                    .blink_pure_ms = BAT_CHARGING_BLINK_INTERVAL_MS,
                                                    .custom_pulse_count = 0,
                                                    .custom_blink_on_duration_ms = 0,
                                                    .pulse = false,
                                                    .pulse_repeat_interval_ms = 0,
                                                    .fade_interval_ms = 0,
                                                    .brightness_pwm_r = 0x00,
                                                    .brightness_pwm_g = 0x80,
                                                    .brightness_pwm_b = 0x00};
// Charger is connected and battery is fully charged
const led_config_t m_led_config_battery_full = {.color = LED_COLOR_GREEN,
                                                .type = LED_TYPE_SOLID_ON,
                                                .blink_interval_ms = 0,
                                                .blink_pure_ms = 0,
                                                .custom_pulse_count = 0,
                                                .custom_blink_on_duration_ms = 0,
                                                .pulse = false,
                                                .pulse_repeat_interval_ms = 0,
                                                .fade_interval_ms = 0,
                                                .brightness_pwm_r = 0x00,
                                                .brightness_pwm_g = 0x80,
                                                .brightness_pwm_b = 0x00};
// Battery is low
const led_config_t m_led_config_battery_low = {.color = LED_COLOR_AMBER,
                                               .type = LED_TYPE_CUSTOM_BLINK,
                                               .blink_interval_ms = CFG_BAT_LOW_BLINK_INTERVAL_MS,
                                               .blink_pure_ms = BAT_LOW_BLINK_INTERVAL_MS,
                                               .custom_pulse_count = 0,
                                               .custom_blink_on_duration_ms = CFG_BAT_LOW_BLINK_ON_DURATION_MS,
                                               .pulse = false,
                                               .pulse_repeat_interval_ms = 0,
                                               .fade_interval_ms = 0,
                                               .brightness_pwm_r = 0xFF,
                                               .brightness_pwm_g = 0x80,
                                               .brightness_pwm_b = 0x00};
// Dose due
const led_config_t m_led_config_dose_due = {.color = LED_COLOR_TURQUOISE,
                                            .type = LED_TYPE_CUSTOM_BLINK,
                                            .blink_interval_ms = CFG_DOSE_DUE_BLINKS_INTERVAL_MS,
                                            .blink_pure_ms = DOSE_DUE_BLINK_PULSE_INTERVAL_MS,
                                            .custom_pulse_count = DOSE_DUE_BLINK_COUNT,
                                            .custom_blink_on_duration_ms = CFG_DOSE_DUE_BLINK_ON_DURATION_MS,
                                            .pulse = true,
                                            .pulse_repeat_interval_ms = DOSE_DUE_BLINK_INTERVAL_S,
                                            .fade_interval_ms = 0,
                                            .brightness_pwm_r = 0x40,
                                            .brightness_pwm_g = 0xE0,
                                            .brightness_pwm_b = 0xD0};

#endif // NOTIFICATIONS_H_