/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file fds_manager.c
 * @ingroup fds_manager_module
 * @brief Implementation of the FDS Manager module.
 *
 * @note This module provides an interface for storing and retrieving records in non-volatile memory (NVM)
 *
 * This module will not be called frequently, so blocking calls and delays are acceptable.
 *
 * @todo Remove delays
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "fds_manager.h"
#include "fds_manager_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_FDS_MANAGER_MODULE;

// Timeout for FDS operations in milliseconds. 1ms was determined to be sufficient in testing.
// This was increased by a factor of 5 due to the infrequent use of this module and to add extra margin
#define FDS_TIMEOUT_MS      (5u)
#define DEVICE_DATA_FILE_ID (0x0001)
#define BYTES_IN_WORD       (4u)
#define MAX_INIT_MS         (1000u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface function declarations
static result_t store_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t data);
static result_t retrieve_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t *data);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static uint64_t records[RECORD_ID_MAX] = {0};

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t store_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t data)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, FDS_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(record_id >= RECORD_ID_MAX, FDS_MANAGER_ERROR_INVALID_RECORD_ID);

   records[record_id] = data;

   return RESULT_OK;
}

static result_t retrieve_uint64_t(const fds_manager_interface_t *const ifc, RECORD_ID record_id, uint64_t *data)
{
   RETURN_ERR_IF_UNINITIALIZED(ifc, FDS_MANAGER_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_NULL(data, FDS_MANAGER_ERROR_NULL_PTR);
   RETURN_ERR_IF_TRUE(record_id >= RECORD_ID_MAX, FDS_MANAGER_ERROR_INVALID_RECORD_ID);

   *data = records[record_id];

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t fds_manager_init(fds_manager_t *const self)
{
   RETURN_ERR_IF_NULL(self, FDS_MANAGER_ERROR_NULL_PTR);

   self->_initialization_status = INITIALIZED;

   return RESULT_OK;
}