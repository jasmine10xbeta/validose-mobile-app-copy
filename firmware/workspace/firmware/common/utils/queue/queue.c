/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file queue.c
 * @ingroup queue
 * @brief
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

// Standard includes
#include "nrf_mtx.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Custom includes
#include "../../common.h"
#include "queue.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_QUEUE;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t enqueue(const queue_interface_t *const interface, const void *const element);
static result_t
   enqueue_multiple(const queue_interface_t *const interface, const void *const elements, size_t num_elements);
static result_t put(const queue_interface_t *const interface,
                    const void *const elements,
                    size_t num_elements,
                    queue_status_t *status);
static result_t dequeue(const queue_interface_t *const interface, void *const element);
static result_t dequeue_multiple(const queue_interface_t *const interface, void *const elements, size_t num_elements);
static result_t pop(const queue_interface_t *const interface);
static result_t pop_multiple(const queue_interface_t *const interface, size_t num_elements);
static result_t peek(const queue_interface_t *const interface, void *const element);
static result_t peek_multiple(const queue_interface_t *const interface, void *const elements, size_t num_elements);
static result_t flush(const queue_interface_t *const interface);
static result_t get_count(const queue_interface_t *const interface, size_t *const count);
static result_t get_available_space(const queue_interface_t *const interface, size_t *const available_space);
static result_t get_element_size(const queue_interface_t *const interface, size_t *const element_size);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t enqueue(const queue_interface_t *const interface, const void *const element)
{
   return enqueue_multiple(interface, element, 1u);
}

// Enqueue multiple elements to the back of the queue and update the status of the queue
// If the pointer to the status is NULL then the status is not updated
static result_t put(const queue_interface_t *const interface,
                    const void *const elements,
                    size_t num_elements,
                    queue_status_t *status)
{
   result_t result = enqueue_multiple(interface, elements, num_elements);

   if(status != NULL)
   {
      status->available_space = interface->parent->_total_elements - interface->parent->_element_count;
      status->element_count = interface->parent->_element_count;
      status->percent_full = (uint8_t)((interface->parent->_element_count * 100u) / interface->parent->_total_elements);
   }
   return result;
}

// Enqueue multiple elements to the back of the queue
static result_t
   enqueue_multiple(const queue_interface_t *const interface, const void *const elements, size_t num_elements)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(elements, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == num_elements, QUEUE_ERROR_INVALID_ARG);

   // Attempt to lock mutex - return immediately if busy
   RETURN_ERR_IF_TRUE(!nrf_mtx_trylock(&(interface->parent->_queue_mutex)), QUEUE_BUSY);

   result_t result = RESULT_OK;
   queue_t *self = interface->parent;

   // Indicate if the queue has insufficient space to enqueue all the elements
   if((self->_total_elements - self->_element_count) < num_elements)
   {
      SET_ERR(result, QUEUE_FULL);
   }

   if(IS_OK(result))
   {
      size_t idx = self->_queue_back;
      const uint8_t *read_addr = (const uint8_t *)elements;
      for(size_t element_count = 0u; element_count < num_elements; element_count++)
      {
         void *write_addr = (void *)((size_t)self->_storage + idx * self->_element_size);
         memmove(write_addr, read_addr, self->_element_size);
         read_addr += self->_element_size;
         idx = (idx + 1u) % self->_total_elements;
      }

      self->_element_count += num_elements;
      self->_queue_back = (self->_queue_back + num_elements) % self->_total_elements;
   }

   nrf_mtx_unlock(&(self->_queue_mutex));
   return result;
}

static result_t dequeue(const queue_interface_t *const interface, void *const element)
{
   return dequeue_multiple(interface, element, 1u);
}

// Dequeue multiple elements from the front of the queue
static result_t dequeue_multiple(const queue_interface_t *const interface, void *const elements, size_t num_elements)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(elements, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == num_elements, QUEUE_ERROR_INVALID_ARG);

   // Attempt to lock mutex - return immediately if busy
   RETURN_ERR_IF_TRUE(!nrf_mtx_trylock(&(interface->parent->_queue_mutex)), QUEUE_BUSY);

   result_t result = RESULT_OK;
   queue_t *self = interface->parent;

   // Indicate if the queue is empty
   if(0u == self->_element_count)
   {
      SET_ERR(result, QUEUE_EMPTY);
   }

   // Indicate if the queue has insufficient elements to dequeue
   if(self->_element_count < num_elements && IS_OK(result))
   {
      SET_ERR(result, QUEUE_INSUFFICIENT_ELEMENTS);
   }

   if(IS_OK(result))
   {
      size_t idx = self->_queue_front;
      uint8_t *write_addr = (uint8_t *)elements;
      for(size_t element_count = 0u; element_count < num_elements; element_count++)
      {
         const void *read_addr = (void *)((size_t)self->_storage + idx * self->_element_size);
         memmove(write_addr, read_addr, self->_element_size);
         write_addr += self->_element_size;
         idx = (idx + 1u) % self->_total_elements;
      }
      self->_element_count -= num_elements;
      self->_queue_front = (self->_queue_front + num_elements) % self->_total_elements;
   }

   nrf_mtx_unlock(&(self->_queue_mutex));
   return result;
}

static result_t peek(const queue_interface_t *const interface, void *const element)
{
   return peek_multiple(interface, element, 1u);
}

