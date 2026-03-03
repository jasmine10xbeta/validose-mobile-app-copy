/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file notifications_module.c
 * @ingroup modules/notifications
 * @brief
 */

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
// Standard includes
#include "app_timer.h"
#include "nrf_delay.h"

// Custom includes
#include "common.h"
#include "debug.h"
#include "i2c_driver.h"
#include "i2c_driver_interface.h"
#include "notifications.h"
#include "notifications_module.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_NOTIFICATIONS_MODULE;

/**********************************************************************************************************************
 * Definitions
 *********************************************************************************************************************/
// User configurable definitions
#define NUM_LED_DRIVERS      (2u)           // add new drivers here
#define NUM_STEPS            (uint8_t)(64u) // for fading gamma correction (more steps -> smoother fading)
#define BLINK_MULTIPLIER     (2u)           // Blink on interval + blink off interval
#define TIMED_EVT_NEG_MARGIN (200u) // To compensate for when the event is set vs. when the timer is actually armed
#define ERROR_TIMED_DURATION ((ERROR_BLINK_INTERVAL_MS * NUM_BLINKS_ERROR * BLINK_MULTIPLIER) - TIMED_EVT_NEG_MARGIN)

// I2C addresses of drivers - add new drivers here
#define DRV_I2C_ADDRESS_A (0x6C)
#define DRV_I2C_ADDRESS_B (0x6F)

// Buzzer ON/OFF counter resolution in msec.
#define BEEP_INTERVAL_MS (500u)
#define BUZZER_TICK      APP_TIMER_TICKS(BEEP_INTERVAL_MS)

/**********************************************************************************************************************
 * Types
 *********************************************************************************************************************/
typedef uint32_t m_ret_code_t;

/**********************************************************************************************************************
 * Static function declarations
 *********************************************************************************************************************/
// Interface functions
static result_t set_notification_event(const notifications_module_interface_t *const interface,
                                       NOTIFICATION_EVENT notification_event);
static result_t get_active_notification_event(const notifications_module_interface_t *const interface,
                                              NOTIFICATION_EVENT *notification_event);
static result_t clear_priority_queue(const notifications_module_interface_t *const interface);
static result_t process(const notifications_module_interface_t *const interface);
static result_t clear_notification_event(const notifications_module_interface_t *const interface,
                                         NOTIFICATION_EVENT notification_event);
static result_t get_waiting_event_count(const notifications_module_interface_t *const interface,
                                        uint8_t *num_events_waiting_in_queue);

// Internal (non-interface) functions

/**
 * @brief Start LED blinking sequence according to configuration
 *
 * @param led_config  LED configuration - set in notification.h.
 */
static result_t leds_blink(led_config_t led_config);

/**
 * @brief Start LED blinking sequence according to configuration
 *
 * Custom due to variable on/off durations and/or multiple pulses
 *
 * @param led_config  LED configuration - set in notification.h.
 */
static result_t leds_custom_blink(led_config_t led_config);
/**
 * @brief Start LED fading sequence according to configuration
 *
 * Performs a gamma-corrected fade effect - fading up (brighter) and down (dimmer)
 *
 * @param[in] led_config  LED configuration - set in notification.h.
 */
static result_t leds_fade(led_config_t led_config);
/**
 * @brief Turn the target LED(s) on for a solid sequence (i.e. not blinking or fading).
 *
 * @param led_config  LED configuration - set in notification.h.
 */
static result_t leds_on(led_config_t led_config);
/**
 * @brief Turn off all LEDs
 */
static result_t leds_off(void);
/**
 * @brief Enqueue a notification event.
 *
 * Sets the notification's is_waiting flag in the priority queue.
 *
 * @param notification_module_instance  Pointer to notification module instance.
 * @param notification_event            Event to enqueue.
 */
static result_t enqueue_notification(notification_module_t *notification_module_instance,
                                     NOTIFICATION_EVENT notification_event);
/**
 * @brief Dequeue the waiting notification event with the highest priority.
 *
 * Sets the notification's is_active flag in the priority queue and removes the is_waiting flag.
 * Calls do_notification.
 *
 * @param notification_module_instance  Pointer to notification module instance.
 */
