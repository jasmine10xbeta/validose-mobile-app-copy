/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file nfc_tag_eeprom_queue.c
 * @ingroup nfc_driver
 * @brief NFC tag EEPROM queue implementation for ST25DV04K on the ring platform.
 *
 * This source file implements a circular queue stored in the EEPROM of the ST25DV NFC tag.
 * It provides wear leveling for queue status, supports enqueue, dequeue, peek, and pop operations,
 * and ensures data integrity by updating status only after successful data writes/reads.
 *
 * Usage:
 *   - Use nfc_tag_eeprom_queue_init() to initialize the queue.
 *   - Use the interface functions to enqueue, dequeue, peek, and pop elements.
 *
 * Limitations:
 *   - EEPROM size is limited to 512 bytes (ST25DV04K).
 *   - All operations are blocking and intended for use in non-interrupt context.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "nfc_tag_eeprom_queue.h"
#include "nfc_tag_driver_interface.h"

#include <assert.h>
#include <string.h>

#include "app_util.h"
#include "nrf_delay.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_NFC_TAG_EEPROM_QUEUE;

/***********************************************************************************************************************
 * Memory Map
 **********************************************************************************************************************/
/**
 * 0  |---------  RESERVED  NFC READABLE  --------|
 * 4  |---------  RESERVED  NFC READABLE  --------|
 * 8  |---------  RESERVED  NFC READABLE  --------|
 * 12 |---------  RESERVED  NFC READABLE  --------|
 * 16 |---------  RESERVED  NFC READABLE  --------|
 * 20 |---------  RESERVED  NFC READABLE  --------|
 * 24 |---------  RESERVED  NFC READABLE  --------|
 * 28 |---------  RESERVED  NFC READABLE  --------|
 * 32 |--BYTE 1--|--BYTE 2--|--BYTE 3--|--BYTE 4--| - Queue Config
 * 36 |-----------------UPDATE COUNTER------------| - Update Counter
 * 40 |--BYTE 1--|--BYTE 2--|--BYTE 3--|--BYTE 4--| - Status Record 1
 * 44 |--BYTE 1--|--BYTE 2--|--BYTE 3--|--BYTE 4--| - Status Record 2
 * 48 |--BYTE 1--|--BYTE 2--|--BYTE 3--|--BYTE 4--| - Status Record 3
 * 52 |--BYTE 1--|--BYTE 2--|--BYTE 3--|--BYTE 4--| - Status Record 4
 * 56 |-------------------------------------------| - Data Start Address
 * 60 |-------------------------------------------|
 * 64 |-------------------------------------------|
 */

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// Memory mapping
#define RESERVED_START_BYTES      (uint16_t)(32u) // Size of reserved area at start of EEPROM
#define QUEUE_CONFIG_SIZE_BYTES   (uint16_t)(4u)  // Size of the queue configuration struct
#define UPDATE_COUNTER_SIZE_BYTES (uint16_t)(4u)  // Size of the update counter struct
#define STATUS_SIZE_BYTES         (uint16_t)(4u)  // Size of the status struct
#define STATUS_COUNT              (uint16_t)(4u)  // Number of status sections stored in EEPROM

#define CONFIG_START_ADDRESS   (RESERVED_START_BYTES)
#define UPDATE_COUNTER_ADDRESS (CONFIG_START_ADDRESS + QUEUE_CONFIG_SIZE_BYTES)
#define STATUS_START_ADDRESS   (UPDATE_COUNTER_ADDRESS + UPDATE_COUNTER_SIZE_BYTES)
#define DATA_START_ADDRESS     (STATUS_START_ADDRESS + STATUS_SIZE_BYTES * STATUS_COUNT)
#define DATA_BYTES             (EEPROM_BYTES - DATA_START_ADDRESS)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct __attribute__((packed, aligned(1)))
{
   uint8_t max_elements;      /*>  Maximum number of elements that can be stored in the queue */
   uint8_t element_size;      /*>  Size of each element in bytes */
   uint8_t status_slot_count; /*>  Number of status slots used for wear leveling */
   uint8_t reserved;          /*>  Reserved for future use */
} queue_config_t;

STATIC_ASSERT(sizeof(queue_config_t) == QUEUE_CONFIG_SIZE_BYTES,
              "Sizeof queue_config_t must match QUEUE_CONFIG_SIZE_BYTES");

