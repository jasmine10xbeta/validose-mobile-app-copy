import {
  BASELINING_FEEDBACK_T_SIZE_BYTES,
  CALIBRATION_FEEDBACK_T_SIZE_BYTES,
  START_CALIBRATION_PARAM_T_SIZE_BYTES,
  WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES,
} from "./constants";
import {
  BaseliningFeedback,
  CalibrationFeedback,
  StartCalibrationParam,
  WeightStackCalibrationRecord,
} from "./types";

export function decodeBaseliningFeedback(payload: Uint8Array): BaseliningFeedback | null {
  if (payload.length !== BASELINING_FEEDBACK_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    current_state: view.getUint8(0),
    motion_state: view.getUint8(1),
    std_dev: view.getUint16(2, true),
    avg_weight_mg: view.getInt32(4, true),
    is_ring_present: view.getUint8(8) !== 0,
    medication_nfc_id: payload.slice(9, 19),
  };
}

export function decodeCalibrationFeedback(payload: Uint8Array): CalibrationFeedback | null {
  if (payload.length !== CALIBRATION_FEEDBACK_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    current_state: view.getUint8(0),
    motion_state: view.getUint8(1),
    std_dev: view.getUint16(2, true),
    avg_weight_mg: view.getInt32(4, true),
    is_ring_present: view.getUint8(8) !== 0,
  };
}

export function decodeStartCalibrationParam(payload: Uint8Array): StartCalibrationParam | null {
  if (payload.length !== START_CALIBRATION_PARAM_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    start: view.getUint8(0) !== 0,
    calibration_weight_mg: view.getUint32(1, true),
  };
}

export function decodeWeightStackCalibrationRecord(
  payload: Uint8Array
): WeightStackCalibrationRecord | null {
  if (payload.length !== WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES) {
    return null;
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    zero_offset: view.getInt32(0, true),
    calibration_factor: view.getInt32(4, true),
    full_assembly_weight_mg: view.getUint32(8, true),
  };
}
