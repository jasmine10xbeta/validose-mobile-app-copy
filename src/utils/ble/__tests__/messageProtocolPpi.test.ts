import {
  BASELINING_FEEDBACK_T_SIZE_BYTES,
  CALIBRATION_FEEDBACK_T_SIZE_BYTES,
  DOCK_STATUS_T_SIZE_BYTES,
  DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES,
  DOSE_EVENT_T_SIZE_BYTES,
  DOSE_SCHEDULE_T_SIZE_BYTES,
  PpiId,
  PpiType,
  RAW_DEBUG_LOG_T_SIZE_BYTES,
  RING_STATUS_T_LEGACY_SIZE_BYTES,
  RING_STATUS_T_SIZE_BYTES,
  decodeDockWeightMeasurement,
  decodeDoseEventPpi,
  decodeDoseSchedulePpi,
  decodePpiPayload,
  decodeRingStatus,
  encodeDoseSchedulePpi,
  encodeUint32LE,
  getExpectedPayloadLength,
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

  test("decode ring_status_t uses 47-byte layout", () => {
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
    offset += 2;
    view.setUint16(offset, 3, true); // cap_on
    offset += 2;
    view.setUint16(offset, 4, true); // cap_off
    offset += 2;
    view.setUint16(offset, 5, true); // current_prox

    const decoded = decodeRingStatus(payload);
    expect(decoded).not.toBeNull();
    expect(decoded?.hardware_version).toEqual({ major: 1, minor: 2, patch: 3 });
    expect(decoded?.firmware_version).toEqual({ major: 4, minor: 5, patch: 6 });
    expect(decoded?.battery_charge_status).toBe(2);
    expect(decoded?.temperature_celsius).toBe(28);
    expect(decoded?.error_fifo_used_percent).toBe(13);
    expect(decoded?.battery_sample_frequency_millihz).toBe(250);
    expect(decoded?.cap_on).toBe(3);
    expect(decoded?.cap_off).toBe(4);
    expect(decoded?.current_prox).toBe(5);
    expect((decoded as any)?.docking_fifo_used_percent).toBeUndefined();
  });

  test("decode ring_status_t supports legacy 41-byte layout", () => {
    const payload = new Uint8Array(RING_STATUS_T_LEGACY_SIZE_BYTES);
    const view = new DataView(payload.buffer);

    let offset = 0;
    view.setUint8(offset++, 1);
    view.setUint8(offset++, 0);
    view.setUint8(offset++, 9);
    view.setUint8(offset++, 2);
    view.setUint8(offset++, 3);
    view.setUint8(offset++, 4);
    for (let i = 0; i < 10; i += 1) {
      view.setUint8(offset++, i + 1);
    }
    view.setUint32(offset, 1700000100, true);
    offset += 4;
    view.setUint32(offset, 1690000000, true);
    offset += 4;
    view.setUint8(offset++, 1);
    view.setUint32(offset, 777, true);
    offset += 4;
    view.setInt16(offset, 31, true);
    offset += 2;
    view.setUint8(offset++, 21);
    view.setUint8(offset++, 22);
    view.setUint8(offset++, 23);
    view.setUint8(offset++, 24);
    view.setUint8(offset++, 25);
    view.setUint8(offset++, 26);
    view.setUint8(offset++, 27);
    view.setUint8(offset++, 28);
    view.setUint16(offset, 500, true);

    const decoded = decodeRingStatus(payload);
    expect(decoded).not.toBeNull();
    expect(decoded?.hardware_version).toEqual({ major: 1, minor: 0, patch: 9 });
    expect(decoded?.firmware_version).toEqual({ major: 2, minor: 3, patch: 4 });
    expect(decoded?.timestamp_unix_s).toBe(1700000100);
    expect(decoded?.battery_charge_status).toBe(1);
    expect(decoded?.temperature_celsius).toBe(31);
    expect(decoded?.battery_sample_frequency_millihz).toBe(500);
    expect(decoded?.cap_on).toBe(0);
    expect(decoded?.cap_off).toBe(0);
    expect(decoded?.current_prox).toBe(0);
  });

  test("AD_TIME payload lengths match firmware contract", () => {
    expect(getExpectedPayloadLength(PpiId.AD_TIME, PpiType.RQ)).toBe(0);
    expect(getExpectedPayloadLength(PpiId.AD_TIME, PpiType.RE)).toBe(4);
    expect(getExpectedPayloadLength(PpiId.AD_TIME, PpiType.PUSH)).toBe(4);
  });

  test("dock and calibration payload lengths match firmware workspace", () => {
    expect(getExpectedPayloadLength(PpiId.AD_DOCK_STATUS, PpiType.RQ)).toBe(0);
    expect(getExpectedPayloadLength(PpiId.AD_DOCK_STATUS, PpiType.RE)).toBe(
      DOCK_STATUS_T_SIZE_BYTES
    );
    expect(getExpectedPayloadLength(PpiId.AD_START_CALIBRATION, PpiType.RQ)).toBe(5);
    expect(getExpectedPayloadLength(PpiId.AD_START_CALIBRATION, PpiType.RE)).toBe(1);
    expect(getExpectedPayloadLength(PpiId.AD_CALIBRATION_DATA, PpiType.RQ)).toBe(0);
    expect(getExpectedPayloadLength(PpiId.AD_CALIBRATION_DATA, PpiType.RE)).toBe(12);
    expect(getExpectedPayloadLength(PpiId.AD_CALIBRATION_DATA, PpiType.PUSH)).toBe(12);
    expect(getExpectedPayloadLength(PpiId.AD_CALIBRATION_FEEDBACK, PpiType.PUSH)).toBe(
      CALIBRATION_FEEDBACK_T_SIZE_BYTES
    );
    expect(getExpectedPayloadLength(PpiId.AD_DOCK_DEBUG_LOG, PpiType.PUSH)).toBe(
      RAW_DEBUG_LOG_T_SIZE_BYTES
    );
    expect(getExpectedPayloadLength(PpiId.AD_RING_DEBUG_LOG, PpiType.PUSH)).toBe(
      RAW_DEBUG_LOG_T_SIZE_BYTES
    );
  });

  test("AD_DOCK_DEBUG_LOG PUSH decodes raw_debug_log_t payload", () => {
    const payload = new Uint8Array(RAW_DEBUG_LOG_T_SIZE_BYTES);
    const view = new DataView(payload.buffer);
    view.setUint8(0, 3); // level
    view.setUint32(1, 1700000000, true); // timestamp
    view.setUint8(5, 19); // module_id
    view.setUint16(6, 321, true); // line
    for (let i = 0; i < 6; i += 1) {
      view.setUint32(8 + i * 4, i + 10, true);
    }

    const decoded = decodePpiPayload(PpiId.AD_DOCK_DEBUG_LOG, PpiType.PUSH, payload);
    expect(decoded.value).toEqual({
      level: 3,
      timestamp: 1700000000,
      module_id: 19,
      line: 321,
      arg_values: [10, 11, 12, 13, 14, 15],
    });
  });

  test("AD_START_BASELINING PUSH decodes firmware feedback payload", () => {
    const payload = new Uint8Array(BASELINING_FEEDBACK_T_SIZE_BYTES);
    const view = new DataView(payload.buffer);

    view.setUint8(0, 4); // current_state
    view.setUint8(1, 1); // motion_state
    view.setUint16(2, 77, true); // std_dev
    view.setInt32(4, 123456, true); // avg_weight_mg
    view.setUint8(8, 1); // is_ring_present
    for (let i = 0; i < 10; i += 1) {
      view.setUint8(9 + i, i + 1);
    }

    const decoded = decodePpiPayload(PpiId.AD_START_BASELINING, PpiType.PUSH, payload);
    expect(decoded.value).toMatchObject({
      current_state: 4,
      motion_state: 1,
      std_dev: 77,
      avg_weight_mg: 123456,
      is_ring_present: true,
    });
    expect(((decoded.value as any).medication_nfc_id as Uint8Array).length).toBe(10);
  });

  test("AD_CALIBRATION_FEEDBACK PUSH decodes calibration feedback payload", () => {
    const payload = new Uint8Array(CALIBRATION_FEEDBACK_T_SIZE_BYTES);
    const view = new DataView(payload.buffer);

    view.setUint8(0, 2); // current_state
    view.setUint8(1, 0); // motion_state
    view.setUint16(2, 12, true); // std_dev
    view.setInt32(4, -50, true); // avg_weight_mg
    view.setUint8(8, 0); // is_ring_present

    const decoded = decodePpiPayload(PpiId.AD_CALIBRATION_FEEDBACK, PpiType.PUSH, payload);
    expect(decoded.value).toEqual({
      current_state: 2,
      motion_state: 0,
      std_dev: 12,
      avg_weight_mg: -50,
      is_ring_present: false,
    });
  });

  test("AD_TIME decode handles RQ as empty and RE/PUSH as unix uint32", () => {
    const rqDecoded = decodePpiPayload(PpiId.AD_TIME, PpiType.RQ, new Uint8Array(0));
    expect(rqDecoded.value).toBeNull();

    const unixTime = 1700000000;
    const payload = encodeUint32LE(unixTime);

    const reDecoded = decodePpiPayload(PpiId.AD_TIME, PpiType.RE, payload);
    expect(reDecoded.value).toBe(unixTime);

    const pushDecoded = decodePpiPayload(PpiId.AD_TIME, PpiType.PUSH, payload);
    expect(pushDecoded.value).toBe(unixTime);
  });
});
