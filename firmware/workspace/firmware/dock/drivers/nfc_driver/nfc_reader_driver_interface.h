/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NFC_READER_INTERFACE_H_
#define NFC_READER_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define NFC_POWER_LEVEL_LOW  (3u) // Required for reliable comms with Type V tag (Ring)
#define NFC_POWER_LEVEL_MED  (6u)
#define NFC_POWER_LEVEL_HIGH (12u) // High enough to charge ring battery at the required rate.

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct comms_driver; // Forward declaration
typedef struct nfc_driver_interface nfc_driver_interface_t;

typedef enum
{
   NFC_TAG_TYPE_V = 0,
   NFC_TAG_TYPE_A,
   NFC_TAG_TYPE_MAX
} NFC_TAG_TYPE;

typedef struct nfc_driver_interface
{
   struct comms_driver *parent; // Reference to the containing instance.

   /**
    * @brief Set the NFC reader RF output power level.
    *
    * Sets the transmitter output power for the NFC reader. The power level affects communication range and tag charging
    * rate. The value is device-specific (0 = minimum, 15 = maximum for ST25R3916B).
    *
    * @param interface Pointer to the NFC driver interface.
    * @param level Output power level to set (0-15).
    * @return RESULT_OK on success, or an error code on failure (e.g., out of range, uninitialized).
    */
   result_t (*set_output_power)(const nfc_driver_interface_t *const interface, uint8_t level);

   /**
    * @brief Get the current RF output power level.
    *
    * Retrieves the currently configured output power level for the NFC reader. The value is device-specific (0 =
    * minimum, 15 = maximum for ST25R3916B).
    *
    * @param interface Pointer to the NFC driver interface.
    * @param level Pointer to store the current output power level (0-15).
    * @return RESULT_OK on success, or an error code on failure (e.g., uninitialized, null pointer).
    */
   result_t (*get_output_power)(const nfc_driver_interface_t *const interface, uint8_t *level);

   /**
    * @brief Check if a ring (NFC tag) is present in the RF field.
    *
    * Returns true if a compatible NFC tag is currently detected and present in the field.
    *
    * @param interface Pointer to the NFC driver interface.
    * @param is_ring_present Pointer to store the presence status (true if present, false if absent).
    * @return RESULT_OK on success, or an error code on failure (e.g., uninitialized, null pointer).
    */
   result_t (*is_ring_present)(const nfc_driver_interface_t *const interface, bool *is_ring_present);

   /**
    * @brief Set the tag detection type (NFC-A or NFC-V).
    *
    * Configures the driver to search for a specific tag family (NFC-A/ISO14443A or NFC-V/ISO15693).
    * This may reset internal state and trigger a new discovery cycle.
    *
    * @param interface Pointer to the NFC driver interface.
    * @param tag_type Tag type to detect (NFC_TAG_TYPE_A or NFC_TAG_TYPE_V).
    * @param power_level Set the field strength (0-15)
    * @return RESULT_OK on success, or an error code on failure (e.g., out of range, uninitialized).
    */
   result_t (*set_detection_type)(const nfc_driver_interface_t *const interface,
                                  NFC_TAG_TYPE tag_type,
                                  uint8_t power_level);

   /**
    * @brief Process NFC driver state (tag detection, presence checks, etc).
    *
    * Advances the driver's state machine, performing tag detection, presence checks, and mailbox polling as needed.
    * Should be called periodically from the main loop or a timer.
    *
    * @param interface Pointer to the NFC driver interface.
    * @param enable_tag_detection If true, enables tag detection; if false, disables detection. Power state changes
    *                             (field off / chip low-power / wake-up mode) are controlled via the explicit power
    *                             control APIs on this interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*process)(const nfc_driver_interface_t *const interface, bool enable_tag_detection);

   /**
    * @brief Perform automatic antenna tuning (AAT).
    *
    * Runs the antenna tuning algorithm to optimize resonance for the current coil and environment.
    * This may improve communication reliability and range. This only needs to be done once for a given set of hardware
    * after which the output should be manually saved in the @file nfc_custom_analog_table.c.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*auto_tune_antenna)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Retrieve the UID of the currently detected NFC tag.
    *
    * If a tag is present, copies its UID (unique identifier) and length to the provided buffers.
    * The UID is only valid if a tag is currently detected. If no tag is present, an error is returned.
    *
    * @param interface Pointer to the NFC driver interface.
    * @param uid_buffer Buffer to store the UID (must be at least NFC_TAG_MAX_UID_SIZE bytes).
    * @param uid_length Pointer to store the UID length in bytes.
    * @return RESULT_OK on success, NFC_R_ERROR_NO_TAG if no tag is present, or an error code on failure.
    */
   result_t (*get_tag_uid)(const nfc_driver_interface_t *const interface, uint8_t *uid_buffer, uint8_t *uid_length);

   /**
    * @brief Turn off the RF field (disable Tx/Rx).
    *
    * Powers down only the RF field while keeping the NFC front-end powered and the oscillator running.
    * After calling this, tag presence state is no longer valid until the field is enabled again.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*field_off)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Turn on the RF field and start the RFAL guard time (GT).
    *
    * Enables the RF field and starts the guard time timer. Communication should only start after the guard time has
    * expired. This call does not perform tag discovery.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure (e.g., wrong state, uninitialized).
    */
   result_t (*field_on_and_start_gt)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Put the NFC front-end into chip power-down (RFAL low-power mode).
    *
    * Disables the ST25R oscillator/regulators and RF blocks. Use @ref chip_power_up to wake the chip again.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*chip_power_down)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Wake the NFC front-end from chip power-down (RFAL low-power mode).
    *
    * Re-enables the oscillator/regulators. After this call, the RF field is still off; call
    * @ref field_on_and_start_gt (and/or reconfigure discovery) before communicating.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*chip_power_up)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Start the ST25R wake-up mode (low-power environment / presence detection).
    *
    * Wake-up mode is a chip-level low-power state where the ST25R periodically performs measurements (using its
    * wake-up timer) and raises an interrupt when the environment deviates from its stored reference.
    *
    * While wake-up mode is enabled, the RF field is OFF and normal NFC communication is not possible.
    * To observe wake-up events, poll @ref wakeup_mode_has_woke() periodically (it is clear-on-read).
    *
    * To return to normal full-power operation after calling this:
    *  - Call @ref wakeup_mode_stop() to exit wake-up mode (oscillator back on).
    *  - Reconfigure the desired poller mode using @ref set_detection_type() (also turns the field on and starts GT),
    *    or call @ref field_on_and_start_gt() and then wait for @ref is_gt_expired() before communicating.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*wakeup_mode_start)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Stop the ST25R wake-up mode and return to normal operation (oscillator on).
    *
    * After this call, the RF field is still off; call @ref field_on_and_start_gt (and/or reconfigure discovery)
    * before communicating.
    *
    * @param interface Pointer to the NFC driver interface.
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*wakeup_mode_stop)(const nfc_driver_interface_t *const interface);

   /**
    * @brief Query whether wake-up mode has detected an environment change since the last query.
    *
    * This function is clear-on-read: once it returns true for a wake-up event, subsequent calls will return false
    * until another wake-up event occurs. Wake-up mode remains enabled and can report multiple wake-up events without
    * requiring a stop/start sequence.
    *
    * @param interface Pointer to the NFC driver interface.
    * @param has_woke Pointer to store the status (true if a wake event has been detected).
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*wakeup_mode_has_woke)(const nfc_driver_interface_t *const interface, bool *has_woke);

   /**
    * @brief Query whether the RFAL guard time (GT) has expired.
    *
    * After enabling the RF field with @ref field_on_and_start_gt (or after changing detection mode which also turns the
    * field on), communication should only start once the guard time has expired.
    *
    * @param interface Pointer to the NFC driver interface.
    * @param is_expired Pointer to store the guard-time status (true when GT has expired).
    * @return RESULT_OK on success, or an error code on failure.
    */
   result_t (*is_gt_expired)(const nfc_driver_interface_t *const interface, bool *is_expired);

} nfc_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // NFC_READER_INTERFACE_H_