static result_t dequeue_notification(notification_module_t *notification_module_instance);
/**
 * @brief Execute (service) the dequeued notification event.
 *
 * @param notification_module_instance  Pointer to notification module instance.
 * @param     priority_index            Index in priotity queue of dequeued notification.
 */
static result_t do_notification(notification_module_t *notification_module_instance, uint8_t priority_index);
/**
 * @brief Clear an active, timed notification event after the allotted event duration has expired.
 *
 * @param self          Pointer to notification module instance.
 * @param active_event  Event to clear when the timeout elapses.
 */
static result_t clear_event_after_ms(notification_module_t *const self, NOTIFICATION_EVENT active_event);
/**
 * @brief Arm the timer used for timed notifications.
 *
 * Gets the current time and sets t0_ms, sets the is_armed flag.
 *
 * @param self   Pointer to notification module instance.
 * @param index  Index timed event's timer to arm - index is of timed event queue (corresponds to the priority queue).
 */
static void timed_event_timer_arm(notification_module_t *const self, uint8_t index);
/**
 * @brief Handles buzzer sequence.
 */
static void app_timer_handler_buzzer(void *p_context)
   /**********************************************************************************************************************
    * Variables
    *********************************************************************************************************************/
   APP_TIMER_DEF(m_buzzer_timer); // Timer for buzzer sequence

static buzzer_interface_t *m_buzzer_ifc = NULL;
static buzzer_t m_buzzer = {0};
static volatile uint8_t m_buzzer_num_beeps_left = 0;
static volatile bool m_is_buzzer_beeps_completed = true;
static bool m_is_buzzer_enabled = false;

static led_config_t m_led_config = {0};

// To initialize/ reset LED blinking and fading routines
static const bool m_fade_up = true;
static const uint8_t m_gamma_step_start = 0;
static const bool m_led_on_init = false;

// LED drivers in the project - add new drivers here
static const is31fl3206_driver_t m_led_driver_one = {0}; // add new drivers here
static is31fl3206_driver_t m_led_drivers[NUM_LED_DRIVERS] = {m_led_driver_one};
// I2C addresses of all drivers - add new drivers here
static uint8_t m_led_drivers_i2c_address_lookup_table[NUM_LED_DRIVERS] = {DRV_I2C_ADDRESS_A, DRV_I2C_ADDRESS_B};

// LED Timer Instances
// Initialization/ reset data sent to timers internal to the led driver
static led_timer_t m_led_timer_blinking = {
   .led_timer_context.led_on = m_led_on_init,
};

static led_timer_t m_led_timer_blinking_custom = {
   .led_timer_context.led_on = m_led_on_init,
};

static led_timer_t m_led_timer_blinking_custom_off = {
   .led_timer_context.led_on = m_led_on_init,
};

static led_timer_t m_led_timer_fading = {
   .led_timer_context.gamma_step = m_gamma_step_start,
   .led_timer_context.led_on = m_led_on_init,
   .led_timer_context.fade_up = m_fade_up,
};

static notification_config_t m_current_notification_config = {0};
// Notification configurations - user configureable
// -----------------------------------------------
// Error occurred
static const notification_config_t m_notif_config_error // errors include invalid dose schedule!
   = {.led_config = m_led_config_error,
      .num_buzzer_beeps = NUM_BEEPS_ERROR,
      .is_timed = true,
      .duration_if_timed_ms = ERROR_TIMED_DURATION};

// Bluetooth pairing is occurring
static const notification_config_t m_notif_config_ble_pairing
   = {.led_config = m_led_config_ble_pairing, .num_buzzer_beeps = 0, .is_timed = false, .duration_if_timed_ms = 0};

// Battery is charging
static const notification_config_t m_notif_config_battery_charging
   = {.led_config = m_led_config_battery_charging, .num_buzzer_beeps = 0, .is_timed = false, .duration_if_timed_ms = 0};

// Charger is connected and battery is fully charged
static const notification_config_t m_notif_config_battery_full
   = {.led_config = m_led_config_battery_full, .num_buzzer_beeps = 0, .is_timed = false, .duration_if_timed_ms = 0};

