/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_size_detection_interface.h
 * @ingroup dose_size_detection_module
 * @brief Interface for the dose size detection module for the dock. This module is ONLY for the interim release,
 * whereafter dose size detection will be handled by the backend.
 *
 * Expected usage sequence:
 * 1. General Control initializes the dose size detection module with `dose_size_detection_init`
 *    - Calibration data is passed in at initialization
 * 2. General Control regularly calls the `process` method to update the dose size detection state machine
 * 3. If General Control receives a dose event from the Ring:
 *    3.1 General Control polls `get_dose_size_data_state` to check if dose size data is ready
 *    3.2 If data state is `DSD_DATA_STATE_READY`
 *       3.2.1 General Control retrieves the dose size data using the `fetch_dose_size_data` method
 *       3.2.2 General Control attaches the dose size data to the dose event for backend reporting
 *    3.3 If data state is `DSD_DATA_STATE_BAD`
 *       3.3.1 General Control can call `get_dose_size_data_bad_reason` to get the reason for bad data
 *       3.3.2 General Control can fetch the bad dose size data using the `fetch_dose_size_data` method, if desired
 *    3.4 If data state is `DSD_DATA_STATE_BUSY`
 *       3.4.1 General Control can keep polling until the data is either ready or bad
 *             - Dose Size Detection has internal timeouts to ensure that it will eventually resolve.
 */

#ifndef DOSE_SIZE_DETECTION_INTERFACE_H_
#define DOSE_SIZE_DETECTION_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/** Minimum weight for the full assembly weight to be considered valid during baselining, in milligrams */
#define DSD_BASELINE_WEIGHT_THRESHOLD_MG (10000u)

/** Maximum time in milliseconds to wait for stable samples during baselining */
#define DSD_BASELINING_TIMEOUT_MS (60000u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for the dose size detection module
 */
typedef enum
{
   DOSE_SIZE_DETECTION_ERROR_NONE = 0,

   DOSE_SIZE_DETECTION_ERROR_PTR_NULL,
   DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED,

   DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED, /*< The weight scale is not calibrated and ready for use */
   DOSE_SIZE_DETECTION_ERROR_NOT_BASELINED,        /*< The dose size detection baseline has not been set */

   DOSE_SIZE_DETECTION_ERROR_NO_DATA,  /**< No data available */
   DOSE_SIZE_DETECTION_ERROR_BUSY,     /**< Operaion still in progress */
   DOSE_SIZE_DETECTION_ERROR_DATA_BAD, /**< Data is invalid and should not be used */

   DOSE_SIZE_DETECTION_ERROR_INVALID_ARGUMENT,        /*< Invalid argument provided */
   DOSE_SIZE_DETECTION_ERROR_INVALID_BASELINE_WEIGHT, /*< The weight reading during baselining was invalid */
   DOSE_SIZE_DETECTION_ERROR_TIMEOUT,                 /*< Baselining process timed out */

   DOSE_SIZE_DETECTION_ERROR_MAX, /**< Sentinel value */
} DOSE_SIZE_DETECTION_ERROR;

/**
 * @brief Dose size data state
 *
 * This enum defines the different states of the dose size data, which can be used to track the progress of dose size
 * detection and ensure that data is being used appropriately based on its freshness and validity.
 */
typedef enum
{
   DSD_DATA_STATE_NO_DATA = 0, /**< There is currently no dose size data available or being processed */
   DSD_DATA_STATE_BUSY,        /**< Dose size detection is currently in progress, and data is being processed */
   DSD_DATA_STATE_READY,       /**< A new dose size has been computed and is ready to be retrieved */
   DSD_DATA_STATE_BAD,         /**< The computed dose size data is invalid and should not be used. */

   DSD_DATA_STATE_MAX /**< Sentinel value */
} DSD_DATA_STATE;

/**
 * @brief Dose size data bad reason
 *
 * This enum defines the different reasons why computed dose size data may be considered bad/invalid, which can be used
 * for debugging and to inform decision-making in the app/backend when dose size data is deemed unusable.
 */
typedef enum
{
   DSD_DATA_BAD_REASON_NONE = 0,           /**< No issues. Data is valid */
   DSD_DATA_BAD_REASON_INVALID_DRB_DATA,   /**< One or more of the dose ring baseline (DRB) measurements were invalid */
   DSD_DATA_BAD_REASON_NEGATIVE_DOSE_SIZE, /**< The computed dose size is negative */
   DSD_DATA_BAD_REASON_NEGATIVE_TOTAL_DISPENSED, /**< The computed total dispensed amount is negative */
   DSD_DATA_BAD_REASON_TOTAL_DISPENSED_OVERFLOW, /**< The computed total dispensed amount overflowed */

   DSD_DATA_BAD_REASON_INTERRUPTED, /**< The detection process was interrupted before completion */
   DSD_DATA_BAD_REASON_TIMEOUT,     /**< The detection process took too long and timed out */

   DSD_DATA_BAD_REASON_MAX /**< Sentinel value */
} DSD_DATA_BAD_REASON;

/**
 * @brief Output struct for dose size detection data
 */
typedef struct
{
   uint32_t dose_size_mg;       /**< Computed dose size in milligrams */
   uint32_t total_dispensed_mg; /**< Total dispensed amount in milligrams */
   uint16_t sigma_total_mg;     /**< Overall standard deviation of weight measurements in milligrams */
} dose_size_data_out_t;

struct dose_size_detection; // Forward declaration

/**
 * @brief Interface struct for dose size detection module
 */
