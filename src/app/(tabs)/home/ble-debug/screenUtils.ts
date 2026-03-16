import { Buffer } from "buffer";

import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "@/constants/ble";
import { PpiId, PpiType } from "@/utils/ble/messageProtocolPpi";

import { QUICK_FLOW_META } from "./constants";
import { formatDecodedValue, prettifyForLog } from "./helpers";
import type {
  BaseliningFeedbackSnapshot,
  CalibrationDataSnapshot,
  CalibrationFeedbackSnapshot,
  MpFramePreview,
  QuickFlowAction,
} from "./types";

type DiscoveredCharacteristic = {
  uuid?: string;
  properties?: string[];
};

type DiscoveredService = {
  uuid?: string;
  characteristics?: DiscoveredCharacteristic[];
};

const MP_MIN_FRAME_LEN = 14; // CRC(2) + header(8) + PPI envelope(4)
const CALIBRATION_COMPLETE_STATE_CODES = new Set<number>([5]);

type BaseliningStateEntry = {
  code: number;
  label: string;
  hint?: string;
};

export type CalibrationStateEntry = {
  code: number;
  label: string;
  stepNumber: number | null;
  instruction: string;
  hint?: string;
};

export const BASELINING_STATE_ENTRIES: BaseliningStateEntry[] = [
  { code: 0, label: "Wait For Ring Removal" },
  { code: 1, label: "Set Dock Weight" },
  { code: 2, label: "Wait For Ring" },
  { code: 3, label: "Set Ring + Dock Weight" },
  { code: 4, label: "Wait For Backend Validation" },
  { code: 5, label: "Set Local Medication UID" },
  { code: 6, label: "Complete" },
  {
    code: 7,
    label: "Error",
    hint: "Restart baselining if this state is reported.",
  },
  { code: 8, label: "Max" },
];

export const CALIBRATION_STATE_ENTRIES: CalibrationStateEntry[] = [
  {
    code: 0,
    label: "Wait For Ring Removal",
    stepNumber: 1,
    instruction: "Remove ring from dock.",
  },
  {
    code: 1,
    label: "Set Dock Weight",
    stepNumber: 2,
    instruction: "Place dock on a stable, level surface and keep it still while tare is captured.",
  },
  {
    code: 2,
    label: "Wait For Calibration Weight",
    stepNumber: 3,
    instruction: "Place calibration weight on the dock, then press Weight Present.",
  },
  {
    code: 3,
    label: "Calibrating",
    stepNumber: 4,
    instruction: "Do not move the dock or calibration weight while calibration is running.",
  },
  {
    code: 4,
    label: "Store Updated Param",
    stepNumber: 5,
    instruction: "Wait while firmware stores updated calibration values.",
  },
  {
    code: 5,
    label: "Complete",
    stepNumber: 6,
    instruction: "Calibration is complete. Request calibration data to verify values.",
  },
  {
    code: 6,
    label: "Error",
    stepNumber: null,
    instruction: "Calibration entered ERROR state. Stop and restart calibration.",
    hint: "Recheck setup stability and ring removal before retrying.",
  },
];

export function getCalibrationStateEntry(stateCode: number | null): CalibrationStateEntry | null {
  if (stateCode === null) {
    return null;
  }
  return CALIBRATION_STATE_ENTRIES.find((entry) => entry.code === stateCode) ?? null;
}

export function getCalibrationInstructionForState(stateCode: number | null): string | null {
  return getCalibrationStateEntry(stateCode)?.instruction ?? null;
}

function normalizeUuidKey(uuid: string): string {
  return uuid.replace(/[^0-9a-fA-F]/g, "").toLowerCase();
}

function isUuidMatchByShortKey(uuid: string, shortUuid: string): boolean {
  const normalized = normalizeUuidKey(uuid);
  const shortKey = shortUuid.toLowerCase();

  if (!normalized) return false;
  if (normalized === shortKey) return true;
  return normalized.startsWith(`0000${shortKey}`);
}