// Battery is low
static const notification_config_t m_notif_config_battery_low = {.led_config = m_led_config_battery_low,
                                                                 .num_buzzer_beeps = NUM_BEEPS_BATTERY_LOW,
                                                                 .is_timed = false,
                                                                 .duration_if_timed_ms = 0};
// Dose due
static const notification_config_t m_notif_config_dose_due = {.led_config = m_led_config_dose_due,
                                                              .num_buzzer_beeps = NUM_BEEPS_DOSE_DUE,
                                                              .is_timed = false,
                                                              .duration_if_timed_ms = 0};
// -----------------------------------------------
/**********************************************************************************************************************
 * Static non-interface function definitions
 *********************************************************************************************************************/
static result_t enqueue_notification(notification_module_t *notification_module_instance,
                                     NOTIFICATION_EVENT notification_event)
{
   RETURN_ERR_IF_NULL(notification_module_instance, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(NOTIFICATION_EVENT_MAX <= notification_event, NOTIFICATION_MODULE_ERROR_PRIORITY_QUEUE);
   notification_module_t *self = notification_module_instance;
   RETURN_OK_IF_TRUE(self->_priority_queue[notification_event].is_waiting);

   result_t result = RESULT_OK;

   self->_priority_queue[notification_event].is_waiting = true;

   switch(notification_event)
   {
      case NOTIFICATION_EVENT_BAT_MANAGER_BAT_LOW:
         m_current_notification_config = m_notif_config_battery_low;
         break;
      case NOTIFICATION_EVENT_BAT_MANAGER_CHARGING:
         m_current_notification_config = m_notif_config_battery_charging;
         break;
      case NOTIFICATION_EVENT_BAT_MANAGER_CONNECTED_FULL:
         m_current_notification_config = m_notif_config_battery_full;
         break;
      case NOTIFICATION_EVENT_BLE_PAIRING:
         m_current_notification_config = m_notif_config_ble_pairing;
         break;
      case NOTIFICATION_EVENT_DOSAGE_DOSE_DUE:
         m_current_notification_config = m_notif_config_dose_due;
         break;
      case NOTIFICATION_EVENT_ERROR:
         m_current_notification_config = m_notif_config_error;
         break;

      case NOTIFICATION_EVENT_MAX:
      default:
         result = leds_off();
         m_buzzer_ifc->set_off(m_buzzer_ifc);
         m_is_buzzer_enabled = false;

         RETURN_ON_ERR(result);
         break;
   }

   self->_priority_queue[notification_event].notification_config = m_current_notification_config;

   return result;
}

static result_t dequeue_notification(notification_module_t *self)
{
   RETURN_ERR_IF_NULL(self, NOTIFICATION_MODULE_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   for(uint8_t priority_index = 0u; priority_index < NOTIFICATION_EVENT_MAX; priority_index++)
   {
      if(self->_priority_queue[priority_index].is_waiting)
      {
         // Waiting event with highest priority found
         result = do_notification(self, priority_index);
         ON_ERR_DEBUG_ERROR(result, "set probe LEDs\n");

         if(IS_OK(result))
         {
            self->_priority_queue[priority_index].is_active = true;
            self->_priority_queue[priority_index].is_waiting = false;
         }

         break; // When the highest priority is found, execute event and break
      }
   }

   return result;
}

static result_t do_notification(notification_module_t *self, uint8_t index)
{
   RETURN_ERR_IF_NULL(self, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(index >= NOTIFICATION_EVENT_MAX, NOTIFICATION_MODULE_ERROR_PRIORITY_QUEUE);

   notification_config_t notif_config = self->_priority_queue[index].notification_config;
   m_led_config = notif_config.led_config;

   result_t result = RESULT_OK;

   switch(m_led_config.type)
   {
      case LED_TYPE_SOLID_ON:
         result = leds_on(m_led_config);
         break;
      case LED_TYPE_BLINK:
         result = leds_blink(m_led_config);
         break;
      case LED_TYPE_CUSTOM_BLINK:
         result = leds_custom_blink(m_led_config);
         break;
      case LED_TYPE_FADE:
         result = leds_fade(m_led_config);
         break;
      case LED_TYPE_MAX:
         result = leds_off();
         break;
      default:
         result = leds_off();
   }

   if(IS_OK(result) && (notif_config.is_timed))
   {
      // Arm timer for timed notification event
      timed_event_timer_arm(self, index);
   }

   if(IS_OK(result) && (notif_config.num_buzzer_beeps > 0))
   {
      m_buzzer_num_beeps_left
         = (uint8_t)(notif_config.num_buzzer_beeps - 1u); // -1 since the first beep is started here

      if(!m_is_buzzer_beeps_completed)
      {
         // Previous beeps not completed. Stop timer first before starting new set of beeps. This keeps the beep time
         // consistent.
         m_ret_code_t err_code = app_timer_stop(m_buzzer_timer);
         UPDATE_IF_NRF_ERR(err_code, result, NOTIFICATION_MODULE_ERROR_BUZZER_INIT);
      }

      if(IS_OK(result))
      {
         m_ret_code_t err_code = app_timer_start(m_buzzer_timer, APP_TIMER_TICKS(BEEP_INTERVAL_MS), NULL);
         m_is_buzzer_beeps_completed = false;
         UPDATE_IF_NRF_ERR(err_code, result, NOTIFICATION_MODULE_ERROR_BUZZER_INIT);
      }

      if(IS_OK(result))
      {
         result = m_buzzer_ifc->set_on(m_buzzer_ifc);
         m_is_buzzer_enabled = true;
      }
   }

   return result;
}

static void app_timer_handler_buzzer(void *p_context) // NOSONAR - p_context required by SDK
{
   UNUSED_PARAMETER(p_context);

   if(m_is_buzzer_enabled)
   {
      m_buzzer_ifc->set_off(m_buzzer_ifc);

      if(m_buzzer_num_beeps_left > 0)
      {
         m_buzzer_num_beeps_left--;
      }
      else
      {
         (void)app_timer_stop(m_buzzer_timer);
         m_is_buzzer_beeps_completed = true;
      }

      m_is_buzzer_enabled = false;
   }
   else
   {
      m_buzzer_ifc->set_on(m_buzzer_ifc);
      m_is_buzzer_enabled = true;
   }
}

static result_t leds_blink(led_config_t led_config)
{
   result_t result = RESULT_OK;

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      result = m_led_drivers[intf].interface.set_led_color_type(&m_led_drivers[intf].interface, &led_config);
      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_COLOR_TYPE_SET);
      BREAK_ON_ERR(result);
   }

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      IF_OK_RUN_AND_UPDATE(
         result, m_led_drivers[intf].interface.led_blink_timers_start(&m_led_drivers[intf].interface, &led_config));

      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_TIMERS_START);
      BREAK_ON_ERR(result);
   }

   return result;
}