typedef struct dose_size_detection_interface dose_size_detection_interface_t;
struct dose_size_detection_interface
{
   struct dose_size_detection *parent; // Reference to the containing instance.

   /**
    * @brief Process function for the dose size detection module
    *
    * This function should be called periodically with the current ring presence status, to allow
    * the dose size detection module to manage its state machine, timing, and data processing.
    *
    * @param[in,out] interface Pointer to the interface instance
    * @param[in] is_ring_present Boolean indicating if the ring is currently present
    *
    * @retval RESULT_OK if the function executed successfully
    * @retval DOSE_SIZE_DETECTION_ERROR_PTR_NULL if the interface pointer is NULL
    * @retval DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED if the module has not been initialized
    * @retval DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED if the weight scale is not calibrated and ready for use
    */
   result_t (*process)(const dose_size_detection_interface_t *const interface, bool is_ring_present);

   /**
    * @brief Try to establish baseline for dose size detection
    *
    * Use this function during the baselining process to attempt to establish the baseline for dose size detection.
    * It will monitor the weight readings and update the full assembly weight and last total dispensed weight
    * if it detects that the weight readings are stable and meet the criteria for a valid baseline.
    *
    * @param[in] interface Pointer to the interface instance
    * @param[in] is_ring_present Whether the ring is currently present. Must be constantly present during baselining.
    * @param[out] baselining_successful Pointer to store whether the baselining was successful. This will be set to true
    * if the baseline was successfully established during this function call.
    *
    * @retval RESULT_OK if the function executed successfully
    * @retval DOSE_SIZE_DETECTION_ERROR_PTR_NULL if any pointer argument is NULL
    * @retval DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED if the module has not been initialized
    * @retval DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED if the weight scale is not calibrated and ready for use
    * @retval DOSE_SIZE_DETECTION_ERROR_TIMEOUT if the baselining process timed out before a valid stable sample
    */
   result_t (*try_baseline_if_stable)(const dose_size_detection_interface_t *const interface,
                                      bool is_ring_present,
                                      bool *baselining_successful);

   /**
    * @brief Abort the baselining process
    *
    * If the baselining process needs to be aborted before completion (e.g., due to ring removal, error conditions, or
    * user intervention), this function can be called to reset any baselining state and revert any changes made during
    * the baselining process.
    *
    * @param[in] interface Pointer to the interface instance
    *
    * @retval RESULT_OK if the function executed successfully or if baselining was not in progress
    * @retval DOSE_SIZE_DETECTION_ERROR_PTR_NULL if the interface pointer is NULL
    * @retval DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED if the module has not been initialized
    */
   result_t (*abort_baselining)(const dose_size_detection_interface_t *const interface);

   /**
    * @brief Get the full assembly weight
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] full_assembly_weight_mg_out Pointer to store the full assembly weight in milligrams
    */
   result_t (*get_full_assembly_weight)(const dose_size_detection_interface_t *const interface,
                                        uint32_t *full_assembly_weight_mg_out);

   /**
    * @brief Get the dose size data state
    *
    * Retrieve the current state of the dose size data. This function allows the caller to determine whether dose size
    * data is available, being processed, ready for retrieval, or invalid.
    *
    * See DSD_DATA_STATE for possible states.
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] data_state_out Pointer to store the current dose size data state
    *
    * @return Result of the operation
    */
   result_t (*get_dose_size_data_state)(const dose_size_detection_interface_t *const interface,
                                        DSD_DATA_STATE *data_state_out);

   /**
    * @brief Get the reason why dose size data is bad/invalid, if applicable
    *
    * If the dose size data state is `DSD_DATA_STATE_BAD`, this function can be called to retrieve the specific reason
    * why the data is considered bad.
    *
    * @param[in] interface Pointer to the interface instance
    * @param[out] bad_reason_out Pointer to store the reason why dose size data is bad
    *
    * @return Result of the operation
    */
   result_t (*get_dose_size_data_bad_reason)(const dose_size_detection_interface_t *const interface,
                                             DSD_DATA_BAD_REASON *bad_reason_out);

   /**
    * @brief Fetch the latest dose size data
    *
    * @param[in,out] interface Pointer to the interface instance
    * @param[out] dose_size_data_out Pointer to store the fetched dose size data
    *
    * @retval RESULT_OK if data was successfully fetched and is valid
    * @retval DOSE_SIZE_DETECTION_ERROR_DATA_BAD if the fetched data is invalid. Still returns the data.
    * @retval DOSE_SIZE_DETECTION_ERROR_NO_DATA if there is no data
    * @retval DOSE_SIZE_DETECTION_ERROR_BUSY if data is still being processed and not yet ready
    * @retval DOSE_SIZE_DETECTION_ERROR_PTR_NULL if any pointer argument is NULL
    * @retval DOSE_SIZE_DETECTION_ERROR_NOT_INITIALIZED if the module has not been initialized
    *
    * @note Fetching consumes the data. After a successful fetch, the data state will transition back to
    * `DSD_DATA_STATE_NO_DATA` until new data is computed.
    *
    * @note If the data is not fetched before new data is computed, the old data will be overwritten and lost. Check
    * dose size data state regularly to avoid this.
    *
    * @note Data will still be returned even if the data state is `DSD_DATA_STATE_BAD`, to allow retrieval of the
    * computed data for debugging and analysis purposes, but the function will return an error code to indicate that the
    * data should not be used.
    */
   result_t (*fetch_dose_size_data)(const dose_size_detection_interface_t *const interface,
                                    dose_size_data_out_t *dose_size_data_out);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif /* DOSE_SIZE_DETECTION_INTERFACE_H_ */
