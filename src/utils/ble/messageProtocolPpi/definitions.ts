import {
  BASELINING_FEEDBACK_T_SIZE_BYTES,
  BATTERY_LEVEL_T_SIZE_BYTES,
  BLUETOOTH_STATUS_T_SIZE_BYTES,
  CALIBRATION_FEEDBACK_T_SIZE_BYTES,
  DOCK_CHARGE_STATUS_T_SIZE_BYTES,
  DOCK_STATUS_T_SIZE_BYTES,
  DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES,
  DOSE_DATA_T_SIZE_BYTES,
  DOSE_EVENT_T_SIZE_BYTES,
  DOSE_SCHEDULE_T_SIZE_BYTES,
  RAW_DEBUG_LOG_T_SIZE_BYTES,
  RING_DOCKED_STATUS_T_SIZE_BYTES,
  RING_STATUS_T_SIZE_BYTES,
  START_CALIBRATION_PARAM_T_SIZE_BYTES,
  TEMPERATURE_LOG_T_SIZE_BYTES,
  WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES,
} from "./constants";
import { PpiDefinition, PpiId } from "./types";

export const PPI_DEFINITIONS: Record<PpiId, PpiDefinition> = {
  [PpiId.AD_DOSE_SCHEDULE]: {
    id: PpiId.AD_DOSE_SCHEDULE,
    name: "PPI_AD_DOSE_SCHEDULE",
    lengths: { rq: 0, re: DOSE_SCHEDULE_T_SIZE_BYTES, push: DOSE_SCHEDULE_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_STATUS]: {
    id: PpiId.AD_DOCK_STATUS,
    name: "PPI_AD_DOCK_STATUS",
    lengths: { rq: 0, re: DOCK_STATUS_T_SIZE_BYTES, push: DOCK_STATUS_T_SIZE_BYTES },
  },
  [PpiId.AD_RING_STATUS]: {
    id: PpiId.AD_RING_STATUS,
    name: "PPI_AD_RING_STATUS",
    lengths: { rq: 0, re: RING_STATUS_T_SIZE_BYTES, push: RING_STATUS_T_SIZE_BYTES },
  },
  [PpiId.AD_TIME]: {
    id: PpiId.AD_TIME,
    name: "PPI_AD_TIME",
    lengths: { rq: 0, re: 4, push: 4 },
  },
  [PpiId.AD_WEIGHT_MEAS_RATE]: {
    id: PpiId.AD_WEIGHT_MEAS_RATE,
    name: "PPI_AD_WEIGHT_MEAS_RATE",
    lengths: { push: 0 },
  },
  [PpiId.AD_IMU_SAMPLE_RATE]: {
    id: PpiId.AD_IMU_SAMPLE_RATE,
    name: "PPI_AD_IMU_SAMPLE_RATE",
    lengths: { push: 0 },
  },
  [PpiId.AD_DOCK_BATT_SAMPLE_RATE]: {
    id: PpiId.AD_DOCK_BATT_SAMPLE_RATE,
    name: "PPI_AD_DOCK_BATT_SAMPLE_RATE",
    lengths: { push: 0 },
  },
  [PpiId.AD_RING_BATT_SAMPLE_RATE]: {
    id: PpiId.AD_RING_BATT_SAMPLE_RATE,
    name: "PPI_AD_RING_BATT_SAMPLE_RATE",
    lengths: { push: 0 },
  },
  [PpiId.AD_DOCK_TEMP_SAMPLE_RATE]: {
    id: PpiId.AD_DOCK_TEMP_SAMPLE_RATE,
    name: "PPI_AD_DOCK_TEMP_SAMPLE_RATE",
    lengths: { push: 0 },
  },
  [PpiId.AD_DOSE_EVENT_REPORT]: {
    id: PpiId.AD_DOSE_EVENT_REPORT,
    name: "PPI_AD_DOSE_EVENT_REPORT",
    lengths: { push: DOSE_EVENT_T_SIZE_BYTES },
  },
  [PpiId.AD_DOSE_DATA_POINT]: {
    id: PpiId.AD_DOSE_DATA_POINT,
    name: "PPI_AD_DOSE_DATA_POINT",
    lengths: { push: DOSE_DATA_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_TEMP_LOG]: {
    id: PpiId.AD_DOCK_TEMP_LOG,
    name: "PPI_AD_DOCK_TEMP_LOG",
    lengths: { push: TEMPERATURE_LOG_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_WEIGHT_LOG]: {
    id: PpiId.AD_DOCK_WEIGHT_LOG,
    name: "PPI_AD_DOCK_WEIGHT_LOG",
    lengths: { push: DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_CHARGE_STATUS]: {
    id: PpiId.AD_DOCK_CHARGE_STATUS,
    name: "PPI_AD_DOCK_CHARGE_STATUS",
    lengths: { push: DOCK_CHARGE_STATUS_T_SIZE_BYTES },
  },
  [PpiId.AD_RING_DOCKED_STATUS]: {
    id: PpiId.AD_RING_DOCKED_STATUS,
    name: "PPI_AD_RING_DOCKED_STATUS",
    lengths: { push: RING_DOCKED_STATUS_T_SIZE_BYTES },
  },
  [PpiId.AD_BL_STATUS]: {
    id: PpiId.AD_BL_STATUS,
    name: "PPI_AD_BL_STATUS",
    lengths: { push: BLUETOOTH_STATUS_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_BATT_LEVEL_LOG]: {
    id: PpiId.AD_DOCK_BATT_LEVEL_LOG,
    name: "PPI_AD_DOCK_BATT_LEVEL_LOG",
    lengths: { push: BATTERY_LEVEL_T_SIZE_BYTES },
  },
  [PpiId.AD_RING_BATT_LEVEL_LOG]: {
    id: PpiId.AD_RING_BATT_LEVEL_LOG,
    name: "PPI_AD_RING_BATT_LEVEL_LOG",
    lengths: { push: BATTERY_LEVEL_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_DEBUG_LOG]: {
    id: PpiId.AD_DOCK_DEBUG_LOG,
    name: "PPI_AD_DOCK_DEBUG_LOG",
    lengths: { push: RAW_DEBUG_LOG_T_SIZE_BYTES },
  },
  [PpiId.AD_RING_DEBUG_LOG]: {
    id: PpiId.AD_RING_DEBUG_LOG,
    name: "PPI_AD_RING_DEBUG_LOG",
    lengths: { push: RAW_DEBUG_LOG_T_SIZE_BYTES },
  },
  [PpiId.AD_START_BASELINING]: {
    id: PpiId.AD_START_BASELINING,
    name: "PPI_AD_START_BASELINING",
    lengths: { rq: 1, re: 1, push: BASELINING_FEEDBACK_T_SIZE_BYTES },
  },
  [PpiId.AD_BASELINING_FEEDBACK]: {
    id: PpiId.AD_BASELINING_FEEDBACK,
    name: "PPI_AD_BASELINING_FEEDBACK",
    lengths: { push: BASELINING_FEEDBACK_T_SIZE_BYTES },
  },
  [PpiId.AD_VALIDATE_MED]: {
    id: PpiId.AD_VALIDATE_MED,
    name: "PPI_AD_VALIDATE_MED",
    lengths: { rq: 0, re: 1 },
  },
  [PpiId.AD_START_CALIBRATION]: {
    id: PpiId.AD_START_CALIBRATION,
    name: "PPI_AD_START_CALIBRATION",
    lengths: { rq: START_CALIBRATION_PARAM_T_SIZE_BYTES, re: 1 },
  },
  [PpiId.AD_CALIBRATION_DATA]: {
    id: PpiId.AD_CALIBRATION_DATA,
    name: "PPI_AD_CALIBRATION_DATA",
    lengths: {
      rq: 0,
      re: WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES,
      push: WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES,
    },
  },
  [PpiId.AD_CALIBRATION_WEIGHT_PRESENT]: {
    id: PpiId.AD_CALIBRATION_WEIGHT_PRESENT,
    name: "PPI_AD_CALIBRATION_WEIGHT_PRESENT",
    lengths: { push: 1 },
  },
  [PpiId.AD_CALIBRATION_FEEDBACK]: {
    id: PpiId.AD_CALIBRATION_FEEDBACK,
    name: "PPI_AD_CALIBRATION_FEEDBACK",
    lengths: { push: CALIBRATION_FEEDBACK_T_SIZE_BYTES },
  },
  [PpiId.AD_DEVELOPMENT_CMD]: {
    id: PpiId.AD_DEVELOPMENT_CMD,
    name: "PPI_AD_DEVELOPMENT_CMD",
    lengths: { rq: 1 },
  },
};
