/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file nfc_tag_eeprom_queue_interface.h
 * @ingroup nfc_driver
 * @brief NFC tag EEPROM queue interface for ST25DV04K on the ring platform.
 *
 * This header defines the interface for queue operations on the EEPROM of the ST25DV NFC tag.
 * It provides function pointers for enqueue, dequeue, peek, and pop operations, as well as
 * methods to query queue state, abstracting hardware details for higher-level modules.
 *
 * Reference: See nfc_tag_eeprom_queue_interface.h for queue interface details.
 *
 * Limitations:
 *   - EEPROM size is limited to 512 bytes (ST25DV04K).
 *   - All operations are blocking and intended for use in non-interrupt context.
 */

#ifndef NFC_TAG_EEPROM_QUEUE_INTERFACE_H_
#define NFC_TAG_EEPROM_QUEUE_INTERFACE_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"
#include "debug.h"
#include "nfc_tag_driver_interface.h"
#include "result.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define EEPROM_BYTES NFC_TAG_MAX_EEPROM_SIZE

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct nfc_queue_status
{
   uint8_t percent_full;   /**< Percentage of the queue that is currently full (0-100). */
   size_t element_count;   /**< Number of elements currently stored in the queue. */
   size_t available_space; /**< Number of additional elements that can be stored in the queue. */
} nfc_queue_status_t;

struct nfc_tag_eeprom_queue; // Forward declaration
typedef struct nfc_tag_eeprom_queue_interface nfc_tag_eeprom_queue_interface_t;

typedef enum
{
   NFC_TAG_EEPROM_ERROR_NONE = 0,                /**< No error. */
   NFC_TAG_EEPROM_ERROR_PTR_NULL,                /**< Null pointer passed to function. */
   NFC_TAG_EEPROM_ERROR_INIT,                    /**< Initialization error. */
   NFC_TAG_EEPROM_ERROR_INVALID_ARG,             /**< Invalid argument provided. */
   NFC_TAG_EEPROM_ERROR_UNIT_UNINITIALIZED,      /**< Driver or hardware not initialized. */
   NFC_TAG_EEPROM_ERROR_INSUFFICIENT_SPACE,      /**< Not enough space in EEPROM to complete operation. */
   NFC_TAG_EEPROM_ERROR_INSUFFICIENT_ELEMENTS,   /**< Not enough elements in queue to complete operation. */
   NFC_TAG_EEPROM_ERROR_MISMATCHED_ELEMENT_SIZE, /**< Element size does not match initialized size. */
   NFC_TAG_EEPROM_ERROR_MISMATCHED_MAX_ELEMENTS, /**< Maximum elements does not match initialized size. */
   NFC_TAG_EEPROM_ERROR_WRITE_VERIFY_FAILED,     /**< Verification of written data failed. */
   NFC_TAG_EEPROM_ERROR_MISMATCHED_CONFIG,       /**< Configuration read from EEPROM does not match expected values. */
   NFC_TAG_EEPROM_ERROR_MISMATCHED_STATUS,       /**< Status read from EEPROM does not match expected values. */
   NFC_TAG_EEPROM_ERROR_MAX                      /**< Maximum error code value. */
} NFC_TAG_EEPROM_ERROR;

