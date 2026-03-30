import { Buffer } from "buffer";

import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { bleLog, bleLogWarn } from "@/utils/ble/logger";
import {
  MessageProtocolInterface,
  MsgProtError,
} from "../messageProtocol";
import {
  BatteryLevel,
  buildPpiPayload,
  decodeDoseEventPpi,
  decodePpiPayload,
  isTxStatusSendable,
  PpiId,
  PpiType,
} from "../messageProtocolPpi";
import { TX_READY_POLL_MS, TX_READY_TIMEOUT_MS } from "./constants";
import { getMessageProtocolInstance } from "./state";

export function relayMessageProtocolConsoleLog(
  level: "DBG" | "INFO" | "WARN" | "ERR",
  args: unknown[]
): void {
  const prefix = `[MP][PROTOCOL][${level}]`;
  if (!args.length) {
    bleLog(prefix);
    return;
  }

  const [first, ...rest] = args;
  if (typeof first === "string") {
    const message = `${prefix} ${first}`;
    const payload = rest.length === 0 ? undefined : rest.length === 1 ? rest[0] : rest;
    bleLog(message, payload);
    return;
  }

  bleLog(prefix, args.length === 1 ? args[0] : args);
}

export async function waitForTxSendable(
  protocol: MessageProtocolInterface,
  timeoutMs = TX_READY_TIMEOUT_MS,
  pollMs = TX_READY_POLL_MS
): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() <= deadline) {
    if (isTxStatusSendable(protocol.getTxPacketStatus())) {
      return true;
    }

    // Drive retries/timeouts immediately instead of waiting only for the interval loop.
    await protocol.process();
    await new Promise((resolve) => setTimeout(resolve, pollMs));
  }

  return isTxStatusSendable(protocol.getTxPacketStatus());
}

export async function ensureProtocolReadyForDataSend(
  protocol: MessageProtocolInterface,
  context: string,
  options: {
    timeoutMs?: number;
    pollMs?: number;
  } = {}
): Promise<boolean> {
  if (protocol.getCurrentSessionId() === 0) {
    const syncResult = await protocol.startSync();
    if (syncResult !== MsgProtError.NONE) {
      bleLogWarn(`[MP] Could not start sync before ${context}.`, { syncResult });
      return false;
    }
  }

  const txReady = await waitForTxSendable(
    protocol,
    options.timeoutMs,
    options.pollMs
  );
  if (!txReady) {
    bleLogWarn(`[MP] TX not ready for ${context}; waiting for sync/ACK state.`);
  }

  return txReady;
}

function resolveLegacyBatteryLevel(
  currentDevice: {
    batteryLevel?: number;
    dockBatteryLevel?: number;
    ringBatteryLevel?: number;
  } | null | undefined,
  nextDockBatteryLevel?: number,
  nextRingBatteryLevel?: number,
): number {
  const dockBatteryLevel =
    typeof nextDockBatteryLevel === "number"
      ? nextDockBatteryLevel
      : currentDevice?.dockBatteryLevel;
  const ringBatteryLevel =
    typeof nextRingBatteryLevel === "number"
      ? nextRingBatteryLevel
      : currentDevice?.ringBatteryLevel;

  const levels = [dockBatteryLevel, ringBatteryLevel].filter(
    (level): level is number => typeof level === "number" && Number.isFinite(level),
  );

  if (!levels.length) {
    return typeof currentDevice?.batteryLevel === "number" ? currentDevice.batteryLevel : -1;
  }

  return Math.min(...levels);
}

async function sendPpiRequest(
  ppiId: PpiId,
  options: {
    timeoutMs?: number;
    pollMs?: number;
  } = {}
): Promise<boolean> {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) return false;

  const maxAttempts = 3;

  for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
    const txReady = await waitForTxSendable(
      messageProtocol,
      options.timeoutMs,
      options.pollMs
    );
    if (!txReady) {
      bleLogWarn(`[MP] TX not ready to queue runtime PPI request ${ppiId}.`);
      return false;
    }

    const txPacket = buildPpiPayload(ppiId, PpiType.RQ, new Uint8Array(0));
    const decoded = decodePpiPayload(txPacket.ppi, txPacket.type as PpiType, txPacket.payload);
    bleLog("[MP][TX]", {
      ppi: txPacket.ppi,
      ppiName: PpiId[txPacket.ppi as PpiId] ?? `PPI_${txPacket.ppi}`,
      type: txPacket.type,
      typeName: PpiType[txPacket.type as PpiType] ?? `TYPE_${txPacket.type}`,
      payloadHex: Buffer.from(txPacket.payload).toString("hex"),
      decoded: decoded.value,
    });

    const result = messageProtocol.send(txPacket);
    if (result === MsgProtError.NONE) {
      await messageProtocol.process();
      const completed = await waitForTxSendable(messageProtocol);
      if (!completed) {
        bleLogWarn(`[MP] Runtime PPI request ${ppiId} did not complete in the ready window.`);
      }
      return completed;
    }

    if (result === MsgProtError.BUSY) {
      bleLogWarn(`[MP] TX busy while queueing runtime PPI request ${ppiId}; retrying.`, {
        attempt,
        maxAttempts,
      });
      await messageProtocol.process();
      await new Promise((resolve) => setTimeout(resolve, TX_READY_POLL_MS));
      continue;
    }

    bleLogWarn(`[MP] Failed to queue runtime PPI request ${ppiId}.`, { result });
    return false;
  }

  bleLogWarn(`[MP] Failed to queue runtime PPI request ${ppiId} after retries.`);
  return false;
}

