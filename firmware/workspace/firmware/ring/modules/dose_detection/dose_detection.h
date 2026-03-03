/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef DOSE_DETECTION_H
#define DOSE_DETECTION_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "cap_detection_interface.h"
#include "dose_detection_interface.h"
#include "queue.h"
#include "statemachine.h"
#include "system_time_interface.h"
#include "tilt_detection_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Size of the dose event queue buffer */
#define DOSE_EVENT_QUEUE_BUFFER_SIZE (sizeof(dose_detection_event_t) * MAX_DOSE_EVENTS)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions
 */
typedef enum
{
   DOSE_DETECTION_ERROR_NONE = 0,
   DOSE_DETECTION_ERROR_NULL_PTR,        /**< Unexpected null reference */
   DOSE_DETECTION_ERROR_NOT_INITIALIZED, /**< Module not initialized */
   DOSE_DETECTION_ERROR_ERROR_MAX,
} DOSE_DETECTION_ERROR;

/**
 * @brief Structure for the dose detection module
 */
typedef struct dose_detection
{
   // Interface
   dose_detection_interface_t interface;

   // Dependencies
   system_time_interface_t *_system_time_interface; /**< Pointer to the system time interface */
   cap_detection_interface_t *_cap_interface;       /**< Pointer to the cap detection interface */
   tilt_detection_interface_t *_tilt_interface;     /**< Pointer to the tilt detection interface */

   // Private data
   statemachine_t _fsm; /**< Dose detection state machine */

   uint64_t _dose_start_time; /**< Timestamp when dose event started. Used as input for the state machine. Must be set
                                 after transitioning to PRIMED state. */
   uint16_t _tilt_count;      /**< Number of tilts detected during current dose event */

   uint8_t _dose_event_queue_buffer[DOSE_EVENT_QUEUE_BUFFER_SIZE];
   queue_t _dose_event_queue;

   bool _is_initialized; /**< Whether this instance is initialized */
} dose_detection_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes the dose detection module
 *
 * @param[in, out] p_self Pointer to the dose detection instance to initialize
 * @param[in] p_system_time Pointer to the system time interface to use
 * @param[in] p_cap_interface Pointer to the cap detection interface to use
 * @param[in] p_tilt_interface Pointer to the tilt detection interface to use
 *
 * @return result_t A status code indicating success or failure
 */
result_t dose_detection_init(dose_detection_t *const p_self,
                             system_time_interface_t *const p_system_time,
                             cap_detection_interface_t *const p_cap_interface,
                             tilt_detection_interface_t *const p_tilt_interface);

#endif // DOSE_DETECTION_H