// Struct to contain the status of the NFC tag EEPROM queue
// Uint8_t used to minimize access and verification times
typedef struct __attribute__((packed, aligned(1)))
{
   uint8_t element_count; /*>  Number of elements in queue */
   uint8_t queue_front;   /*>  Index of front of the queue */
   uint8_t queue_back;    /*>  Index of back of the queue */
   uint8_t counter;       /*>  Counter for rollover detection */
} nfc_internal_queue_status_t;

STATIC_ASSERT(sizeof(nfc_internal_queue_status_t) == STATUS_SIZE_BYTES,
              "Sizeof nfc_internal_queue_status_t must match STATUS_SIZE_BYTES");

/** Struct to store the number of status updates that have occurred.
 *
 * Only updated when the status counter rolls over.
 * This means that this value may be out of sync with the actual number of updates and the total writes will need to be
 * determined from both this value and the status counter.
 * Estimated write endurance for this section of EEPROM is therefore approximatly 512 000 000 updates for the queue on
 * the ST25DV04K.
 */
typedef struct __attribute__((packed, aligned(1)))
{
   uint32_t
      update_counter; /*>  Monotonically counter of the number of updates. Only updated on status counter rollover. */
} update_counter_t;

STATIC_ASSERT(sizeof(update_counter_t) == UPDATE_COUNTER_SIZE_BYTES,
              "Sizeof update_counter_t must match UPDATE_COUNTER_SIZE_BYTES");

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
static result_t nfc_tag_eeprom_queue_enqueue(nfc_tag_eeprom_queue_interface_t *interface, void *element);
static result_t nfc_tag_eeprom_queue_enqueue_multiple(nfc_tag_eeprom_queue_interface_t *interface,
                                                      void *element,
                                                      uint8_t num_elements);
static result_t nfc_tag_eeprom_queue_dequeue(nfc_tag_eeprom_queue_interface_t *interface, void *element);
static result_t nfc_tag_eeprom_queue_dequeue_multiple(nfc_tag_eeprom_queue_interface_t *interface,
                                                      void *elements,
                                                      uint8_t num_elements);
static result_t nfc_tag_eeprom_queue_peek(nfc_tag_eeprom_queue_interface_t *interface, void *element);
static result_t nfc_tag_eeprom_queue_peek_multiple(nfc_tag_eeprom_queue_interface_t *interface,
                                                   void *elements,
                                                   uint8_t num_elements);
static result_t nfc_tag_eeprom_queue_pop(nfc_tag_eeprom_queue_interface_t *interface);
static result_t nfc_tag_eeprom_queue_pop_multiple(nfc_tag_eeprom_queue_interface_t *interface, uint8_t num_elements);
static result_t nfc_tag_eeprom_queue_get_available_spaces(nfc_tag_eeprom_queue_interface_t *interface,
                                                          uint8_t *spaces_count);
static result_t nfc_tag_eeprom_queue_get_element_count(nfc_tag_eeprom_queue_interface_t *interface,
                                                       uint8_t *element_count);
static result_t nfc_tag_read_config(nfc_tag_eeprom_queue_interface_t *interface,
                                    uint8_t *max_elements,
                                    uint8_t *element_size,
                                    uint8_t *status_slot_count);
static result_t nfc_tag_read_total_writes(nfc_tag_eeprom_queue_interface_t *interface, uint32_t *write_counter);
static result_t nfc_tag_get_element_size(nfc_tag_eeprom_queue_interface_t *interface, size_t *element_size);
static result_t nfc_tag_get_percent_full(nfc_tag_eeprom_queue_interface_t *interface, uint8_t *percent_full);
static result_t nfc_put(nfc_tag_eeprom_queue_interface_t *interface,
                        void *elements,
                        uint8_t num_elements,
                        nfc_queue_status_t *status);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t nfc_put(nfc_tag_eeprom_queue_interface_t *interface,
                        void *elements,
                        uint8_t num_elements,
                        nfc_queue_status_t *status)
{
   // result_t result = nfc_tag_eeprom_queue_enqueue_multiple(
   //    (nfc_tag_eeprom_queue_interface_t *)interface, (void *)elements, num_elements);

   // if(IS_OK(result) && (status != NULL))
   // {
   //    status->element_count = m_current_status.element_count;
   //    status->available_space = m_current_config.max_elements - m_current_status.element_count;
   //    status->percent_full = (uint8_t)((m_current_status.element_count * 100u) / m_current_config.max_elements);
   // }
   // return result;
   return RESULT_OK;
}