// Peek multiple elements from the front of the queue without removing them
static result_t peek_multiple(const queue_interface_t *const interface, void *const elements, size_t num_elements)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(elements, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == num_elements, QUEUE_ERROR_INVALID_ARG);

   // Attempt to lock mutex - return immediately if busy
   RETURN_ERR_IF_TRUE(!nrf_mtx_trylock(&(interface->parent->_queue_mutex)), QUEUE_BUSY);

   queue_t *self = interface->parent;
   result_t result = RESULT_OK;

   // Indicate if the queue is empty
   if(0u == self->_element_count)
   {
      SET_ERR(result, QUEUE_EMPTY);
   }

   // Indicate if the queue has insufficient elements to peek
   if((self->_element_count < num_elements) && IS_OK(result))
   {
      SET_ERR(result, QUEUE_INSUFFICIENT_ELEMENTS);
   }

   if(IS_OK(result))
   {
      size_t idx = self->_queue_front;

      uint8_t *write_addr = (uint8_t *)elements;
      for(size_t element_count = 0u; element_count < num_elements; element_count++)
      {
         const void *read_addr = (void *)((size_t)self->_storage + idx * self->_element_size);
         memmove(write_addr, read_addr, self->_element_size);
         write_addr += self->_element_size;
         idx = (idx + 1u) % self->_total_elements;
      }
   }

   nrf_mtx_unlock(&(self->_queue_mutex));
   return result;
}

static result_t flush(const queue_interface_t *const interface)
{
   // Null checks
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);

   // Attempt to lock mutex - return immediately if busy
   RETURN_ERR_IF_TRUE(!nrf_mtx_trylock(&(interface->parent->_queue_mutex)), QUEUE_BUSY);

   // Reset queue
   memset(interface->parent->_storage, 0, interface->parent->_storage_size);
   interface->parent->_element_count = 0;
   interface->parent->_queue_front = 0;
   interface->parent->_queue_back = 0;

   // Release lock
   nrf_mtx_unlock(&(interface->parent->_queue_mutex));

   return RESULT_OK;
}

static result_t pop(const queue_interface_t *const interface)
{
   return pop_multiple(interface, 1u);
}

// Remove multiple elements from the front of the queue without returning them
static result_t pop_multiple(const queue_interface_t *const interface, size_t num_elements)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0u == num_elements, QUEUE_ERROR_INVALID_ARG);

   // Attempt to lock mutex - return immediately if busy
   RETURN_ERR_IF_TRUE(!nrf_mtx_trylock(&(interface->parent->_queue_mutex)), QUEUE_BUSY);

   queue_t *self = interface->parent;
   result_t result = RESULT_OK;

   // Indicate if the queue is empty
   if(0u == self->_element_count)
   {
      SET_ERR(result, QUEUE_EMPTY);
   }

   // Indicate if the queue has insufficient elements to pop
   if((self->_element_count < num_elements) && IS_OK(result))
   {
      SET_ERR(result, QUEUE_INSUFFICIENT_ELEMENTS);
   }

   if(IS_OK(result))
   {
      self->_element_count -= num_elements;
      self->_queue_front = (self->_queue_front + num_elements) % self->_total_elements;
   }

   nrf_mtx_unlock(&(self->_queue_mutex));
   return result;
}

static result_t get_count(const queue_interface_t *const interface, size_t *const count)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(count, QUEUE_ERROR_PTR_NULL);

   *count = interface->parent->_element_count;

   return RESULT_OK;
}

static result_t get_available_space(const queue_interface_t *const interface, size_t *const available_space)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(available_space, QUEUE_ERROR_PTR_NULL);

   *available_space = interface->parent->_total_elements - interface->parent->_element_count;

   return RESULT_OK;
}

static result_t get_element_size(const queue_interface_t *const interface, size_t *const element_size)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(element_size, QUEUE_ERROR_PTR_NULL);

   *element_size = interface->parent->_element_size;

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t queue_init(queue_t *const self, uint8_t *const storage, size_t storage_size, size_t element_size)
{
   result_t result = RESULT_OK;

   RETURN_ERR_IF_NULL(self, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_NULL(storage, QUEUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_TRUE(0 == storage_size, QUEUE_ERROR_INIT_STORAGE_SIZE);
   RETURN_ERR_IF_TRUE(0 == element_size, QUEUE_ERROR_INIT_STORAGE_SIZE);
   RETURN_ERR_IF_TRUE((storage_size % element_size) != 0, QUEUE_ERROR_INIT_STORAGE_SIZE);

   self->_initialized = false;
   self->interface.parent = self;

   self->_storage = storage;
   self->_storage_size = storage_size;
   self->_element_size = element_size;
   self->_total_elements = storage_size / element_size;
   self->_element_count = 0;
   self->_queue_front = 0;
   self->_queue_back = 0;

   // Initialize all interface pointers to point to internal static functions by default
   self->interface.enqueue = enqueue;
   self->interface.enqueue_multiple = enqueue_multiple;
   self->interface.dequeue = dequeue;
   self->interface.dequeue_multiple = dequeue_multiple;
   self->interface.peek = peek;
   self->interface.peek_multiple = peek_multiple;
   self->interface.pop = pop;
   self->interface.pop_multiple = pop_multiple;
   self->interface.flush = flush;
   self->interface.get_count = get_count;
   self->interface.get_available_space = get_available_space;
   self->interface.get_element_size = get_element_size;
   self->interface.put = put;

   nrf_mtx_init(&(self->_queue_mutex));

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
