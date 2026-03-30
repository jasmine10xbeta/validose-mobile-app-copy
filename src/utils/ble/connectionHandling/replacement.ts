import { Buffer } from "buffer";

import { REPLACEMENT_FLOW_SIGNAL } from "@/constants/replacementFlow";
import { bleLog, bleLogWarn } from "@/utils/ble/logger";
import { writeCharacteristic } from "../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { MsgProtError, MpPacketPayload, subscribeToBleCharacteristic } from "../messageProtocol";
import {
  buildPpiPayload,
  decodePpiPayload,
  encodeBool,
  encodeDoseSchedulePpi,
  MAX_DOSES_PER_DAY,
  PpiId,
  PpiType,
  type DoseSchedulePpi,
} from "../messageProtocolPpi";
import { ensureProtocolReadyForDataSend, waitForTxSendable } from "./protocol";
import { getMessageProtocolInstance } from "./state";

type ReplacementFlowHexListener = (hex: string) => void;
type ReplacementStageSignal =
  | "step1"
  | "checking1"
  | "step2"
  | "step3"
  | "step3Docking"
  | "step4Checking"
  | "success"
  | "error";

const replacementFlowListeners = new Set<ReplacementFlowHexListener>();
let replacementFlowRegisteredProtocol: ReturnType<typeof getMessageProtocolInstance> | null = null;
const REPLACEMENT_STAGE_SIGNAL_PREFIX = "stage:";
let hasSentValidateMedForActiveFlow = false;
let hasSentDoseScheduleForActiveFlow = false;
let isAutoFinalizeInProgressForActiveFlow = false;

const REPLACEMENT_DOSE_SCHEDULE_PUSH_A: DoseSchedulePpi = {
  medication_type: 0,
  dosage_mg: 2,
  temp_upper_limit_deg_c: 60,
  temp_lower_limit_deg_c: 0,
  temp_avg_window_duration_sec: 1800,
  dose_days_bitfield: 0x7f,
  dose_window_duration_minutes: 30,
  dose_window_count: 4,
  dose_window_start_times_minutes: [630, 840, 1050, 1260].slice(0, MAX_DOSES_PER_DAY),
};

function toReplacementStageSignal(stage: ReplacementStageSignal): string {
  return `${REPLACEMENT_STAGE_SIGNAL_PREFIX}${stage}`;
}

function resetAutoFinalizeState(): void {
  hasSentValidateMedForActiveFlow = false;
  hasSentDoseScheduleForActiveFlow = false;
  isAutoFinalizeInProgressForActiveFlow = false;
}

function normalizeHex(rawHex: string): string | null {
  const normalized = rawHex.replace(/[^0-9a-fA-F]/g, "").toLowerCase();
  if (!normalized.length || normalized.length % 2 !== 0) {
    return null;
  }
  return normalized;
}

function isCharacteristicNotFoundError(error: unknown): boolean {
  const normalized = String(error ?? "").toLowerCase();
  return (
    normalized.includes("characteristic with the given uuid not found") ||
    normalized.includes("characteristic with uuid not found") ||
    normalized.includes("characteristic_not_found")
  );
}

function getUuidShortForm(uuid: string): string | null {
  const normalized = uuid.replace(/[^0-9a-fA-F]/g, "").toLowerCase();
  if (normalized.length < 8) {
    return null;
  }
  const short = normalized.slice(4, 8);
  return short.length === 4 ? short : null;
}

async function writeReplacementLegacyCharacteristic(
  characteristicUuid: string,
  base64Value: string
): Promise<boolean> {
  const direct = await writeCharacteristic(characteristicUuid, base64Value);
  if (direct === true) {
    return true;
  }

  const shortUuid = getUuidShortForm(characteristicUuid);
  if (!shortUuid) {
    return false;
  }

  const shortForm = await writeCharacteristic(shortUuid, base64Value);
  return shortForm === true;
}

