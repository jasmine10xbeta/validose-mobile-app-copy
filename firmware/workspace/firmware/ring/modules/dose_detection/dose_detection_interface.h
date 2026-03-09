/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef DOSE_DETECTION_INTERFACE_H
#define DOSE_DETECTION_INTERFACE_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"
#include "dose_detection_fsm.h" // For DOSE_DETECTION_STATE

#include <app_util.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define MAX_DOSE_EVENTS                       (4u)    /**< Maximum number of dose events to store in the queue */
#define DOSE_DETECTION_VALID_DOSE_DURATION_MS (1000u) /**< Duration in milliseconds to consider a dose event valid */

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct dose_detection; // Forward declaration

typedef enum
{
   DOSE_EVENT_STATE_IDLE = 0,             /**< Cap is closed */
   DOSE_EVENT_STATE_CAP_OPENED,           /**< Cap opened, no tilt yet */
   DOSE_EVENT_STATE_TILT_DETECTED,        /**< Tilt(s) detected, cap still open */
   DOSE_EVENT_STATE_CAP_CLOSED_NO_TILT,   /**< Cap closed, no tilt during open */
   DOSE_EVENT_STATE_CAP_CLOSED_WITH_TILT, /**< Cap closed after tilt(s) */

   DOSE_EVENT_STATE_MAX /**< Sentinel value */
} DOSE_EVENT_STATE;

STATIC_ASSERT(DOSE_EVENT_STATE_MAX <= UINT8_MAX, "DOSE_EVENT_STATE_MAX exceeds storage size of uint8_t");

/**
 * @brief Dose event structure
 */
typedef struct __attribute__((packed, aligned(1)))
{
   uint64_t dose_start_time_ms; /**< When cap was removed in milliseconds since boot (system time) */
   uint64_t dose_end_time_ms;   /**< When cap was replaced in milliseconds since boot (system time) */
   uint16_t tilt_count;         /**< Total tilts during this period */
   uint8_t state;               /**< State of the event (see DOSE_EVENT_STATE enum) */
} dose_detection_event_t;

/**
 * @brief Interface for the dose detection module
 */
typedef struct dose_detection_interface dose_detection_interface_t;
struct dose_detection_interface
{
   struct dose_detection *parent; /**< Pointer to the parent instance. */

   /**
    * @brief Run the dose detection process
    *
    * This function must be called periodically to process the dose detection logic.
    *
    * @param[in,out] interface Pointer to the interface instance
    *
    * @return result_t A status code indicating success or failure
    */
   result_t (*process)(dose_detection_interface_t *interface);

   /**
    * @brief Check if a dose event is available
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] is_available Pointer to a boolean that will be set to true if an event is available
    *
    * @return result_t A status code indicating success or failure
    */
   result_t (*is_dose_event_available)(dose_detection_interface_t *interface, bool *is_available);

   /**
    * @brief Attempts to retrieve a single dose event
    *
    * This function retrieves a single dose event if available and removes it from the internal queue. If no event is
    * available, it sets the event_retrieved flag to false.
    *
    * @param[in,out] interface Pointer to the interface instance
    * @param[out] event_out Pointer to the memory allocated to receive the event data
    * @param[out] event_retrieved Pointer to a boolean that will be set to true if an event was retrieved
    *
    * @return result_t A status code indicating success or failure
    */
   result_t (*try_get_dose_event)(dose_detection_interface_t *interface,
                                  dose_detection_event_t *event_out,
                                  bool *event_retrieved);

   /**
    * @brief Clears the dose events in the dose detection module
    *
    * @param[in,out] interface Pointer to the interface instance.
    * @return result_t A status code indicating success or failure.
    */
   result_t (*clear_dose_events)(dose_detection_interface_t *interface);

   /**
    * @brief Retrieves the current state of the dose detection module
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] state_out Pointer to a DOSE_DETECTION_STATE that will receive the current state
    *
    * @return result_t A status code indicating success or failure.
    */
   result_t (*get_current_state)(dose_detection_interface_t *interface, DOSE_DETECTION_STATE *state_out);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // DOSE_DETECTION_INTERFACE_H