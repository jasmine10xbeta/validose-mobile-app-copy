/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @defgroup gc_module General Control Module
 * @ingroup modules
 * @brief Main application behaviour and functionality.
 * @details
 *
 * @file comms_test.h
 * @ingroup comms_test_module
 * @brief
 */

#ifndef COMMS_TEST_H_
#define COMMS_TEST_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "../../common/common.h"
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
   GC_ERROR_NONE = 0,
   GC_ERROR_PTR_NULL,
   GC_ERROR_OVERFLOW,
   GC_ERROR_NRF_ERR_CHECK,
   GC_ERROR_NRF_ERR_CHECK_DFU,
   GC_ERROR_FDS,
   GC_ERROR_POWER_MANAGEMENT_INIT,
   GC_ERROR_APPLICATION_TIMER_INIT,
   GC_ERROR_CHARGER_CONNECTED,
   GC_ERROR_SYSTICK_INIT,
   GC_ERROR_FLASH_STORAGE_INIT,
   GC_ERROR_CRYPTO_INIT,
   GC_ERROR_MEM_INIT,
   GC_ERROR_PASSKEY_DECRYPT,
   GC_ERROR_DFU_TIMER_INIT,
   GC_ERROR_PMIC_RESET_TIMER_INIT,
   GC_ERROR_SYSTICK,
   GC_ERROR_INVALID_UNIX_TIME_RECEIVED,
   GC_ERROR_INVALID_DOSE_SCHEDULE_RECEIVED,
   GC_ERROR_NULL_DOSE_SCHEDULE_RECEIVED,
   GC_ERROR_DOSE_SCHEDULE_TEMPERATURE_THRESHOLD_EXCEEDED,
   GC_ERROR_BAD_DOSE_EVENT_RECEIVED_FROM_RING,
   GC_ERROR_BUTTON_INIT,
   GC_ERROR_INVALID_PARAM,
   GC_ERROR_MAX,
} GC_ERROR;
/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t general_control_init(void);
result_t general_control_run(void);
#endif // GENERAL_CONTROL_H_