function isUuidEquivalent(uuid: string, expectedUuid: string): boolean {
  return normalizeUuidKey(uuid) === normalizeUuidKey(expectedUuid);
}

function hasProperty(
  characteristic: DiscoveredCharacteristic,
  predicate: (prop: string) => boolean
): boolean {
  return (
    Array.isArray(characteristic.properties) &&
    characteristic.properties.some((prop) => typeof prop === "string" && predicate(prop.toLowerCase()))
  );
}

export function resolveMessageProtocolUuidsFromDiscovery(
  discovery: unknown,
  defaults: {
    txUuid: string;
    rxUuid: string;
    serviceShortUuid: string;
    txShortUuid: string;
    rxShortUuid: string;
  }
): {
  txUuid: string;
  rxUuid: string;
  serviceUuid: string;
  source: string;
} {
  let txUuid = defaults.txUuid;
  let rxUuid = defaults.rxUuid;
  let serviceUuid = SERVICE_UUIDS.MESSAGE_PROTOCOL_SERVICE;
  let source = "defaults";

  if (!Array.isArray(discovery)) {
    return { txUuid, rxUuid, serviceUuid, source };
  }

  const services = discovery as DiscoveredService[];
  type Candidate = {
    txUuid: string;
    rxUuid: string;
    serviceUuid: string;
    source: string;
    score: number;
  };

  const candidates: Candidate[] = [];

  for (const service of services) {
    if (typeof service?.uuid !== "string" || !service.characteristics?.length) {
      continue;
    }

    const characteristics = service.characteristics.filter(
      (characteristic): characteristic is DiscoveredCharacteristic & { uuid: string } =>
        typeof characteristic?.uuid === "string" && characteristic.uuid.length > 0
    );
    if (!characteristics.length) continue;

    const explicitTx = characteristics.find(
      (characteristic) =>
        isUuidMatchByShortKey(characteristic.uuid, defaults.txShortUuid) ||
        isUuidEquivalent(characteristic.uuid, defaults.txUuid) ||
        isUuidEquivalent(characteristic.uuid, CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_NUS_TX)
    );
    const explicitRx = characteristics.find(
      (characteristic) =>
        isUuidMatchByShortKey(characteristic.uuid, defaults.rxShortUuid) ||
        isUuidEquivalent(characteristic.uuid, defaults.rxUuid) ||
        isUuidEquivalent(characteristic.uuid, CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_NUS_RX)
    );
    const writeCharacteristic = characteristics.find((characteristic) =>
      hasProperty(characteristic, (prop) => prop === "write" || prop === "writewithoutresponse")
    );
    const notifyCharacteristic = characteristics.find((characteristic) =>
      hasProperty(characteristic, (prop) => prop === "notify" || prop === "indicate")
    );

    const isPreferredService =
      isUuidMatchByShortKey(service.uuid, defaults.serviceShortUuid) ||
      isUuidEquivalent(service.uuid, SERVICE_UUIDS.MESSAGE_PROTOCOL_NUS_SERVICE);
    const hasWriteNotifyPair = Boolean(writeCharacteristic && notifyCharacteristic);

    const resolvedTx = explicitTx?.uuid ?? writeCharacteristic?.uuid;
    const resolvedRx = explicitRx?.uuid ?? notifyCharacteristic?.uuid;
    const singleCharacteristicUuid =
      !resolvedTx && !resolvedRx && isPreferredService && characteristics.length === 1
        ? characteristics[0].uuid
        : null;

    if (!resolvedTx && !resolvedRx && !singleCharacteristicUuid) {
      continue;
    }
    if (!explicitTx && !explicitRx && !isPreferredService && !hasWriteNotifyPair) {
      continue;
    }

    let score = 0;
    if (isPreferredService) score += 40;
    if (explicitTx) score += 30;
    if (explicitRx) score += 30;
    if (writeCharacteristic) score += 12;
    if (notifyCharacteristic) score += 12;
    if (singleCharacteristicUuid) score += 3;

    const candidateSource = singleCharacteristicUuid
      ? "single-characteristic-fallback"
      : explicitTx || explicitRx
        ? "discovery-explicit-uuid"
        : "discovery-properties";

    candidates.push({
      txUuid: resolvedTx ?? singleCharacteristicUuid ?? defaults.txUuid,
      rxUuid: resolvedRx ?? singleCharacteristicUuid ?? defaults.rxUuid,
      serviceUuid: service.uuid,
      source: candidateSource,
      score,
    });
  }

  candidates.sort((a, b) => b.score - a.score);
  if (candidates[0]) {
    txUuid = candidates[0].txUuid;
    rxUuid = candidates[0].rxUuid;
    serviceUuid = candidates[0].serviceUuid;
    source = candidates[0].source;
  }

  return { txUuid, rxUuid, serviceUuid, source };
}