async function sendPpiViaMessageProtocol(
  ppi: PpiId,
  type: PpiType,
  payload: Uint8Array,
  context: string,
): Promise<boolean> {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) {
    return false;
  }

  const txReady = await ensureProtocolReadyForDataSend(
    messageProtocol,
    context
  );
  if (!txReady) {
    bleLogWarn(`[Replacement] Message protocol TX not ready for ${context}.`);
    return false;
  }

  const result = messageProtocol.send(
    buildPpiPayload(ppi, type, payload)
  );

  const decoded = decodePpiPayload(ppi, type as PpiType, payload).value;
  bleLog("[MP][TX][Replacement]", {
    context,
    ppi,
    ppiName: PpiId[ppi as PpiId] ?? `PPI_${ppi}`,
    type,
    typeName: PpiType[type as PpiType] ?? `TYPE_${type}`,
    payloadHex: Buffer.from(payload).toString("hex"),
    decoded,
    sendResult: result,
  });

  if (result !== MsgProtError.NONE) {
    bleLogWarn(`[Replacement] Message protocol send failed for ${context}.`, {
      ppi,
      type,
      result,
    });
    return false;
  }

  await messageProtocol.process();
  const completed = await waitForTxSendable(messageProtocol);
  if (!completed) {
    bleLogWarn(`[Replacement] Message protocol TX did not complete for ${context}.`, {
      ppi,
      type,
    });
  }

  return completed;
}

async function writeReplacementViaMessageProtocol(commandByte: number, label: string): Promise<boolean> {
  return sendPpiViaMessageProtocol(
    PpiId.AD_DEVELOPMENT_CMD,
    PpiType.RQ,
    new Uint8Array([commandByte & 0xff]),
    `replacement ${label} command`
  );
}

async function writeBaseliningStartStopViaMessageProtocol(
  start: boolean,
  label: string
): Promise<boolean> {
  return sendPpiViaMessageProtocol(
    PpiId.AD_START_BASELINING,
    PpiType.RQ,
    encodeBool(start),
    `replacement ${label} baselining start=${start}`
  );
}

async function writeValidateMedViaMessageProtocol(success: boolean): Promise<boolean> {
  return sendPpiViaMessageProtocol(
    PpiId.AD_VALIDATE_MED,
    PpiType.RE,
    encodeBool(success),
    `replacement validate med success=${success}`
  );
}

async function writeDoseSchedulePushAViaMessageProtocol(): Promise<boolean> {
  return sendPpiViaMessageProtocol(
    PpiId.AD_DOSE_SCHEDULE,
    PpiType.PUSH,
    encodeDoseSchedulePpi(REPLACEMENT_DOSE_SCHEDULE_PUSH_A),
    "replacement push dose schedule A"
  );
}

function dispatchReplacementFlowHex(rawHex: string): void {
  const normalizedHex = rawHex.trim().toLowerCase();
  if (!normalizedHex) {
    return;
  }

  for (const listener of replacementFlowListeners) {
    try {
      listener(normalizedHex);
    } catch (listenerError) {
      bleLogWarn("[Replacement] Replacement flow listener failed.", listenerError);
    }
  }
}

function decodeFeedbackRecord(packet: MpPacketPayload): Record<string, unknown> | null {
  const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload).value;
  if (!decoded || typeof decoded !== "object" || Array.isArray(decoded)) {
    return null;
  }

  return decoded as Record<string, unknown>;
}

function extractCurrentStateFromFeedbackRecord(decoded: Record<string, unknown>): number | null {
  const maybeState = decoded.current_state;
  if (typeof maybeState !== "number" || !Number.isFinite(maybeState)) {
    return null;
  }

  return maybeState;
}

function mapBaseliningStateToSignal(currentState: number): string | null {
  if (currentState <= 0) return toReplacementStageSignal("step1");
  if (currentState === 1) return toReplacementStageSignal("checking1");
  if (currentState === 2) return toReplacementStageSignal("step2");
  if (currentState === 3) return toReplacementStageSignal("step3");
  if (currentState === 4) return toReplacementStageSignal("step3Docking");
  if (currentState === 5) return toReplacementStageSignal("step4Checking");
  if (currentState === 6) return toReplacementStageSignal("success");
  if (currentState >= 7) return toReplacementStageSignal("error");
  return null;
}

function mapCalibrationStateToSignal(currentState: number): string | null {
  if (currentState <= 0) return toReplacementStageSignal("step1");
  if (currentState === 1) return toReplacementStageSignal("checking1");
  if (currentState === 2) return toReplacementStageSignal("step2");
  if (currentState === 3) return toReplacementStageSignal("step3");
  if (currentState === 4) return toReplacementStageSignal("step4Checking");
  if (currentState === 5) return toReplacementStageSignal("success");
  if (currentState >= 6) return toReplacementStageSignal("error");
  return null;
}

