/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup dose_size_detection
 * @ingroup modules
 * @brief The dose size detection statemachine module for the dock manages the state transitions for dose size
 * detection.
 *
 * @file dsd_fsm.h
 * @ingroup dose_size_detection
 * @brief Dose size detection state machine header file.
 */

#ifndef DSD_FSM_H_
#define DSD_FSM_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "debug.h"
#include "statemachine.h"
#include "system_time.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define STABLE_WAIT_TIME_S (60u) // Max time to wait for stable measurement (1min)
#define T_SETTLE_RING_S    (20u) // Time after ring replaced to be considered settled

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
/**
 * @brief List of state machine states.
 *
 * DSD_STATEMACHINE_STATE_RING_PRESENT: Ring is present on the dock, dose event has not yet started. Record baseline DRB
 * weight.
 * DSD_STATEMACHINE_STATE_RING_ABSENT: Ring has been removed from the dock, possible dose event in progress. Record
 * drift of Dock (zero offset) weight.
 * DSD_STATEMACHINE_STATE_RING_REPLACED: Ring has been replaced on the dock, dose
 * event may have ended. Record post-dose DRB weight.
 * DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED: Ring has been replaced and weight measurement is
 * settled. Calculate dose size.
 * DSD_STATEMACHINE_STATE_IDLE: State machine is idle. Waiting for stable weight measurement to resume dose size
 * detection.
 */
typedef enum
{
   DSD_STATEMACHINE_STATE_RING_PRESENT = 0,
   DSD_STATEMACHINE_STATE_RING_ABSENT,
   DSD_STATEMACHINE_STATE_RING_REPLACED,
   DSD_STATEMACHINE_STATE_RING_REPLACED_SETTLED,
   DSD_STATEMACHINE_STATE_IDLE,
   DSD_STATEMACHINE_STATE_MAX
} DSD_STATEMACHINE_STATE;

/**
 * @brief List of error codes for the dose size detection state machine.
 */
typedef enum
{
   DSD_FSM_ERROR_NONE = 0,
   DSD_FSM_ERROR_NULL_PTR,
   DSD_FSM_ERROR_INVALID_STATE,
   DSD_FSM_ERROR_MAX
} DSD_FSM_ERROR;

/**
 * @brief Sampling frequency for dose size detection states.
 */
typedef enum
{
   DSD_FREQ_SLOW = 0,
   DSD_FREQ_FAST,
   DSD_FREQ_IDLE,
   DSD_FREQ_MAX
} DSD_FREQ;

/**
 * @brief Input data structure for the dose size detection state machine. This data is used by the state machine
 * logic to determine the next state.
 */
typedef struct
{
   bool is_ring_present;         /**< Whether the ring is currently present on the dock */
   bool got_valid_weight_sample; /**< Whether a valid sample was obtained the previous cycle */
   bool is_max_time_elapsed;     /**< Whether the maximum wait time for a stable measurement has elapsed */
   bool is_dose_size_computed;   /**< Whether the dose size has been computed after ring replacement */
} dsd_fsm_inputs_t;

/**
 * @brief Output data structure for the dose size detection state machine.
 *
 * @param changed Indicates if the state changed since the last call to the state machine.
 * @param is_transition_valid Indicates whether this transition interrupts the dose size detection sequence.
 * E.g. ring removed during dose size computation would be invalid.
 * @param sampling_frequency Sampling frequency for the current state.
 * @param wait_time Maximum stable weight measurement wait time for the current state (s)
 * OR time to wait after ring replacement to be considered settled (s).
 * @param is_dose_size_computed Indicates whether the dose size has been computed during the ring replaced settle state.
 * It is reset during the transition from ring replaced settled to ring present to ensure that a new dose size is
 * computed for each dose event.
 */
typedef struct
{
   bool changed;
   bool is_sequence_interrupted; /**< Whether the dose size detection sequence was interrupted */
   bool is_dose_size_computed;
   DSD_FREQ sampling_frequency;
   uint32_t wait_time;
} dsd_fsm_outputs_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
extern const char *dsd_fsm_state_names[];
/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t dsd_fsm_init(statemachine_t *const self);
#endif // DSD_FSM_H_
