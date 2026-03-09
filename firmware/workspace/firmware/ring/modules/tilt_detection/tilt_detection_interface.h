/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tilt_detection_interface.h
 * @brief Interface file for the tilt detection module
 *
 * @note The tilt detection module has the limitation that the IMU must be set to active while actively detecting tilts.
 * This is because the IMU needs to be at a constant known sampling rate for accurate tilt duration calculations.
 * Therefore, the `get_tilts` method will set the IMU to active if it is not already, and using the `set_module_active`
 * and `set_module_dormant` methods will reset the tilt detection state.
 */

#ifndef TILT_DETECTION_INTERFACE_H
#define TILT_DETECTION_INTERFACE_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define TILT_DETECTION_LOWER_LIMIT_DEGREES (60.0f) /**< Lower angle at which a tilt is considered ended */
#define TILT_DETECTION_UPPER_LIMIT_DEGREES (90.0f) /**< Upper angle at which a tilt is considered started */

/** Minimum duration in milliseconds for a tilt to be considered valid */
#define TILT_DETECTION_DURATION_THRESHOLD_MS (500u)

#define TILT_DETECTION_MAX_TILTS (5u) /**< Maximum number of tilts that can be returned from `get_tilts` method */

#define TILT_DETECTION_DEFAULT_UPRIGHT_AXIS TILT_DETECTION_AXIS_Z /**< Default upright axis used for tilt detection */

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief Error codes for the tilt detection module
 */
typedef enum
{
   TILT_DETECTION_ERROR_NONE = 0,
   TILT_DETECTION_ERROR_PTR_NULL,
   TILT_DETECTION_ERROR_INVALID_AXIS,
   TILT_DETECTION_ERROR_CLEAR_IMU_DATA,
   TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT,
   TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRIES,
   TILT_DETECTION_ERROR_SET_IMU_ACTIVE,
   TILT_DETECTION_ERROR_SET_IMU_DORMANT,
   TILT_DETECTION_ERROR_NO_IMU_DATA,
   TILT_DETECTION_ERROR_MAX,
} TILT_DETECTION_ERROR;

/**
 * @brief Tilt data
 *
 * @note This structure is packed and aligned to 1 byte
 */
typedef struct __attribute__((packed, aligned(1)))
{
   uint64_t detected_at_time_ms; /**< Time at which the tilt was detected in milliseconds since boot (system time) */
   uint64_t duration_ms;         /**< Duration of the tilt in milliseconds */
} tilt_data_t;

/**
 * @brief Tilt detection axis
 */
typedef enum
{
   TILT_DETECTION_AXIS_UNKNOWN = 0,
   TILT_DETECTION_AXIS_X,
   TILT_DETECTION_AXIS_Y,
   TILT_DETECTION_AXIS_Z,
   TILT_DETECTION_AXIS_X_INVERTED,
   TILT_DETECTION_AXIS_Y_INVERTED,
   TILT_DETECTION_AXIS_Z_INVERTED,
   TILT_DETECTION_AXIS_MAX /**< Sentinel value */
} TILT_DETECTION_AXIS;

struct tilt_detection; // Forward declaration

/**
 * @brief Tilt detection interface
 */
typedef struct tilt_detection_interface tilt_detection_interface_t;
struct tilt_detection_interface
{
   struct tilt_detection *parent; /**< Pointer to the containing instance */

   /**
    * @brief Auto detect the axis that is most upright.
    *
    * @param[in] interface A pointer to the interface instance.
    * @param[out] axis A pointer to a TILT_DETECTION_AXIS that will be set to the detected axis
    *
    * @return result_t A status code indicating success or failure
    */
   result_t (*auto_detect_upright_axis)(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS *axis);

   /**
    * @brief Sets the axis that is most upright.
    *
    * @param[in,out] interface A pointer to the interface instance.
    * @param[in] axis The axis to set as upright
    *
    * @return result_t A status code indicating success or failure
    */
   result_t (*set_upright_axis)(tilt_detection_interface_t *interface, TILT_DETECTION_AXIS axis);

   /**
    * @brief Clears the current tilt detection state
    *
    * @param[in,out] interface A pointer to the interface instance.
    *
    * @return result_t A status code indicating success or failure.
    */
   result_t (*clear_tilts)(tilt_detection_interface_t *interface);

   /**
    * @brief Retrieves the given number of tilts from the fifo
    *
    * This function will get the data from IMU, analyze it and store it in the tilt buffer
    *
    * @param[in,out] interface A pointer to the interface instance.
    * @param[out] new_tilt_count Pointer to the memory allocated to receive the new tilt count
    * @param[out] tilts Pointer to the memory allocated to receive the tilts
    *
    * @return result_t A status code indicating success or failure.
    *
    * @note The tilts array must be at least TILT_DETECTION_MAX_TILTS in size
    */
   result_t (*get_tilts)(tilt_detection_interface_t *interface, uint8_t *new_tilt_count, tilt_data_t *tilts);

   /**
    * @brief Activates the tilt detection module
    *
    * @param[in,out] interface A pointer to the interface instance.
    *
    * @return result_t A status code indicating success or failure.
    *
    * @note Resets the tilt detection state.
    */
   result_t (*set_module_active)(tilt_detection_interface_t *interface);

   /**
    * @brief Deactivates the tilt detection module
    *
    * @param[in,out] interface A pointer to the interface instance.
    *
    * @return result_t A status code indicating success or failure.
    *
    * @note Resets the tilt detection state.
    */
   result_t (*set_module_dormant)(tilt_detection_interface_t *interface);
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // TILT_DETECTION_INTERFACE_H