static result_t nfc_tag_get_percent_full(nfc_tag_eeprom_queue_interface_t *interface, uint8_t *percent_full)
{
   // RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(percent_full, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_TRUE(0u == m_current_config.max_elements, NFC_TAG_EEPROM_ERROR_INVALID_ARG); // Check division by
   // zero

   // *percent_full = (uint8_t)((m_current_status.element_count * 100u) / m_current_config.max_elements);

   return RESULT_OK;
}

static result_t nfc_tag_eeprom_queue_enqueue(nfc_tag_eeprom_queue_interface_t *interface, void *element)
{
   return nfc_tag_eeprom_queue_enqueue_multiple(interface, element, 1u);
}

static result_t nfc_tag_eeprom_queue_enqueue_multiple(nfc_tag_eeprom_queue_interface_t *interface,
                                                      void *element,
                                                      uint8_t num_elements)
{
   // RETURN_ERR_IF_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(element, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_TRUE(0u == num_elements, NFC_TAG_EEPROM_ERROR_INVALID_ARG);

   // nfc_tag_eeprom_queue_t *self = interface->parent;
   // result_t result = RESULT_OK;

   // // Check available space
   // UPDATE_ERR_IF_TRUE(result,
   //                    (m_current_config.max_elements - m_current_status.element_count) < num_elements,
   //                    NFC_TAG_EEPROM_ERROR_INSUFFICIENT_SPACE);

   // if(IS_OK(result))
   // {
   //    // Calculate write position
   //    uint8_t write_index = m_current_status.queue_back;
   //    uint16_t eeprom_address = (uint16_t)(DATA_START_ADDRESS + (write_index * m_current_config.element_size));

   //    // Write elements to EEPROM (handle wrap-around)
   //    uint8_t *src = (uint8_t *)element;
   //    for(uint8_t i = 0u; i < num_elements && IS_OK(result); i++)
   //    {
   //       result = self->nfc_interface->write_eeprom_blocking(self->nfc_interface,
   //                                                           eeprom_address,
   //                                                           src + (i * m_current_config.element_size),
   //                                                           (uint16_t)m_current_config.element_size);

   //       write_index = (uint8_t)((write_index + 1u) % m_current_config.max_elements);
   //       eeprom_address = (uint16_t)(DATA_START_ADDRESS + (write_index * m_current_config.element_size));
   //    }

   //    // Only update status if all writes were successful
   //    // This prevents corruption of the queue state in case of partial write failures
   //    // The whole write operation can be retried later - More simple than attempting to recover from partial writes
   //    if(IS_OK(result))
   //    {
   //       // Update status
   //       m_current_status.element_count += num_elements;
   //       m_current_status.queue_back = write_index;
   //       result = update_status(interface, &m_current_status);
   //    }
   // }
   // return result;
   return RESULT_OK;
}

static result_t nfc_tag_eeprom_queue_dequeue(nfc_tag_eeprom_queue_interface_t *interface, void *element)
{
   return nfc_tag_eeprom_queue_dequeue_multiple(interface, element, 1u);
}

static result_t nfc_tag_eeprom_queue_dequeue_multiple(nfc_tag_eeprom_queue_interface_t *interface,
                                                      void *elements,
                                                      uint8_t num_elements)
{
   // RETURN_ERR_IF_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(elements, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_TRUE(0u == num_elements, NFC_TAG_EEPROM_ERROR_INVALID_ARG);

   // result_t result = RESULT_OK;
   // nfc_tag_eeprom_queue_t *self = interface->parent;

   // // Check if enough elements are available
   // UPDATE_ERR_IF_TRUE(
   //    result, m_current_status.element_count < num_elements, NFC_TAG_EEPROM_ERROR_INSUFFICIENT_ELEMENTS);

   // if(IS_OK(result))
   // {
   //    // Calculate read position
   //    uint8_t read_index = m_current_status.queue_front;
   //    uint16_t eeprom_address = (uint16_t)(DATA_START_ADDRESS + (read_index * m_current_config.element_size));

   //    // Read elements from EEPROM (handle wrap-around)
   //    uint8_t *dst = (uint8_t *)elements;
   //    for(uint8_t i = 0; i < num_elements && IS_OK(result); i++)
   //    {
   //       result = self->nfc_interface->read_eeprom_blocking(self->nfc_interface,
   //                                                          eeprom_address,
   //                                                          dst + (i * m_current_config.element_size),
   //                                                          (uint16_t)m_current_config.element_size);

   //       read_index = (uint8_t)((read_index + 1u) % m_current_config.max_elements);
   //       eeprom_address = (uint16_t)(DATA_START_ADDRESS + (read_index * m_current_config.element_size));
   //    }

   //    // Only update status if all reads were successful
   //    // This prevents corruption of the queue state in case of partial read failures
   //    // The whole read operation can be retried later
   //    if(IS_OK(result))
   //    {
   //       m_current_status.element_count -= num_elements;
   //       m_current_status.queue_front = read_index;
   //       result = update_status(interface, &m_current_status);
   //    }
   // }
   // return result;
   return RESULT_OK;
}

