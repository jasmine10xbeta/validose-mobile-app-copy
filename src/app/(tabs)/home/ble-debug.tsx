import { Buffer } from "buffer";
import { useFocusEffect } from "@react-navigation/native";
import { useRouter } from "expo-router";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  Modal,
  PermissionsAndroid,
  Platform,
  Pressable,
  ScrollView,
  Text,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "@/constants/ble";
import {
  addBleDebugLog,
  getBleDebugLogs,
  subscribeBleDebugLogs,
} from "@/utils/ble/debugLogStore";
import {
  BleMessageProtocol,
  MessageProtocolInterface,
  MsgProtError,
} from "@/utils/ble/messageProtocol";
import {
  MAX_DOSES_PER_DAY,
  PpiId,
  PpiType,
  buildPpiPayload,
  decodePpiPayload,
  encodeDoseSchedulePpi,
  encodeUint32LE,
  isTxStatusSendable,
  validatePayloadLength,
} from "@/utils/ble/messageProtocolPpi";
import {
  discoverServicesAndCharacteristics,
  disconnect,
  getConnectedDevice,
  isDeviceConnected,
} from "../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { ActionButton } from "./ble-debug/components/ActionButton";
import { QuickPpiButton } from "./ble-debug/components/QuickPpiButton";
import {
  PPI_ACK_TIMEOUT_MS,
  PPI_LATE_ACK_WATCH_TIMEOUT_MS,
  PPI_MAX_RETRIES,
  PPI_TX_COMPLETION_WAIT_MS,
  PPI_TX_READY_POLL_MS,
  PPI_TX_READY_TIMEOUT_MS,
  QUICK_FLOW_ACTIONS,
  QUICK_FLOW_META,
} from "./ble-debug/constants";
import {
  extractConnectedDeviceLabel,
  formatDecodedValue,
  formatHexBytes,
  prettifyForLog,
  resolveConnectionState,
} from "./ble-debug/helpers";
import { styles } from "./ble-debug/styles";
import type {
  FlowStepState,
  MpFramePreview,
  PpiRxPreview,
  PpiTxPreview,
  QuickFlowAction,
} from "./ble-debug/types";

const MP_SERVICE_UUID = SERVICE_UUIDS.CUSTOM_SERVICE;
const MP_TX_UUID = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_TX;
const MP_RX_UUID = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_RX;
const MP_SERVICE_SHORT_UUID = "1500";
const MP_TX_SHORT_UUID = "1508";
const MP_RX_SHORT_UUID = "1509";
// Firmware parity: MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS = 0 (process on demand).
const MP_PROCESS_INTERVAL_MS = 0;
const MP_MAX_PACKET_LEN = 244;
const MP_MIN_FRAME_LEN = 14; // CRC(2) + header(8) + PPI envelope(4)

type DiscoveredCharacteristic = {
  uuid?: string;
  properties?: string[];
};

type DiscoveredService = {
  uuid?: string;
  characteristics?: DiscoveredCharacteristic[];
};

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

function resolveMessageProtocolUuidsFromDiscovery(discovery: unknown): {
  txUuid: string;
  rxUuid: string;
  source: string;
} {
  let txUuid = MP_TX_UUID;
  let rxUuid = MP_RX_UUID;
  let source = "defaults";

  if (!Array.isArray(discovery)) {
    return { txUuid, rxUuid, source };
  }

  const services = discovery as DiscoveredService[];
  const customService = services.find(
    (service) =>
      typeof service?.uuid === "string" &&
      isUuidMatchByShortKey(service.uuid, MP_SERVICE_SHORT_UUID)
  );

  if (!customService?.characteristics?.length) {
    return { txUuid, rxUuid, source };
  }

  const characteristics = customService.characteristics.filter(
    (characteristic): characteristic is DiscoveredCharacteristic & { uuid: string } =>
      typeof characteristic?.uuid === "string" && characteristic.uuid.length > 0
  );

  const hasProperty = (
    characteristic: DiscoveredCharacteristic,
    predicate: (prop: string) => boolean
  ) =>
    Array.isArray(characteristic.properties) &&
    characteristic.properties.some((prop) => typeof prop === "string" && predicate(prop.toLowerCase()));

  const writeCharacteristic = characteristics.find((characteristic) =>
    hasProperty(
      characteristic,
      (prop) => prop === "write" || prop === "writewithoutresponse"
    )
  );
  const notifyCharacteristic = characteristics.find((characteristic) =>
    hasProperty(
      characteristic,
      (prop) => prop === "notify" || prop === "indicate"
    )
  );

  const explicitTx = characteristics.find((characteristic) =>
    isUuidMatchByShortKey(characteristic.uuid, MP_TX_SHORT_UUID)
  );
  const explicitRx = characteristics.find((characteristic) =>
    isUuidMatchByShortKey(characteristic.uuid, MP_RX_SHORT_UUID)
  );

  if (explicitTx?.uuid) {
    txUuid = explicitTx.uuid;
    source = "discovery-explicit-uuid";
  }
  if (explicitRx?.uuid) {
    rxUuid = explicitRx.uuid;
    source = source === "discovery-explicit-uuid" ? source : "discovery-explicit-uuid";
  }

  if (!explicitTx?.uuid && writeCharacteristic?.uuid) {
    txUuid = writeCharacteristic.uuid;
    source = source === "defaults" ? "discovery-properties" : `${source}+properties`;
  }
  if (!explicitRx?.uuid && notifyCharacteristic?.uuid) {
    rxUuid = notifyCharacteristic.uuid;
    source = source === "defaults" ? "discovery-properties" : `${source}+properties`;
  }

  if (
    !explicitTx?.uuid &&
    !explicitRx?.uuid &&
    !writeCharacteristic?.uuid &&
    !notifyCharacteristic?.uuid &&
    characteristics.length === 1
  ) {
    txUuid = characteristics[0].uuid;
    rxUuid = characteristics[0].uuid;
    source = "single-characteristic-fallback";
  }

  return { txUuid, rxUuid, source };
}

function getMpPacketTypeLabel(pktType: number): string {
  switch (pktType) {
    case 0:
      return "DATA";
    case 1:
      return "ACK";
    case 2:
      return "NAK";
    default:
      return `UNKNOWN_${pktType}`;
  }
}

