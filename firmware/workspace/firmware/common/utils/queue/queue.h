/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup queue Generic Queue
 * @ingroup common
 * @brief Queue implements a basic queue data structure. It stores fixed-size elements in a provided static memory
 * buffer.
 * @details
 *
 * @file queue.h
 * @ingroup queue
 * @brief
 */

#ifndef UTILS_DATA_STRUCTURES_QUEUE_QUEUE_H_
#define UTILS_DATA_STRUCTURES_QUEUE_QUEUE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "nrf_mtx.h"
// Custom includes
#include "queue_interface.h"

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
   QUEUE_ERROR_NONE = 0,
   QUEUE_ERROR_INVALID_ARG,
   QUEUE_ERROR_PTR_NULL,
   QUEUE_ERROR_INIT_GENERAL,
   QUEUE_ERROR_INIT_STORAGE_SIZE,
   QUEUE_BUSY,
   QUEUE_FULL,
   QUEUE_EMPTY,
   QUEUE_INSUFFICIENT_ELEMENTS,
   QUEUE_ERROR_MAX,
} QUEUE_ERROR;

typedef struct queue
{
   queue_interface_t interface;

   uint8_t *_storage;      /**<  Memory in which elements are stored. */
   size_t _storage_size;   /**<  Total size of storage in bytes. */
   size_t _element_size;   /**<  Size of elements stored in the queue. */
   size_t _total_elements; /**<  Total number of elements that can be stored in the provided storage. */
   size_t _element_count;  /**<  Number of elements in queue. */
   size_t _queue_front;    /**<  Index of front of the queue. */
   size_t _queue_back;     /**<  Index of back of the queue. */

   nrf_mtx_t _queue_mutex;

   bool _initialized;
} queue_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
/**
 * @brief Initializes a @p queue_t instance
 *
 * @param self Pointer to the @p queue_t instance to initialize.
 * @param storage Memory in which to store elements.
 * @param storage_size Size of storage in bytes.
 * @param element_size Size of elements held by the queue.
 */
result_t queue_init(queue_t *const self, uint8_t *const storage, size_t storage_size, size_t element_size);
#endif /* UTILS_DATA_STRUCTURES_QUEUE_QUEUE_H_ */