static result_t nfc_tag_eeprom_queue_pop(nfc_tag_eeprom_queue_interface_t *interface)
{
   return nfc_tag_eeprom_queue_pop_multiple(interface, 1u);
}

static result_t nfc_tag_eeprom_queue_pop_multiple(nfc_tag_eeprom_queue_interface_t *interface, uint8_t num_elements)
{
   // RETURN_ERR_IF_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_TRUE(0u == num_elements, NFC_TAG_EEPROM_ERROR_INVALID_ARG);

   // result_t result = RESULT_OK;

   // // Check if enough elements are available
   // UPDATE_ERR_IF_TRUE(
   //    result, m_current_status.element_count < num_elements, NFC_TAG_EEPROM_ERROR_INSUFFICIENT_ELEMENTS);

   // if(IS_OK(result))
   // {
   //    // Update status only (no data read)
   //    m_current_status.element_count -= num_elements;
   //    m_current_status.queue_front
   //       = (uint8_t)(m_current_status.queue_front + num_elements) % m_current_config.max_elements;
   //    result = update_status(interface, &m_current_status);
   // }
   // return result;
   return RESULT_OK;
}

static result_t nfc_tag_eeprom_queue_peek(nfc_tag_eeprom_queue_interface_t *interface, void *element)
{
   return nfc_tag_eeprom_queue_peek_multiple(interface, element, 1u);
}

static result_t
   nfc_tag_eeprom_queue_peek_multiple(nfc_tag_eeprom_queue_interface_t *interface, void *elements, uint8_t num_elements)
{
   // RETURN_ERR_IF_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(elements, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_TRUE(0u == num_elements, NFC_TAG_EEPROM_ERROR_INVALID_ARG);

   // result_t result = RESULT_OK;
   // nfc_tag_eeprom_queue_t *self = interface->parent;

   // UPDATE_ERR_IF_TRUE(
   //    result, m_current_status.element_count < num_elements, NFC_TAG_EEPROM_ERROR_INSUFFICIENT_ELEMENTS);

   // if(IS_OK(result))
   // {
   //    // Calculate read position
   //    uint8_t read_index = m_current_status.queue_front;
   //    uint16_t eeprom_address = (uint16_t)(DATA_START_ADDRESS + (read_index * m_current_config.element_size));

   //    // Read elements from EEPROM (handle wrap-around)
   //    uint8_t *dst = (uint8_t *)elements;
   //    for(size_t i = 0; i < num_elements && IS_OK(result); i++)
   //    {
   //       result = self->nfc_interface->read_eeprom_blocking(self->nfc_interface,
   //                                                          eeprom_address,
   //                                                          dst + (i * m_current_config.element_size),
   //                                                          (uint16_t)m_current_config.element_size);

   //       read_index = (uint8_t)(read_index + 1u) % m_current_config.max_elements;
   //       eeprom_address = (uint16_t)(DATA_START_ADDRESS + (read_index * m_current_config.element_size));
   //    }
   // }
   // return result;
   return RESULT_OK;
}

// Utility functions
static result_t nfc_tag_eeprom_queue_get_element_count(nfc_tag_eeprom_queue_interface_t *interface,
                                                       uint8_t *element_count)
{
   // RETURN_ERR_IF_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_NULL(element_count, NFC_TAG_EEPROM_ERROR_PTR_NULL);

   // *element_count = m_current_status.element_count;

   return RESULT_OK;
}

