/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup dose_scheduler_module Dose Scheduler
 * @ingroup modules
 * @brief The dose scheduler determines whether a dose window is currently open based on a supplied dose schedule
 * @details
 * It provides indications of when a dose window is open.
 * The dose schedule can be updated and the currently open window can be closed if the dose is reported as administered.
 * It performs validity checking on the provided schedule.abs
 * If an invalid schedule is applied the scheduler will default to a no-dose-windows state.
 *
 * @file dose_scheduler.h
 * @ingroup dose_scheduler_module
 * @brief Header file for the dose scheduler module
 *
 * This module provides functionality to manage and monitor a dose schedule.
 */

#ifndef DOSE_SCHEDULER_H_
#define DOSE_SCHEDULER_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include <stdbool.h>
#include <stdint.h>

// Custom includes
#include "common.h"
#include "dose_scheduler_interface.h"
#include "rtc_system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Dose scheduler
 */
typedef struct dose_scheduler dose_scheduler_t;
struct dose_scheduler
{
   // Interface
   dose_scheduler_interface_t interface; /**<  Interface to the scheduler */

   // Dependencies
   const rtc_system_time_interface_t *_rtc_interface; /**<  Interface to the RTC */

   // Private Data
   dose_schedule_t _schedule;         /**<  Currently loaded dose schedule */
   uint32_t _start_date_unix_seconds; /**<  Date when the schedule was started */
   bool _dose_schedule_loaded;        /**<  Whether a valid dose schedule is currently loaded */

   bool _dose_pending_for_window; /**<  Internal flag showing if a dose has been administered */

   bool _is_initialized; /**<  Whether the instance has been initialized */
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes a dose scheduler instance
 *
 * @param[in,out] p_self Pointer to the dose scheduler instance to initialize
 * @param[in] rtc_interface Pointer to the RTC system time interface to use
 *
 * @return Status code indicating the result of the operation
 */
result_t dose_scheduler_init(dose_scheduler_t *const p_self, rtc_system_time_interface_t *const rtc_interface);

#endif /* DOSE_SCHEDULER_H_ */
