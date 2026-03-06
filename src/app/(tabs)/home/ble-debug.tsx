import { Buffer } from "buffer";
import { useFocusEffect } from "@react-navigation/native";
import { useRouter } from "expo-router";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  Modal,
  type Permission,
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
  MsgProtTxPacketStatus,
} from "@/utils/ble/messageProtocol";
import {
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
  DOSE_SCHEDULE_PUSH_PAYLOADS,
  PPI_LATE_ACK_WATCH_POLL_MS,
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
  normalizeDecodedValue,
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

type IncomingDetailsTab = "LATEST" | "RESOLVED";

type PendingResponseMatcher = {
  actionName: string;
  ppi: number;
  requestType: number;
  sentAtMs: number;
  sentAt: string;
};

type PushAckPreview = {
  actionName: string;
  ppi: number;
  type: number;
  ackedAt: string;
  ackedAtMs: number;
  sessionId: number | null;
  pktCounter: number | null;
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

function parseMpFrameBytes(raw: Uint8Array): MpFramePreview | null {
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

function isResponseTypeForRequest(requestType: number, incomingType: number): boolean {
  if (requestType === PpiType.RQ) {
    return incomingType === PpiType.RE;
  }

  if (requestType === PpiType.PUSH) {
    return incomingType === PpiType.RE || incomingType === PpiType.PUSH;
  }

  return incomingType !== PpiType.RQ;
}

export default function BleDebugScreen() {
  const router = useRouter();
  const [logCount, setLogCount] = useState(() => getBleDebugLogs().length);
  const [loadingAction, setLoadingAction] = useState<string | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [connectedDeviceLabel, setConnectedDeviceLabel] = useState("");
  const [lastPpiTxPreview, setLastPpiTxPreview] = useState<PpiTxPreview | null>(null);
  const [lastPpiRxPreview, setLastPpiRxPreview] = useState<PpiRxPreview | null>(null);
  const [incomingDetailsTab, setIncomingDetailsTab] = useState<IncomingDetailsTab>("RESOLVED");
  const [pendingResponseMatcher, setPendingResponseMatcher] = useState<PendingResponseMatcher | null>(
    null
  );
  const [resolvedResponsePreview, setResolvedResponsePreview] = useState<PpiRxPreview | null>(null);
  const [resolvedPushAckPreview, setResolvedPushAckPreview] = useState<PushAckPreview | null>(null);
  const [selectedFlowAction, setSelectedFlowAction] = useState<QuickFlowAction>("TIME_PUSH");
  const [flowStatusByAction, setFlowStatusByAction] = useState<Record<string, string>>({});
  const [flowHelpVisible, setFlowHelpVisible] = useState(false);
  const [protocolRunning, setProtocolRunning] = useState(false);

  const ppiProtocolRef = useRef<BleMessageProtocol | null>(null);
  const ensurePpiProtocolRef = useRef<() => Promise<BleMessageProtocol>>(async () => {
    throw new Error("Message protocol not initialized.");
  });
  const connectedDeviceIdRef = useRef("");
  const pendingResponseMatcherRef = useRef<PendingResponseMatcher | null>(null);
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
      "App runs as master and owns SYNC control (SYNC_START/SYNC_ACK/SYNC_MISMATCH).",
      "App and firmware exchange ACK/NAK for DATA delivery and retries.",
      "Check TX ready. TX ready means current TX state is COMPLETED or ABANDONED.",
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

  const selectedFlowPayloadValue = useMemo(() => {
    if (selectedFlowMeta.payloadPreview !== undefined) {
      return selectedFlowMeta.payloadPreview;
    }

    if (selectedFlowMeta.typeId === PpiType.PUSH) {
      if (selectedFlowAction === "TIME_PUSH") {
        return {
          unix_time_s: "Current phone unix timestamp at send time",
        };
      }
      return "(runtime payload)";
    }

    return "(none)";
  }, [selectedFlowAction, selectedFlowMeta]);

  const selectedFlowPayloadValueText = useMemo(
    () => prettifyForLog(selectedFlowPayloadValue),
    [selectedFlowPayloadValue]
  );

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

  const resolvedResponseHumanReadable = useMemo(() => {
    if (!resolvedResponsePreview) return "";
    if (
      typeof resolvedResponsePreview.ppi !== "number" ||
      typeof resolvedResponsePreview.type !== "number"
    ) {
      return "";
    }

    return formatPpiHumanReadable(
      resolvedResponsePreview.ppi,
      resolvedResponsePreview.type,
      resolvedResponsePreview.payloadHex,
      resolvedResponsePreview.decoded
    );
  }, [resolvedResponsePreview]);

  function renderIncomingPreviewDetails(preview: PpiRxPreview, humanReadable: string) {
    return (
      <>
        <Text style={styles.ppiPreviewMeta}>
          {preview.receivedAt} · {preview.source}
        </Text>
        {typeof preview.ppi === "number" ? (
          <Text style={styles.ppiPreviewLine}>
            PPI: {preview.ppiName} ({preview.ppi}) · Type: {preview.typeName} ({preview.type}) · Len:{" "}
            {preview.pktPayloadLen ?? 0}
          </Text>
        ) : null}
        <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {preview.payloadHex ? formatHexBytes(preview.payloadHex) : "(empty)"}
        </Text>
        <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {preview.payloadBase64 || "(empty)"}
        </Text>
        <Text style={styles.ppiPreviewSectionLabel}>Incoming MP Header</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {preview.mpFrame
            ? JSON.stringify(
                {
                  pkt_crc: preview.mpFrame.crc,
                  pkt_counter: preview.mpFrame.pktCounter,
                  session_id: preview.mpFrame.sessionId,
                  pkt_type: preview.mpFrame.pktType,
                  pkt_type_label: getMpPacketTypeLabel(preview.mpFrame.pktType),
                  status: preview.mpFrame.status,
                  payload_type: preview.mpFrame.payloadType,
                  payload_ppi: preview.mpFrame.payloadPpi,
                  pkt_payload_len: preview.mpFrame.pktPayloadLen,
                  frame_len: preview.mpFrame.frameLength,
                },
                null,
                2
              )
            : "(not captured yet)"}
        </Text>
        <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Hex</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {preview.fullFrameHex ? formatHexBytes(preview.fullFrameHex) : "(not captured yet)"}
        </Text>
        <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Base64</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {preview.fullFrameBase64 || "(not captured yet)"}
        </Text>
        <Text style={styles.ppiPreviewSectionLabel}>Decoded Value</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {formatDecodedValue(preview.decoded)}
        </Text>
        {/*<Text style={styles.ppiPreviewSectionLabel}>Parsed Payload</Text>
        <Text style={styles.ppiPreviewCode} selectable>
          {humanReadable || "(not available)"}
        </Text>*/}
      </>
    );
  }

  function openFlowHelp(action: QuickFlowAction) {
    setSelectedFlowAction(action);
    setFlowHelpVisible(true);
  }

  function isSameMatcher(
    left: PendingResponseMatcher | null,
    right: PendingResponseMatcher
  ): boolean {
    if (!left) return false;
    return (
      left.actionName === right.actionName &&
      left.ppi === right.ppi &&
      left.requestType === right.requestType &&
      left.sentAtMs === right.sentAtMs
    );
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

    const required: Permission[] = [];
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

  function toErrorDetails(error: unknown) {
    if (error instanceof Error) {
      return {
        name: error.name,
        message: error.message,
        stack: error.stack ?? "",
      };
    }

    return {
      message: String(error),
    };
  }

  function enrichParsedIncomingPayload(details: unknown): unknown {
    if (!details || typeof details !== "object") {
      return details;
    }

    const record = details as Record<string, unknown>;
    const payloadRecord = record.payload;
    if (!payloadRecord || typeof payloadRecord !== "object") {
      return details;
    }

    const payload = payloadRecord as Record<string, unknown>;
    const ppi = payload.ppi;
    const type = payload.type;
    const payloadHex = payload.payloadHex;
    if (typeof ppi !== "number" || typeof type !== "number" || typeof payloadHex !== "string") {
      return details;
    }

    const normalizedHex = payloadHex.replace(/[^0-9a-fA-F]/g, "");
    try {
      const decoded = decodePpiPayload(
        ppi,
        type as PpiType,
        new Uint8Array(Buffer.from(normalizedHex, "hex"))
      );
      return {
        ...record,
        payload: {
          ...payload,
          decoded: normalizeDecodedValue(decoded.value),
        },
      };
    } catch {
      return details;
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
        const payload =
          first === "Parsed incoming value."
            ? enrichParsedIncomingPayload(rest[0])
            : rest[0];
        addLog(`[PPI][MP][${level}] ${first}`, payload);
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
      connectedDeviceIdRef.current = connected ? label : "";
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
    pendingResponseMatcherRef.current = null;
    setPendingResponseMatcher(null);
    setResolvedResponsePreview(null);
    setResolvedPushAckPreview(null);
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
    timeoutMs = PPI_TX_READY_TIMEOUT_MS,
    pollMs = PPI_TX_READY_POLL_MS
  ) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() <= deadline) {
      if (isTxStatusSendable(protocol.getTxPacketStatus())) {
        return true;
      }

      await protocol.process();
      await new Promise((resolve) => setTimeout(resolve, pollMs));
    }

    return isTxStatusSendable(protocol.getTxPacketStatus());
  }

  async function ensurePpiProtocolReadyForDataSend(protocol: MessageProtocolInterface) {
    if (protocol.getCurrentSessionId() === 0) {
      addLog("[PPI][SYNC] Session is 0, starting explicit sync before DATA send.");
      const syncResult = await protocol.startSync();
      if (syncResult !== MsgProtError.NONE) {
        addLog("[PPI][SYNC][ERR] Failed to start sync.", { syncResult });
        return false;
      }
    }

    return waitForPpiTxSendable(protocol);
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

  async function watchPushAckResolution(
    protocol: BleMessageProtocol,
    matcher: PendingResponseMatcher,
    txFrame: MpFramePreview | null
  ) {
    lateAckWatchTokenRef.current += 1;
    const watchToken = lateAckWatchTokenRef.current;
    const deadline = Date.now() + PPI_LATE_ACK_WATCH_TIMEOUT_MS;

    while (Date.now() <= deadline) {
      if (lateAckWatchTokenRef.current !== watchToken) {
        return;
      }

      try {
        await protocol.process();
      } catch {
        // Keep polling until timeout; process errors can be transient in debug mode.
      }

      const txStatus = protocol.getTxPacketStatus();
      if (txStatus === MsgProtTxPacketStatus.COMPLETED) {
        const ackedAtMs = Date.now();
        setResolvedPushAckPreview({
          actionName: matcher.actionName,
          ppi: matcher.ppi,
          type: matcher.requestType,
          ackedAt: new Date(ackedAtMs).toLocaleTimeString(),
          ackedAtMs,
          sessionId: txFrame?.sessionId ?? null,
          pktCounter: txFrame?.pktCounter ?? null,
        });

        if (isSameMatcher(pendingResponseMatcherRef.current, matcher)) {
          pendingResponseMatcherRef.current = null;
          setPendingResponseMatcher(null);
        }
        return;
      }

      if (txStatus === MsgProtTxPacketStatus.ABANDONED) {
        return;
      }

      await new Promise((resolve) => setTimeout(resolve, PPI_LATE_ACK_WATCH_POLL_MS));
    }
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
      enableSyncControl: true,
      sendAckNak: true,
      autoConsumeRx: true,
      onRxDataAcked: (ackedPacket) => {
        const payloadHex = Buffer.from(ackedPacket.payload.payload).toString("hex");
        const deviceId = connectedDeviceIdRef.current || "unknown";
        if (!payloadHex || deviceId === "unknown") {
          return;
        }

        // void ingestRawHardwareData({
        //   payloadHex,
        //   timestamp: new Date(),
        //   deviceId,
        // }).catch((error) => {
        //   addLog("[PPI][INGEST][WARN] Failed to ingest ACKed payload.", {
        //     error: toErrorDetails(error),
        //     deviceId,
        //     sessionId: ackedPacket.sessionId,
        //     pktCounter: ackedPacket.pktCounter,
        //     ppi: ackedPacket.payload.ppi,
        //     type: ackedPacket.payload.type,
        //   });
        // });
      },
      logger: {
        debug: (...args: unknown[]) => relayMessageProtocolLog("DEBUG", args),
        info: (...args: unknown[]) => relayMessageProtocolLog("INFO", args),
        warn: (...args: unknown[]) => relayMessageProtocolLog("WARN", args),
        error: (...args: unknown[]) => relayMessageProtocolLog("ERR", args),
      },
      onRxPacket: (packet) => {
        const payloadHex = Buffer.from(packet.payload).toString("hex");
        const payloadBase64 = Buffer.from(packet.payload).toString("base64");

        let rxFrame: MpFramePreview | null = null;
        try {
          const rxFrameRaw = ppiProtocolRef.current?.getLastRxPacketRaw() ?? new Uint8Array(0);
          rxFrame = parseMpFrameBytes(rxFrameRaw);
        } catch (error) {
          addLog("[PPI][NOTIFY][WARN] Failed to parse RX frame.", toErrorDetails(error));
        }

        let decodedValue: unknown = packet.payload;
        try {
          const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
          decodedValue = decoded.value;
        } catch (error) {
          addLog("[PPI][NOTIFY][WARN] Failed to decode PPI payload.", {
            ppi: packet.ppi,
            type: packet.type,
            payloadHex,
            error: toErrorDetails(error),
          });
        }
        const normalizedDecodedValue = normalizeDecodedValue(decodedValue);
        const receivedAtMs = Date.now();
        const receivedAt = new Date(receivedAtMs).toLocaleTimeString();
        const rxPreview: PpiRxPreview = {
          source: "Message Protocol",
          receivedAt,
          receivedAtMs,
          ppi: packet.ppi,
          ppiName: PpiId[packet.ppi as PpiId] ?? `PPI_${packet.ppi}`,
          type: packet.type,
          typeName: PpiType[packet.type as PpiType] ?? `TYPE_${packet.type}`,
          pktPayloadLen: packet.pktPayloadLen,
          payloadHex,
          payloadBase64,
          fullFrameHex: rxFrame?.frameHex ?? "",
          fullFrameBase64: rxFrame?.frameBase64 ?? "",
          mpFrame: rxFrame,
          decoded: normalizedDecodedValue,
        };

        try {
          setLastPpiRxPreview(rxPreview);
        } catch (error) {
          addLog("[PPI][NOTIFY][ERR] Failed to update RX preview state.", toErrorDetails(error));
        }

        const pending = pendingResponseMatcherRef.current;
        if (
          pending &&
          packet.ppi === pending.ppi &&
          isResponseTypeForRequest(pending.requestType, packet.type) &&
          receivedAtMs >= pending.sentAtMs
        ) {
          setResolvedResponsePreview(rxPreview);
          addLog(`[PPI][${pending.actionName}] RESPONSE_RESOLVED`, {
            sentAt: pending.sentAt,
            receivedAt: rxPreview.receivedAt,
            ppi: rxPreview.ppi,
            ppiName: rxPreview.ppiName,
            type: rxPreview.type,
            typeName: rxPreview.typeName,
            payloadLen: rxPreview.pktPayloadLen,
            decoded: rxPreview.decoded,
          });
          pendingResponseMatcherRef.current = null;
          setPendingResponseMatcher(null);
        }
      },
    });

    ppiProtocolRef.current = protocol;
    let syncStartResult: MsgProtError | null = null;
    let syncReady = false;
    try {
      await protocol.start();
      syncStartResult = await protocol.startSync();
      syncReady =
        syncStartResult === MsgProtError.NONE
          ? await waitForPpiTxSendable(protocol)
          : false;
    } catch (error) {
      try {
        protocol.stop();
      } catch {
        // Ignore stop failures during startup cleanup.
      }
      ppiProtocolRef.current = null;
      setProtocolRunning(false);
      isGattDiscoveredRef.current = false;
      gattDiscoveryInFlightRef.current = null;
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
      syncStartResult,
      syncReady,
    });
    if (!syncReady) {
      addLog("[PPI][WARN] Sync did not complete in readiness window; waiting for SYNC_ACK.");
    }

    return protocol;
  }

  ensurePpiProtocolRef.current = ensurePpiProtocol;

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
    const txReady = await ensurePpiProtocolReadyForDataSend(protocol);
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
          "TX ready requires current TX state to be COMPLETED or ABANDONED before queuing the next DATA frame.",
        lastTxFrameHex: lastTxFrameHex || null,
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

    const sentAtMs = Date.now();
    const sentAt = new Date(sentAtMs).toLocaleTimeString();
    const matcher: PendingResponseMatcher = {
      actionName,
      ppi,
      requestType: type,
      sentAtMs,
      sentAt,
    };
    lateAckWatchTokenRef.current += 1;
    pendingResponseMatcherRef.current = matcher;
    setPendingResponseMatcher(matcher);
    setResolvedResponsePreview(null);
    setResolvedPushAckPreview(null);

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
      sentAt,
    });
    setFlowStatusByAction((prev) => ({ ...prev, [actionName]: status }));

    addLog(`[PPI][${actionName}] ${status}`, {
      ppi,
      type,
      payloadLen: payload.length,
      sessionId: txFrame?.sessionId ?? null,
      pktCounter: txFrame?.pktCounter ?? null,
    });

    if (type === PpiType.PUSH) {
      void watchPushAckResolution(protocol, matcher, txFrame);
    }

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

  function buildDoseSchedulePayload(
    action:
      | "DOSE_SCHEDULE_PUSH"
      | "DOSE_SCHEDULE_PUSH_ALT_1"
      | "DOSE_SCHEDULE_PUSH_ALT_2"
  ) {
    return encodeDoseSchedulePpi(DOSE_SCHEDULE_PUSH_PAYLOADS[action]);
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
      const payload = buildDoseSchedulePayload("DOSE_SCHEDULE_PUSH");
      await sendPpi("DOSE_SCHEDULE_PUSH", PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, payload);
    });
  }

  async function onPpiDoseSchedulePushAlt1() {
    setSelectedFlowAction("DOSE_SCHEDULE_PUSH_ALT_1");
    await withBusy("ppi-dose-schedule-push-alt-1", async () => {
      const payload = buildDoseSchedulePayload("DOSE_SCHEDULE_PUSH_ALT_1");
      await sendPpi("DOSE_SCHEDULE_PUSH_ALT_1", PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, payload);
    });
  }

  async function onPpiDoseSchedulePushAlt2() {
    setSelectedFlowAction("DOSE_SCHEDULE_PUSH_ALT_2");
    await withBusy("ppi-dose-schedule-push-alt-2", async () => {
      const payload = buildDoseSchedulePayload("DOSE_SCHEDULE_PUSH_ALT_2");
      await sendPpi("DOSE_SCHEDULE_PUSH_ALT_2", PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, payload);
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
        connectedDeviceIdRef.current = "";
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

  useEffect(() => {
    if (!isConnected) {
      return;
    }

    if (ppiProtocolRef.current) {
      return;
    }

    let cancelled = false;
    const maxAttempts = 3;

    const runAutoStart = async () => {
      for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
        if (cancelled || ppiProtocolRef.current) {
          return;
        }

        const startProtocol = ensurePpiProtocolRef.current;
        try {
          await startProtocol();
          return;
        } catch (error) {
          addLog(
            `[PPI][AUTO][WARN] Auto-start attempt ${attempt}/${maxAttempts} failed.`,
            toErrorDetails(error)
          );

          if (attempt < maxAttempts) {
            // Force fresh GATT discovery before retrying subscription/notifications.
            isGattDiscoveredRef.current = false;
            gattDiscoveryInFlightRef.current = null;
            await new Promise((resolve) => setTimeout(resolve, 700));
          }
        }
      }
    };

    void runAutoStart();

    return () => {
      cancelled = true;
    };
  }, [isConnected]);

  const quickActionMap: Record<QuickFlowAction, () => Promise<void>> = {
    TIME_RQ: onPpiTimeRequest,
    TIME_PUSH: onPpiTimePush,
    DOSE_SCHEDULE_RQ: onPpiDoseScheduleRequest,
    DOSE_SCHEDULE_PUSH: onPpiDoseSchedulePush,
    DOSE_SCHEDULE_PUSH_ALT_1: onPpiDoseSchedulePushAlt1,
    DOSE_SCHEDULE_PUSH_ALT_2: onPpiDoseSchedulePushAlt2,
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
        return "AD_DOSE_SCHEDULE (PUSH, preset A)";
      case "DOSE_SCHEDULE_PUSH_ALT_1":
        return "AD_DOSE_SCHEDULE (PUSH, preset B)";
      case "DOSE_SCHEDULE_PUSH_ALT_2":
        return "AD_DOSE_SCHEDULE (PUSH, preset C)";
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
            {/* <Text style={styles.sectionHint}>{statusText}</Text> */}
          </View>
        </View>

        <View style={styles.panel}>
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Quick Actions</Text>
            {/* <Text style={styles.sectionHint}>
              Message protocol running means RX notifications (UUID 0x1509) are active and `process()` is executed on-demand to drive ACK/retry state.
            </Text>
            <Text style={styles.sectionHint}>
              App master mode owns session sync control and initiates `SYNC_START` as needed.
            </Text>
            <Text style={styles.sectionHint}>
              TX ready means the TX state is COMPLETED or ABANDONED, so the next DATA frame can be queued.
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
                {/*<Text style={styles.ppiPreviewSectionLabel}>Parsed Payload</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxHumanReadable || "(not available)"}
                </Text>*/}
                <Text style={styles.ppiPreviewSectionLabel}>Full Frame Hex (CRC + header + PPI + payload)</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.fullFrameHex
                    ? formatHexBytes(lastPpiTxPreview.fullFrameHex)
                    : "(not captured yet)"}
                </Text>
                <Text style={styles.ppiPreviewSectionLabel}>Full Frame Base64</Text>
                <Text style={styles.ppiPreviewCode} selectable>
                  {lastPpiTxPreview.fullFrameBase64 || "(not captured yet)"}
                </Text>
              </>
            )}

            <Text style={styles.ppiPreviewPrimaryLabel}>Last Incoming Update</Text>
            <View style={styles.panelTabs}>
              <Pressable
                style={[styles.tabButton, incomingDetailsTab === "RESOLVED" ? styles.tabButtonActive : null]}
                onPress={() => setIncomingDetailsTab("RESOLVED")}
              >
                <Text
                  style={[styles.tabText, incomingDetailsTab === "RESOLVED" ? styles.tabTextActive : null]}
                >
                  Response To Last Sent
                </Text>
              </Pressable>
              <Pressable
                style={[styles.tabButton, incomingDetailsTab === "LATEST" ? styles.tabButtonActive : null]}
                onPress={() => setIncomingDetailsTab("LATEST")}
              >
                <Text style={[styles.tabText, incomingDetailsTab === "LATEST" ? styles.tabTextActive : null]}>
                  Latest Incoming
                </Text>
              </Pressable>
            </View>
            {incomingDetailsTab === "LATEST" ? (
              !lastPpiRxPreview ? (
                <Text style={styles.ppiPreviewEmpty}>No incoming value yet.</Text>
              ) : (
                renderIncomingPreviewDetails(lastPpiRxPreview, lastPpiRxHumanReadable)
              )
            ) : !lastPpiTxPreview || lastPpiTxPreview.status !== "SENT_DATA" ? (
              <Text style={styles.ppiPreviewEmpty}>No sent request available yet.</Text>
            ) : resolvedResponsePreview ? (
              <>
                <Text style={styles.ppiPreviewMeta}>
                  Resolved for {lastPpiTxPreview.action} sent at {lastPpiTxPreview.sentAt}
                </Text>
                {renderIncomingPreviewDetails(resolvedResponsePreview, resolvedResponseHumanReadable)}
              </>
            ) : resolvedPushAckPreview &&
              lastPpiTxPreview.type === PpiType.PUSH &&
              resolvedPushAckPreview.actionName === lastPpiTxPreview.action ? (
              <>
                <Text style={styles.ppiPreviewMeta}>
                  ACK received for {resolvedPushAckPreview.actionName} at {resolvedPushAckPreview.ackedAt}
                </Text>
                <Text style={styles.ppiPreviewLine}>
                  PPI: {resolvedPushAckPreview.ppi} · Type: {resolvedPushAckPreview.type} · session_id:{" "}
                  {resolvedPushAckPreview.sessionId ?? "(unknown)"} · pkt_counter:{" "}
                  {resolvedPushAckPreview.pktCounter ?? "(unknown)"}
                </Text>
              </>
            ) : pendingResponseMatcher ? (
              <Text style={styles.ppiPreviewEmpty}>
                Waiting for{" "}
                {pendingResponseMatcher.requestType === PpiType.PUSH ? "ACK/response" : "response"} to{" "}
                {pendingResponseMatcher.actionName} (PPI {pendingResponseMatcher.ppi}) sent at{" "}
                {pendingResponseMatcher.sentAt}.
              </Text>
            ) : (
              <Text style={styles.ppiPreviewEmpty}>No matching response resolved yet for last sent request.</Text>
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
            <View style={styles.flowModalCurrentCard}>
              <Text style={styles.flowModalCurrentLabel}>
                {selectedFlowMeta.typeId === PpiType.PUSH ? "Value To Push" : "Payload Value"}
              </Text>
              <Text style={styles.flowModalCurrentText} selectable>
                {selectedFlowPayloadValueText}
              </Text>
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
