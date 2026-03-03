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

#define STDDEV_LONG_BUFFER_CAP          (FS_FAST_HZ * STABLE_WEIGHT_DURATION_SEC) // std-dev buffer size (fast sampling)
#define STDDEV_LONG_BUFFER_REPLACED_CAP (FS_FAST_HZ * STABLE_WEIGHT_DURATION_SEC) // std-dev buffer size (fast sampling)

#define IDLE_STATE_WAIT_MAX_S                                                                                          \
   (30u * 60u) // maximum time to wait in idle state for stable weight measurement before triggering alert (30min)
/**
 * These values differ as when the ring is present, the samples are taken at a much lower rate and have higher noise
 * (the stddev is only determined over a 100ms window). When the ring is absent, the samples are taken at a higher rate
 * with lower noise (the stddev is determined over a 1s window), so the thresholds are much lower.
 */
#define SIGMA_LOW_RING_PRESENT_MG  (100u)  // stationary if below
#define SIGMA_HIGH_RING_PRESENT_MG (1000u) // Moving if above
#define SIGMA_LOW_RING_ABSENT_MG   (20u)   // stationary if below

/**
 * @brief Thresholds for determining if weight is stable or not, based on standard deviation over a window of samples.
 * DRFIT_SIGMA_MG is used to determine if the curren weight sample (over 100ms) is stable enough to consider for
 * re-zeroing. RE_ZERO_MAX_DRIFT_MG and RE_ZERO_MAX_DRIFT_NEG_MG define the maximum drift limits beyond which re-zeroing
 * is triggered.
 */
#define DRIFT_SIGMA_MG           (SIGMA_LOW_RING_PRESENT_MG)
#define RE_ZERO_MAX_DRIFT_MG     (DOCK_MAX_DRIFT_MG)     // Maximum drift mg
#define RE_ZERO_MAX_DRIFT_NEG_MG (DOCK_MAX_DRIFT_NEG_MG) // Maximum drift mg

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Shared circular buffer for rolling window and moving average
 */
typedef struct
{
   int32_t *buf;      /**< Pointer to circular buffer (mg) */
   uint16_t cap;      /**< Capacity (N) */
   uint16_t count;    /**< Number of valid samples (<= cap) */
   uint16_t head;     /**< Write index (oldest element) */
   bool filled;       /**< True if buffer has wrapped at least once */
   int64_t sum;       /**< Sum of all valid samples */
   int64_t sum_delay; /**< Delayed sum for advanced calculations */
   uint64_t sum2;     /**< Sum of squares of all valid samples */
} circ_buf_dsd_t;

/**
 * @brief Tracks the stability state of weight measurements for dose size detection.
 *
 * @param _last_sample_time_ms Timestamp of the last sample (ms)
 * @param is_not_moving True if the system is not moving
 * @param is_weight_in_bounds True if weight is within expected bounds
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
   bool is_weight_in_bounds;
   bool is_stable;
   bool is_settled;
   bool sample_taken;
   uint32_t max_time_start_time_s;
   uint32_t last_stable_time_s;
} stability_tracker_t;

/**
 * @brief Stores dose-related baseline (Dock, Ring, Bottle) and drift data for dose size detection.
 */
typedef struct
{
   int32_t weight_mg;        /**< Weight in milligrams */
   uint16_t sigma_mg;        /**< Standard deviation in milligrams */
   bool is_valid;            /* Whether the data is valid */
   uint64_t last_updated_ms; /* Timestamp of the last update in milliseconds */
} drb_data_t;

/**
 * @brief Stores all dose size detection data, including DRB, drift, and calculated dose sizes.
 *
 * @param drb_pre DRB data before dose event
 * @param drift_d1 Drift data at first drift point (just after the ring is removed)
 * @param drift_d2 Drift data at second drift point (just before the ring is replaced)
 * @param drb_post DRB data after dose event
 * @param _dose_size_op1_mg Calculated dose size using option 1 (mg)
 * @param _drb_initial_mg Initial DRB value for option 2 (mg)
 * @param _drift_initial_mg Initial drift value for option 2 (mg)
 * @param _dose_size_op2_mg Calculated dose size using option 2 (mg)
 * @param _last_valid_total_dispensed_mg Last valid total dispensed amount (mg) - since full bottle
 */
