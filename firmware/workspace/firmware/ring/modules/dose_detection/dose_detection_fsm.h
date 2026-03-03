/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_detection_fsm.h
 * @ingroup modules/dose_detection
 * @brief Header file for the dose detection finite state machine module.
 */

#ifndef DOSE_DETECTION_FSM_H_
#define DOSE_DETECTION_FSM_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "statemachine.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Dose detection state machine states.
 */
typedef enum
{
   DOSE_DETECTION_STATE_IDLE = 0,
   DOSE_DETECTION_STATE_PRIMED,
   DOSE_DETECTION_STATE_EVALUATING,
   DOSE_DETECTION_STATE_DOSE_DETECTED,

   DOSE_DETECTION_STATE_MAX /**< Sentinel value */
} DOSE_DETECTION_STATE;

typedef enum
{
   DOSE_DETECTION_FSM_ERROR_NONE = 0, /**< No error */

   DOSE_DETECTION_FSM_ERROR_NULL, /**< Unexpected null reference */

   DOSE_DETECTION_FSM_ERROR_MAX /**< Sentinel value */
} DOSE_DETECTION_FSM_ERROR;

/**
 * @brief Input data for the dose detection state machine.
 */
typedef struct
{
   bool cap_on;                 /**< Indicates if the cap is on */
   bool tilt_detected;          /**< Indicates if a tilt has been detected. May be unset while in IDLE state */
   uint64_t current_time_ms;    /**< Current timestamp in milliseconds */
   uint64_t dose_start_time_ms; /**< Timestamp when dose event started. May be unset if while in IDLE state. Must be set
                                after transitioning to PRIMED state. */
   uint64_t valid_dose_threshold_ms; /**< Minimum duration for a valid dose event in milliseconds */
} dose_detection_fsm_inputs_t;

/**
 * @brief Output data for the dose detection state machine.
 */
typedef struct
{
   bool state_changed; /**< Indicates if the state has changed */
} dose_detection_fsm_outputs_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/** State names indexed to the valid states in DOSE_DETECTION_STATE */
extern const char *dose_detection_fsm_state_names[];

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes the dose detection finite state machine.
 *
 * @param[in,out] p_self Pointer to the statemachine instance to initialize.
 *
 * @return result_t Result code indicating success or failure.
 */
result_t dose_detection_fsm_init(statemachine_t *const p_self);

#endif // DOSE_DETECTION_FSM_H_
