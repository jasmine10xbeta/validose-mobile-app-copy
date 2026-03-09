/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file nfc_tag_eeprom_queue.h
 * @ingroup nfc_driver
 * @brief NFC tag EEPROM queue interface for ST25DV04K on the ring platform.
 *
 * This header defines the interface and structure for managing a queue of EEPROM operations
 * for the ST25DV NFC tag. It provides initialization and encapsulation of queue state,
 * abstracting hardware details for higher-level modules on the ring device.
 *
 * Usage:
 *   - Use nfc_tag_eeprom_queue_init() to initialize the queue with the desired element size and NFC interface.
 *
 * Limitations:
 *   - EEPROM size is limited to 512 bytes (ST25DV04K).
 *   - All operations are blocking and intended for use in non-interrupt context.
 */

#ifndef NFC_TAG_EEPROM_QUEUE_H_
#define NFC_TAG_EEPROM_QUEUE_H_

#include "nfc_tag_driver_interface.h"
#include "nfc_tag_eeprom_queue_interface.h"

#include "result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct nfc_tag_eeprom_queue_interface nfc_tag_eeprom_queue_interface_t; // forward declaration

/**
 * @brief Structure representing an NFC tag EEPROM queue.
 *
 * This structure encapsulates the interface and state required to manage
 * a queue for NFC tag EEPROM operations.
 */
typedef struct nfc_tag_eeprom_queue
{
   nfc_tag_eeprom_queue_interface_t interface; /**< Interface for the EEPROM queue operations. */
   nfc_tag_driver_interface_t *nfc_interface;  /**< Pointer to the NFC tag driver interface. */

   bool _initialized; /**< Indicates whether the queue has been initialized. */
} nfc_tag_eeprom_queue_t;

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initializes the NFC tag EEPROM queue.
 *
 * Sets up the EEPROM queue for NFC tag operations, configuring the queue with the specified
 * element size, number of status slots, and associating it with the provided NFC tag driver interface.
 * Optionally initializes the underlying EEPROM memory.
 *
 * @param self               Pointer to the nfc_tag_eeprom_queue_t instance to initialize.
 * @param element_size       Size (in bytes) of each element to be stored in the queue.
 * @param status_slot_count  Number of status slots used for wear leveling.
 * @param nfc_interface      Pointer to the NFC tag driver interface to be used by the queue.
 * @param init_memory        If true, initializes the EEPROM memory for the queue.
 *
 * @return result_t          Result of the initialization operation.
 */
result_t nfc_tag_eeprom_queue_init(nfc_tag_eeprom_queue_t *const self,
                                   uint8_t element_size,
                                   uint8_t status_slot_count,
                                   nfc_tag_driver_interface_t *nfc_interface,
                                   bool init_memory);
#endif /* NFC_TAG_EEPROM_QUEUE_H_ */
