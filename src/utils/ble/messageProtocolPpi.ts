import { MpPacketPayload, MsgProtRxPacketStatus, MsgProtTxPacketStatus } from "./messageProtocol";

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
  AD_DOSE_EVENT_REPORT = 9,
  AD_DOCK_TEMP_LOG = 11,
  AD_DOCK_WEIGHT_LOG = 12,
  AD_DOCK_CHARGE_STATUS = 13,
  AD_RING_DOCKED_STATUS = 14,
  AD_DOCK_BATT_LEVEL_LOG = 16,
  AD_RING_BATT_LEVEL_LOG = 17,
  AD_DOCK_DEBUG_LOG = 18,
  AD_RING_DEBUG_LOG = 19,
  AD_START_BASELINING = 20,
  AD_BASELINING_FEEDBACK = 21,
  AD_VALIDATE_MED = 22,
}

// Sizes in bytes from firmware common.h
export const DOSE_SCHEDULE_T_SIZE_BYTES = 30;
export const DOSE_EVENT_T_SIZE_BYTES = 12;
export const TEMPERATURE_LOG_T_SIZE_BYTES = 6;
export const DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES = 16;
export const DOCK_CHARGE_STATUS_T_SIZE_BYTES = 5;
export const RING_DOCKED_STATUS_T_SIZE_BYTES = 25;
export const BATTERY_LEVEL_T_SIZE_BYTES = 5;
export const RING_STATUS_T_SIZE_BYTES = 41;

export const MAX_DOSES_PER_DAY = 10;

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

