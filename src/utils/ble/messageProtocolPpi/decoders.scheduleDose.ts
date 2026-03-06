import {
  DOSE_DATA_T_SIZE_BYTES,
  DOSE_EVENT_T_SIZE_BYTES,
  DOSE_SCHEDULE_T_SIZE_BYTES,
  MAX_DOSES_PER_DAY,
} from "./constants";
import { DoseDataPoint, DoseEventPpi, DoseSchedulePpi } from "./types";

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

export function decodeDoseDataPoint(payload: Uint8Array): DoseDataPoint | null {
  if (payload.length !== DOSE_DATA_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    event_id: {
      days_since_epoch: view.getUint16(0, true),
      event_ctr: view.getUint8(2),
    },
    relative_timestamp_ms: view.getUint32(3, true),
    accel_x: view.getInt16(7, true),
    accel_y: view.getInt16(9, true),
    accel_z: view.getInt16(11, true),
    angular_accel_x: view.getInt16(13, true),
    angular_accel_y: view.getInt16(15, true),
    angular_accel_z: view.getInt16(17, true),
  };
}
