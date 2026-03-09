/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file system_time_interface.h
 * @ingroup system_time_module
 * @brief
 */

#ifndef SYSTEM_TIME_INTERFACE_H
#define SYSTEM_TIME_INTERFACE_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error definitions for this unit.
 */
typedef enum
{
   SYSTEM_TIME_ERROR_NONE = 0,
   SYSTEM_TIME_ERROR_PTR_NULL,
   SYSTEM_TIME_ERROR_OVERFLOW,
   SYSTEM_TIME_ERROR_MAX,
} SYSTEM_TIME_ERROR;

struct system_time;

typedef struct system_time_interface system_time_interface_t;

/**
 * @brief This struct represents the interface for managing system time.
 *
 * This interface provides methods for incrementing the system time by fixed amounts and retrieving the current time.
 * It is designed to be used in a multi-instance environment.
 */
struct system_time_interface
{
   struct system_time *parent; // Reference to the containing instance.

   /**
    * @brief Increments the system time by 1 millisecond.
    *
    * @param interface The pointer to the system time interface.
    * @return result_t A result indicating success or failure.
    */
   result_t (*inc_time_by_1_ms)(const system_time_interface_t *const interface);

   /**
    * @brief Increments the system time by 10 milliseconds.
    *
    * @param interface The pointer to the system time interface.
    * @return result_t A result indicating success or failure.
    */
   result_t (*inc_time_by_10_ms)(const system_time_interface_t *const interface);

   /**
    * @brief Increments the system time by 100 milliseconds.
    *
    * @param interface The pointer to the system time interface.
    * @return result_t A result indicating success or failure.
    */
   result_t (*inc_time_by_100_ms)(const system_time_interface_t *const interface);

   /**
    * @brief Increments the system time by 1000 milliseconds (1 second).
    *
    * @param interface The pointer to the system time interface.
    * @return result_t A result indicating success or failure.
    */
   result_t (*inc_time_by_1000_ms)(const system_time_interface_t *const interface);

   /**
    * @brief Increments the system time by a given number of milliseconds.
    *
    * @param interface The pointer to the system time interface.
    * @param value_ms The number of milliseconds to increment the system time by.
    * @return result_t A result indicating success or failure.
    */
   result_t (*inc_time_by_set_val_ms)(const system_time_interface_t *const interface, uint16_t value_ms);

   /**
    * @brief Sets the system time to a given number of milliseconds.
    *
    * @param interface The pointer to the system time interface.
    * @param value_ms The given number of milliseconds to set the time to
    * @return result_t A result indicating success or failure.
    */
   result_t (*set_time_ms)(const system_time_interface_t *const interface, uint64_t value_ms);

   /**
    * @brief Retrieves the current system time.
    *
    * @param interface The pointer to the system time interface.
    * @param current_system_time A pointer to store the current system time.
    * @return result_t A result indicating success or failure.
    */
   result_t (*get_time_ms)(const system_time_interface_t *const interface, uint64_t *current_system_time);

   /**
    * @brief Retrieves the initialization status of the system time module.
    *
    * @param interface The pointer to the system time interface.
    * @param is_initialized A pointer to store the initialization status (true if initialized, false otherwise).
    * @return result_t A result indicating success or failure.
    */
   result_t (*get_initialization_status)(const system_time_interface_t *const interface, bool *is_initialized);
};

#endif