static result_t nfc_tag_eeprom_queue_get_available_spaces(nfc_tag_eeprom_queue_interface_t *interface,
                                                          uint8_t *spaces_count)
{
   // RETURN_ERR_IF_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, NFC_TAG_EEPROM_ERROR_INIT);
   // RETURN_ERR_IF_NULL(spaces_count, NFC_TAG_EEPROM_ERROR_PTR_NULL);

   // *spaces_count = m_current_config.max_elements - m_current_status.element_count;

   return RESULT_OK;
}

static result_t nfc_tag_read_config(nfc_tag_eeprom_queue_interface_t *interface,
                                    uint8_t *max_elements,
                                    uint8_t *element_size,
                                    uint8_t *status_slot_count)
{
   // RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(max_elements, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(element_size, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(status_slot_count, NFC_TAG_EEPROM_ERROR_PTR_NULL);

   // nfc_tag_eeprom_queue_t *self = interface->parent;
   // queue_config_t config = {0};

   // result_t result = self->nfc_interface->read_eeprom_blocking(
   //    self->nfc_interface, CONFIG_START_ADDRESS, (uint8_t *)&config, sizeof(queue_config_t));

   // if(IS_OK(result))
   // {
   //    *max_elements = config.max_elements;
   //    *element_size = config.element_size;
   //    *status_slot_count = config.status_slot_count;
   // }
   // return result;

   return RESULT_OK;
}

static result_t nfc_tag_get_element_size(nfc_tag_eeprom_queue_interface_t *interface, size_t *element_size)
{
   // RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(element_size, NFC_TAG_EEPROM_ERROR_PTR_NULL);

   // *element_size = (size_t)m_current_config.element_size;

   return RESULT_OK;
}

static result_t nfc_tag_read_total_writes(nfc_tag_eeprom_queue_interface_t *interface, uint32_t *write_counter)
{
   // RETURN_ERR_IF_INTERFACE_NULL(interface, NFC_TAG_EEPROM_ERROR_PTR_NULL);
   // RETURN_ERR_IF_NULL(write_counter, NFC_TAG_EEPROM_ERROR_PTR_NULL);

   // nfc_tag_eeprom_queue_t *self = interface->parent;

   // update_counter_t counter = {0};
   // result_t result = self->nfc_interface->read_eeprom_blocking(
   //    self->nfc_interface, UPDATE_COUNTER_ADDRESS, (uint8_t *)&counter, sizeof(update_counter_t));

   // if(IS_OK(result))
   // {
   //    *write_counter = (counter.update_counter * STATUS_COUNT) + m_current_status.counter;
   // }

   // return result;
   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t nfc_tag_eeprom_queue_init(nfc_tag_eeprom_queue_t *const self,
                                   uint8_t element_size,
                                   uint8_t status_slot_count,
                                   bool init_memory)
{
   RETURN_ERR_IF_NULL(self, NFC_TAG_EEPROM_ERROR_PTR_NULL);

   result_t result = RESULT_OK;

   self->interface.parent = self;

   self->interface.nfc_dequeue = nfc_tag_eeprom_queue_dequeue;
   self->interface.nfc_dequeue_multiple = nfc_tag_eeprom_queue_dequeue_multiple;
   self->interface.nfc_enqueue = nfc_tag_eeprom_queue_enqueue;
   self->interface.nfc_enqueue_multiple = nfc_tag_eeprom_queue_enqueue_multiple;
   self->interface.nfc_peek = nfc_tag_eeprom_queue_peek;
   self->interface.nfc_peek_multiple = nfc_tag_eeprom_queue_peek_multiple;
   self->interface.nfc_pop = nfc_tag_eeprom_queue_pop;
   self->interface.nfc_pop_multiple = nfc_tag_eeprom_queue_pop_multiple;
   self->interface.nfc_get_available_spaces = nfc_tag_eeprom_queue_get_available_spaces;
   self->interface.nfc_get_element_count = nfc_tag_eeprom_queue_get_element_count;
   self->interface.nfc_read_config = nfc_tag_read_config;
   self->interface.nfc_read_total_writes = nfc_tag_read_total_writes;
   self->interface.nfc_get_element_size = nfc_tag_get_element_size;
   self->interface.nfc_get_percent_full = nfc_tag_get_percent_full;
   self->interface.nfc_put = nfc_put;

   self->_initialized = true;

   return RESULT_OK;
}