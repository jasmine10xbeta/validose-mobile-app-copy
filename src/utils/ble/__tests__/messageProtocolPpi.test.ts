import {
  DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES,
  DOSE_EVENT_T_SIZE_BYTES,
  DOSE_SCHEDULE_T_SIZE_BYTES,
  RING_STATUS_T_SIZE_BYTES,
  decodeDockWeightMeasurement,
  decodeDoseEventPpi,
  decodeDoseSchedulePpi,
  decodeRingStatus,
  encodeDoseSchedulePpi,
} from "../messageProtocolPpi";

describe("messageProtocolPpi firmware layout parity", () => {
  test("encode/decode dose_schedule_t uses 30-byte layout", () => {
    const encoded = encodeDoseSchedulePpi({
      medication_type: 0,
      dosage_mg: 250,
      temp_upper_limit_deg_c: 42,
      temp_lower_limit_deg_c: 5,
      temp_avg_window_duration_sec: 900,
      dose_days_bitfield: 0x7f,
      dose_window_duration_minutes: 30,
      dose_window_count: 3,
      dose_window_start_times_minutes: [480, 840, 1260],
    });

    expect(encoded.length).toBe(DOSE_SCHEDULE_T_SIZE_BYTES);

    const decoded = decodeDoseSchedulePpi(encoded);
    expect(decoded).not.toBeNull();
    expect(decoded?.dosage_mg).toBe(250);
    expect(decoded?.dose_window_count).toBe(3);
    expect(decoded?.dose_window_start_times_minutes.slice(0, 3)).toEqual([480, 840, 1260]);
  });

  test("decode dose_event_t uses 12-byte layout", () => {
    const payload = new Uint8Array(DOSE_EVENT_T_SIZE_BYTES);
    const view = new DataView(payload.buffer);

    view.setUint16(0, 123, true); // days_since_epoch
    view.setUint8(2, 9); // event_ctr
    view.setUint32(3, 1710000000, true); // start_timestamp_unix_s
    view.setUint16(7, 75, true); // duration_s
    view.setUint8(9, 1); // dose_completed_in_time
    view.setUint16(10, 11, true); // tilt_count

    const decoded = decodeDoseEventPpi(payload);
    expect(decoded).not.toBeNull();
    expect(decoded?.event_id.days_since_epoch).toBe(123);
    expect(decoded?.event_id.event_ctr).toBe(9);
    expect(decoded?.start_timestamp_unix_s).toBe(1710000000);
    expect(decoded?.duration_s).toBe(75);
    expect(decoded?.dose_completed_in_time).toBe(1);
    expect(decoded?.tilt_count).toBe(11);
  });

  test("decode dock_weight_measurement_t uses 16-byte layout", () => {
    const payload = new Uint8Array(DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES);
    const view = new DataView(payload.buffer);

    view.setInt32(0, 123456, true); // weight_mg
    view.setUint32(4, 654321, true); // total_dispensed_mg
    view.setUint16(8, 55, true); // std_dev
    view.setInt16(10, 215, true); // temperature_deg_c_div10
    view.setUint32(12, 1700000000, true); // timestamp_unix_s

    const decoded = decodeDockWeightMeasurement(payload);
    expect(decoded).not.toBeNull();
    expect(decoded?.weight_mg).toBe(123456);
    expect(decoded?.total_dispensed_mg).toBe(654321);
    expect(decoded?.std_dev).toBe(55);
    expect(decoded?.temperature_deg_c_div10).toBe(215);
    expect(decoded?.timestamp_unix_s).toBe(1700000000);
  });

  test("decode ring_status_t uses 41-byte layout", () => {
    const payload = new Uint8Array(RING_STATUS_T_SIZE_BYTES);
    const view = new DataView(payload.buffer);

    let offset = 0;
    // hardware semantic version
    view.setUint8(offset++, 1);
    view.setUint8(offset++, 2);
    view.setUint8(offset++, 3);
    // firmware semantic version
    view.setUint8(offset++, 4);
    view.setUint8(offset++, 5);
    view.setUint8(offset++, 6);

    for (let i = 0; i < 10; i += 1) {
      view.setUint8(offset++, i + 10);
    }

    view.setUint32(offset, 1700000001, true);
    offset += 4;
    view.setUint32(offset, 1700000002, true);
    offset += 4;
    view.setUint8(offset++, 2);
    view.setUint32(offset, 54321, true);
    offset += 4;
    view.setInt16(offset, 28, true);
    offset += 2;

    view.setUint8(offset++, 10); // dose_fifo_used_percent
    view.setUint8(offset++, 11); // battery_fifo_used_percent
    view.setUint8(offset++, 12); // imu_fifo_used_percent
    view.setUint8(offset++, 13); // error_fifo_used_percent
    view.setUint8(offset++, 14); // dose_fifo_used_percent_watermark
    view.setUint8(offset++, 15); // battery_fifo_used_percent_watermark
    view.setUint8(offset++, 16); // imu_fifo_used_percent_watermark
    view.setUint8(offset++, 17); // error_fifo_used_percent_watermark
    view.setUint16(offset, 250, true); // battery_sample_frequency_millihz

    const decoded = decodeRingStatus(payload);
    expect(decoded).not.toBeNull();
    expect(decoded?.hardware_version).toEqual({ major: 1, minor: 2, patch: 3 });
    expect(decoded?.firmware_version).toEqual({ major: 4, minor: 5, patch: 6 });
    expect(decoded?.battery_charge_status).toBe(2);
    expect(decoded?.temperature_celsius).toBe(28);
    expect(decoded?.error_fifo_used_percent).toBe(13);
    expect(decoded?.battery_sample_frequency_millihz).toBe(250);
    expect((decoded as any)?.docking_fifo_used_percent).toBeUndefined();
  });
});
