import {
  decodeBaseliningFeedback,
  decodeCalibrationFeedback,
  decodeStartCalibrationParam,
  decodeWeightStackCalibrationRecord,
} from "./decoders.calibration";
import {
  decodeDoseDataPoint,
  decodeDoseEventPpi,
  decodeDoseSchedulePpi,
} from "./decoders.scheduleDose";
import {
  decodeBatteryLevel,
  decodeBluetoothStatus,
  decodeDockChargeStatus,
  decodeDockStatus,
  decodeDockWeightMeasurement,
  decodeRawDebugLog,
  decodeRingDockedStatus,
  decodeRingStatus,
  decodeTemperatureLog,
} from "./decoders.status";
import { decodeBool, decodeUint32LE, decodeUint8 } from "./helpers";
import { DecodedPpiPayload, PpiId, PpiType } from "./types";

/**
 * Best-effort PPI decoder. Unknown payloads are returned as raw Uint8Array.
 */
export function decodePpiPayload(ppi: number, type: PpiType, payload: Uint8Array): DecodedPpiPayload {
  switch (ppi) {
    case PpiId.AD_DOSE_SCHEDULE: {
      const value = decodeDoseSchedulePpi(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_STATUS: {
      const value = decodeDockStatus(payload) ?? payload;
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
    case PpiId.AD_WEIGHT_MEAS_RATE:
    case PpiId.AD_IMU_SAMPLE_RATE:
    case PpiId.AD_DOCK_BATT_SAMPLE_RATE:
    case PpiId.AD_RING_BATT_SAMPLE_RATE:
    case PpiId.AD_DOCK_TEMP_SAMPLE_RATE: {
      const value = payload.length === 0 ? null : payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOSE_EVENT_REPORT: {
      const value = decodeDoseEventPpi(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOSE_DATA_POINT: {
      const value = decodeDoseDataPoint(payload) ?? payload;
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
    case PpiId.AD_BL_STATUS: {
      const value = decodeBluetoothStatus(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_BATT_LEVEL_LOG:
    case PpiId.AD_RING_BATT_LEVEL_LOG: {
      const value = decodeBatteryLevel(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DOCK_DEBUG_LOG:
    case PpiId.AD_RING_DEBUG_LOG: {
      const value = decodeRawDebugLog(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_START_BASELINING: {
      // Firmware workspace currently emits baselining feedback on this PPI as PUSH.
      if (type === PpiType.PUSH) {
        const value = decodeBaseliningFeedback(payload) ?? payload;
        return { ppi, type, value };
      }
      const value = decodeBool(payload);
      return { ppi, type, value: value ?? payload };
    }
    case PpiId.AD_BASELINING_FEEDBACK: {
      const value = decodeBaseliningFeedback(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_VALIDATE_MED: {
      const value = type === PpiType.RE ? decodeBool(payload) : null;
      return { ppi, type, value: value ?? payload };
    }
    case PpiId.AD_START_CALIBRATION: {
      if (type === PpiType.RQ) {
        const value = decodeStartCalibrationParam(payload) ?? payload;
        return { ppi, type, value };
      }
      const value = decodeBool(payload);
      return { ppi, type, value: value ?? payload };
    }
    case PpiId.AD_CALIBRATION_DATA: {
      const value = decodeWeightStackCalibrationRecord(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_CALIBRATION_WEIGHT_PRESENT: {
      const value = decodeBool(payload);
      return { ppi, type, value: value ?? payload };
    }
    case PpiId.AD_CALIBRATION_FEEDBACK: {
      const value = decodeCalibrationFeedback(payload) ?? payload;
      return { ppi, type, value };
    }
    case PpiId.AD_DEVELOPMENT_CMD: {
      const value = decodeUint8(payload);
      return { ppi, type, value: value ?? payload };
    }
    default:
      return { ppi, type, value: payload };
  }
}
