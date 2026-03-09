/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file dose_scheduler_interface.h
 * @ingroup dose_scheduler_module
 * @brief Interface file for the dose scheduler module
 */

#ifndef DOSE_SCHEDULER_INTERFACE_H_
#define DOSE_SCHEDULER_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for the dose scheduler module
 */
typedef enum
{
   DOSE_SCHEDULER_ERROR_NONE = 0,

   // Generic errors
   DOSE_SCHEDULER_ERROR_NULL,          /**< Unexpected NULL reference */
   DOSE_SCHEDULER_ERROR_INTERNAL,      /**< Internal error */
   DOSE_SCHEDULER_ERROR_UNINITIALIZED, /**< Module not initialized */

   // Dose schedule errors
   DOSE_SCHEDULER_ERROR_INVALID_DOSAGE,            /**< Dosage amount out of range */
   DOSE_SCHEDULER_ERROR_INVALID_TEMP_THRESHOLD,    /**< Temperature threshold out of range */
   DOSE_SCHEDULER_ERROR_INVALID_TEMP_AVG_WINDOW,   /**< Temperature averaging window out of range */
   DOSE_SCHEDULER_ERROR_INVALID_WINDOW_DURATION,   /**< Dose window duration out of range */
   DOSE_SCHEDULER_ERROR_INVALID_DOSE_WINDOW_COUNT, /**< Invalid dose window count */
   DOSE_SCHEDULER_ERROR_INVALID_WINDOW_START,      /**< Dose window start time out of range */
   DOSE_SCHEDULER_ERROR_OVERLAPPING_WINDOWS,       /**< Dose windows overlap */
   DOSE_SCHEDULER_ERROR_ADJACENT_WINDOWS,          /**< Dose windows are back-to-back */
   DOSE_SCHEDULER_ERROR_INVALID_TIMESTAMP,         /**< Invalid timestamp supplied */

   DOSE_SCHEDULER_ERROR_MAX, /**< Sentinel value */
} DOSE_SCHEDULER_ERROR;

struct dose_scheduler; // Forward declaration

/**
 * @brief Dose scheduler interface
 */
typedef struct dose_scheduler_interface dose_scheduler_interface_t;
struct dose_scheduler_interface
{
   struct dose_scheduler *parent; /**< Pointer to the containing instance */

   /**
    * @brief Loads a dose schedule into the dose scheduler
    *
    * This function passes a dose schedule to the dose scheduler.
    * If the schedule is detected to be invalid, the scheduler will clear the previously loaded schedule.
    * If the timestamp provided is in the future, the scheduler will clear the previously loaded schedule.
    *
    * @param[in,out] interface A pointer to the interface instance
    * @param[in] dose_schedule A pointer to the the dose schedule to load. If schedule validation checks fail, an
    * appropriate result is returned indicating the failed parameter.
    * @param[in] start_timestamp_unix_seconds Unix timestamp in seconds indicating when the schedule became active. Must
    * be before or same day of the current time.
    *
    * @return Status code indicating the result of the operation
    *
    * @note The start time of the schedule is considered midnight of the date provided in the timestamp.
    */
   result_t (*load_schedule)(const dose_scheduler_interface_t *const interface,
                             const dose_schedule_t *const dose_schedule,
                             uint32_t start_time_unix_seconds);

   /**
    * @brief This function checks whether a dose window is open
    *
    * @param interface A pointer to the dose scheduler interface
    * @param window_open A pointer to a boolean that will be set to indicate the dose window status
    *
    * @return Status code indicating the result of the operation
    */
   result_t (*check_dose_window)(const dose_scheduler_interface_t *const interface, bool *const window_open);

   /**
    * @brief This function closes the currently open dose window - can be used to close a dose window if a dose is
    * detected as administered
    *
    * @param[in,out] interface A pointer to the dose scheduler interface
    *
    * @return Status code indicating the result of the operation
    */
   result_t (*close_dose_window)(const dose_scheduler_interface_t *const interface);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Validates a dose schedule
 *
 * This function checks that the data provided in a dose schedule is valid and can be loaded into the scheduler.
 *
 * @param[in] p_schedule Pointer to the dose schedule to validate
 *
 * @return Status code indicating the result of the operation
 */
result_t validate_dose_schedule(const dose_schedule_t *const p_schedule);

#endif /* DOSE_SCHEDULER_INTERFACE_H_ */