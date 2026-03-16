/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup dose_size_detection
 * @ingroup modules
 * @brief The dose size detection module for the dock manages the overall dose size detection. This module is ONLY for
 * the interim release, whereafter dose size detection will be handled by the backend.
 *
 * @file dose_size_detection.h
 * @ingroup dose_size_detection
 * @brief
 */

#ifndef DOSE_SIZE_DETECTION_H_
#define DOSE_SIZE_DETECTION_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "debug.h"
#include "dose_size_detection_interface.h"
#include "dsd_fsm.h"
#include "system_time.h"
#include "weight_sensor.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Number of entries in the rolling best-sample window. */
#define DSD_BEST_SAMPLE_WINDOW_LEN ((uint8_t)DSD_BEST_SAMPLE_WINDOW_DURATION_S)

/** Sentinel index indicating there is no best sample currently available in window. */
#define DSD_BEST_SAMPLE_IDX_INVALID (DSD_BEST_SAMPLE_WINDOW_LEN)

/** How many seconds of stable weight measurement are required to consider the weight as stable */
#define STABLE_WEIGHT_DURATION_SEC (2u)

/** How many seconds the weight must be stable to consider the weight as settled */
#define SETTLED_WEIGHT_DURATION_SEC (2u)

// Note that at a later stage these values are to be populated from the backend or app during calibration, per bottle
#define DOCK_MAX_DRIFT_MG     (300)   // Maximum drift mg
#define DOCK_MAX_DRIFT_NEG_MG (-300)  // Maximum drift mg
#define BOTTLE_E_WEIGHT_MG    (17000) // Bottle empty weight (17g) - this needs to be confirmed
#define BOTTLE_F_WEIGHT_MG    (27050) // Bottle full weight (27g)
#define HYST_WEIGHT_MG        (1000)  // Weight hysteresis

#define DROP_WEIGHT_MIN_MG             (18)                                                  // minimum drop weight mg
#define DROP_WEIGHT_MG                 (25)                                                  // average drop weight mg
#define DROP_WEIGHT_MAX_MG             (35)                                                  // maximum drop weight mg
#define MIN_DOSE_SIZE_MG               (0)                                                   // minimum dose size mg
#define MAX_DOSE_SIZE_MG               (DROP_WEIGHT_MAX_MG * MAX_NUM_DROPS_ALLOWED_PER_DOSE) // maximum dose size mg
#define MAX_NUM_DROPS_ALLOWED_PER_DOSE (5u)                                                  // todo: dynamic allocation

#define DOCK_RING_EMPTY_LOWER_W_MG (DOCK_MAX_DRIFT_NEG_MG + BOTTLE_E_WEIGHT_MG - HYST_WEIGHT_MG)
#define DOCK_RING_FULL_UPPER_W_MG  (DOCK_MAX_DRIFT_MG + BOTTLE_F_WEIGHT_MG + HYST_WEIGHT_MG)

// Weight measurement
#define FS_FAST_HZ (10u) // sampling rate for fast sampling
#define FS_SLOW_HZ (1u)  // sampling rate for slow sampling

#define T_IDLE_SAMPLE_MS                                                                                               \
   (60000u) // idle sampling time (ms) - this is implemented when a stable weight measurement could not be obtained for
            // STABLE_WAIT_TIME_S

#define IDLE_STATE_WAIT_MAX_S                                                                                          \
   (30u * 60u) // maximum time to wait in idle state for stable weight measurement before triggering alert (30min)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Tracks the stability state of weight measurements for dose size detection.
 *
 * @param _last_sample_time_ms Timestamp of the last sample (ms)
 * @param is_not_moving True if the system is not moving
 * @param is_stable True if the weight measurement is stable
 * @param is_settled True if the weight measurement has been stable long enough to be considered settled
 * @param sample_taken True if a weight sample has been taken
 * @param max_time_start_time_s Start time for the maximum duration the system is allowed to wait for a stable
 * measurement before lowering the sampling frequency to save power (s)
 * @param last_stable_time_s Timestamp of the last stable state (s)
 */
typedef struct
{
   uint64_t _last_sample_time_ms;

   bool is_not_moving;
   bool is_stable;
   bool is_settled;
   bool sample_taken;
   uint32_t max_time_start_time_s;
   uint32_t last_stable_time_s;
} stability_tracker_t;

/**
 * @brief A struct representing a single weight sample for dose size detection.
 */