function parseMpFrameBytes(raw: Uint8Array): MpFramePreview | null {
  if (!raw.length || raw.length < MP_MIN_FRAME_LEN) return null;

  const frame = Buffer.from(raw);
  const pktPayloadLen = frame.readUInt16LE(12);
  const frameLength = MP_MIN_FRAME_LEN + pktPayloadLen;
  if (frame.length < frameLength) return null;

  const packet = frame.subarray(0, frameLength);
  const payload = packet.subarray(MP_MIN_FRAME_LEN);
  const frameBytesHex = Array.from(packet, (value) => value.toString(16).padStart(2, "0"));
  const frameBytesIndexedHex = frameBytesHex.map((value, index) => `[${index}] 0x${value}`);

  return {
    frameLength,
    crc: packet.readUInt16LE(0),
    pktCounter: packet.readUInt16LE(2),
    sessionId: packet.readUInt32LE(4),
    pktType: packet.readUInt8(8),
    status: packet.readUInt8(9),
    payloadType: packet.readUInt8(10),
    payloadPpi: packet.readUInt8(11),
    pktPayloadLen,
    payloadHex: Buffer.from(payload).toString("hex"),
    payloadBase64: Buffer.from(payload).toString("base64"),
    frameHex: Buffer.from(packet).toString("hex"),
    frameBase64: Buffer.from(packet).toString("base64"),
    frameBytesHex,
    frameBytesIndexedHex,
  };
}

function parseMpFrameHex(frameHex: string): MpFramePreview | null {
  if (!frameHex) return null;
  try {
    return parseMpFrameBytes(new Uint8Array(Buffer.from(frameHex, "hex")));
  } catch {
    return null;
  }
}