typedef struct
{
   drb_data_t drb_pre;
   drb_data_t drift_d1;
   drb_data_t drift_d2;
   drb_data_t drb_post;

   // for dsd option 1
   int32_t _dose_size_op1_mg;

   // for dsd option 2
   int32_t _drb_initial_mg;
   int32_t _drift_initial_mg;
   int32_t _dose_size_op2_mg;

   /** Combined standard deviation, calculated as the combined standard deviation of `drift_d2` and `drb_post`. */
   uint16_t sigma_total_mg;

} dose_size_data_t;

/**
 * @brief Main dose size detection structure that encapsulates all related data and interfaces.
 *
 * @param interface Dose size detection interface
 * @param _weight_sensor_interface Pointer to the weight sensor interface
 * @param _system_time_interface Pointer to the system time interface
 * @param _stddev_long Circular buffer for long-term standard deviation calculations
 * @param _stddev_long_buffer Buffer for storing long-term standard deviation samples
 * @param _sigma_long_mg Current long-term standard deviation value (mg) - used for stability tracking
 * @param _statemachine State machine for dose size detection
 * @param _sm_inputs Inputs to the state machine
 * @param _sm_outputs Outputs from the state machine
 * @param _dose_size_data Dose size detection data
 * @param _stability_tracker Stability tracker for weight measurements
 * @param _is_next_invalid_sample_the_first True if the next invalid sample is the first one after a valid sample, used
 * to determine stability wait-time timeouts
 * @param _is_scale_ready True if the weight scale is calibrated and ready for use
 * @param _time_enter_idle_state_ms Timestamp of when the state machine entered the IDLE state (ms)
 * @param _seq_interrupted_or_timer_expired True if the dose size detection sequence was interrupted or the max wait
 * timer (in IDLE state) expired
 * @param _initialized True if the dose size detection module has been initialized
 */
typedef struct dose_size_detection
{
   dose_size_detection_interface_t interface;
   const weight_sensor_interface_t *_weight_sensor_interface;
   const system_time_interface_t *_system_time_interface;

   circ_buf_dsd_t _stddev_long; // long mean window for baseline (mg)
   int32_t _stddev_long_buffer[STDDEV_LONG_BUFFER_CAP];
   uint16_t _sigma_long_mg;

   statemachine_t _statemachine;
   DSD_STATEMACHINE_STATE _previous_state;
   dsd_fsm_inputs_t _sm_inputs;
   dsd_fsm_outputs_t _sm_outputs;

   uint32_t _last_total_dispensed_weight_mg; /**< Total weight dispensed since full bottle (mg) */
   uint32_t _full_assembly_weight_mg;        /**< Full assembly weight including full bottle (mg) */

   DSD_DATA_STATE _dose_size_data_state;           /**< Current of the dose size data */
   DSD_DATA_BAD_REASON _dose_size_data_bad_reason; /**< Reason for bad/invalid dose size data, if applicable */
   dose_size_data_t _dose_size_data;

   uint32_t _wait_time_ms;       /**< Time to wait before a state-specific condition */
   DSD_FREQ _sampling_frequency; /**< Current sampling frequency based on state machine state */

   stability_tracker_t _stability_tracker;

   uint64_t _baselining_start_time_ms;                  /**< Timestamp for current baselining attempt start (ms) */
   uint64_t _last_baselining_sample_time_ms;            /**< Timestamp of last sample consumed during baselining (ms) */
   uint32_t _full_assembly_weight_mg_before_baselining; /**< Baseline value before current baselining attempt */
   uint32_t _last_total_dispensed_weight_mg_before_baselining; /**< Dispensed total before baselining attempt */
   bool _is_baselining_in_progress; /**< True while try_baseline_if_stable is attempting to converge */

   uint64_t _time_enter_idle_state_ms;
   bool _got_valid_weight_sample; /**< Whether a valid weight sample has been obtained. Cleared in process method */

   bool _seq_interrupted_or_timer_expired;
   bool _is_next_invalid_sample_the_first;
   bool _initialized;
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
