import {
  BATTERY_LEVEL_T_SIZE_BYTES,
  BLUETOOTH_STATUS_T_SIZE_BYTES,
  CAP_DETECTION_CFG_T_SIZE_BYTES,
  CAP_DETECTION_STATUS_T_SIZE_BYTES,
  DOCK_CHARGE_STATUS_T_SIZE_BYTES,
  DOCK_STATUS_T_SIZE_BYTES,
  DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES,
  RAW_DEBUG_LOG_T_SIZE_BYTES,
  RING_DOCKED_STATUS_T_SIZE_BYTES,
  RING_STATUS_T_LEGACY_SIZE_BYTES,
  RING_STATUS_T_SIZE_BYTES,
  TEMPERATURE_LOG_T_SIZE_BYTES,
} from "./constants";
import {
  BatteryLevel,
  BluetoothStatus,
  CapDetectionConfig,
  CapDetectionStatus,
  DockChargeStatus,
  DockStatus,
  DockWeightMeasurement,
  RawDebugLog,
  RingDockedStatus,
  RingStatus,
  TemperatureLog,
} from "./types";

export function decodeDockStatus(payload: Uint8Array): DockStatus | null {
  if (payload.length !== DOCK_STATUS_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  let offset = 0;

  const hardware_version = {
    major: view.getUint8(offset),
    minor: view.getUint8(offset + 1),
    patch: view.getUint8(offset + 2),
  };
  offset += 3;

  const firmware_version = {
    major: view.getUint8(offset),
    minor: view.getUint8(offset + 1),
    patch: view.getUint8(offset + 2),
  };
  offset += 3;

  const mac = payload.slice(offset, offset + 10);
  offset += 10;

  const timestamp_unix_s = view.getUint32(offset, true);
  offset += 4;
  const ship_mode_exit_timestamp_unix = view.getUint32(offset, true);
  offset += 4;
  const uptime_s = view.getUint32(offset, true);

  return {
    hardware_version,
    firmware_version,
    mac,
    timestamp_unix_s,
    ship_mode_exit_timestamp_unix,
    uptime_s,
  };
}

export function decodeTemperatureLog(payload: Uint8Array): TemperatureLog | null {
  if (payload.length !== TEMPERATURE_LOG_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    timestamp_unix_s: view.getUint32(0, true),
    temperature_deg_c: view.getInt16(4, true),
  };
}

export function decodeDockWeightMeasurement(payload: Uint8Array): DockWeightMeasurement | null {
  if (payload.length !== DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    weight_mg: view.getInt32(0, true),
    total_dispensed_mg: view.getUint32(4, true),
    std_dev: view.getUint16(8, true),
    temperature_deg_c_div10: view.getInt16(10, true),
    timestamp_unix_s: view.getUint32(12, true),
  };
}

export function decodeDockChargeStatus(payload: Uint8Array): DockChargeStatus | null {
  if (payload.length !== DOCK_CHARGE_STATUS_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    timestamp_unix_s: view.getUint32(0, true),
    charge_status: view.getUint8(4),
  };
}

export function decodeRingDockedStatus(payload: Uint8Array): RingDockedStatus | null {
  if (payload.length !== RING_DOCKED_STATUS_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  const timestamp_unix_s = view.getUint32(0, true);
  const docked_status = view.getUint8(4);
  const ring_nfc_id = payload.slice(5, 15);
  const medication_nfc_id = payload.slice(15, 25);

  return {
    timestamp_unix_s,
    docked_status,
    ring_nfc_id,
    medication_nfc_id,
  };
}

export function decodeBatteryLevel(payload: Uint8Array): BatteryLevel | null {
  if (payload.length !== BATTERY_LEVEL_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    timestamp_unix_s: view.getUint32(0, true),
    battery_level: view.getUint8(4),
  };
}

export function decodeBluetoothStatus(payload: Uint8Array): BluetoothStatus | null {
  if (payload.length !== BLUETOOTH_STATUS_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    timestamp_unix_s: view.getUint32(0, true),
    bluetooth_status: view.getUint8(4),
  };
}

export function decodeCapDetectionConfig(payload: Uint8Array): CapDetectionConfig | null {
  if (payload.length !== CAP_DETECTION_CFG_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    threshhold: view.getUint16(0, true),
    hysteresis: view.getUint16(2, true),
  };
}

export function decodeCapDetectionStatus(payload: Uint8Array): CapDetectionStatus | null {
  if (payload.length !== CAP_DETECTION_STATUS_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    config: {
      threshhold: view.getUint16(0, true),
      hysteresis: view.getUint16(2, true),
    },
    prox_value: view.getUint16(4, true),
    is_cap_closed: view.getUint8(6) !== 0,
    timestamp_unix_s: view.getUint32(7, true),
    poll_period_ms: view.getUint32(11, true),
  };
}

export function decodeRawDebugLog(payload: Uint8Array): RawDebugLog | null {
  if (payload.length !== RAW_DEBUG_LOG_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  const arg_values: number[] = [];
  let offset = 8;
  while (offset + 4 <= payload.length) {
    arg_values.push(view.getUint32(offset, true));
    offset += 4;
  }

  return {
    level: view.getUint8(0),
    timestamp: view.getUint32(1, true),
    module_id: view.getUint8(5),
    line: view.getUint16(6, true),
    arg_values,
  };
}

export function decodeRingStatus(payload: Uint8Array): RingStatus | null {
  if (
    payload.length !== RING_STATUS_T_SIZE_BYTES &&
    payload.length !== RING_STATUS_T_LEGACY_SIZE_BYTES
  ) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  let offset = 0;

  const hardware_version = {
    major: view.getUint8(offset),
    minor: view.getUint8(offset + 1),
    patch: view.getUint8(offset + 2),
  };
  offset += 3;

  const firmware_version = {
    major: view.getUint8(offset),
    minor: view.getUint8(offset + 1),
    patch: view.getUint8(offset + 2),
  };
  offset += 3;

  const mac = payload.slice(offset, offset + 10);
  offset += 10;

  const timestamp_unix_s = view.getUint32(offset, true);
  offset += 4;
  const ship_mode_exit_timestamp_unix = view.getUint32(offset, true);
  offset += 4;
  const battery_charge_status = view.getUint8(offset);
  offset += 1;
  const uptime_s = view.getUint32(offset, true);
  offset += 4;
  const temperature_celsius = view.getInt16(offset, true);
  offset += 2;

  const dose_fifo_used_percent = view.getUint8(offset++);
  const battery_fifo_used_percent = view.getUint8(offset++);
  const imu_fifo_used_percent = view.getUint8(offset++);
  const error_fifo_used_percent = view.getUint8(offset++);
  const dose_fifo_used_percent_watermark = view.getUint8(offset++);
  const battery_fifo_used_percent_watermark = view.getUint8(offset++);
  const imu_fifo_used_percent_watermark = view.getUint8(offset++);
  const error_fifo_used_percent_watermark = view.getUint8(offset++);

  const battery_sample_frequency_millihz = view.getUint16(offset, true);
  let cap_on = 0;
  let cap_off = 0;
  let current_prox = 0;
  offset += 2;
  if (offset + 6 <= payload.length) {
    cap_on = view.getUint16(offset, true);
    offset += 2;
    cap_off = view.getUint16(offset, true);
    offset += 2;
    current_prox = view.getUint16(offset, true);
  }

  return {
    hardware_version,
    firmware_version,
    mac,
    timestamp_unix_s,
    ship_mode_exit_timestamp_unix,
    battery_charge_status,
    uptime_s,
    temperature_celsius,
    dose_fifo_used_percent,
    battery_fifo_used_percent,
    imu_fifo_used_percent,
    error_fifo_used_percent,
    dose_fifo_used_percent_watermark,
    battery_fifo_used_percent_watermark,
    imu_fifo_used_percent_watermark,
    error_fifo_used_percent_watermark,
    battery_sample_frequency_millihz,
    cap_on,
    cap_off,
    current_prox,
  };
}