static result_t leds_custom_blink(led_config_t led_config)
{
   result_t result = RESULT_OK;

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      result = m_led_drivers[intf].interface.set_led_color_type(&m_led_drivers[intf].interface, &led_config);
      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_COLOR_TYPE_SET);
      BREAK_ON_ERR(result);
   }

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      IF_OK_RUN_AND_UPDATE(
         result,
         m_led_drivers[intf].interface.led_custom_blink_timers_start(&m_led_drivers[intf].interface, &led_config));

      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_TIMERS_START);
      BREAK_ON_ERR(result);
   }

   return RESULT_OK;
}

static result_t leds_fade(led_config_t led_config)
{
   result_t result = RESULT_OK;

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      result = m_led_drivers[intf].interface.generate_gamma_table(&m_led_drivers[intf].interface, &led_config);
      BREAK_ON_ERR(result);
   }

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      IF_OK_RUN_AND_UPDATE(
         result, m_led_drivers[intf].interface.set_led_color_type(&m_led_drivers[intf].interface, &led_config));
      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_COLOR_TYPE_SET);
      BREAK_ON_ERR(result);
   }

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      IF_OK_RUN_AND_UPDATE(
         result, m_led_drivers[intf].interface.led_fade_timers_start(&m_led_drivers[intf].interface, &led_config));
      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_TIMERS_START);
      BREAK_ON_ERR(result);
   }

   return result;
}