export async function requestRuntimePpiState(
  options: {
    timeoutMs?: number;
    pollMs?: number;
  } = {}
): Promise<void> {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) return;

  const txReady = await ensureProtocolReadyForDataSend(
    messageProtocol,
    "runtime PPI state request",
    options
  );
  if (!txReady) return;

  await sendPpiRequest(PpiId.AD_DOCK_BATT_LEVEL_LOG, options);
  await sendPpiRequest(PpiId.AD_RING_BATT_LEVEL_LOG, options);
  await sendPpiRequest(PpiId.AD_RING_STATUS, options);
}

function buildDeviceKeyCandidates(device: { deviceId: string; deviceName: string }, backendDeviceId?: string): string[] {
  const unique = new Set<string>();
  const candidates = [backendDeviceId, device.deviceId, device.deviceName];

  for (const candidate of candidates) {
    const normalized = typeof candidate === "string" ? candidate.trim() : "";
    if (normalized) {
      unique.add(normalized);
    }
  }

  return Array.from(unique);
}

export function setupMessageProtocolHandlers(deviceIdentifier: string): void {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) {
    return;
  }

  messageProtocol.registerRxHandler(PpiId.AD_DOSE_EVENT_REPORT, "*", async (packet) => {
    if (packet.type !== PpiType.PUSH && packet.type !== PpiType.RE) {
      return;
    }

    const decoded = decodeDoseEventPpi(packet.payload);
    if (!decoded) {
      bleLogWarn("[MP] Dose event payload size mismatch.");
      return;
    }

    bleLog("💊 [MP] Dose event received from device.", {
      deviceIdentifier,
      eventId: decoded.event_id,
      startTimestampUnixS: decoded.start_timestamp_unix_s,
      durationS: decoded.duration_s,
      doseCompletedInTime: decoded.dose_completed_in_time,
      tiltCount: decoded.tilt_count,
    });

    const deviceStore = useDeviceStore.getState();
    const scheduleStore = useScheduleStore.getState();
    const device = deviceStore.getDevice(deviceIdentifier);
    if (!device) {
      bleLogWarn("[MP] Dose event received for unknown device.", { deviceIdentifier });
      return;
    }

    const treatment =
      useTreatmentStore.getState().getDeviceTreatment(device.deviceId) ??
      useTreatmentStore.getState().getDeviceTreatment(device.deviceName);
    if (!treatment?.medication_code) {
      bleLogWarn("[MP] Dose event received but no treatment/medication code is available.", {
        deviceId: device.deviceId,
        deviceName: device.deviceName,
      });
      return;
    }

    const backendDeviceId =
      (typeof treatment.device_id === "string" && treatment.device_id.trim().length
        ? treatment.device_id.trim()
        : "") ||
      (typeof device.deviceName === "string" && device.deviceName.trim().length
        ? device.deviceName.trim()
        : device.deviceId);
    const scheduleDeviceKeys = buildDeviceKeyCandidates(device, backendDeviceId);

    const eventAtIso = new Date(decoded.start_timestamp_unix_s * 1000).toISOString();
    try {
      // Dose event backend sync is now handled through /hardware/ingest only.
      // Keep local schedule acknowledgment for UI state progression.
      let acknowledgedSchedule = null;
      let acknowledgedScheduleDeviceKey = "";

      for (const key of scheduleDeviceKeys) {
        acknowledgedSchedule = scheduleStore.acknowledgeDoseEventByTimestamp(
          key,
          eventAtIso,
          decoded,
        );
        if (acknowledgedSchedule) {
          acknowledgedScheduleDeviceKey = key;
          break;
        }
      }

      if (acknowledgedSchedule?.id && acknowledgedScheduleDeviceKey) {
        scheduleStore.markBackendSynced(acknowledgedScheduleDeviceKey, acknowledgedSchedule.id);
      }

      bleLog("💊 [MP] Received dose event (ingest-only backend flow).", {
        rawDeviceIdentifier: deviceIdentifier,
        deviceId: device.deviceId,
        deviceName: device.deviceName,
        backendDeviceId,
        scheduleDeviceKeys,
        acknowledgedScheduleDeviceKey,
        eventId: decoded.event_id,
        eventAtIso,
        doseCompletedInTime: decoded.dose_completed_in_time,
        durationS: decoded.duration_s,
        tiltCount: decoded.tilt_count,
      });
    } catch (doseEventError) {
      bleLogWarn("[MP] Failed to process local dose event acknowledgment.", {
        rawDeviceIdentifier: deviceIdentifier,
        deviceId: device.deviceId,
        deviceName: device.deviceName,
        backendDeviceId,
        scheduleDeviceKeys,
        eventId: decoded.event_id,
        eventAtIso,
        error: doseEventError instanceof Error ? doseEventError.message : String(doseEventError),
      });
    }
  });

  messageProtocol.registerRxHandler(PpiId.AD_TIME, PpiType.RE, (packet) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    bleLog("🕒 [MP] Time update response", decoded.value);
  });

  const updateDockBatteryFromPacket = (packet: { ppi: number; type: number; payload: Uint8Array }) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    const batteryValue = decoded.value as BatteryLevel | null;
    if (!batteryValue || typeof batteryValue.battery_level !== "number") return;

    const deviceStore = useDeviceStore.getState();
    const currentDevice = deviceStore.getDevice(deviceIdentifier);
    const legacyBatteryLevel = resolveLegacyBatteryLevel(
      currentDevice,
      batteryValue.battery_level,
      undefined,
    );

    deviceStore.updateDevice(deviceIdentifier, {
      dockBatteryLevel: batteryValue.battery_level,
      batteryLevel: legacyBatteryLevel,
    });
  };

  const updateRingBatteryFromPacket = (packet: { ppi: number; type: number; payload: Uint8Array }) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    const batteryValue = decoded.value as BatteryLevel | null;
    if (!batteryValue || typeof batteryValue.battery_level !== "number") return;

    const deviceStore = useDeviceStore.getState();
    const currentDevice = deviceStore.getDevice(deviceIdentifier);
    const legacyBatteryLevel = resolveLegacyBatteryLevel(
      currentDevice,
      undefined,
      batteryValue.battery_level,
    );

    deviceStore.updateDevice(deviceIdentifier, {
      ringBatteryLevel: batteryValue.battery_level,
      batteryLevel: legacyBatteryLevel,
    });
  };

  const updateDeviceErrorFromRingStatus = (packet: { ppi: number; type: number; payload: Uint8Array }) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    const ringStatus = decoded.value as { error_fifo_used_percent?: number } | null;
    const hasError =
      !!ringStatus && typeof ringStatus.error_fifo_used_percent === "number"
        ? ringStatus.error_fifo_used_percent > 0
        : false;

    useDeviceStore.getState().updateDevice(deviceIdentifier, {
      error: hasError ? "ERROR" : "",
    });
  };

  messageProtocol.registerRxHandler(PpiId.AD_DOCK_BATT_LEVEL_LOG, PpiType.PUSH, updateDockBatteryFromPacket);
  messageProtocol.registerRxHandler(PpiId.AD_DOCK_BATT_LEVEL_LOG, PpiType.RE, updateDockBatteryFromPacket);
  messageProtocol.registerRxHandler(PpiId.AD_RING_BATT_LEVEL_LOG, PpiType.PUSH, updateRingBatteryFromPacket);
  messageProtocol.registerRxHandler(PpiId.AD_RING_BATT_LEVEL_LOG, PpiType.RE, updateRingBatteryFromPacket);
  messageProtocol.registerRxHandler(PpiId.AD_RING_STATUS, PpiType.PUSH, updateDeviceErrorFromRingStatus);
  messageProtocol.registerRxHandler(PpiId.AD_RING_STATUS, PpiType.RE, updateDeviceErrorFromRingStatus);

  const logFeedbackPacket = (packet: { ppi: number; type: number; payload: Uint8Array }) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    const ppiName = PpiId[packet.ppi as PpiId] ?? `PPI_${packet.ppi}`;
    const typeName = PpiType[packet.type as PpiType] ?? `TYPE_${packet.type}`;
    bleLog("🧪 [MP] Baselining/Calibration feedback", {
      deviceIdentifier,
      ppi: packet.ppi,
      ppiName,
      type: packet.type,
      typeName,
      payloadHex: Buffer.from(packet.payload).toString("hex"),
      decoded: decoded.value,
    });
  };

  messageProtocol.registerRxHandler(PpiId.AD_START_BASELINING, PpiType.PUSH, logFeedbackPacket);
  messageProtocol.registerRxHandler(PpiId.AD_BASELINING_FEEDBACK, PpiType.PUSH, logFeedbackPacket);
  messageProtocol.registerRxHandler(PpiId.AD_START_CALIBRATION, PpiType.PUSH, logFeedbackPacket);
  messageProtocol.registerRxHandler(PpiId.AD_CALIBRATION_FEEDBACK, PpiType.PUSH, logFeedbackPacket);
}