function decodeReplacementFlowFeedbackPacket(packet: MpPacketPayload): {
  flow: "BASELINING" | "CALIBRATION";
  currentState: number;
  decoded: Record<string, unknown>;
} | null {
  const isBaseliningPacket =
    packet.ppi === PpiId.AD_START_BASELINING ||
    packet.ppi === PpiId.AD_BASELINING_FEEDBACK;
  const isCalibrationPacket =
    packet.ppi === PpiId.AD_START_CALIBRATION ||
    packet.ppi === PpiId.AD_CALIBRATION_FEEDBACK;

  if (!isBaseliningPacket && !isCalibrationPacket) {
    return null;
  }

  const decoded = decodeFeedbackRecord(packet);
  const decodedCurrentState = decoded ? extractCurrentStateFromFeedbackRecord(decoded) : null;
  const fallbackCurrentState =
    packet.payload.length > 0 && Number.isFinite(packet.payload[0]) ? packet.payload[0] : null;
  const currentState = decodedCurrentState ?? fallbackCurrentState;

  if (currentState === null) {
    return null;
  }

  const normalizedDecoded = decoded ?? { current_state: currentState };

  if (isBaseliningPacket) {
    return { flow: "BASELINING", currentState, decoded: normalizedDecoded };
  }

  if (isCalibrationPacket) {
    return { flow: "CALIBRATION", currentState, decoded: normalizedDecoded };
  }

  return null;
}

function maybeAutoFinalizeBaseliningForReplacement(currentState: number): void {
  if (currentState <= 2 || currentState >= 6) {
    resetAutoFinalizeState();
    return;
  }

  if (currentState !== 3 && currentState !== 4 && currentState !== 5) {
    return;
  }

  if (isAutoFinalizeInProgressForActiveFlow) {
    return;
  }

  if (hasSentValidateMedForActiveFlow && hasSentDoseScheduleForActiveFlow) {
    return;
  }

  isAutoFinalizeInProgressForActiveFlow = true;
  void (async () => {
    if (!hasSentValidateMedForActiveFlow) {
      const validateSent = await writeValidateMedViaMessageProtocol(true);
      if (!validateSent) {
        bleLogWarn("[Replacement] Failed to auto-send Validate Med response.");
        return;
      }

      hasSentValidateMedForActiveFlow = true;
      bleLog("[Replacement] Auto-sent Validate Med OK for baselining flow.");
    }

    if (!hasSentDoseScheduleForActiveFlow) {
      const doseScheduleSent = await writeDoseSchedulePushAViaMessageProtocol();
      if (!doseScheduleSent) {
        bleLogWarn("[Replacement] Failed to auto-send Dose Schedule PUSH A after validation.");
        return;
      }

      hasSentDoseScheduleForActiveFlow = true;
      bleLog("[Replacement] Auto-sent Dose Schedule PUSH A for baselining flow.");
    }
  })()
    .catch((error) => {
      bleLogWarn("[Replacement] Auto baselining finalize sequence failed.", error);
    })
    .finally(() => {
      isAutoFinalizeInProgressForActiveFlow = false;
    });
}

function handleReplacementFlowPacket(packet: MpPacketPayload): void {
  if (!packet?.payload) {
    return;
  }

  const feedback = decodeReplacementFlowFeedbackPacket(packet);
  if (feedback) {
    bleLog("[Replacement] Flow feedback received.", {
      flow: feedback.flow,
      ppi: packet.ppi,
      type: packet.type,
      currentState: feedback.currentState,
      decoded: feedback.decoded,
    });

    if (feedback.flow === "BASELINING") {
      maybeAutoFinalizeBaseliningForReplacement(feedback.currentState);
    }

    const mappedSignal =
      feedback.flow === "BASELINING"
        ? mapBaseliningStateToSignal(feedback.currentState)
        : mapCalibrationStateToSignal(feedback.currentState);

    if (mappedSignal) {
      dispatchReplacementFlowHex(mappedSignal);
    }

    return;
  }

  if (packet.ppi !== PpiId.AD_DEVELOPMENT_CMD || packet.payload.length === 0) {
    return;
  }

  const payloadHex = Buffer.from(packet.payload).toString("hex").trim().toLowerCase();
  if (!payloadHex) {
    return;
  }

  dispatchReplacementFlowHex(payloadHex);
}