function formatPpiHumanReadable(
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

export default function BleDebugScreen() {
  const router = useRouter();
  const [logCount, setLogCount] = useState(() => getBleDebugLogs().length);
  const [loadingAction, setLoadingAction] = useState<string | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [connectedDeviceLabel, setConnectedDeviceLabel] = useState("");
  const [lastPpiTxPreview, setLastPpiTxPreview] = useState<PpiTxPreview | null>(null);
  const [lastPpiRxPreview, setLastPpiRxPreview] = useState<PpiRxPreview | null>(null);
  const [selectedFlowAction, setSelectedFlowAction] = useState<QuickFlowAction>("TIME_PUSH");
  const [flowStatusByAction, setFlowStatusByAction] = useState<Record<string, string>>({});
  const [flowHelpVisible, setFlowHelpVisible] = useState(false);
  const [protocolRunning, setProtocolRunning] = useState(false);

  const ppiProtocolRef = useRef<BleMessageProtocol | null>(null);
  const resolvedMpTxUuidRef = useRef(MP_TX_UUID);
  const resolvedMpRxUuidRef = useRef(MP_RX_UUID);
  const lateAckWatchTokenRef = useRef(0);
  const gattDiscoveryInFlightRef = useRef<Promise<void> | null>(null);
  const isGattDiscoveredRef = useRef(false);

  const selectedFlowMeta = useMemo(
    () => QUICK_FLOW_META[selectedFlowAction] ?? QUICK_FLOW_META.TIME_PUSH,
    [selectedFlowAction]
  );
  const selectedFlowStatus = useMemo(
    () => flowStatusByAction[selectedFlowMeta.actionName] ?? "",
    [flowStatusByAction, selectedFlowMeta.actionName]
  );
  const isBlockedByOtherAction = useCallback(
    (action: string) => Boolean(loadingAction && loadingAction !== action),
    [loadingAction]
  );

  const statusText = useMemo(() => {
    const actionText: Record<string, string> = {
      "ppi-time-rq": "Requesting current dock time (AD_TIME RQ).",
      "ppi-time-push": "Sending current phone time to device (AD_TIME PUSH).",
      "ppi-dose-schedule-rq": "Requesting dose schedule from dock (AD_DOSE_SCHEDULE RQ).",
      "ppi-dose-schedule-push": "Pushing demo dose schedule to dock (AD_DOSE_SCHEDULE PUSH).",
      "ppi-dock-status-rq": "Requesting dock status (AD_DOCK_STATUS RQ).",
      "ppi-ring-status-rq": "Requesting ring status (AD_RING_STATUS RQ).",
      "ppi-dock-battery-rq": "Requesting dock battery (AD_DOCK_BATT_LEVEL_LOG RQ).",
      "ppi-ring-battery-rq": "Requesting ring battery (AD_RING_BATT_LEVEL_LOG RQ).",
      disconnect: "Disconnecting from the peripheral.",
    };
    if (loadingAction && actionText[loadingAction]) {
      return actionText[loadingAction];
    }

    if (lastPpiTxPreview?.status === "SENT_DATA") {
      return `Done. ${lastPpiTxPreview.action.replaceAll("_", " ")} DATA frame was sent.`;
    }
    if (lastPpiTxPreview?.status === "TX_BUSY") {
      return "TX not ready yet. Wait until current transmit state is ABANDONED or NEW.";
    }

    if (!isConnected) {
      return "Not connected. Use the BLE Debug Console scanner to connect to a VAL device first.";
    }

    // return "Ready. Message protocol is running and TX mailbox is idle for the next DATA frame.";
    return "";
  }, [isConnected, lastPpiTxPreview, loadingAction]);

  const protocolInfo = useMemo(
    () => ({
      ackTimeoutMs: PPI_ACK_TIMEOUT_MS,
      maxRetries: PPI_MAX_RETRIES,
      txReadyTimeoutMs: PPI_TX_READY_TIMEOUT_MS,
      txCompletionWaitMs: PPI_TX_COMPLETION_WAIT_MS,
      lateAckWatchMs: PPI_LATE_ACK_WATCH_TIMEOUT_MS,
      processIntervalMs: MP_PROCESS_INTERVAL_MS,
      maxPacketLength: MP_MAX_PACKET_LEN,
      txState: lastPpiTxPreview?.status || "IDLE",
      protocolState: protocolRunning ? "Running" : "Stopped",
      currentSessionId: ppiProtocolRef.current?.getCurrentSessionId() ?? null,
      logCount,
    }),
    [lastPpiTxPreview?.status, protocolRunning, logCount]
  );

  const flowSteps = useMemo(
    () => [
      "Ensure Message Protocol is running.",
      "App runs as master and generates session_id locally.",
      "SYNC control flow is disabled in app runtime.",
      "Check TX ready. TX ready means current TX state is ABANDONED or NEW.",
      `Build payload (${selectedFlowMeta.payloadHint}) and validate ${selectedFlowMeta.ppiName} ${selectedFlowMeta.typeName} (${selectedFlowMeta.lenHint}).`,
      `Send DATA frame with type=${selectedFlowMeta.typeId}, ppi=${selectedFlowMeta.ppiId}.`,
      "Done.",
    ],
    [selectedFlowMeta]
  );

  const flowProgress = useMemo(() => {
    const txReadyIndex = 3;
    const validateIndex = 4;
    const sendIndex = 5;
    const doneIndex = 6;

    let activeIndex = 0;
    let errorIndex: number | null = null;

    if (loadingAction === selectedFlowMeta.busyKey) {
      activeIndex = sendIndex;
    }

    if (selectedFlowStatus === "SENT_DATA") {
      activeIndex = doneIndex;
    } else if (selectedFlowStatus === "TX_BUSY") {
      errorIndex = txReadyIndex;
      activeIndex = txReadyIndex;
    } else if (selectedFlowStatus === "PAYLOAD_LENGTH_MISMATCH") {
      errorIndex = validateIndex;
      activeIndex = validateIndex;
    } else if (selectedFlowStatus.startsWith("SEND_ERROR_")) {
      errorIndex = sendIndex;
      activeIndex = sendIndex;
    }

    return { activeIndex, errorIndex };
  }, [loadingAction, selectedFlowMeta.busyKey, selectedFlowStatus]);

  const flowStatusBadge = useMemo(() => {
    if (!selectedFlowStatus && !loadingAction) return "IDLE";
    if (loadingAction === selectedFlowMeta.busyKey && !selectedFlowStatus) return "STARTING";
    return selectedFlowStatus || "IN_PROGRESS";
  }, [loadingAction, selectedFlowMeta.busyKey, selectedFlowStatus]);

  const lastPpiTxHumanReadable = useMemo(() => {
    if (!lastPpiTxPreview) return "";

    const payloadBytes = lastPpiTxPreview.payloadHex
      ? new Uint8Array(Buffer.from(lastPpiTxPreview.payloadHex, "hex"))
      : new Uint8Array(0);
    const decoded = decodePpiPayload(
      lastPpiTxPreview.ppi,
      lastPpiTxPreview.type as PpiType,
      payloadBytes
    );

    return formatPpiHumanReadable(
      lastPpiTxPreview.ppi,
      lastPpiTxPreview.type,
      lastPpiTxPreview.payloadHex,
      decoded.value
    );
  }, [lastPpiTxPreview]);

  const lastPpiRxHumanReadable = useMemo(() => {
    if (!lastPpiRxPreview) return "";
    if (typeof lastPpiRxPreview.ppi !== "number" || typeof lastPpiRxPreview.type !== "number") {
      return "";
    }

    return formatPpiHumanReadable(
      lastPpiRxPreview.ppi,
      lastPpiRxPreview.type,
      lastPpiRxPreview.payloadHex,
      lastPpiRxPreview.decoded
    );
  }, [lastPpiRxPreview]);

  function openFlowHelp(action: QuickFlowAction) {
    setSelectedFlowAction(action);
    setFlowHelpVisible(true);
  }

  function addLog(message: string, payload?: unknown) {
    const body = payload === undefined ? "" : `\n${prettifyForLog(payload)}`;
    const formatted = `${new Date().toLocaleTimeString()}  ${message}${body}`;
    addBleDebugLog(formatted);
  }

  async function ensureAndroidBlePermissions(context: string): Promise<boolean> {
    if (Platform.OS !== "android") {
      return true;
    }

    const androidApi =
      typeof Platform.Version === "number"
        ? Platform.Version
        : Number.parseInt(String(Platform.Version), 10);

    if (!Number.isFinite(androidApi)) {
      return true;
    }

    const required: PermissionsAndroid.Permission[] = [];
    if (androidApi >= 31) {
      required.push(
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT
      );
    }
    if (androidApi >= 23) {
      required.push(PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION);
    }

    const permissionsToCheck = Array.from(new Set(required));
    if (!permissionsToCheck.length) {
      return true;
    }

    try {
      const checkResults = await Promise.all(
        permissionsToCheck.map(async (permission) => ({
          permission,
          granted: await PermissionsAndroid.check(permission),
        }))
      );

      const missingPermissions = checkResults
        .filter((entry) => !entry.granted)
        .map((entry) => entry.permission);

      if (!missingPermissions.length) {
        return true;
      }

      const requestResult = await PermissionsAndroid.requestMultiple(missingPermissions);
      const deniedPermissions = missingPermissions.filter(
        (permission) => requestResult[permission] !== PermissionsAndroid.RESULTS.GRANTED
      );

      if (deniedPermissions.length) {
        addLog(`[PERMISSION][ERR] Android BLE permission denied (${context}).`, {
          deniedPermissions,
          requestResult,
        });
        return false;
      }

      return true;
    } catch (error) {
      addLog(`[PERMISSION][ERR] Failed to request Android BLE permissions (${context}).`, String(error));
      return false;
    }
  }

  function decodeUtf8Safe(bytes: Uint8Array): string {
    if (!bytes.length) return "";
    try {
      return Buffer.from(bytes).toString("utf8");
    } catch {
      return "";
    }
  }

  function relayMessageProtocolLog(level: "DEBUG" | "INFO" | "WARN" | "ERR", args: unknown[]) {
    if (!args.length) {
      addLog(`[PPI][MP][${level}]`);
      return;
    }

    const [first, ...rest] = args;
    if (typeof first === "string") {
      if (!rest.length) {
        addLog(`[PPI][MP][${level}] ${first}`);
        return;
      }
      if (rest.length === 1) {
        addLog(`[PPI][MP][${level}] ${first}`, rest[0]);
        return;
      }
      addLog(`[PPI][MP][${level}] ${first}`, rest);
      return;
    }

    addLog(`[PPI][MP][${level}]`, args);
  }

  async function withBusy(name: string, run: () => Promise<void>) {
    setLoadingAction(name);
    try {
      await run();
    } catch (error) {
      addLog(`[${name.toUpperCase()}][ERR] ${String(error)}`);
    } finally {
      setLoadingAction(null);
    }
  }

  const refreshConnectionBanner = useCallback(async () => {
    try {
      const [connectedResponse, connectedDevice] = await Promise.all([
        isDeviceConnected(),
        getConnectedDevice(),
      ]);
      const connected = resolveConnectionState(connectedResponse);
      const label =
        extractConnectedDeviceLabel(connectedDevice) ||
        extractConnectedDeviceLabel(connectedResponse) ||
        "";

      setIsConnected(connected);
      setConnectedDeviceLabel(connected ? label : "");
    } catch {
      // Ignore banner refresh failures in debug UI.
    }
  }, []);

  function stopPpiProtocol() {
    lateAckWatchTokenRef.current += 1;
    if (ppiProtocolRef.current) {
      ppiProtocolRef.current.stop();
      ppiProtocolRef.current = null;
      setProtocolRunning(false);
      addLog("[PPI] Message protocol stopped.");
    }
  }

  async function ensureGattDiscovered(reason: string) {
    if (isGattDiscoveredRef.current) return;
    if (gattDiscoveryInFlightRef.current) {
      await gattDiscoveryInFlightRef.current;
      return;
    }

    const discoverTask = (async () => {
      const maxAttempts = 3;
      let lastError: unknown = null;

      for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
        try {
          addLog(
            `[DISCOVER] Discovering services/characteristics (${reason}) [${attempt}/${maxAttempts}]...`
          );
          const response = await discoverServicesAndCharacteristics();
          const resolved = resolveMessageProtocolUuidsFromDiscovery(response);
          resolvedMpTxUuidRef.current = resolved.txUuid;
          resolvedMpRxUuidRef.current = resolved.rxUuid;
          isGattDiscoveredRef.current = true;
          addLog("[DISCOVER] Services/characteristics ready.", response);
          // addLog("[DISCOVER] Resolved message protocol UUIDs.", {
          //   source: resolved.source,
          //   txCharacteristicUuid: resolved.txUuid,
          //   rxCharacteristicUuid: resolved.rxUuid,
          //   expectedTxUuid: MP_TX_UUID,
          //   expectedRxUuid: MP_RX_UUID,
          // });
          return;
        } catch (error) {
          lastError = error;
          addLog(`[DISCOVER][WARN] Discovery attempt ${attempt} failed.`, String(error));
          if (attempt < maxAttempts) {
            await new Promise((resolve) => setTimeout(resolve, 300));
          }
        }
      }

      addLog("[DISCOVER][ERR] Could not discover services/characteristics.", String(lastError));
      throw lastError;
    })();

    gattDiscoveryInFlightRef.current = discoverTask;
    try {
      await discoverTask;
    } finally {
      gattDiscoveryInFlightRef.current = null;
    }
  }

  async function waitForPpiTxSendable(
    protocol: MessageProtocolInterface,
    _timeoutMs = PPI_TX_READY_TIMEOUT_MS,
    _pollMs = PPI_TX_READY_POLL_MS
  ) {
    return isTxStatusSendable(protocol.getTxPacketStatus());
  }

  async function waitForLatestTxFrameHex(
    protocol: BleMessageProtocol,
    previousHex: string,
    timeoutMs = 2000,
    pollMs = 25
  ) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() <= deadline) {
      const frameHex = Buffer.from(protocol.getLastTxPacketRaw()).toString("hex");
      if (frameHex && frameHex !== previousHex) return frameHex;
      await new Promise((resolve) => setTimeout(resolve, pollMs));
    }

    const fallbackHex = Buffer.from(protocol.getLastTxPacketRaw()).toString("hex");
    if (fallbackHex && fallbackHex !== previousHex) return fallbackHex;
    return "";
  }

  async function ensurePpiProtocol() {
    if (ppiProtocolRef.current) {
      setProtocolRunning(true);
      return ppiProtocolRef.current;
    }

    const hasPermissions = await ensureAndroidBlePermissions("protocol-start");
    if (!hasPermissions) {
      throw new Error("Bluetooth permissions not granted.");
    }

    await ensureGattDiscovered("protocol-start");
    const txUuid = resolvedMpTxUuidRef.current || MP_TX_UUID;
    const rxUuid = resolvedMpRxUuidRef.current || MP_RX_UUID;

    const protocol = new BleMessageProtocol({
      txCharacteristicUUID: txUuid,
      rxCharacteristicUUID: rxUuid,
      serviceUUID: MP_SERVICE_UUID,
      processIntervalMs: MP_PROCESS_INTERVAL_MS,
      ackTimeoutMs: PPI_ACK_TIMEOUT_MS,
      maxRetries: PPI_MAX_RETRIES,
      maxPacketLength: MP_MAX_PACKET_LEN,
      isMaster: true,
      enableSyncControl: false,
      sendAckNak: false,
      autoConsumeRx: true,
      logger: {
        debug: (...args: unknown[]) => relayMessageProtocolLog("DEBUG", args),
        info: (...args: unknown[]) => relayMessageProtocolLog("INFO", args),
        warn: (...args: unknown[]) => relayMessageProtocolLog("WARN", args),
        error: (...args: unknown[]) => relayMessageProtocolLog("ERR", args),
      },
      onRxPacket: (packet) => {
        const rxFrameRaw = ppiProtocolRef.current?.getLastRxPacketRaw() ?? new Uint8Array(0);
        const rxFrame = parseMpFrameBytes(rxFrameRaw);
        const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
        const payloadHex = Buffer.from(packet.payload).toString("hex");
        const payloadBase64 = Buffer.from(packet.payload).toString("base64");
        const payloadUtf8 = decodeUtf8Safe(packet.payload);
        setLastPpiRxPreview({
          source: "Message Protocol",
          receivedAt: new Date().toLocaleTimeString(),
          ppi: packet.ppi,
          ppiName: PpiId[packet.ppi as PpiId] ?? `PPI_${packet.ppi}`,
          type: packet.type,
          typeName: PpiType[packet.type as PpiType] ?? `TYPE_${packet.type}`,
          pktPayloadLen: packet.pktPayloadLen,
          payloadHex,
          payloadBase64,
          payloadUtf8,
          fullFrameHex: rxFrame?.frameHex ?? "",
          fullFrameBase64: rxFrame?.frameBase64 ?? "",
          mpFrame: rxFrame,
          decoded: decoded.value,
        });
        addLog("[PPI][NOTIFY]", {
          ppi: packet.ppi,
          type: packet.type,
          payloadHex,
          payloadUtf8,
          frameHex: rxFrame?.frameHex ?? null,
          frameBytesHex: rxFrame?.frameBytesHex ?? null,
          frameBytesIndexedHex: rxFrame?.frameBytesIndexedHex ?? null,
          sessionId: rxFrame?.sessionId ?? null,
          pktCounter: rxFrame?.pktCounter ?? null,
          pktType: rxFrame?.pktType ?? null,
          status: rxFrame?.status ?? null,
          decoded: decoded.value,
        });
      },
    });

    ppiProtocolRef.current = protocol;
    try {
      await protocol.start();
    } catch (error) {
      ppiProtocolRef.current = null;
      throw error;
    }
    setProtocolRunning(true);
    addLog("[PPI] Message protocol started", {
      serviceUuid: MP_SERVICE_UUID,
      txCharacteristicUuid: txUuid,
      rxCharacteristicUuid: rxUuid,
      mode: "MASTER",
      sessionId: protocol.getCurrentSessionId(),
      ackTimeoutMs: PPI_ACK_TIMEOUT_MS,
      maxRetries: PPI_MAX_RETRIES,
    });

    return protocol;
  }

  async function sendPpi(
    actionName: string,
    ppi: PpiId,
    type: PpiType,
    payload: Uint8Array
  ) {
    const payloadHex = Buffer.from(payload).toString("hex");
    const payloadBase64 = Buffer.from(payload).toString("base64");
    const basePreview: Omit<PpiTxPreview, "status" | "sentAt"> = {
      action: actionName,
      ppi,
      ppiName: PpiId[ppi] ?? `PPI_${ppi}`,
      type,
      typeName: PpiType[type] ?? `TYPE_${type}`,
      pktPayloadLen: payload.length,
      payloadHex,
      payloadBase64,
      fullFrameHex: "",
      fullFrameBase64: "",
      mpFrame: null,
    };

    const protocol = await ensurePpiProtocol();
    const txReady = await waitForPpiTxSendable(protocol);
    if (!txReady) {
      const lastTxFrameHex = Buffer.from(protocol.getLastTxPacketRaw()).toString("hex");
      const txFrame = parseMpFrameHex(lastTxFrameHex);
      setLastPpiTxPreview({
        ...basePreview,
        fullFrameHex: lastTxFrameHex,
        fullFrameBase64: lastTxFrameHex ? Buffer.from(lastTxFrameHex, "hex").toString("base64") : "",
        mpFrame: txFrame,
        status: "TX_BUSY",
        sentAt: new Date().toLocaleTimeString(),
      });
      setFlowStatusByAction((prev) => ({ ...prev, [actionName]: "TX_BUSY" }));
      addLog(`[PPI][${actionName}] Message protocol TX not ready.`, {
        reason:
          "TX ready requires current TX state to be ABANDONED or NEW before queuing the next DATA frame.",
        lastTxFrameHex: lastTxFrameHex || null,
        lastTxFrameBytesHex: txFrame?.frameBytesHex ?? null,
        lastTxFrameBytesIndexedHex: txFrame?.frameBytesIndexedHex ?? null,
        lastTxFrame: txFrame,
      });
      return "TX_BUSY";
    }

    if (!validatePayloadLength(ppi, type, payload)) {
      setLastPpiTxPreview({
        ...basePreview,
        status: "PAYLOAD_LENGTH_MISMATCH",
        sentAt: new Date().toLocaleTimeString(),
      });
      setFlowStatusByAction((prev) => ({ ...prev, [actionName]: "PAYLOAD_LENGTH_MISMATCH" }));
      addLog(`[PPI][${actionName}] Payload length mismatch`, {
        ppi,
        type,
        length: payload.length,
      });
      return "PAYLOAD_LENGTH_MISMATCH";
    }

    const previousFrameHex = Buffer.from(protocol.getLastTxPacketRaw()).toString("hex");
    const result = protocol.send(buildPpiPayload(ppi, type, payload));
    if (result !== MsgProtError.NONE) {
      const errorStatus = `SEND_ERROR_${result}`;
      setLastPpiTxPreview({
        ...basePreview,
        status: errorStatus,
        sentAt: new Date().toLocaleTimeString(),
      });
      setFlowStatusByAction((prev) => ({ ...prev, [actionName]: errorStatus }));
      addLog(`[PPI][${actionName}] Send failed`, { result });
      return errorStatus;
    }

    await protocol.process();
    const fullFrameHex = await waitForLatestTxFrameHex(protocol, previousFrameHex);
    const fullFrameBase64 = fullFrameHex ? Buffer.from(fullFrameHex, "hex").toString("base64") : "";
    const txFrame = parseMpFrameHex(fullFrameHex);
    const status = "SENT_DATA";

    setLastPpiTxPreview({
      ...basePreview,
      fullFrameHex,
      fullFrameBase64,
      mpFrame: txFrame,
      status,
      sentAt: new Date().toLocaleTimeString(),
    });
    setFlowStatusByAction((prev) => ({ ...prev, [actionName]: status }));

    // addLog(`[PPI][${actionName}] ${status}`, {
    //   ppi,
    //   type,
    //   payloadHex,
    //   fullFrameHex: fullFrameHex || null,
    //   fullFrameBytesHex: txFrame?.frameBytesHex ?? null,
    //   fullFrameBytesIndexedHex: txFrame?.frameBytesIndexedHex ?? null,
    //   frame: txFrame,
    //   note: "DATA frame sent.",
    // });

    return status;
  }

  function runZeroPayloadAction(
    selectedAction: QuickFlowAction,
    busyKey: string,
    actionName: string,
    ppi: PpiId,
    type: PpiType
  ) {
    return async () => {
      setSelectedFlowAction(selectedAction);
      await withBusy(busyKey, async () => {
        await sendPpi(actionName, ppi, type, new Uint8Array(0));
      });
    };
  }

  const onPpiTimeRequest = runZeroPayloadAction(
    "TIME_RQ",
    "ppi-time-rq",
    "TIME_RQ",
    PpiId.AD_TIME,
    PpiType.RQ
  );

  async function onPpiTimePush() {
    setSelectedFlowAction("TIME_PUSH");
    await withBusy("ppi-time-push", async () => {
      const unixTime = Math.floor(Date.now() / 1000);
      const payload = encodeUint32LE(unixTime);
      await sendPpi("TIME_PUSH", PpiId.AD_TIME, PpiType.PUSH, payload);
    });
  }

  function buildDemoDoseSchedulePayload() {
    return encodeDoseSchedulePpi({
      medication_type: 0,
      dosage_mg: 2,
      temp_upper_limit_deg_c: 60,
      temp_lower_limit_deg_c: 0,
      temp_avg_window_duration_sec: 1800,
      dose_days_bitfield: 0x7f,
      dose_window_duration_minutes: 30,
      dose_window_count: 4,
      dose_window_start_times_minutes: [630, 840, 1050, 1260].slice(0, MAX_DOSES_PER_DAY),
    });
  }

  const onPpiDoseScheduleRequest = runZeroPayloadAction(
    "DOSE_SCHEDULE_RQ",
    "ppi-dose-schedule-rq",
    "DOSE_SCHEDULE_RQ",
    PpiId.AD_DOSE_SCHEDULE,
    PpiType.RQ
  );

  async function onPpiDoseSchedulePush() {
    setSelectedFlowAction("DOSE_SCHEDULE_PUSH");
    await withBusy("ppi-dose-schedule-push", async () => {
      const payload = buildDemoDoseSchedulePayload();
      await sendPpi("DOSE_SCHEDULE_PUSH", PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, payload);
    });
  }

  const onDockStatusRequest = runZeroPayloadAction(
    "DOCK_STATUS_RQ",
    "ppi-dock-status-rq",
    "DOCK_STATUS_RQ",
    PpiId.AD_DOCK_STATUS,
    PpiType.RQ
  );

  const onRingStatusRequest = runZeroPayloadAction(
    "RING_STATUS_RQ",
    "ppi-ring-status-rq",
    "RING_STATUS_RQ",
    PpiId.AD_RING_STATUS,
    PpiType.RQ
  );

  const onDockBatteryRequest = runZeroPayloadAction(
    "DOCK_BATTERY_RQ",
    "ppi-dock-battery-rq",
    "DOCK_BATTERY_RQ",
    PpiId.AD_DOCK_BATT_LEVEL_LOG,
    PpiType.RQ
  );

  const onRingBatteryRequest = runZeroPayloadAction(
    "RING_BATTERY_RQ",
    "ppi-ring-battery-rq",
    "RING_BATTERY_RQ",
    PpiId.AD_RING_BATT_LEVEL_LOG,
    PpiType.RQ
  );

  async function onDisconnect() {
    await withBusy("disconnect", async () => {
      try {
        stopPpiProtocol();
        resolvedMpTxUuidRef.current = MP_TX_UUID;
        resolvedMpRxUuidRef.current = MP_RX_UUID;
        isGattDiscoveredRef.current = false;
        gattDiscoveryInFlightRef.current = null;
        const res = await disconnect();
        addLog("[DISCONNECT]", res);
        setIsConnected(false);
        setConnectedDeviceLabel("");
      } catch (error) {
        addLog(`[DISCONNECT][ERR] ${String(error)}`);
      }
    });
  }

  useEffect(() => {
    return () => {
      lateAckWatchTokenRef.current += 1;
      if (ppiProtocolRef.current) {
        ppiProtocolRef.current.stop();
        ppiProtocolRef.current = null;
      }
      setProtocolRunning(false);
    };
  }, []);

  useEffect(() => {
    setLogCount(getBleDebugLogs().length);
    return subscribeBleDebugLogs(() => {
      setLogCount(getBleDebugLogs().length);
    });
  }, []);

  useFocusEffect(
    useCallback(() => {
      void refreshConnectionBanner();
    }, [refreshConnectionBanner])
  );

  useEffect(() => {
    void refreshConnectionBanner();
  }, [refreshConnectionBanner]);

  const quickActionMap: Record<QuickFlowAction, () => Promise<void>> = {
    TIME_RQ: onPpiTimeRequest,
    TIME_PUSH: onPpiTimePush,
    DOSE_SCHEDULE_RQ: onPpiDoseScheduleRequest,
    DOSE_SCHEDULE_PUSH: onPpiDoseSchedulePush,
    DOCK_STATUS_RQ: onDockStatusRequest,
    RING_STATUS_RQ: onRingStatusRequest,
    DOCK_BATTERY_RQ: onDockBatteryRequest,
    RING_BATTERY_RQ: onRingBatteryRequest,
  };

  function quickActionSubtitle(action: QuickFlowAction) {
    switch (action) {
      case "TIME_RQ":
        return "AD_TIME (RQ, no payload)";
      case "TIME_PUSH":
        return "AD_TIME (PUSH, uint32 unix)";
      case "DOSE_SCHEDULE_RQ":
        return "AD_DOSE_SCHEDULE (RQ, no payload)";
      case "DOSE_SCHEDULE_PUSH":
        return "AD_DOSE_SCHEDULE (PUSH, dose_schedule_t)";
      case "DOCK_STATUS_RQ":
        return "AD_DOCK_STATUS (RQ, no payload)";
      case "RING_STATUS_RQ":
        return "AD_RING_STATUS (RQ, no payload)";
      case "DOCK_BATTERY_RQ":
        return "AD_DOCK_BATT_LEVEL_LOG (RQ, no payload)";
      case "RING_BATTERY_RQ":
        return "AD_RING_BATT_LEVEL_LOG (RQ, no payload)";
      default:
        return "";
    }
  }

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
        <View style={styles.topInfoWrap}>
          <View style={styles.header}>
            <Pressable style={styles.backButton} onPress={() => router.back()}>
              <Text style={styles.backButtonText}>{"<"}</Text>
            </Pressable>
            <Text style={styles.title}>BLE Debug</Text>
            <Pressable style={styles.logsButton} onPress={() => router.push("/home/ble-debug-logs")}>
              <Text style={styles.logsButtonText}>Logs</Text>
            </Pressable>
          </View>

          <View
            style={[
              styles.connectionBanner,
              isConnected ? styles.connectionBannerConnected : styles.connectionBannerDisconnected,
            ]}
          >
            <View style={styles.connectionBannerTop}>
              <View
                style={[
                  styles.connectionDot,
                  isConnected ? styles.connectionDotConnected : styles.connectionDotDisconnected,
                ]}
              />
              <Text
                style={[
                  styles.connectionStatusText,
                  isConnected
                    ? styles.connectionStatusTextConnected
                    : styles.connectionStatusTextDisconnected,
                ]}
              >
                {isConnected ? "Connected" : "Disconnected"}
              </Text>
            </View>
            <Text style={styles.connectionDeviceText} numberOfLines={1}>
              {isConnected ? connectedDeviceLabel || "Unknown Device" : "No active device"}
            </Text>
            <View style={styles.deviceActionRow}>
              <ActionButton
                label="Disconnect"
                onPress={onDisconnect}
                disabled={!isConnected || isBlockedByOtherAction("disconnect")}
                loading={loadingAction === "disconnect"}
                tone="danger"
                style={styles.deviceActionButton}
              />
            </View>
            <Text style={styles.sectionHint}>{statusText}</Text>
          </View>
        </View>

        <View style={styles.panel}>
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Quick Actions</Text>
            {/* <Text style={styles.sectionHint}>
              Message protocol running means RX notifications (UUID 0x1509) are active and `process()` is executed on-demand to drive ACK/retry state.
            </Text>
            <Text style={styles.sectionHint}>
              App master mode generates `session_id` while SYNC control flow is disabled.
            </Text>
            <Text style={styles.sectionHint}>
              TX ready means the TX state is ABANDONED or NEW, so the next DATA frame can be queued.
            </Text> */}
            <View style={styles.quickPpiGrid}>
              {QUICK_FLOW_ACTIONS.map((item) => {
                const meta = QUICK_FLOW_META[item.key];
                const busyKey = meta.busyKey;
                return (
                  <QuickPpiButton
                    key={item.key}
                    title={meta.title}
                    subtitle={quickActionSubtitle(item.key)}
                    onPress={() => {
                      void quickActionMap[item.key]();
                    }}
                    onInfoPress={() => openFlowHelp(item.key)}
                    disabled={!isConnected || isBlockedByOtherAction(busyKey)}
                    loading={loadingAction === busyKey}
                  />
                );
              })}
            </View>
          </View>

          <View style={styles.ppiPreviewCard}>
            <Text style={styles.ppiPreviewTitle}>Payload Details</Text>
            <Text style={styles.ppiPreviewPrimaryLabel}>Last Sent</Text>
            {!lastPpiTxPreview ? (
              <Text style={styles.ppiPreviewEmpty}>No payload sent yet.</Text>
            ) : (
              <>
                <Text style={styles.ppiPreviewMeta}>
                  {lastPpiTxPreview.sentAt} · {lastPpiTxPreview.action} · {lastPpiTxPreview.status}
                </Text>
                <Text style={styles.ppiPreviewLine}>
                  PPI: {lastPpiTxPreview.ppiName} ({lastPpiTxPreview.ppi}) · Type: {lastPpiTxPreview.typeName} ({lastPpiTxPreview.type}) · Len: {lastPpiTxPreview.pktPayloadLen}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Structure (PPI payload envelope)</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {JSON.stringify(
                    {
                      type: lastPpiTxPreview.type,
                      ppi: lastPpiTxPreview.ppi,
                      pktPayloadLen: lastPpiTxPreview.pktPayloadLen,
                    },
                    null,
                    2
                  )}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Structure (Message Protocol header)</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.mpFrame
                    ? JSON.stringify(
                        {
                          pkt_crc: lastPpiTxPreview.mpFrame.crc,
                          pkt_counter: lastPpiTxPreview.mpFrame.pktCounter,
                          session_id: lastPpiTxPreview.mpFrame.sessionId,
                          pkt_type: lastPpiTxPreview.mpFrame.pktType,
                          pkt_type_label: getMpPacketTypeLabel(lastPpiTxPreview.mpFrame.pktType),
                          status: lastPpiTxPreview.mpFrame.status,
                          payload_type: lastPpiTxPreview.mpFrame.payloadType,
                          payload_ppi: lastPpiTxPreview.mpFrame.payloadPpi,
                          pkt_payload_len: lastPpiTxPreview.mpFrame.pktPayloadLen,
                          frame_len: lastPpiTxPreview.mpFrame.frameLength,
                        },
                        null,
                        2
                      )
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.payloadHex ? formatHexBytes(lastPpiTxPreview.payloadHex) : "(empty)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.payloadBase64 || "(empty)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Parsed Payload</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxHumanReadable || "(not available)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Full Frame Hex (CRC + header + PPI + payload)</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.fullFrameHex
                    ? formatHexBytes(lastPpiTxPreview.fullFrameHex)
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Full Frame Bytes (index:hex)</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.mpFrame?.frameBytesIndexedHex?.length
                    ? lastPpiTxPreview.mpFrame.frameBytesIndexedHex.join("\n")
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Full Frame Base64</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.fullFrameBase64 || "(not captured yet)"}
                </Text>
              </>
            )}

            <Text style={styles.ppiPreviewPrimaryLabel}>Last Incoming Update</Text>
            {!lastPpiRxPreview ? (
              <Text style={styles.ppiPreviewEmpty}>No incoming value yet.</Text>
            ) : (
              <>
                <Text style={styles.ppiPreviewMeta}>
                  {lastPpiRxPreview.receivedAt} · {lastPpiRxPreview.source}
                </Text>
                {typeof lastPpiRxPreview.ppi === "number" ? (
                  <Text style={styles.ppiPreviewLine}>
                    PPI: {lastPpiRxPreview.ppiName} ({lastPpiRxPreview.ppi}) · Type: {lastPpiRxPreview.typeName} ({lastPpiRxPreview.type}) · Len: {lastPpiRxPreview.pktPayloadLen ?? 0}
                  </Text>
                ) : null}
                <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.payloadHex ? formatHexBytes(lastPpiRxPreview.payloadHex) : "(empty)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.payloadBase64 || "(empty)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Payload UTF-8</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.payloadUtf8 || "(empty/non-utf8)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Incoming MP Header</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.mpFrame
                    ? JSON.stringify(
                        {
                          pkt_crc: lastPpiRxPreview.mpFrame.crc,
                          pkt_counter: lastPpiRxPreview.mpFrame.pktCounter,
                          session_id: lastPpiRxPreview.mpFrame.sessionId,
                          pkt_type: lastPpiRxPreview.mpFrame.pktType,
                          pkt_type_label: getMpPacketTypeLabel(lastPpiRxPreview.mpFrame.pktType),
                          status: lastPpiRxPreview.mpFrame.status,
                          payload_type: lastPpiRxPreview.mpFrame.payloadType,
                          payload_ppi: lastPpiRxPreview.mpFrame.payloadPpi,
                          pkt_payload_len: lastPpiRxPreview.mpFrame.pktPayloadLen,
                          frame_len: lastPpiRxPreview.mpFrame.frameLength,
                        },
                        null,
                        2
                      )
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Hex</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.fullFrameHex
                    ? formatHexBytes(lastPpiRxPreview.fullFrameHex)
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Bytes (index:hex)</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.mpFrame?.frameBytesIndexedHex?.length
                    ? lastPpiRxPreview.mpFrame.frameBytesIndexedHex.join("\n")
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Base64</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxPreview.fullFrameBase64 || "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Decoded Value</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {formatDecodedValue(lastPpiRxPreview.decoded)}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Parsed Payload</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiRxHumanReadable || "(not available)"}
                </Text>
              </>
            )}
          </View>

          <View style={styles.protocolInfoCard}>
            <Text style={styles.protocolInfoTitle}>Protocol Info</Text>
            {/* <Text style={styles.sectionHint}>
              `session_id` is carried in frame headers (bytes 4-7, little-endian) and generated in app master mode.
            </Text> */}
            <View style={styles.protocolInfoGrid}>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>State</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.protocolState}</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Current TX</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.txState}</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Current session_id (app)</Text>
                <Text style={styles.protocolInfoValue}>
                  {protocolInfo.currentSessionId === null ? "(not started)" : protocolInfo.currentSessionId}
                </Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Debug events</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.logCount}</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>ACK timeout</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.ackTimeoutMs} ms</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Max retries</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.maxRetries}</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>TX ready timeout</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.txReadyTimeoutMs} ms</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>TX completion wait</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.txCompletionWaitMs} ms</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Late ACK watch</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.lateAckWatchMs} ms</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Process interval</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.processIntervalMs} ms</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Max packet length</Text>
                <Text style={styles.protocolInfoValue}>{protocolInfo.maxPacketLength} bytes</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>Service UUID</Text>
                <Text style={styles.protocolInfoValue}>{MP_SERVICE_UUID}</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>TX UUID (App → Device)</Text>
                <Text style={styles.protocolInfoValue}>{MP_TX_UUID}</Text>
              </View>
              <View style={styles.protocolInfoRow}>
                <Text style={styles.protocolInfoLabel}>RX UUID (Device → App)</Text>
                <Text style={styles.protocolInfoValue}>{MP_RX_UUID}</Text>
              </View>
            </View>
          </View>
        </View>
      </ScrollView>

      <Modal
        visible={flowHelpVisible}
        animationType="slide"
        transparent
        onRequestClose={() => setFlowHelpVisible(false)}
      >
        <View style={styles.flowModalBackdrop}>
          <Pressable style={styles.flowModalDismissArea} onPress={() => setFlowHelpVisible(false)} />
          <View style={styles.flowModalCard}>
            <View style={styles.flowModalHandle} />
            <View style={styles.flowModalHeader}>
              <View style={styles.flowModalHeaderTextWrap}>
                <Text style={styles.flowModalTitle}>Protocol Steps</Text>
                <Text style={styles.flowModalSubtitle}>{selectedFlowMeta.title} · Master Data Mode</Text>
              </View>
              <Pressable style={styles.flowModalClose} onPress={() => setFlowHelpVisible(false)}>
                <Text style={styles.flowModalCloseText}>Done</Text>
              </Pressable>
            </View>
            <View style={styles.flowModalMetaRow}>
              <View style={styles.flowModalMetaPill}>
                <Text style={styles.flowModalMetaText}>{selectedFlowMeta.ppiName}</Text>
              </View>
              <View style={styles.flowModalMetaPill}>
                <Text style={styles.flowModalMetaText}>{selectedFlowMeta.typeName}</Text>
              </View>
              <View style={[styles.flowModalMetaPill, styles.flowModalMetaPillStatus]}>
                <Text style={styles.flowModalMetaTextStatus}>{flowStatusBadge}</Text>
              </View>
            </View>
            <ScrollView
              style={styles.flowModalStepsScroll}
              contentContainerStyle={styles.flowStepsList}
              showsVerticalScrollIndicator={false}
            >
              {flowSteps.map((step, index) => {
                let stepState: FlowStepState = "pending";
                if (flowProgress.errorIndex !== null) {
                  if (index < flowProgress.errorIndex) stepState = "done";
                  else if (index === flowProgress.errorIndex) stepState = "error";
                } else if (selectedFlowStatus === "SENT_DATA") {
                  stepState = "done";
                } else if (index < flowProgress.activeIndex) {
                  stepState = "done";
                } else if (index === flowProgress.activeIndex) {
                  stepState = "active";
                }

                return (
                  <View key={`${selectedFlowAction}-${index}`} style={styles.flowStepRow}>
                    <View style={styles.flowStepRail}>
                      <View
                        style={[
                          styles.flowStepDot,
                          stepState === "active" && styles.flowStepDotActive,
                          stepState === "done" && styles.flowStepDotDone,
                          stepState === "error" && styles.flowStepDotError,
                        ]}
                      />
                      {index < flowSteps.length - 1 ? (
                        <View
                          style={[
                            styles.flowStepConnector,
                            stepState === "done" && styles.flowStepConnectorDone,
                            stepState === "error" && styles.flowStepConnectorError,
                          ]}
                        />
                      ) : null}
                    </View>
                    <View
                      style={[
                        styles.flowStepCard,
                        stepState === "active" && styles.flowStepCardActive,
                        stepState === "done" && styles.flowStepCardDone,
                        stepState === "error" && styles.flowStepCardError,
                      ]}
                    >
                      <Text
                        style={[
                          styles.flowStepText,
                          stepState === "active" && styles.flowStepTextActive,
                          stepState === "done" && styles.flowStepTextDone,
                          stepState === "error" && styles.flowStepTextError,
                        ]}
                      >
                        {index + 1}. {step}
                      </Text>
                    </View>
                  </View>
                );
              })}
            </ScrollView>
          </View>
        </View>
      </Modal>
    </SafeAreaView>
  );
}