export const PPI_DEFINITIONS: Record<PpiId, PpiDefinition> = {
  [PpiId.AD_DOSE_SCHEDULE]: {
    id: PpiId.AD_DOSE_SCHEDULE,
    name: "PPI_AD_DOSE_SCHEDULE",
    lengths: { rq: 0, re: DOSE_SCHEDULE_T_SIZE_BYTES, push: DOSE_SCHEDULE_T_SIZE_BYTES },
  },
  [PpiId.AD_DOCK_STATUS]: {
    id: PpiId.AD_DOCK_STATUS,
    name: "PPI_AD_DOCK_STATUS",
    lengths: { rq: 0, re: null, push: null },
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
  [PpiId.AD_DOSE_EVENT_REPORT]: {
    id: PpiId.AD_DOSE_EVENT_REPORT,
    name: "PPI_AD_DOSE_EVENT_REPORT",
    lengths: { push: DOSE_EVENT_T_SIZE_BYTES },
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
    lengths: { push: null },
  },
  [PpiId.AD_RING_DEBUG_LOG]: {
    id: PpiId.AD_RING_DEBUG_LOG,
    name: "PPI_AD_RING_DEBUG_LOG",
    lengths: { push: null },
  },
  [PpiId.AD_START_BASELINING]: {
    id: PpiId.AD_START_BASELINING,
    name: "PPI_AD_START_BASELINING",
    lengths: { rq: 1, re: 1 },
  },
  [PpiId.AD_BASELINING_FEEDBACK]: {
    id: PpiId.AD_BASELINING_FEEDBACK,
    name: "PPI_AD_BASELINING_FEEDBACK",
    lengths: { push: null },
  },
  [PpiId.AD_VALIDATE_MED]: {
    id: PpiId.AD_VALIDATE_MED,
    name: "PPI_AD_VALIDATE_MED",
    lengths: { rq: 0, re: 1 },
  },
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
};

export function buildPpiPayload(ppi: PpiId, type: PpiType, payload: Uint8Array): MpPacketPayload {
  return {
    type,
    ppi,
    pktPayloadLen: payload.length,
    payload,
  };
}

export function isTxStatusSendable(status: number): boolean {
  return status === MsgProtTxPacketStatus.ABANDONED || status === MsgProtTxPacketStatus.NEW;
}

export function isRxStatusReadable(status: number): boolean {
  return status === MsgProtRxPacketStatus.NEW;
}

export function getExpectedPayloadLength(ppi: PpiId, type: PpiType): number | null {
  const def = PPI_DEFINITIONS[ppi];
  if (!def) {
    return null;
  }

  switch (type) {
    case PpiType.RQ:
      return def.lengths.rq ?? null;
    case PpiType.RE:
      return def.lengths.re ?? null;
    case PpiType.PUSH:
      return def.lengths.push ?? null;
    default:
      return null;
  }
}

export function validatePayloadLength(ppi: PpiId, type: PpiType, payload: Uint8Array): boolean {
  const expected = getExpectedPayloadLength(ppi, type);
  if (expected === null || expected === undefined) {
    return true;
  }
  return payload.length === expected;
}

export function encodeBool(value: boolean): Uint8Array {
  return new Uint8Array([value ? 1 : 0]);
}

export function decodeBool(payload: Uint8Array): boolean | null {
  if (payload.length !== 1) {
    return null;
  }
  return payload[0] !== 0;
}

export function encodeUint32LE(value: number): Uint8Array {
  const buffer = new ArrayBuffer(4);
  const view = new DataView(buffer);
  view.setUint32(0, value >>> 0, true);
  return new Uint8Array(buffer);
}

export function decodeUint32LE(payload: Uint8Array): number | null {
  if (payload.length !== 4) {
    return null;
  }
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return view.getUint32(0, true);
}

export function encodeDoseSchedulePpi(input: DoseSchedulePpi): Uint8Array {
  // Fixed-size struct from firmware dose_schedule_t.
  const buffer = new ArrayBuffer(DOSE_SCHEDULE_T_SIZE_BYTES);
  const view = new DataView(buffer);

  const doseWindowCount = Math.min(
    MAX_DOSES_PER_DAY,
    Math.max(0, input.dose_window_count >>> 0)
  );
  const doseWindowStartTimes = input.dose_window_start_times_minutes ?? [];

  view.setUint8(0, input.medication_type & 0xff);
  view.setUint16(1, input.dosage_mg & 0xffff, true);
  view.setInt8(3, input.temp_upper_limit_deg_c);
  view.setInt8(4, input.temp_lower_limit_deg_c);
  view.setUint16(5, input.temp_avg_window_duration_sec & 0xffff, true);
  view.setUint8(7, input.dose_days_bitfield & 0xff);
  view.setUint8(8, input.dose_window_duration_minutes & 0xff);
  view.setUint8(9, doseWindowCount);

  let offset = 10;
  for (let i = 0; i < MAX_DOSES_PER_DAY; i += 1) {
    const startMin = doseWindowStartTimes[i] ?? 0;
    view.setUint16(offset, startMin & 0xffff, true);
    offset += 2;
  }

  return new Uint8Array(buffer);
}

export function decodeDoseSchedulePpi(payload: Uint8Array): DoseSchedulePpi | null {
  if (payload.length !== DOSE_SCHEDULE_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  const medication_type = view.getUint8(0);
  const dosage_mg = view.getUint16(1, true);
  const temp_upper_limit_deg_c = view.getInt8(3);
  const temp_lower_limit_deg_c = view.getInt8(4);
  const temp_avg_window_duration_sec = view.getUint16(5, true);
  const dose_days_bitfield = view.getUint8(7);
  const dose_window_duration_minutes = view.getUint8(8);
  const dose_window_count = view.getUint8(9);

  const dose_window_start_times_minutes: number[] = [];
  let offset = 10;
  for (let i = 0; i < MAX_DOSES_PER_DAY; i += 1) {
    dose_window_start_times_minutes.push(view.getUint16(offset, true));
    offset += 2;
  }

  return {
    medication_type,
    dosage_mg,
    temp_upper_limit_deg_c,
    temp_lower_limit_deg_c,
    temp_avg_window_duration_sec,
    dose_days_bitfield,
    dose_window_duration_minutes,
    dose_window_count,
    dose_window_start_times_minutes,
  };
}

export function decodeDoseEventPpi(payload: Uint8Array): DoseEventPpi | null {
  if (payload.length !== DOSE_EVENT_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  // Field layout matches firmware dose_event_t (packed).
  const days_since_epoch = view.getUint16(0, true);
  const event_ctr = view.getUint8(2);
  const start_timestamp_unix_s = view.getUint32(3, true);
  const duration_s = view.getUint16(7, true);
  const dose_completed_in_time = view.getUint8(9);
  const tilt_count = view.getUint16(10, true);

  return {
    event_id: { days_since_epoch, event_ctr },
    start_timestamp_unix_s,
    duration_s,
    dose_completed_in_time,
    tilt_count,
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

export function decodeRingStatus(payload: Uint8Array): RingStatus | null {
  if (payload.length !== RING_STATUS_T_SIZE_BYTES) {
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
  };
}

export type DecodedPpiPayload = {
  ppi: number;
  type: PpiType;
  value: unknown;
};

/**
 * Best-effort PPI decoder. Unknown payloads are returned as raw Uint8Array.
 */
export function decodePpiPayload(ppi: number, type: PpiType, payload: Uint8Array): DecodedPpiPayload {
  switch (ppi) {
    case PpiId.AD_DOSE_SCHEDULE: {
      const value = decodeDoseSchedulePpi(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_RING_STATUS: {
      const value = decodeRingStatus(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_TIME: {
      if (type === PpiType.RQ) {
        const value = payload.length === 0 ? null : payload;
        return { ppi, type, value };
      }
      const value = decodeUint32LE(payload);
      return { ppi, type, value: value ?? payload };
    }
    case PpiId.AD_DOSE_EVENT_REPORT: {
      const value = decodeDoseEventPpi(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_TEMP_LOG: {
      const value = decodeTemperatureLog(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_WEIGHT_LOG: {
      const value = decodeDockWeightMeasurement(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_CHARGE_STATUS: {
      const value = decodeDockChargeStatus(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_RING_DOCKED_STATUS: {
      const value = decodeRingDockedStatus(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_BATT_LEVEL_LOG:
    case PpiId.AD_RING_BATT_LEVEL_LOG: {
      const value = decodeBatteryLevel(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_START_BASELINING: {
      const value = decodeBool(payload);
      return { ppi, type, value: value ?? payload };
    }
    case PpiId.AD_VALIDATE_MED: {
      const value = type === PpiType.RE ? decodeBool(payload) : null;
      return { ppi, type, value: value ?? payload };
    }
    default:
      return { ppi, type, value: payload };
  }
}
