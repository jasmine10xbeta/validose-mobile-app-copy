/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup ring_sources Dock Source Manager
 * @ingroup modules
 * @brief This module provides a simple interface to manage data sources for the ring.
 * @details The Ring Source module provides a single interface to retrieve and manage data from multiple sources.
 *
 * It allows data sources to be added or removed without affecting the communication logic implemented by the dock
 * manager.
 *
 * Separating the logic improves readability and clarity of the code while also serving to reduce complexity and
 * coupling.
 *
 * @file ring_sources.h
 * @ingroup ring_sources
 * @brief
 */

#ifndef RING_SOURCES_H_
#define RING_SOURCES_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
#include "nfc_tag_eeprom_queue.h"
#include "queue.h"
#include "ring_data_manager_interface.h"
#include "ring_sources_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct ring_sources
{
   ring_sources_interface_t interface; /**< Interface to the dock source manager */

   ring_data_manager_interface_t *_data_manager_ifc;  /**< Interface to the ring data manager */
   nfc_tag_eeprom_queue_interface_t *_dose_queue_ifc; /**< Interface to the dose NFC tag queue */
   queue_interface_t *_error_queue_ifc;               /**< Interface to the error queue */
   queue_interface_t *_battery_queue_ifc;             /**< Interface to the battery queue */
   uint16_t _source_element_size[SOURCE_ID_MAX];      /**< Sizes of elements in each data source */

} ring_sources_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_sources_init(ring_sources_t *const self,
                           ring_data_manager_interface_t *data_manager_ifc,
                           nfc_tag_eeprom_queue_interface_t *dose_queue_ifc,
                           queue_interface_t *error_queue_ifc,
                           queue_interface_t *battery_queue_ifc);
#endif // RING_SOURCES_H_
