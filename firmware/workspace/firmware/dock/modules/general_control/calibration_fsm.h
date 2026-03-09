/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file calibration_fsm.h
 * @ingroup gc_module
 * @brief This state machine manages the weight calibration workflow.
 * @details
 *
 * @note This state machine is intended to be a singleton.
 */

#ifndef CALIBRATION_FSM_H_
#define CALIBRATION_FSM_H_
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
#define WAIT_FOR_CALIBRATION_WEIGHT_STATE_TIMEOUT_MS (1000u * 60u * 2u)
#define CALIBRATING_TIMEOUT_MS                       (1000u * 60u * 5u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

// See app_dock_ppi.h for the state machine states definition.

typedef enum
{
   CALIBRATION_FSM_ERROR_NONE = 0,
   CALIBRATION_FSM_ERROR_NULL_PTR,
   CALIBRATION_FSM_ERROR_MAX
} CALIBRATION_FSM_ERROR;

/**
 * Data storage into NVM operation result.
 */
typedef enum
{
   DATA_STORAGE_NONE = 0,
   DATA_STORAGE_SUCCESS,
   DATA_STORAGE_FAILED,
   DATA_STORAGE_MAX
} DATA_STORAGE;

/**
 * @brief Input data structure for the state machine. This data is used by the state machine logic to
 * determnine the next state.
 * @note This state machine also keeps track of time as an input but this is automatically done inside the state machine
 * to prevent accidentally omitting new time and have a possible timeout fail as a result.
 */
typedef struct
{
   bool start_calibration;     // To trigger the start of the calibration process.
   bool is_calibration_active; // To indicate whether the process is active and should continue or stop.
   bool is_ring_present;
   DATA_STORAGE data_update_status;    // Was the data update to dock data manager successful
   bool is_empty_dock_weight_set;      // CALIBRATION_STATE_SET_DOCK_WEIGHT
   bool is_calibration_weight_set;     // CALIBRATION_STATE_CALIBRATING
   bool is_calibration_weight_present; // CALIBRATION_STATE_WAIT_FOR_CALIBRATION_WEIGHT
} calibration_fsm_inputs_t;

/**
 * @brief Output data structure for the state machine.
 */
typedef struct
{
   bool changed; // Indicates if the state changed since the last call to the state machine.
} calibration_fsm_outputs_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
extern const char *calibration_fsm_state_names[];
/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t calibration_fsm_init(statemachine_t *const self, const system_time_interface_t *systick_ifc);
#endif // CALIBRATION_FSM_H_
