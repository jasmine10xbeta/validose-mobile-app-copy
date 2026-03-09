/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**

 * @file notifications_module_interface.h (module for notifications HMI)
 * @ingroup modules/notifications
 * @brief
 */

#ifndef NOTIFICATIONS_MODULE_INTERFACE_H_
#define NOTIFICATIONS_MODULE_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "IS31FL3206_driver.h"
#include "buzzer.h"
#include "common.h"
#include "debug.h"
#include "led_driver_interface.h"
#include "result.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define NUM_EVENTS (NOTIFICATION_EVENT_MAX) // Corresponds to NOTIFICATION_EVENT enum
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * NOTIFICATION_EVENT enum encodes priorities into the order. i.e. index 0 is the highest priority and MAX -1  the
 * lowest.
 *
 * @note Error notifications automatically stop after a set number of blinks
 *
 */
typedef enum
{
   NOTIFICATION_EVENT_ERROR = 0u,                 // Generic error to show something went wrong.
   NOTIFICATION_EVENT_BLE_PAIRING,                // Bluetooth pairing is occurring
   NOTIFICATION_EVENT_DOSAGE_DOSE_DUE,            // Dose due
   NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW,        // Battery is low
   NOTIFICATION_EVENT_BAT_MANAGER_CHARGING,       // Battery is charging
   NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL, // Charger is connected & battery is fully charged
   NOTIFICATION_EVENT_MAX
} NOTIFICATION_EVENT;

/**
 * @brief Notification configuration
 * @param led_config LED configuration
 * @param num_buzzer_beeps Number of buzzer beeps per notification configuration
 * @param is_timed Is the notification timed, i.e. does the notification module stop the notification after a set
 * duration/ number of repetitions
 * @param duration_if_timed_ms Duration notification should be active for, if timed. Set in notifications_module.c
 */
typedef struct
{
   led_config_t led_config;
   uint8_t num_buzzer_beeps;
   bool is_timed;
   uint32_t duration_if_timed_ms;
} notification_config_t;

/**
 * @brief Priority queue
 * @param notification_config The notification configuration instance tied to the queue
 * @param is_waiting Flag to indicate that a notification event is in the queue and has not been executed
 * @param is_active Flag to indicate that a notification event is actively being executed
 */
typedef struct
{
   notification_config_t notification_config;
   bool is_waiting;
   bool is_active;
} notification_priority_queue_t;

struct notification_module;

typedef struct notifications_module_interface notifications_module_interface_t;

struct notifications_module_interface
{
   struct notification_module *parent;

   // declaration of all interface functions
   /**
    * @brief Set notification event (for LED and buzzer drivers)
    *
    * @param interface The notification module interface handles.
    * @param notification_event The next notification event to be handled or enqueued.
    *
    * @return Result of the operation.
    */
   result_t (*set_notification_event)(const notifications_module_interface_t *const interface,
                                      NOTIFICATION_EVENT notification_event);
   /**
    * @brief Clear notification event (for LED and buzzer drivers)
    *
    * If the event is currently active, it's deactivated. If it was queued, it is cleared from the priority queue.
    *
    * @param interface The notification module interface handles.
    * @param notification_event The next notification event to be cleared.
    *
    * @return Result of the operation.
    */
   result_t (*clear_notification_event)(const notifications_module_interface_t *const interface,
                                        NOTIFICATION_EVENT notification_event);

   /**
    * @brief Get active notification event
    *
    * @param interface The notification module interface handles.
    * @param notification_event The currently running (active) notification event
    *
    * @return Result of the operation.
    */
   result_t (*get_active_notification_event)(const notifications_module_interface_t *const interface,
                                             NOTIFICATION_EVENT *notification_event);

   /**
    * @brief Clear priority queue (an array of struct notification_priority_queue_t that is used to execute
    *        notification events according to their priority).
    *        This function resets the array to clear all is_active and is_waiting flags to essentially reset
    *        the priority queue so that there are no functions waiting to be executed and no active functions.
    *
    * @param interface The notification module interface handles.
    *
    * @return Result of the operation.
    */
   result_t (*clear_priority_queue)(const notifications_module_interface_t *const interface);

   /**
    * @brief Check priority queue for waiting events and execute next with highest priority.
    * Clear timed events if the event duration has expired.
    *
    * NOTE: This function must be called periodically by the general controller!
    *
    * @param interface The notification module interface handles.
    *
    * @return Result of the operation.
    */
   result_t (*process)(const notifications_module_interface_t *const interface);

   /**
    * @brief Check priority queue for waiting events and report number of waiting events
    * @param interface The notification module interface handles.
    * @param num_events_waiting_in_queue The number of events in the queue waiting to be executed
    */
   result_t (*get_waiting_event_count)(const notifications_module_interface_t *const interface,
                                       uint8_t *num_events_waiting_in_queue);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // NOTIFICATIONS_MODULE_INTERFACE_H_