export function getMpPacketTypeLabel(pktType: number): string {
  switch (pktType) {
    case 0:
      return "DATA";
    case 1:
      return "ACK";
    case 2:
      return "NAK";
    case 3:
      return "SYNC_START";
    case 4:
      return "SYNC_ACK";
    case 5:
      return "SYNC_MISMATCH";
    default:
      return `UNKNOWN_${pktType}`;
  }
}

export function parseMpFrameBytes(raw: Uint8Array): MpFramePreview | null {
  if (!raw.length || raw.length < MP_MIN_FRAME_LEN) return null;

  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  const pktPayloadLen = view.getUint16(12, true);
  const frameLength = MP_MIN_FRAME_LEN + pktPayloadLen;
  if (raw.length < frameLength) return null;

  const packet = raw.subarray(0, frameLength);
  const packetView = new DataView(packet.buffer, packet.byteOffset, packet.byteLength);
  const payload = packet.subarray(MP_MIN_FRAME_LEN);

  return {
    frameLength,
    crc: packetView.getUint16(0, true),
    pktCounter: packetView.getUint16(2, true),
    sessionId: packetView.getUint32(4, true),
    pktType: packetView.getUint8(8),
    status: packetView.getUint8(9),
    payloadType: packetView.getUint8(10),
    payloadPpi: packetView.getUint8(11),
    pktPayloadLen,
    payloadHex: Buffer.from(payload).toString("hex"),
    payloadBase64: Buffer.from(payload).toString("base64"),
    frameHex: Buffer.from(packet).toString("hex"),
    frameBase64: Buffer.from(packet).toString("base64"),
  };
}

export function parseMpFrameHex(frameHex: string): MpFramePreview | null {
  if (!frameHex) return null;
  try {
    return parseMpFrameBytes(new Uint8Array(Buffer.from(frameHex, "hex")));
  } catch {
    return null;
  }
}

export function formatPpiHumanReadable(
  ppi: number,
  type: number,
  _payloadHex: string,
  decodedValue: unknown
): string {
  if (ppi === PpiId.AD_TIME) {
    if (type === PpiType.RQ) {
      return "(empty)";
    }

    if (typeof decodedValue === "number" && Number.isFinite(decodedValue)) {
      return prettifyForLog({
        utc_epoch_s: decodedValue,
      });
    }
  }

  return formatDecodedValue(decodedValue);
}

export function encodeStartCalibrationParamPayload(
  start: boolean,
  calibrationWeightMg: number
): Uint8Array {
  const payload = new Uint8Array(5);
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  view.setUint8(0, start ? 1 : 0);
  view.setUint32(1, calibrationWeightMg >>> 0, true);
  return payload;
}

export function isResponseTypeForRequest(requestType: number, incomingType: number): boolean {
  if (requestType === PpiType.RQ) {
    return incomingType === PpiType.RE;
  }

  if (requestType === PpiType.PUSH) {
    return incomingType === PpiType.RE || incomingType === PpiType.PUSH;
  }

  return incomingType !== PpiType.RQ;
}

