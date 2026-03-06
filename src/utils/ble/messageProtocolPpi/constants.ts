// Sizes in bytes from firmware common.h
export const DOSE_SCHEDULE_T_SIZE_BYTES = 30;
export const DOSE_EVENT_T_SIZE_BYTES = 12;
export const DOSE_DATA_T_SIZE_BYTES = 19;
export const DOCK_STATUS_T_SIZE_BYTES = 28;
export const TEMPERATURE_LOG_T_SIZE_BYTES = 6;
export const DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES = 16;
export const DOCK_CHARGE_STATUS_T_SIZE_BYTES = 5;
export const RING_DOCKED_STATUS_T_SIZE_BYTES = 25;
export const BLUETOOTH_STATUS_T_SIZE_BYTES = 5;
export const BATTERY_LEVEL_T_SIZE_BYTES = 5;
export const RING_STATUS_T_SIZE_BYTES = 47;
// Compatibility path: firmware/workspace RQ->RE encoding for ring status currently sends 41 bytes.
export const RING_STATUS_T_LEGACY_SIZE_BYTES = 41;
// From firmware/common/modules/debug/debug.h:
// RAW_DEBUG_LOG_T_METADATA_SIZE (8) + MAX_SUPPORTED_VARARGS (6) * uint32_t (4) = 32 bytes.
export const RAW_DEBUG_LOG_T_SIZE_BYTES = 32;
export const BASELINING_FEEDBACK_T_SIZE_BYTES = 19;
export const CALIBRATION_FEEDBACK_T_SIZE_BYTES = 9;
export const START_CALIBRATION_PARAM_T_SIZE_BYTES = 5;
export const WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES = 12;

export const MAX_DOSES_PER_DAY = 10;
