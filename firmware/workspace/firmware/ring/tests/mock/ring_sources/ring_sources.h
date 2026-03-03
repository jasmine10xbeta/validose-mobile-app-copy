/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file mock_ring_sources.h
 * @ingroup ring_sources
 * @brief Mock implementation of ring sources for testing purposes.
 */

#ifndef RING_SOURCES_H_
#define RING_SOURCES_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "common.h"
#include "queue.h"
#include "queue_interface.h"
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

   ring_status_t _status; /**< Buffer to store the status for the status source */

   queue_interface_t *_dose_event_queue_ifc;
   queue_interface_t *_ring_battery_level_queue_ifc;
   queue_interface_t *_ring_debug_log_queue_ifc;

} ring_sources_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t ring_sources_init(ring_sources_t *const self);

#endif // RING_SOURCES_H_