export function decodeHexPayload(payloadHex: string): Uint8Array | null {
  const normalizedHex = payloadHex.replace(/[^0-9a-fA-F]/g, "");
  if (!normalizedHex.length) {
    return new Uint8Array(0);
  }

  if (normalizedHex.length % 2 !== 0) {
    return null;
  }

  try {
    return new Uint8Array(Buffer.from(normalizedHex, "hex"));
  } catch {
    return null;
  }
}

export function isQuickFlowAction(action: string): action is QuickFlowAction {
  return Object.prototype.hasOwnProperty.call(QUICK_FLOW_META, action);
}

function asPlainRecord(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return null;
  }
  return value as Record<string, unknown>;
}

function toNullableNumber(value: unknown): number | null {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return null;
  }
  return value;
}

function toNullableBoolean(value: unknown): boolean | null {
  if (typeof value !== "boolean") {
    return null;
  }
  return value;
}

function toHexByteString(value: unknown): string | null {
  if (value instanceof Uint8Array) {
    if (!value.length) return null;
    return Buffer.from(value).toString("hex").toUpperCase();
  }

  if (Array.isArray(value)) {
    const bytes = value.filter((entry): entry is number => typeof entry === "number");
    if (!bytes.length || bytes.length !== value.length) {
      return null;
    }
    return Buffer.from(Uint8Array.from(bytes)).toString("hex").toUpperCase();
  }

  return null;
}

export function describeCalibrationState(stateCode: number | null): string {
  if (stateCode === null) {
    return "--";
  }

  const entry = getCalibrationStateEntry(stateCode);
  if (entry) {
    return `${entry.label} (${stateCode})`;
  }

  if (CALIBRATION_COMPLETE_STATE_CODES.has(stateCode)) {
    return `Complete (${stateCode})`;
  }

  return `State ${stateCode}`;
}

export function describeCalibrationMotion(motionCode: number | null): string {
  if (motionCode === null) {
    return "--";
  }
  if (motionCode === 0) {
    return "Still (0)";
  }
  if (motionCode === 1) {
    return "Moving (1)";
  }
  return `Motion ${motionCode}`;
}

export function describeBaseliningState(stateCode: number | null): string {
  if (stateCode === null) {
    return "--";
  }

  const entry = BASELINING_STATE_ENTRIES.find((item) => item.code === stateCode);
  if (entry) {
    return `${entry.label} (${stateCode})`;
  }

  return `State ${stateCode}`;
}

export function describeBaseliningMotion(motionCode: number | null): string {
  if (motionCode === null) {
    return "--";
  }
  if (motionCode === 0) {
    return "Stable (0)";
  }
  if (motionCode === 1) {
    return "Unstable (1)";
  }
  return `Motion ${motionCode}`;
}

export function extractCalibrationDataSnapshot(
  decoded: unknown,
  updatedAt: string
): CalibrationDataSnapshot | null {
  const valueRecord = asPlainRecord(decoded);
  if (!valueRecord) {
    return null;
  }

  const zeroOffset = toNullableNumber(valueRecord.zero_offset);
  const calibrationFactor = toNullableNumber(valueRecord.calibration_factor);
  const fullAssemblyWeightMg = toNullableNumber(valueRecord.full_assembly_weight_mg);
  if (zeroOffset === null && calibrationFactor === null && fullAssemblyWeightMg === null) {
    return null;
  }

  return {
    updatedAt,
    zeroOffset,
    calibrationFactor,
    fullAssemblyWeightMg,
  };
}