typedef struct
{
   uint64_t time_ms;  /**< Sample timestamp in milliseconds */
   int32_t weight_mg; /**< Weight in milligrams */
   uint16_t sigma_mg; /**< Standard deviation in milligrams */
   bool is_valid;     /**< Whether the sample is valid */
} dsd_sample_t;

/**
 * @brief Main dose size detection structure that encapsulates all related data and interfaces.
 */
typedef struct dose_size_detection
{
   dose_size_detection_interface_t interface;

   const weight_sensor_interface_t *_weight_sensor_interface;
   const system_time_interface_t *_system_time_interface;

   uint32_t _last_total_dispensed_weight_mg; /**< Total weight dispensed since full bottle (mg) */
   uint32_t _full_assembly_weight_mg;        /**< Full assembly weight including full bottle (mg) */

   statemachine_t _statemachine;
   DSD_STATEMACHINE_STATE _previous_state;
   dsd_fsm_inputs_t _sm_inputs;
   dsd_fsm_outputs_t _sm_outputs;

   int32_t _dose_size_mg;    /**< Calculated dose size (mg) */
   uint16_t _sigma_total_mg; /**< Combined standard deviation of drift and post-dose */

   dsd_sample_t _pre_dose_data;  /**< Data captured before dose event */
   dsd_sample_t _drift_data;     /**< Drift data captured while ring is absent */
   dsd_sample_t _post_dose_data; /**< Data captured after dose event */

   DSD_DATA_STATE _dose_size_data_state;           /**< Current of the dose size data */
   DSD_DATA_BAD_REASON _dose_size_data_bad_reason; /**< Reason for bad/invalid dose size data, if applicable */

   uint32_t _wait_time_ms;       /**< Time to wait before a state-specific condition */
   DSD_FREQ _sampling_frequency; /**< Current sampling frequency based on state machine state */

   stability_tracker_t _stability_tracker; /**< Stability tracker for weight measurements */

   /** Circular buffer holding one entry per second over the rolling best-sample window */
   dsd_sample_t _best_sample_window[DSD_BEST_SAMPLE_WINDOW_LEN];
   uint8_t _best_sample_window_head;             /**< Index of the newest second-slot in the window */
   uint8_t _best_sample_window_tail;             /**< Index of the oldest second-slot in the window */
   uint8_t _best_sample_idx;                     /**< Index of the best sample in the current window */
   uint32_t _best_sample_window_latest_second_s; /**< Latest whole second represented by _best_sample_window_head */

   uint64_t _baselining_start_time_ms;                  /**< Timestamp for current baselining attempt start (ms) */
   uint64_t _last_baselining_sample_time_ms;            /**< Timestamp of last sample consumed during baselining (ms) */
   uint32_t _full_assembly_weight_mg_before_baselining; /**< Baseline value before current baselining attempt */
   uint32_t _last_total_dispensed_weight_mg_before_baselining; /**< Dispensed total before baselining attempt */
   bool _is_baselining_in_progress; /**< True while try_baseline_if_stable is attempting to converge */

   uint64_t _time_enter_idle_state_ms;
   bool _got_valid_weight_sample; /**< Whether a valid weight sample has been obtained. Cleared in process method */

   bool _is_blocked; /**< True while DSD prerequisites are unmet */
   /** @todo define valid */
   bool _is_latest_sample_valid; /**< Whether the latest weight sample is valid */
   bool _initialized;            /**< Whether this instance has been initialized */
} dose_size_detection_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initialize an instance of the dose size detection module
 *
 * Initializes the dose size detection instance with the provided interfaces and configuration data.
 * If the configuration data is not provided on initialization, accurate dose size detection may not be possible.
 * Configuration data can be updated later when available, through the appropriate interface functions.
 *
 * @param[in,out] self Pointer to the dose size detection instance to initialize
 * @param[in] weight_sensor_interface Pointer to the weight sensor interface to use for weight measurements
 * @param[in] system_time_interface Pointer to the system time interface to use for timing operations
 * @param[in] full_assembly_weight_mg Assembly weight in milligrams including a full medication bottle. (can be set
 * to 0 if unknown)
 * @param[in] last_total_dispensed_weight_mg Last total dispensed weight in milligrams. (can be set to 0 if unknown)
 *
 * @return Result of the operation
 */
result_t dose_size_detection_init(dose_size_detection_t *const self,
                                  const weight_sensor_interface_t *const weight_sensor_interface,
                                  const system_time_interface_t *const system_time_interface,
                                  uint32_t full_assembly_weight_mg,
                                  uint32_t last_total_dispensed_weight_mg);

#endif // DOSE_SIZE_DETECTION_H_
