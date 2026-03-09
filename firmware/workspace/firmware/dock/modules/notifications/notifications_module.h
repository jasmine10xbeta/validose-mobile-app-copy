/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file notifications_module.h
 * @ingroup modules/notifications
 * @brief
 */

#ifndef NOTIFICATIONS_MODULE_H_
#define NOTIFICATIONS_MODULE_H_

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
// Standard includes
#include "app_timer.h"
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "notifications_module_interface.h"
#include "queue.h"
#include "system_time.h"
#include "system_time_interface.h"

/**********************************************************************************************************************
 * Definitions
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Types
 *********************************************************************************************************************/
/**
 * @brief Definition of notification module errors.
 */
typedef enum
{
   NOTIFICATION_MODULE_ERROR_NONE = 0,              // No error
   NOTIFICATION_MODULE_ERROR_PTR_NULL,              // Null pointer error
   NOTIFICATION_MODULE_ERROR_INIT,                  // Trying to use interface functions of uninitialized module
   NOTIFICATION_MODULE_ERROR_SET_LED_BUZZER_CONFIG, // Cannot set led config or cannot set buzzer config
   NOTIFICATION_MODULE_ERROR_PRIORITY_QUEUE,        // Queue full OR other general queue-related error
   NOTIFICATION_MODULE_ERROR_PRIORITY_QUEUE_EMPTY,  // Notification event called, but nothing to execute (empty queue)
   NOTIFICATION_MODULE_ERROR_COLOR_TYPE_SET,        // Cannot set led type and color
   NOTIFICATION_MODULE_ERROR_TIMERS_START,          // Failed to start app timer
   NOTIFICATION_MODULE_ERROR_LED_DRV_INIT,          // Failed to initialize led driver
   NOTIFICATION_MODULE_ERROR_BUZZER_INIT,           // Failed to initialize buzzer driver
} NOTIFICATION_MODULE_ERROR;

typedef struct notification_module notification_module_t;

/**
 * @brief Timer struct: used to stop/ clear timed events
 * @param t0_ms The time that the event was activated/ executed
 * @param armed Boolean variable indicating if the timer was armed (started).
 * @param fired Boolean variable indicating if the timer has elapsed (fired) and the event has been cleared.
 */
typedef struct
{
   uint64_t t0_ms;
   bool armed;
   bool fired;
} timed_event_delay_t;

/**
 * @brief Notification module
 * @param interface The notification module interface instance
 * @param _systick_interface System time interface instance
 * @param _priority_queue The priority queue in the form of an array, corresponds to the NOTIFICATION_EVENT enum
 * @param _timed_event_timer Array of events' timing information, corresponds to the NOTIFICATION_EVENT enum
 * @param _queue_initialized State of the priority queue
 * @param _module_initialized State of the notification module
 */
struct notification_module
{
   notifications_module_interface_t interface;
   const system_time_interface_t *_systick_interface;
   notification_priority_queue_t _priority_queue[NUM_EVENTS];
   timed_event_delay_t _timed_event_timer[NUM_EVENTS];
   bool _queue_initialized;
   bool _module_initialized;
};

/**********************************************************************************************************************
 * Variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Global functions
 *********************************************************************************************************************/
/**
 * @brief To initialise the notifications module.
 * @param self The notifications module to initialise.
 * @param i2c_interface The i2c_interface to pass to the LED and buzzer driver
 * @param systick_interface The system time interface instance for timed events
 */

result_t notifications_module_init(notification_module_t *const self,
                                   const i2c_driver_interface_t *i2c_interface,
                                   const system_time_interface_t *systick_interface);
#endif // NOTIFICATIONS_MODULE_H_