export function extractCalibrationFeedbackSnapshot(
  decoded: unknown,
  updatedAt: string
): CalibrationFeedbackSnapshot | null {
  const valueRecord = asPlainRecord(decoded);
  if (!valueRecord) {
    return null;
  }

  const currentState = toNullableNumber(valueRecord.current_state);
  const motionState = toNullableNumber(valueRecord.motion_state);
  let isComplete = false;

  const completionBoolKeys = [
    "is_complete",
    "complete",
    "completed",
    "calibration_complete",
    "calibration_completed",
    "is_calibration_complete",
  ] as const;
  for (const key of completionBoolKeys) {
    if (valueRecord[key] === true) {
      isComplete = true;
      break;
    }
  }

  if (!isComplete) {
    const completionTextKeys = ["status", "state", "current_state_name"] as const;
    for (const key of completionTextKeys) {
      const rawValue = valueRecord[key];
      if (typeof rawValue === "string") {
        const normalized = rawValue.trim().toLowerCase();
        if (
          normalized.includes("complete") ||
          normalized.includes("completed") ||
          normalized.includes("done") ||
          normalized.includes("success")
        ) {
          isComplete = true;
          break;
        }
      }
    }
  }

  if (!isComplete && currentState !== null) {
    isComplete = CALIBRATION_COMPLETE_STATE_CODES.has(currentState);
  }

  if (currentState === null && motionState === null && !isComplete) {
    return null;
  }

  return {
    updatedAt,
    currentState,
    motionState,
    isComplete,
  };
}

export function extractBaseliningFeedbackSnapshot(
  decoded: unknown,
  updatedAt: string
): BaseliningFeedbackSnapshot | null {
  const valueRecord = asPlainRecord(decoded);
  if (!valueRecord) {
    return null;
  }

  const currentState = toNullableNumber(valueRecord.current_state);
  const motionState = toNullableNumber(valueRecord.motion_state);
  const stdDev = toNullableNumber(valueRecord.std_dev);
  const avgWeightMg = toNullableNumber(valueRecord.avg_weight_mg);
  const isRingPresent = toNullableBoolean(valueRecord.is_ring_present);
  const medicationNfcIdHex = toHexByteString(valueRecord.medication_nfc_id);

  if (
    currentState === null &&
    motionState === null &&
    stdDev === null &&
    avgWeightMg === null &&
    isRingPresent === null &&
    medicationNfcIdHex === null
  ) {
    return null;
  }

  return {
    updatedAt,
    currentState,
    motionState,
    stdDev,
    avgWeightMg,
    isRingPresent,
    medicationNfcIdHex,
  };
}

export function summarizeFeedbackValue(decoded: unknown): string {
  const valueRecord = asPlainRecord(decoded);
  if (!valueRecord) {
    return formatDecodedValue(decoded);
  }

  const summaryParts: string[] = [];
  if (typeof valueRecord.current_state === "number") {
    summaryParts.push(`state_code ${valueRecord.current_state}`);
  }
  if (typeof valueRecord.motion_state === "number") {
    summaryParts.push(`motion_code ${valueRecord.motion_state}`);
  }
  if (typeof valueRecord.avg_weight_mg === "number") {
    summaryParts.push(`avg ${valueRecord.avg_weight_mg}mg`);
  }
  if (typeof valueRecord.std_dev === "number") {
    summaryParts.push(`sd ${valueRecord.std_dev}`);
  }
  if (typeof valueRecord.is_ring_present === "boolean") {
    summaryParts.push(`ring ${valueRecord.is_ring_present ? "on" : "off"}`);
  }

  if (summaryParts.length) {
    return summaryParts.slice(0, 4).join(" · ");
  }

  return formatDecodedValue(decoded);
}

export function summarizeCalibrationResponseValue(ppi: number, decoded: unknown): string {
  if (typeof decoded === "boolean") {
    return decoded ? "accepted=true" : "accepted=false";
  }

  if (ppi === PpiId.AD_CALIBRATION_DATA) {
    const valueRecord = asPlainRecord(decoded);
    if (valueRecord) {
      const summaryParts: string[] = [];
      if (typeof valueRecord.zero_offset === "number") {
        summaryParts.push(`zero_offset ${valueRecord.zero_offset}`);
      }
      if (typeof valueRecord.calibration_factor === "number") {
        summaryParts.push(`factor ${valueRecord.calibration_factor}`);
      }
      if (typeof valueRecord.full_assembly_weight_mg === "number") {
        summaryParts.push(`full ${valueRecord.full_assembly_weight_mg}mg`);
      }
      if (summaryParts.length) {
        return summaryParts.join(" · ");
      }
    }
  }

  return formatDecodedValue(decoded);
}