function registerMessageProtocolReplacementFlowHandler(): boolean {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) {
    return false;
  }

  if (replacementFlowRegisteredProtocol === messageProtocol) {
    return true;
  }

  messageProtocol.registerRxHandler(PpiId.AD_DEVELOPMENT_CMD, "*", handleReplacementFlowPacket);
  messageProtocol.registerRxHandler(PpiId.AD_START_BASELINING, "*", handleReplacementFlowPacket);
  messageProtocol.registerRxHandler(PpiId.AD_BASELINING_FEEDBACK, "*", handleReplacementFlowPacket);
  messageProtocol.registerRxHandler(PpiId.AD_START_CALIBRATION, "*", handleReplacementFlowPacket);
  messageProtocol.registerRxHandler(PpiId.AD_CALIBRATION_FEEDBACK, "*", handleReplacementFlowPacket);
  replacementFlowRegisteredProtocol = messageProtocol;

  return true;
}

async function waitForMessageProtocolReplacementHandler(timeoutMs = 2500): Promise<boolean> {
  if (registerMessageProtocolReplacementFlowHandler()) {
    return true;
  }

  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    await new Promise((resolve) => setTimeout(resolve, 100));
    if (registerMessageProtocolReplacementFlowHandler()) {
      return true;
    }
  }

  return registerMessageProtocolReplacementFlowHandler();
}

async function writeReplacementCommand(
  commandHex: string,
  label: string,
  options?: { baseliningStart?: boolean }
): Promise<boolean> {
  const normalized = normalizeHex(commandHex);
  if (!normalized) {
    bleLogWarn(`[Replacement] Invalid ${label} command hex.`, { commandHex });
    return false;
  }

  const commandByte = Number.parseInt(normalized.slice(0, 2), 16);
  const base64Value = Buffer.from(normalized, "hex").toString("base64");
  const hasMessageProtocol = !!getMessageProtocolInstance();

  if (typeof options?.baseliningStart === "boolean") {
    resetAutoFinalizeState();
  }

  if (hasMessageProtocol) {
    if (typeof options?.baseliningStart === "boolean") {
      const sentBaseliningControl = await writeBaseliningStartStopViaMessageProtocol(
        options.baseliningStart,
        label
      );
      if (sentBaseliningControl) {
        return true;
      }
      bleLogWarn(
        `[Replacement] Failed AD_START_BASELINING for ${label}. Falling back to development command path.`
      );
    }

    const sentViaMessageProtocol = await writeReplacementViaMessageProtocol(commandByte, label);
    if (sentViaMessageProtocol) {
      return true;
    }
    bleLogWarn(
      `[Replacement] Message protocol command failed for ${label}. Falling back to legacy replacement characteristic.`
    );
  }

  try {
    const success = await writeReplacementLegacyCharacteristic(
      REPLACEMENT_FLOW_SIGNAL.characteristicUuid,
      base64Value
    );
    if (success) {
      return true;
    }

    bleLogWarn(
      `[Replacement] Legacy characteristic write returned false for ${label}.`
    );
    return false;
  } catch (error) {
    if (isCharacteristicNotFoundError(error)) {
      if (!hasMessageProtocol) {
        bleLogWarn(
          `[Replacement] Legacy replacement characteristic missing for ${label}.`
        );
      }
      return false;
    }
    bleLogWarn(`[Replacement] Failed to send ${label} command.`, error);
    return false;
  }
}

export async function writeReplacementProcessStarted(): Promise<boolean> {
  return writeReplacementCommand(
    REPLACEMENT_FLOW_SIGNAL.processStartedWriteHex,
    "process started",
    { baseliningStart: true }
  );
}

export async function writeReplacementProcessStopped(): Promise<boolean> {
  return writeReplacementCommand(
    REPLACEMENT_FLOW_SIGNAL.processStoppedWriteHex,
    "process stopped",
    { baseliningStart: false }
  );
}

export async function writeReplacementProcessRestarted(): Promise<boolean> {
  return writeReplacementCommand(
    REPLACEMENT_FLOW_SIGNAL.processRestartedWriteHex,
    "process restarted",
    { baseliningStart: true }
  );
}

export async function subscribeToReplacementFlowSignal(
  listener: ReplacementFlowHexListener
): Promise<() => void> {
  if (await waitForMessageProtocolReplacementHandler()) {
    replacementFlowListeners.add(listener);
    return () => {
      replacementFlowListeners.delete(listener);
    };
  }

  return subscribeToBleCharacteristic(
    REPLACEMENT_FLOW_SIGNAL.characteristicUuid,
    (bytes) => {
      const payloadHex = Buffer.from(bytes).toString("hex").trim().toLowerCase();
      if (!payloadHex) {
        return;
      }
      listener(payloadHex);
    },
    REPLACEMENT_FLOW_SIGNAL.serviceUuid
  );
}