static result_t leds_on(led_config_t led_config)
{
   result_t result = RESULT_OK;
   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      result = m_led_drivers[intf].interface.set_led_color_type(&m_led_drivers[intf].interface, &led_config);
      UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_COLOR_TYPE_SET);
      BREAK_ON_ERR(result);
   }

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      IF_OK_RUN_AND_UPDATE(result, m_led_drivers[intf].interface.set_led_update(&m_led_drivers[intf].interface));
      BREAK_ON_ERR(result);
   }

   return result;
}

static result_t leds_off(void)
{
   result_t result = RESULT_OK;

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      result = m_led_drivers[intf].interface.set_off_all(&m_led_drivers[intf].interface);
      BREAK_ON_ERR(result);
   }

   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      IF_OK_RUN_AND_UPDATE(result, m_led_drivers[intf].interface.set_led_update(&m_led_drivers[intf].interface));
      BREAK_ON_ERR(result);
   }

   return result;
}

static void timed_event_timer_arm(notification_module_t *const self, uint8_t index)
{
   RETURN_VOID_IF_NULL(self);
   if(index >= NOTIFICATION_EVENT_MAX) // guard clause
   {
      return;
   }

   const system_time_interface_t *systick_interface = self->_systick_interface;

   uint64_t time_now = 0u;
   systick_interface->get_time_ms(systick_interface, &time_now);

   self->_timed_event_timer[index].armed = true;
   self->_timed_event_timer[index].fired = false;
   self->_timed_event_timer[index].t0_ms = time_now;
}

static result_t clear_event_after_ms(notification_module_t *const self, NOTIFICATION_EVENT active_event)
{
   RETURN_ERR_IF_NULL(self, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   // Not an error, but no need to execute this function in this case
   RETURN_OK_IF_TRUE(NOTIFICATION_EVENT_MAX == active_event);
   // Not a timed event
   RETURN_OK_IF_TRUE(!self->_priority_queue[active_event].notification_config.is_timed);
   // Timer not armed OR timer already fired
   RETURN_OK_IF_TRUE((!self->_timed_event_timer[active_event].armed) || (self->_timed_event_timer[active_event].fired));

   result_t result = RESULT_OK;

   notification_config_t config = self->_priority_queue[active_event].notification_config;
   const system_time_interface_t *systick_interface = self->_systick_interface;
   uint64_t time_now = 0u;
   uint64_t time_started = self->_timed_event_timer[active_event].t0_ms;
   uint64_t delta = 0u;

   systick_interface->get_time_ms(systick_interface, &time_now);

   // Handle case where time_started > time_now (e.g., timer wraparound or clock reset)
   if(time_started > time_now)
   {
      // Defensive: clear the event and reset timer
      result = self->interface.clear_notification_event(&(self->interface), active_event);
      ON_ERR_DEBUG_ERROR(result, "clear timed event (timer wraparound)\n");

      if(IS_OK(result))
      {
         self->_timed_event_timer[active_event].armed = false;
         self->_timed_event_timer[active_event].fired = true;
         self->_timed_event_timer[active_event].t0_ms = 0u;
      }
   }
   else
   {
      delta = time_now - time_started;

      if(delta >= config.duration_if_timed_ms)
      {
         // error-timer elapsed
         result = self->interface.clear_notification_event(&(self->interface), active_event);
         ON_ERR_DEBUG_ERROR(result, "clear timed event\n");

         if(IS_OK(result))
         {
            self->_timed_event_timer[active_event].armed = false;
            self->_timed_event_timer[active_event].fired = true;
            self->_timed_event_timer[active_event].t0_ms = 0u;
         }
      }
   }

   return result;
}

/**********************************************************************************************************************
 * Static interface function definitions
 *********************************************************************************************************************/
static result_t clear_notification_event(const notifications_module_interface_t *const interface,
                                         NOTIFICATION_EVENT notification_event)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   notification_module_t *self = interface->parent;
   RETURN_ERR_IF_TRUE((false == self->_module_initialized) || (false == self->_queue_initialized),
                      NOTIFICATION_MODULE_ERROR_INIT);
   RETURN_OK_IF_TRUE((!self->_priority_queue[notification_event].is_waiting)
                     && (!self->_priority_queue[notification_event].is_active));

   result_t result = RESULT_OK;

   self->_priority_queue[notification_event].is_waiting = false;

   if(self->_priority_queue[notification_event].is_active)
   {
      result = leds_off();

      if(IS_OK(result))
      {
         self->_priority_queue[notification_event].is_active = false;
      }
   }

   return result;
}

