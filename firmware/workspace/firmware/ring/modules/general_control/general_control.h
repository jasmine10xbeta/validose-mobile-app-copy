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
 * @file general_control.h
 * @ingroup gc_module
 * @brief
 */

#ifndef GENERAL_CONTROL_H_
#define GENERAL_CONTROL_H_
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
   GC_BLE_HEARTBEAT_FAILED,
   GC_BLE_HEARTBEAT_TIMEOUT,
   GC_ERROR_INVALID_STIM_PARAM_RECEIVED,
   GC_ERROR_POWER_MANAGEMENT_INIT,
   GC_ERROR_APPLICATION_TIMER_INIT,
   GC_ERROR_BATTERY_TOO_LOW_TO_START_STIM,
   GC_ERROR_CHARGER_CONNECTED,
   GC_ERROR_SYSTICK_INIT,
   GC_ERROR_RTC_TIMER_INIT,
   GC_ERROR_RTC_TIMER_START,
   GC_ERROR_FLASH_STORAGE_INIT,
   GC_ERROR_CRYPTO_INIT,
   GC_ERROR_MEM_INIT,
   GC_ERROR_PASSKEY_DECRYPT,
   GC_ERROR_DFU_TIMER_INIT,
   GC_ERROR_PMIC_RESET_TIMER_INIT,
   GC_ERROR_SYSTICK,
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