/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ring_data_manager.h
 * @ingroup ring data manager
 * @brief Header for the ring data manager module
 */
#ifndef RING_DATA_MANAGER_H_
#define RING_DATA_MANAGER_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "common.h"

#include "cap_detection_interface.h"
#include "dose_detection_interface.h"
#include "fds_manager_interface.h"
#include "imu_interface.h"
#include "nfc_tag_driver_interface.h"
#include "nfc_tag_eeprom_queue_interface.h"
#include "queue_interface.h"
#include "ring_battery_manager_interface.h"
#include "ring_data_manager_interface.h"
#include "system_time_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/
typedef struct ring_data_manager
{
   ring_data_manager_interface_t interface;

   uint32_t _initialization_status; /** < Initialization status flag */

   system_time_interface_t *_systick_ifc;                  /** < System tick interface */
   system_time_interface_t *_systime_ifc;                  /** < System time interface */
   fds_manager_interface_t *_fds_manager_ifc;              /** < FDS manager interface */
   imu_interface_t *_imu_ifc;                              /** < IMU interface */
   ring_battery_manager_interface_t *_battery_manager_ifc; /** < Battery manager interface */
   dose_detection_interface_t *_dose_detection_ifc;        /** < Dose detection interface */
   cap_detection_interface_t *_cap_detection_ifc;          /** < Cap detection interface */

   ring_status_t _current_status;    /** < Current ring status structure */
   bool _is_docked;                  /** < Flag indicating if the ring is currently docked */
   uint64_t _battery_log_trigger_ms; /** < Systick at which the next battery log event should be triggered */

   nfc_tag_eeprom_queue_interface_t *_dose_queue_ifc; /** < Dose data queue interface */
   queue_interface_t *_battery_queue_ifc;             /** < Battery data queue interface */
   queue_interface_t *_error_queue_ifc;               /** < Error data queue interface */
} ring_data_manager_t;

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
result_t init_ring_data_manager(ring_data_manager_t *const self,
                                system_time_interface_t *system_time_ifc,
                                system_time_interface_t *systick_time_ifc,
                                fds_manager_interface_t *fds_manager_ifc,
                                imu_interface_t *imu_ifc,
                                ring_battery_manager_interface_t *battery_manager_ifc,
                                dose_detection_interface_t *dose_detection_ifc,
                                cap_detection_interface_t *cap_detection_ifc,
                                nfc_tag_eeprom_queue_interface_t *dose_data_queue_ifc,
                                queue_interface_t *battery_data_queue_ifc,
                                queue_interface_t *error_data_queue_ifc);

#endif // RING_DATA_MANAGER_H_