static result_t set_notification_event(const notifications_module_interface_t *const interface,
                                       NOTIFICATION_EVENT notification_event)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   notification_module_t *self = interface->parent;
   RETURN_ERR_IF_TRUE((false == self->_module_initialized) || (false == self->_queue_initialized),
                      NOTIFICATION_MODULE_ERROR_INIT);
   RETURN_ERR_IF_TRUE(NOTIFICATION_EVENT_MAX <= notification_event, NOTIFICATION_MODULE_ERROR_PRIORITY_QUEUE);
   RETURN_OK_IF_TRUE(self->_priority_queue[notification_event].is_waiting);
   RETURN_OK_IF_TRUE(self->_priority_queue[notification_event].is_active);

   NOTIFICATION_EVENT active_event;

   result_t result = interface->get_active_notification_event(interface, &active_event);
   ON_ERR_DEBUG_ERROR(result, "get active event\n");

   if(active_event < notification_event)
   {
      // Not the same event and has lower priority than active event
      IF_OK_RUN_AND_UPDATE(result, enqueue_notification(self, notification_event));
      ON_ERR_DEBUG_ERROR(result, "enqueue event\n");
   }
   else if(active_event > notification_event)
   {
      // Not the same event and has higher priority than active event
      // OR: No active events (LED_EVENT_MAX)
      if((active_event != NOTIFICATION_EVENT_MAX) && IS_OK(result))
      {
         self->_priority_queue[active_event].is_active = false;
         // will be executed again in periodic process function
         self->_priority_queue[active_event].is_waiting = true;
      }

      IF_OK_RUN_AND_UPDATE(result, enqueue_notification(self, notification_event));
      ON_ERR_DEBUG_ERROR(result, "enqueue event\n");

      IF_OK_RUN_AND_UPDATE(result, leds_off());
      ON_ERR_DEBUG_ERROR(result, "disable LEDs\n");

      IF_OK_RUN_AND_UPDATE(result, dequeue_notification(self));
      ON_ERR_DEBUG_ERROR(result, "dequeue event\n");
   }

   return result;
}

static result_t get_active_notification_event(const notifications_module_interface_t *const interface,
                                              NOTIFICATION_EVENT *notification_event)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(notification_event, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   notification_module_t *self = interface->parent;
   RETURN_ERR_IF_TRUE((false == self->_module_initialized) || (false == self->_queue_initialized),
                      NOTIFICATION_MODULE_ERROR_INIT);

   result_t result = RESULT_OK;
   *notification_event = NOTIFICATION_EVENT_MAX;

   for(uint8_t priority_index = 0; priority_index < NOTIFICATION_EVENT_MAX; priority_index++)
   {
      // there will always be MAX one event currently active (one config running)
      if(self->_priority_queue[priority_index].is_active)
      {
         *notification_event = priority_index;
         break;
      }
   }

   return result;
}

static result_t clear_priority_queue(const notifications_module_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   notification_module_t *self = interface->parent;
   RETURN_ERR_IF_TRUE((false == self->_module_initialized) || (false == self->_queue_initialized),
                      NOTIFICATION_MODULE_ERROR_INIT);

   result_t result = RESULT_OK;

   memset(self->_priority_queue, 0, sizeof(self->_priority_queue));

   return result;
}

static result_t get_waiting_event_count(const notifications_module_interface_t *const interface,
                                        uint8_t *num_events_waiting_in_queue)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   notification_module_t *self = interface->parent;
   RETURN_ERR_IF_TRUE((false == self->_module_initialized) || (false == self->_queue_initialized),
                      NOTIFICATION_MODULE_ERROR_INIT);
   RETURN_ERR_IF_NULL(num_events_waiting_in_queue, NOTIFICATION_MODULE_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   uint8_t num_waiting = 0u;

   for(uint8_t priority_index = 0; priority_index < NOTIFICATION_EVENT_MAX; priority_index++)
   {
      if(self->_priority_queue[priority_index].is_waiting)
      {
         num_waiting++;
      }
   }

   *num_events_waiting_in_queue = num_waiting;

   return result;
}