typedef struct nfc_tag_eeprom_queue_interface
{
   struct nfc_tag_eeprom_queue *parent; // Reference to the containing instance.

   /**
    * @brief Enqueue a single element into the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param element Pointer to the element to enqueue.
    * @return Result of the enqueue operation.
    */
   result_t (*nfc_enqueue)(nfc_tag_eeprom_queue_interface_t *interface, void *element);

   /**
    * @brief Enqueue multiple elements into the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param element Pointer to the first element to enqueue.
    * @param num_elements Number of elements to enqueue.
    * @return Result of the enqueue operation.
    */
   result_t (*nfc_enqueue_multiple)(nfc_tag_eeprom_queue_interface_t *interface, void *element, uint8_t num_elements);

   /**
    * @brief Dequeue a single element from the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param element Pointer to the buffer to store the dequeued element.
    * @return Result of the dequeue operation.
    */
   result_t (*nfc_dequeue)(nfc_tag_eeprom_queue_interface_t *interface, void *element);

   /**
    * @brief Dequeue multiple elements from the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param elements Pointer to the buffer to store the dequeued elements.
    * @param num_elements Number of elements to dequeue.
    * @return Result of the dequeue operation.
    */
   result_t (*nfc_dequeue_multiple)(nfc_tag_eeprom_queue_interface_t *interface, void *elements, uint8_t num_elements);

   /**
    * @brief Peek at the next element in the EEPROM queue without removing it.
    * @param interface Pointer to the queue interface.
    * @param element Pointer to the buffer to store the peeked element.
    * @return Result of the peek operation.
    */
   result_t (*nfc_peek)(nfc_tag_eeprom_queue_interface_t *interface, void *element);

   /**
    * @brief Peek at multiple elements in the EEPROM queue without removing them.
    * @param interface Pointer to the queue interface.
    * @param elements Pointer to the buffer to store the peeked elements.
    * @param num_elements Number of elements to peek.
    * @return Result of the peek operation.
    */
   result_t (*nfc_peek_multiple)(nfc_tag_eeprom_queue_interface_t *interface, void *elements, uint8_t num_elements);

   /**
    * @brief Remove (pop) the next element from the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @return Result of the pop operation.
    */
   result_t (*nfc_pop)(nfc_tag_eeprom_queue_interface_t *interface);

   /**
    * @brief Remove (pop) multiple elements from the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param num_elements Number of elements to pop.
    * @return Result of the pop operation.
    */
   result_t (*nfc_pop_multiple)(nfc_tag_eeprom_queue_interface_t *interface, uint8_t num_elements);

   /**
    * @brief Get the number of available spaces in the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param spaces_count Pointer to store the number of available spaces.
    * @return Result of the operation.
    */
   result_t (*nfc_get_available_spaces)(nfc_tag_eeprom_queue_interface_t *interface, uint8_t *spaces_count);

   /**
    * @brief Get the number of elements currently stored in the EEPROM queue.
    * @param interface Pointer to the queue interface.
    * @param element_count Pointer to store the number of elements.
    * @return Result of the operation.
    */
   result_t (*nfc_get_element_count)(nfc_tag_eeprom_queue_interface_t *interface, uint8_t *element_count);

   /**
    * @brief Reads the queue configuration from the NFC tag EEPROM.
    *
    * This function retrieves the maximum number of elements, the size of each element,
    * and the number of status slots from the NFC tag EEPROM queue configuration.
    *
    * @param interface Pointer to the NFC tag EEPROM queue interface instance.
    * @param max_elements Pointer to a uint8_t variable where the maximum number of elements will be stored.
    * @param element_size Pointer to a uint8_t variable where the element size will be stored.
    * @param status_slot_count Pointer to a uint8_t variable where the status slot count will be stored.
    * @return result_t Result of the read operation (e.g., success or error code).
    */
   result_t (*nfc_read_config)(nfc_tag_eeprom_queue_interface_t *interface,
                               uint8_t *max_elements,
                               uint8_t *element_size,
                               uint8_t *status_slot_count);

   /**
    * @brief Reads the total number of write operations performed on the NFC tag EEPROM.
    *
    * This function retrieves the total writes from the NFC tag EEPROM and stores it in the provided write_counter
    * pointer.
    *
    * @param interface Pointer to the NFC tag EEPROM queue interface structure.
    * @param write_counter Pointer to a uint32_t variable where the total write count will be stored.
    * @return result_t Result of the read operation.
    */
   result_t (*nfc_read_total_writes)(nfc_tag_eeprom_queue_interface_t *interface, uint32_t *write_counter);

   /**
    * @brief Get the size of the elements in the EEPROM queue.
    *
    * @param interface Pointer to the NFC tag EEPROM queue interface instance.
    * @param element_size Pointer to a size_t variable where the element size will be stored.
    * @return result_t Result of the operation (e.g., success or error code).
    */
   result_t (*nfc_get_element_size)(nfc_tag_eeprom_queue_interface_t *interface, size_t *element_size);

   /**
    * @brief Get the size of the elements in the EEPROM queue.
    *
    * @param interface Pointer to the NFC tag EEPROM queue interface instance.
    * @param element_size Pointer to a size_t variable where the element size will be stored.
    * @return result_t Result of the operation (e.g., success or error code).
    */
   result_t (*nfc_get_percent_full)(nfc_tag_eeprom_queue_interface_t *interface, uint8_t *percent_full);

   /**
    * @brief Put data into the EEPROM queue and update queue status.
    *
    * @param interface Pointer to the NFC tag EEPROM queue interface instance.
    * @param elements Pointer to the buffer containing elements to add. Cannot be NULL.
    * @param num_elements Number of elements to add.
    * @param status Pointer to a nfc_queue_status_t structure where the status will be stored. Can be NULL.
    * @return result_t Result of the operation (e.g., success or error code).
    */
   result_t (*nfc_put)(nfc_tag_eeprom_queue_interface_t *interface,
                       void *elements,
                       uint8_t num_elements,
                       nfc_queue_status_t *status);

} nfc_tag_eeprom_queue_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif /** NFC_TAG_EEPROM_QUEUE_INTERFACE_H_ */
