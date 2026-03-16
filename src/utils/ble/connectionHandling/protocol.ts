import { sendDoseEvent } from "@/services/schedule";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
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
  level: "INFO" | "WARN" | "ERR",
  args: unknown[]
): void {
  const prefix = `[MP][PROTOCOL][${level}]`;
  if (!args.length) {
    if (level === "INFO") {
      console.info(prefix);
    } else if (level === "WARN") {
      console.warn(prefix);
    } else {
      console.error(prefix);
    }
    return;
  }

  const [first, ...rest] = args;
  if (typeof first === "string") {
    const message = `${prefix} ${first}`;
    if (level === "INFO") {
      console.info(message, ...rest);
    } else if (level === "WARN") {
      console.warn(message, ...rest);
    } else {
      console.error(message, ...rest);
    }
    return;
  }

  if (level === "INFO") {
    console.info(prefix, ...args);
  } else if (level === "WARN") {
    console.warn(prefix, ...args);
  } else {
    console.error(prefix, ...args);
  }
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
  context: string
): Promise<boolean> {
  if (protocol.getCurrentSessionId() === 0) {
    const syncResult = await protocol.startSync();
    if (syncResult !== MsgProtError.NONE) {
      console.warn(`[MP] Could not start sync before ${context}.`, { syncResult });
      return false;
    }
  }

  const txReady = await waitForTxSendable(protocol);
  if (!txReady) {
    console.warn(`[MP] TX not ready for ${context}; waiting for sync/ACK state.`);
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

async function sendPpiRequest(ppiId: PpiId): Promise<boolean> {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) return false;

  const maxAttempts = 3;

  for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
    const txReady = await waitForTxSendable(messageProtocol);
    if (!txReady) {
      console.warn(`[MP] TX not ready to queue runtime PPI request ${ppiId}.`);
      return false;
    }

    const result = messageProtocol.send(buildPpiPayload(ppiId, PpiType.RQ, new Uint8Array(0)));
    if (result === MsgProtError.NONE) {
      await messageProtocol.process();
      const completed = await waitForTxSendable(messageProtocol);
      if (!completed) {
        console.warn(`[MP] Runtime PPI request ${ppiId} did not complete in the ready window.`);
      }
      return completed;
    }

    if (result === MsgProtError.BUSY) {
      console.warn(`[MP] TX busy while queueing runtime PPI request ${ppiId}; retrying.`, {
        attempt,
        maxAttempts,
      });
      await messageProtocol.process();
      await new Promise((resolve) => setTimeout(resolve, TX_READY_POLL_MS));
      continue;
    }

    console.warn(`[MP] Failed to queue runtime PPI request ${ppiId}.`, { result });
    return false;
  }

  console.warn(`[MP] Failed to queue runtime PPI request ${ppiId} after retries.`);
  return false;
}

export async function requestRuntimePpiState(): Promise<void> {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) return;

  const txReady = await ensureProtocolReadyForDataSend(messageProtocol, "runtime PPI state request");
  if (!txReady) return;

  await sendPpiRequest(PpiId.AD_DOCK_BATT_LEVEL_LOG);
  await sendPpiRequest(PpiId.AD_RING_BATT_LEVEL_LOG);
  await sendPpiRequest(PpiId.AD_RING_STATUS);
}

export function setupMessageProtocolHandlers(deviceIdentifier: string): void {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) {
    return;
  }

  messageProtocol.registerRxHandler(PpiId.AD_DOSE_EVENT_REPORT, PpiType.PUSH, async (packet) => {
    const decoded = decodeDoseEventPpi(packet.payload);
    if (!decoded) {
      console.warn("[MP] Dose event payload size mismatch.");
      return;
    }

    const deviceStore = useDeviceStore.getState();
    const scheduleStore = useScheduleStore.getState();
    const device = deviceStore.getDevice(deviceIdentifier);
    if (!device) {
      console.warn("[MP] Dose event received for unknown device.", { deviceIdentifier });
      return;
    }

    const treatment =
      useTreatmentStore.getState().getDeviceTreatment(device.deviceId) ??
      useTreatmentStore.getState().getDeviceTreatment(device.deviceName);
    if (!treatment?.medication_code) {
      console.warn("[MP] Dose event received but no treatment/medication code is available.", {
        deviceId: device.deviceId,
        deviceName: device.deviceName,
      });
      return;
    }

    const eventAtIso = new Date(decoded.start_timestamp_unix_s * 1000).toISOString();
    const doseEventPayload = {
      event_id: decoded.event_id,
      dose_event_at: eventAtIso,
      dose_state: decoded.dose_completed_in_time ? 1 : 0,
      dose_amount_mg: 0,
    };

    try {
      const backendResponse = await sendDoseEvent(
        doseEventPayload,
        device.deviceId,
        treatment.medication_code,
      );

      let acknowledgedSchedule = null;

      if (typeof backendResponse?.event_id === "string" && backendResponse.event_id.length > 0) {
        acknowledgedSchedule = scheduleStore.acknowledgeDoseEvent(
          device.deviceId,
          backendResponse.event_id,
          decoded,
        );
      }

      if (!acknowledgedSchedule) {
        acknowledgedSchedule = scheduleStore.acknowledgeDoseEventByTimestamp(
          device.deviceId,
          eventAtIso,
          decoded,
        );
      }

      if (acknowledgedSchedule?.id) {
        scheduleStore.markBackendSynced(device.deviceId, acknowledgedSchedule.id);
      }

      console.log("💊 [MP] Received and synced dose event.", {
        deviceId: device.deviceId,
        eventAtIso,
        backendEventId: backendResponse?.event_id,
      });
    } catch (doseEventError) {
      console.warn("[MP] Failed to sync dose event to backend.", {
        deviceId: device.deviceId,
        error: doseEventError instanceof Error ? doseEventError.message : String(doseEventError),
      });
    }
  });

  messageProtocol.registerRxHandler(PpiId.AD_TIME, PpiType.RE, (packet) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    console.log("🕒 [MP] Time update response", decoded.value);
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
}
