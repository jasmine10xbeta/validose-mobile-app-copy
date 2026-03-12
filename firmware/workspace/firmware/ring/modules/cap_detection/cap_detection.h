/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 */

/**
 * @file tmd2635_driver.h (for ir proximity driver TMD2635)
 * @ingroup drivers/tmd2635
 * @brief
 */

#ifndef CAP_DETECTION_MODULE_H_
#define CAP_DETECTION_MODULE_H_

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
// Standard includes
#include <stdbool.h>

// Custom includes
#include "cap_detection_interface.h"
#include "common.h"
#include "ir_proximity_driver_interface.h"

/**********************************************************************************************************************
 * Definitions
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Types
 *********************************************************************************************************************/

/**
 * @brief Cap detection module instance
 */
typedef struct cap_detection cap_detection_t;
struct cap_detection
{
   cap_detection_interface_t interface;
   const ir_proximity_driver_interface_t *_p_prox_ifc; /**< Pointer to the proximity sensor driver interface */
   CAP_STATE _cap_state;                               /** Current cap state */
   uint16_t _threshhold;                               /** Proximity threshold for determining cap state */
   uint16_t _hysteresis;                               /** Hysteresis value */
   bool _is_initialized;                               /** Whether the instance is initialized */
};

/**********************************************************************************************************************
 * Variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Global functions
 *********************************************************************************************************************/
/**
 * @brief To initialize an instance of the cap detection module.
 * @param self iThe instance of cap the detection module to initialize.
 * @param prox The instance of the tmd2635 proximity sensor driver.
 */

result_t cap_detection_init(cap_detection_t *const self,
                            const ir_proximity_driver_interface_t *const p_prox_ifc,
                            uint16_t init_threshhold,
                            uint16_t init_hysteresis);

#endif // CAP_DETECTION_MODULE_H_
