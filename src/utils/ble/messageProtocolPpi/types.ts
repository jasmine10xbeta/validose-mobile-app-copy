/**
 * PPI type values align with firmware PPI_TYPE enum.
 */
export enum PpiType {
  RQ = 0,
  RE = 1,
  PUSH = 2,
  MAX = 3,
}

/**
 * Known PPI IDs from firmware app_dock_ppi.h list.
 */
export enum PpiId {
  AD_DOSE_SCHEDULE = 0,
  AD_DOCK_STATUS = 1,
  AD_RING_STATUS = 2,
  AD_TIME = 3,
  AD_WEIGHT_MEAS_RATE = 4,
  AD_IMU_SAMPLE_RATE = 5,
  AD_DOCK_BATT_SAMPLE_RATE = 6,
  AD_RING_BATT_SAMPLE_RATE = 7,
  AD_DOCK_TEMP_SAMPLE_RATE = 8,
  AD_DOSE_EVENT_REPORT = 9,
  AD_DOSE_DATA_POINT = 10,
  AD_DOCK_TEMP_LOG = 11,
  AD_DOCK_WEIGHT_LOG = 12,
  AD_DOCK_CHARGE_STATUS = 13,
  AD_RING_DOCKED_STATUS = 14,
  AD_BL_STATUS = 15,
  AD_DOCK_BATT_LEVEL_LOG = 16,
  AD_RING_BATT_LEVEL_LOG = 17,
  AD_DOCK_DEBUG_LOG = 18,
  AD_RING_DEBUG_LOG = 19,
  AD_START_BASELINING = 20,
  AD_BASELINING_FEEDBACK = 21,
  AD_VALIDATE_MED = 22,
  AD_START_CALIBRATION = 23,
  AD_CALIBRATION_DATA = 24,
  AD_CALIBRATION_WEIGHT_PRESENT = 25,
  AD_CALIBRATION_FEEDBACK = 26,
  AD_DEVELOPMENT_CMD = 27,
}

export type PpiPayloadLength = {
  rq?: number | null;
  re?: number | null;
  push?: number | null;
};

export type PpiDefinition = {
  id: PpiId;
  name: string;
  lengths: PpiPayloadLength;
};

export type DoseSchedulePpi = {
  medication_type: number;
  dosage_mg: number;
  temp_upper_limit_deg_c: number;
  temp_lower_limit_deg_c: number;
  temp_avg_window_duration_sec: number;
  dose_days_bitfield: number;
  dose_window_duration_minutes: number;
  dose_window_count: number;
  dose_window_start_times_minutes: number[];
};

export type EventId = {
  days_since_epoch: number;
  event_ctr: number;
};

export type DoseEventPpi = {
  event_id: EventId;
  start_timestamp_unix_s: number;
  duration_s: number;
  dose_completed_in_time: number;
  tilt_count: number;
};

export type DoseDataPoint = {
  event_id: EventId;
  relative_timestamp_ms: number;
  accel_x: number;
  accel_y: number;
  accel_z: number;
  angular_accel_x: number;
  angular_accel_y: number;
  angular_accel_z: number;
};

export type DockStatus = {
  hardware_version: SemanticVersion;
  firmware_version: SemanticVersion;
  mac: Uint8Array;
  timestamp_unix_s: number;
  ship_mode_exit_timestamp_unix: number;
  uptime_s: number;
};

export type TemperatureLog = {
  timestamp_unix_s: number;
  temperature_deg_c: number;
};

export type DockWeightMeasurement = {
  weight_mg: number;
  total_dispensed_mg: number;
  std_dev: number;
  temperature_deg_c_div10: number;
  timestamp_unix_s: number;
};

export type DockChargeStatus = {
  timestamp_unix_s: number;
  charge_status: number;
};

export type RingDockedStatus = {
  timestamp_unix_s: number;
  docked_status: number;
  ring_nfc_id: Uint8Array;
  medication_nfc_id: Uint8Array;
};

export type BatteryLevel = {
  timestamp_unix_s: number;
  battery_level: number;
};

export type BluetoothStatus = {
  timestamp_unix_s: number;
  bluetooth_status: number;
};

export type RawDebugLog = {
  level: number;
  timestamp: number;
  module_id: number;
  line: number;
  arg_values: number[];
};

export type SemanticVersion = {
  major: number;
  minor: number;
  patch: number;
};

export type RingStatus = {
  hardware_version: SemanticVersion;
  firmware_version: SemanticVersion;
  mac: Uint8Array;
  timestamp_unix_s: number;
  ship_mode_exit_timestamp_unix: number;
  battery_charge_status: number;
  uptime_s: number;
  temperature_celsius: number;
  dose_fifo_used_percent: number;
  battery_fifo_used_percent: number;
  imu_fifo_used_percent: number;
  error_fifo_used_percent: number;
  dose_fifo_used_percent_watermark: number;
  battery_fifo_used_percent_watermark: number;
  imu_fifo_used_percent_watermark: number;
  error_fifo_used_percent_watermark: number;
  battery_sample_frequency_millihz: number;
  cap_on: number;
  cap_off: number;
  current_prox: number;
};

export type BaseliningFeedback = {
  current_state: number;
  motion_state: number;
  std_dev: number;
  avg_weight_mg: number;
  is_ring_present: boolean;
  medication_nfc_id: Uint8Array;
};

export type CalibrationFeedback = {
  current_state: number;
  motion_state: number;
  std_dev: number;
  avg_weight_mg: number;
  is_ring_present: boolean;
};

export type StartCalibrationParam = {
  start: boolean;
  calibration_weight_mg: number;
};

export type WeightStackCalibrationRecord = {
  zero_offset: number;
  calibration_factor: number;
  full_assembly_weight_mg: number;
};

export type DecodedPpiPayload = {
  ppi: number;
  type: PpiType;
  value: unknown;
};