static result_t process(const notifications_module_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   notification_module_t *self = interface->parent;
   RETURN_ERR_IF_TRUE((false == self->_module_initialized) || (false == self->_queue_initialized),
                      NOTIFICATION_MODULE_ERROR_INIT);

   uint8_t num_events_waiting = 0u;
   NOTIFICATION_EVENT active_event = NOTIFICATION_EVENT_MAX;

   result_t result = interface->get_active_notification_event(interface, &active_event);
   ON_ERR_DEBUG_ERROR(result, "get active event\n");

   // If set, check if the timed event timer has elapsed and if so, clear event.
   IF_OK_RUN_AND_UPDATE(result, clear_event_after_ms(self, active_event));
   ON_ERR_DEBUG_ERROR(result, "clear timed event\n");

   IF_OK_RUN_AND_UPDATE(result, get_waiting_event_count(interface, &num_events_waiting));
   ON_ERR_DEBUG_ERROR(result, "get waiting event count\n");

   // If there are events in the queue
   if((num_events_waiting > 0u) && IS_OK(result))
   {
      // If there are no active events
      if(NOTIFICATION_EVENT_MAX == active_event)
      {
         result = dequeue_notification(self);
         ON_ERR_DEBUG_ERROR(result, "dequeue waiting event\n");
      }
   }

   return result;
}

/**********************************************************************************************************************
 * Global function definitions
 *********************************************************************************************************************/
result_t notifications_module_init(notification_module_t *const self,
                                   const i2c_driver_interface_t *i2c_interface,
                                   const system_time_interface_t *systick_interface)
{
   RETURN_ERR_IF_NULL(self, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(i2c_interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(systick_interface, NOTIFICATION_MODULE_ERROR_PTR_NULL);
   result_t result = RESULT_OK;

   self->_module_initialized = false;
   self->_queue_initialized = false;
   self->interface.parent = self;
   self->interface.set_notification_event = set_notification_event;
   self->interface.get_active_notification_event = get_active_notification_event;
   self->interface.clear_priority_queue = clear_priority_queue;
   self->interface.clear_notification_event = clear_notification_event;
   self->interface.process = process;
   self->interface.get_waiting_event_count = get_waiting_event_count;

   self->_systick_interface = systick_interface;

   memset(self->_priority_queue, 0, sizeof(self->_priority_queue));

   /* Initialize LED Driver
    Set LED initial config and update LED driver registers */
   for(uint8_t intf = 0; intf < NUM_LED_DRIVERS; intf++)
   {
      uint8_t i2c_address = m_led_drivers_i2c_address_lookup_table[intf];
      result = led_control_init(&m_led_drivers[intf],
                                i2c_interface,
                                m_led_timer_blinking,
                                m_led_timer_blinking_custom,
                                m_led_timer_blinking_custom_off,
                                m_led_timer_fading,
                                NUM_STEPS,
                                i2c_address);

      IF_OK_RUN_AND_UPDATE(result, m_led_drivers[intf].interface.set_led_driver_config(&m_led_drivers[intf].interface));
      BREAK_ON_ERR(result);
   }

   UPDATE_ERR(result, NOTIFICATION_MODULE_ERROR_LED_DRV_INIT);

   IF_OK_RUN_AND_UPDATE(result, leds_off());

   if(IS_OK(result))
   {
      // Init timer
      ret_code_t err_code = app_timer_create(&m_buzzer_timer, APP_TIMER_MODE_REPEATED, app_timer_handler_buzzer);
      UPDATE_IF_NRF_ERR(err_code, result, NOTIFICATION_MODULE_ERROR_BUZZER_INIT);
   }

   // Initialize buzzer driver
   IF_OK_RUN_AND_UPDATE(result, buzzer_init(&m_buzzer));

   if(IS_OK(result))
   {
      m_buzzer_ifc = &(m_buzzer.interface);
      m_buzzer_ifc->set_off(m_buzzer_ifc);
      m_is_buzzer_enabled = false;
   }

   if(IS_OK(result))
   {
      self->_queue_initialized = true;
      self->_module_initialized = true;
   }

   return result;
}
