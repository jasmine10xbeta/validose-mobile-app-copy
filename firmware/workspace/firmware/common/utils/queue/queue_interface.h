/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file queue_interface.h
 * @ingroup queue
 * @brief The queue interface defines a minimum set of behaviors for a queue implementation.
 * - It allows for the use of any underlying queue implementation, while keeping the specifics of that implementation
 * unknown to the consumer.
 * - The concrete implementation stores a configurable number of elements, of a configurable fixed size.
 * - The queue works in a FIFO manner - elements are dequeued in the same order they were enqueued.
 */

#ifndef QUEUE_INTERFACE_H_
#define QUEUE_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "../../common.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct queue_status
{
   uint8_t percent_full;   /**< Percentage of the queue that is currently full (0-100). */
   size_t element_count;   /**< Number of elements currently stored in the queue. */
   size_t available_space; /**< Number of additional elements that can be stored in the queue. */
} queue_status_t;

struct queue;                                     // Forward declaration
typedef struct queue_interface queue_interface_t; // Forward declaration

/*
 * @struct queue_interface
 * @brief A structure exposing a generic interface for queue implementations.
 */
typedef struct queue_interface
{
   struct queue *parent; // Reference to the containing instance.

   /**
    * @brief Adds the @p element to the back of the queue.
    *
    * @param element Element to add.
    */
   result_t (*enqueue)(const queue_interface_t *const interface, const void *const element);

   /**
    * @brief Adds multiple elements to the back of the queue.
    *
    * @param elements Buffer containing elements to add. Cannot be NULL.
    * @param num_elements Number of elements to add.
    */
   result_t (*enqueue_multiple)(const queue_interface_t *const interface,
                                const void *const elements,
                                size_t num_elements);

   /**
    * @brief Removes the next element from the front of the queue.
    *
    * @param element Destination to store removed element. Cannot be NULL.
    */
   result_t (*dequeue)(const queue_interface_t *const interface, void *const element);

   /**
    * @brief Removes multiple elements from the front of the queue.
    *
    * @param elements Destination buffer to store removed elements. Cannot be NULL.
    * @param num_elements Number of elements to dequeue.
    */
   result_t (*dequeue_multiple)(const queue_interface_t *const interface, void *const elements, size_t num_elements);

   /**
    * @brief Copies the element at the front of the queue without removing it.
    *
    * @param element Destination to store peeked element. Cannot be NULL.
    */
   result_t (*peek)(const queue_interface_t *const interface, void *const element);

   /**
    * @brief Copies multiple elements from the front of the queue without removing them.
    *
    * @param elements Destination buffer to store the peeked elements. Cannot be NULL.
    * @param num_elements Number of elements to peek.
    */
   result_t (*peek_multiple)(const queue_interface_t *const interface, void *const elements, size_t num_elements);

   /**
    * @brief Removes the element at the front of the queue without returning it.
    */
   result_t (*pop)(const queue_interface_t *const interface);

   /**
    * @brief Removes multiple elements from the front of the queue without returning them.
    *
    * @param num_elements Number of elements to pop.
    */
   result_t (*pop_multiple)(const queue_interface_t *const interface, size_t num_elements);

   /**
    * @brief Removes all elements in the queue.
    *
    * @note Resets the counters and trivially zeroes the data
    */
   result_t (*flush)(const queue_interface_t *const interface);

   /**
    * @brief Queries the number of items in the queue.
    *
    * @param count Destination to store the number of items in the queue.
    */
   result_t (*get_count)(const queue_interface_t *const interface, size_t *const count);

   /**
    * @brief Queries the available space in the queue in terms of the number of elements that can still be stored.
    *
    * @param available_space Destination to store the available space in the queue.
    */
   result_t (*get_available_space)(const queue_interface_t *const interface, size_t *const available_space);

   /**
    * @brief Queries the size of the elements stored in the queue.
    *
    * @param element_size Destination to store the size of each element in the queue.
    */
   result_t (*get_element_size)(const queue_interface_t *const interface, size_t *const element_size);

   /**
    * @brief Adds multiple elements to the back of the queue and updates the queue status.
    *
    * @param elements Buffer containing elements to add. Cannot be NULL.
    * @param num_elements Number of elements to add.
    * @param status Pointer to a queue_status_t structure where the status will be stored. Can be NULL.
    */
   result_t (*put)(const queue_interface_t *const interface,
                   const void *const elements,
                   size_t num_elements,
                   queue_status_t *status);

} queue_interface_t;
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif /* QUEUE_INTERFACE_H_ */
