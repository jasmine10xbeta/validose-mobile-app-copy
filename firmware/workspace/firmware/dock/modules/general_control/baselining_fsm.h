/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file baselining_fsm.h
 * @ingroup gc_module
 * @brief This state machine manages the new medication weight baselining workflow.
 * @details
 *
 * @note This state machine is intended to be a singleton.
 */

#ifndef BASELINING_FSM_H_
#define BASELINING_FSM_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "app_dock_ppi.h"
#include "common.h"
#include "statemachine.h"
#include "system_time.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define WAIT_FOR_RING_REMOVAL_STATE_TIMEOUT_MS       (1000u * 60u * 1u)
#define SET_DOCK_WEIGHT_STATE_TIMEOUT_MS             (1000u * 60u * 5u)
#define WAIT_FOR_RING_STATE_TIMEOUT_MS               (1000u * 60u * 1u)
#define SET_RING_DOCK_WEIGHT_STATE_TIMEOUT_MS        (1000u * 60u * 5u)
#define WAIT_FOR_BACKEND_VALIDATION_STATE_TIMEOUT_MS (1000u * 60u * 5u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

// See app_dock_ppi.h for the state machine states definition.

typedef enum
{
   BASELINING_FSM_ERROR_NONE = 0,
   BASELINING_FSM_ERROR_NULL_PTR,
   BASELINING_FSM_ERROR_MAX
} BASELINING_FSM_ERROR;

typedef enum
{
   BACKEND_VAL_STATUS_NONE = 0,
   BACKEND_VAL_STATUS_SUCCESS,
   BACKEND_VAL_STATUS_UNSUCCESSFUL,
   BACKEND_VAL_STATUS_MAX
} BACKEND_VAL_STATUS;

typedef enum
{
   DOSE_SCH_STATUS_NONE = 0,
   DOSE_SCH_STATUS_VALID,
   DOSE_SCH_STATUS_INVALID,
   DOSE_SCH_STATUS_MAX
} DOSE_SCH_STATUS;

typedef enum
{
   MED_UID_STORAGE_NONE = 0,
   MED_UID_STORAGE_SUCCESS,
   MED_UID_STORAGE_FAILED,
   MED_UID_STORAGE_MAX
} MED_UID_STORAGE;

/**
 * @brief Input data structure for the state machine. This data is used by the state machine logic to
 * determnine the next state.
 * @note This state machine also keeps track of time as an input but this is automatically done inside the state machine
 * to prevent accidentally omitting new time and have a possible timeout fail as a result.
 */
typedef struct
{
   bool start_baselining;     // To trigger the start of the baselining process.
   bool is_baselining_active; // To indicate whether the process is active and should continue or stop.
   bool is_ring_present;
   bool is_medication_uid_avail;
   MED_UID_STORAGE med_update_status; // Was the medication UID update (after backend validation) to dock data manager
                                      // successful
   bool is_empty_dock_weight_set;     // Empty dock weight
   bool is_ring_dock_full_med_weight_set;
   BACKEND_VAL_STATUS backend_validation_status;
   DOSE_SCH_STATUS dose_schedule_status;
} baselining_fsm_inputs_t;

/**
 * @brief Output data structure for the state machine.
 */
typedef struct
{
   bool changed; // Indicates if the state changed since the last call to the state machine.
} baselining_fsm_outputs_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
extern const char *baselining_fsm_state_names[];
/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t baselining_fsm_init(statemachine_t *const self, const system_time_interface_t *systick_ifc);
#endif // BASELINING_FSM_H_
