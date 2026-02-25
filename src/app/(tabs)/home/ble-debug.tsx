import { Buffer } from "buffer";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { useRouter } from "expo-router";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  Keyboard,
  KeyboardAvoidingView,
  Modal,
  NativeScrollEvent,
  NativeSyntheticEvent,
  PermissionsAndroid,
  Platform,
  Pressable,
  ScrollView,
  Text,
  TextInput,
  TouchableWithoutFeedback,
  useWindowDimensions,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "@/constants/ble";
import { validoseGrey } from "@/constants/colors";
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
  connect,
  discoverServicesAndCharacteristics,
  disconnect,
  getConnectedDevice,
  isDeviceConnected,
  scanLeDevice,
  subscribeToCharacteristic,
} from "../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { ActionButton } from "./ble-debug/components/ActionButton";
import { QuickPpiButton } from "./ble-debug/components/QuickPpiButton";
import {
  DEBUG_INPUTS_STORAGE_KEY,
  PPI_LATE_ACK_WATCH_POLL_MS,
  PPI_LATE_ACK_WATCH_TIMEOUT_MS,
  PPI_MANUAL_ACK_TIMEOUT_MS,
  PPI_MANUAL_MAX_RETRIES,
  PPI_MANUAL_SYNC_RETRY_MS,
  PPI_NRF_MANUAL_ACK_TIMEOUT_MS,
  PPI_NRF_MANUAL_MAX_RETRIES,
  PPI_NRF_TX_COMPLETION_WAIT_MS,
  PPI_TX_COMPLETION_WAIT_MS,
  PPI_TX_READY_POLL_MS,
  PPI_TX_READY_TIMEOUT_MS,
  QUICK_FLOW_META,
} from "./ble-debug/constants";
import {
  extractConnectedDeviceLabel,
  formatDecodedValue,
  prettifyForLog,
  resolveConnectionState,
} from "./ble-debug/helpers";
import { styles } from "./ble-debug/styles";
import type {
  FlowStepState,
  PersistedDebugInputs,
  PpiRxPreview,
  PpiTxPreview,
  QuickFlowAction,
} from "./ble-debug/types";

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

  // Show only the decoded payload content that maps directly to transmitted bytes.
  return formatDecodedValue(decodedValue);
}

export default function BleDebugScreen() {
  const router = useRouter();
  const { width: windowWidth } = useWindowDimensions();
  const [deviceId, setDeviceId] = useState("");
  const [serviceUuid, setServiceUuid] = useState(SERVICE_UUIDS.CUSTOM_SERVICE);
  const [characteristicUuid, setCharacteristicUuid] = useState(
    CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL
  );
  const [logCount, setLogCount] = useState(() => getBleDebugLogs().length);
  const [loadingAction, setLoadingAction] = useState<string | null>(null);
  const [inputsHydrated, setInputsHydrated] = useState(false);
  const [manualNrfMode, setManualNrfMode] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const [connectedDeviceLabel, setConnectedDeviceLabel] = useState("");
  const [lastPpiTxPreview, setLastPpiTxPreview] = useState<PpiTxPreview | null>(null);
  const [lastPpiRxPreview, setLastPpiRxPreview] = useState<PpiRxPreview | null>(null);
  const [selectedFlowAction, setSelectedFlowAction] = useState<QuickFlowAction>("TIME_PUSH");
  const [flowStatusByAction, setFlowStatusByAction] = useState<Record<string, string>>({});
  const [flowHelpVisible, setFlowHelpVisible] = useState(false);
  const [activeCarouselCard, setActiveCarouselCard] = useState(0);
  const [protocolRunning, setProtocolRunning] = useState(false);

  const subscriptionsRef = useRef<(() => void)[]>([]);
  const ppiProtocolRef = useRef<BleMessageProtocol | null>(null);
  const lateAckWatchTokenRef = useRef(0);
  const gattDiscoveryInFlightRef = useRef<Promise<void> | null>(null);
  const isGattDiscoveredRef = useRef(false);

  const canRun = useMemo(() => Boolean(characteristicUuid.trim()), [characteristicUuid]);
  const carouselCardWidth = useMemo(
    () => Math.max(260, Math.min(windowWidth - 64, 528)),
    [windowWidth]
  );
  const carouselGap = 12;
  const carouselSnapOffsets = useMemo(() => {
    const secondCardStart = carouselCardWidth + carouselGap;
    return [0, secondCardStart];
  }, [carouselCardWidth]);
  const statusText = useMemo(() => {
    const actionText: Record<string, string> = {
      connect: "Trying to connect to your selected device.",
      "ppi-time-rq": "Requesting current dock time (AD_TIME RQ).",
      "ppi-time-re": "Sending AD_TIME RE test payload (unix uint32).",
      "ppi-time-push": "Sending current phone time to device (AD_TIME PUSH).",
      "ppi-dose-schedule-rq": "Requesting dose schedule from dock (AD_DOSE_SCHEDULE RQ).",
      "ppi-dose-schedule-re": "Sending AD_DOSE_SCHEDULE RE test payload.",
      "ppi-dose-schedule-push": "Pushing demo dose schedule to dock (AD_DOSE_SCHEDULE PUSH).",
      subscribe: "Starting notifications on the selected characteristic.",
      disconnect: "Disconnecting from the peripheral.",
    };
    if (loadingAction && actionText[loadingAction]) {
      return actionText[loadingAction];
    }

    if (lastPpiTxPreview?.status === "SENT_ACKED") {
      return `Done. ${lastPpiTxPreview.action.replaceAll("_", " ")} was acknowledged by peripheral.`;
    }
    if (lastPpiTxPreview?.status === "SENT_WAITING_ACK") {
      return "Write sent. Waiting for ACK notification from peripheral.";
    }
    if (lastPpiTxPreview?.status === "WAITING_SYNC_ACK") {
      return "Waiting for SYNC_ACK from peripheral before sending data.";
    }
    if (lastPpiTxPreview?.status === "TX_BUSY") {
      return "Protocol is busy. Please retry after a moment.";
    }

    if (!isConnected) {
      return "Not connected. Connect a device to start protocol actions.";
    }

    return "Ready: idle state, no active protocol send in progress.";
  }, [isConnected, lastPpiTxPreview, loadingAction]);
  const protocolInfo = useMemo(() => {
    const ackTimeoutMs = manualNrfMode ? PPI_NRF_MANUAL_ACK_TIMEOUT_MS : PPI_MANUAL_ACK_TIMEOUT_MS;
    const maxRetries = manualNrfMode ? PPI_NRF_MANUAL_MAX_RETRIES : PPI_MANUAL_MAX_RETRIES;
    const syncRetryIntervalMs = manualNrfMode
      ? PPI_NRF_MANUAL_ACK_TIMEOUT_MS
      : PPI_MANUAL_SYNC_RETRY_MS;
    const txCompletionWaitMs = manualNrfMode
      ? PPI_NRF_TX_COMPLETION_WAIT_MS
      : PPI_TX_COMPLETION_WAIT_MS;

    return {
      mode: manualNrfMode ? "Manual nRF (no SYNC_START required)" : "Full Sync (SYNC_START + SYNC_ACK)",
      isMaster: manualNrfMode ? "false" : "true",
      relaxedAckMatching: manualNrfMode ? "true" : "false",
      ackTimeoutMs,
      maxRetries,
      syncRetryIntervalMs,
      txReadyTimeoutMs: PPI_TX_READY_TIMEOUT_MS,
      txCompletionWaitMs,
      lateAckWatchMs: PPI_LATE_ACK_WATCH_TIMEOUT_MS,
      processIntervalMs: 250,
      maxPacketLength: 244,
      txState: lastPpiTxPreview?.status || "IDLE",
      protocolState: protocolRunning ? "Running" : "Stopped",
      logCount,
    };
  }, [lastPpiTxPreview?.status, manualNrfMode, protocolRunning, logCount]);
  const selectedFlowMeta = useMemo(() => QUICK_FLOW_META[selectedFlowAction], [selectedFlowAction]);
  const selectedFlowStatus = useMemo(
    () => flowStatusByAction[selectedFlowMeta.actionName] ?? "",
    [flowStatusByAction, selectedFlowMeta.actionName]
  );
  const isActionInProgress = useMemo(() => Boolean(loadingAction), [loadingAction]);
  const isBlockedByOtherAction = useCallback(
    (action: string) => Boolean(loadingAction && loadingAction !== action),
    [loadingAction]
  );
  const selectedFlowModeText = manualNrfMode ? "Manual nRF Mode" : "Full Sync Mode";
  const flowSteps = useMemo(() => {
    const steps = [
      "Tap the quick action button.",
      `Build payload (${selectedFlowMeta.payloadHint}) and validate ${selectedFlowMeta.ppiName} ${selectedFlowMeta.typeName}.`,
      "Ensure message protocol is running and TX is ready.",
    ];

    if (!manualNrfMode) {
      steps.push("Send SYNC_START to firmware.");
      steps.push("Wait for SYNC_ACK from firmware.");
    }

    steps.push(
      `Send DATA frame with type=${selectedFlowMeta.typeId}, ppi=${selectedFlowMeta.ppiId}, ${selectedFlowMeta.lenHint}.`
    );
    steps.push("Wait for ACK with matching session_id and pkt_counter.");
    steps.push("Complete with SENT_ACKED, or fail with TX_ABANDONED after timeout/retries.");

    return steps;
  }, [manualNrfMode, selectedFlowMeta]);
  const flowProgress = useMemo(() => {
    const syncAckIndex = manualNrfMode ? -1 : 4;
    const sendIndex = manualNrfMode ? 3 : 5;
    const ackIndex = manualNrfMode ? 4 : 6;
    const doneIndex = manualNrfMode ? 5 : 7;

    let activeIndex = 0;
    let errorIndex: number | null = null;

    if (loadingAction === selectedFlowMeta.busyKey) {
      activeIndex = 2;
    }

    if (selectedFlowStatus === "WAITING_SYNC_ACK" && syncAckIndex >= 0) {
      activeIndex = syncAckIndex;
    } else if (selectedFlowStatus === "SENT_WAITING_ACK") {
      activeIndex = ackIndex;
    } else if (selectedFlowStatus === "SENT_ACKED") {
      activeIndex = doneIndex;
    } else if (selectedFlowStatus === "TX_BUSY") {
      activeIndex = 2;
    } else if (selectedFlowStatus === "PAYLOAD_LENGTH_MISMATCH") {
      errorIndex = 1;
      activeIndex = 1;
    } else if (selectedFlowStatus.startsWith("SEND_ERROR_")) {
      errorIndex = sendIndex;
      activeIndex = sendIndex;
    } else if (selectedFlowStatus === "TX_ABANDONED") {
      errorIndex = doneIndex;
      activeIndex = doneIndex;
    }

    return { activeIndex, errorIndex };
  }, [loadingAction, manualNrfMode, selectedFlowMeta.busyKey, selectedFlowStatus]);
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

    const required: string[] = [];
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

  const refreshConnectionBanner = useCallback(async (fallbackLabel = "") => {
    try {
      const [connectedResponse, connectedDevice] = await Promise.all([
        isDeviceConnected(),
        getConnectedDevice(),
      ]);
      const connected = resolveConnectionState(connectedResponse);
      const label =
        extractConnectedDeviceLabel(connectedDevice) ||
        extractConnectedDeviceLabel(connectedResponse) ||
        fallbackLabel;

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
          isGattDiscoveredRef.current = true;
          addLog("[DISCOVER] Services/characteristics ready.", response);
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

  function onToggleManualNrfMode() {
    const next = !manualNrfMode;
    setManualNrfMode(next);
    stopPpiProtocol();
    setLastPpiTxPreview(null);
    setFlowStatusByAction({});
    addLog(`[PPI] Manual nRF mode ${next ? "ON" : "OFF"}.`, {
      behavior: next
        ? "Skip SYNC_START/SYNC_ACK. Send DATA directly; only ACK required."
        : "Use full Message Protocol sync flow.",
    });
  }

  async function waitForPpiTxSendable(
    protocol: MessageProtocolInterface,
    timeoutMs = PPI_TX_READY_TIMEOUT_MS,
    pollMs = PPI_TX_READY_POLL_MS
  ) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() <= deadline) {
      if (isTxStatusSendable(protocol.getTxPacketStatus())) return true;
      await protocol.process();
      await new Promise((resolve) => setTimeout(resolve, pollMs));
    }
    return isTxStatusSendable(protocol.getTxPacketStatus());
  }

  async function waitForPpiTxCompletion(
    protocol: MessageProtocolInterface,
    timeoutMs = PPI_TX_COMPLETION_WAIT_MS,
    pollMs = 50
  ) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() <= deadline) {
      const status = protocol.getTxPacketStatus();
      if (status === MsgProtTxPacketStatus.COMPLETED) return "COMPLETED";
      if (status === MsgProtTxPacketStatus.ABANDONED) return "ABANDONED";
      await protocol.process();
      await new Promise((resolve) => setTimeout(resolve, pollMs));
    }
    return "TIMEOUT";
  }

  async function watchForLateTxCompletion(
    protocol: MessageProtocolInterface,
    actionName: string,
    ppi: number,
    type: number,
    payloadHex: string
  ) {
    const watchToken = ++lateAckWatchTokenRef.current;
    const deadline = Date.now() + PPI_LATE_ACK_WATCH_TIMEOUT_MS;

    while (Date.now() <= deadline) {
      if (lateAckWatchTokenRef.current !== watchToken) {
        return;
      }

      const status = protocol.getTxPacketStatus();
      if (status === MsgProtTxPacketStatus.COMPLETED || status === MsgProtTxPacketStatus.ABANDONED) {
        const resolvedStatus = status === MsgProtTxPacketStatus.COMPLETED ? "SENT_ACKED" : "TX_ABANDONED";

        setLastPpiTxPreview((prev) => {
          if (!prev || prev.action !== actionName || prev.status !== "SENT_WAITING_ACK") {
            return prev;
          }
          return {
            ...prev,
            status: resolvedStatus,
            sentAt: new Date().toLocaleTimeString(),
          };
        });
        setFlowStatusByAction((prev) => ({ ...prev, [actionName]: resolvedStatus }));

        addLog(`[PPI][${actionName}] ${resolvedStatus} (late update).`, {
          ppi,
          type,
          payloadHex,
        });
        return;
      }

      await protocol.process();
      await new Promise((resolve) => setTimeout(resolve, PPI_LATE_ACK_WATCH_POLL_MS));
    }
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

    const mpService = serviceUuid.trim() || SERVICE_UUIDS.CUSTOM_SERVICE;
    const mpCharacteristic = characteristicUuid.trim() || CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL;
    if (mpCharacteristic.toLowerCase() !== CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL.toLowerCase()) {
      addLog(
        "[PPI][WARN] Characteristic UUID is not the expected MESSAGE_PROTOCOL UUID. Attempting anyway.",
        {
          expected: CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL,
          provided: mpCharacteristic,
        }
      );
    }

    const ackTimeoutMs = manualNrfMode ? PPI_NRF_MANUAL_ACK_TIMEOUT_MS : PPI_MANUAL_ACK_TIMEOUT_MS;
    const maxRetries = manualNrfMode ? PPI_NRF_MANUAL_MAX_RETRIES : PPI_MANUAL_MAX_RETRIES;
    const syncRetryIntervalMs = manualNrfMode
      ? PPI_NRF_MANUAL_ACK_TIMEOUT_MS
      : PPI_MANUAL_SYNC_RETRY_MS;

    const protocol = new BleMessageProtocol({
      txCharacteristicUUID: mpCharacteristic,
      rxCharacteristicUUID: mpCharacteristic,
      serviceUUID: mpService,
      processIntervalMs: 250,
      ackTimeoutMs,
      maxRetries,
      syncRetryIntervalMs,
      maxPacketLength: 244,
      isMaster: !manualNrfMode,
      relaxedAckMatching: manualNrfMode,
      autoConsumeRx: true,
      logger: {
        warn: (...args: unknown[]) => addLog("[PPI][MP][WARN]", args),
        error: (...args: unknown[]) => addLog("[PPI][MP][ERR]", args),
      },
      onRxPacket: (packet) => {
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
          decoded: decoded.value,
        });
        addLog("[PPI][NOTIFY]", {
          ppi: packet.ppi,
          type: packet.type,
          payloadHex,
          payloadUtf8,
          decoded: decoded.value,
        });
      },
    });

    await protocol.start();
    ppiProtocolRef.current = protocol;
    setProtocolRunning(true);
    addLog("[PPI] Message protocol started", {
      serviceUuid: mpService,
      characteristicUuid: mpCharacteristic,
      manualNrfMode,
      isMaster: !manualNrfMode,
      ackTimeoutMs,
      syncRetryIntervalMs,
      maxRetries,
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
    };

    // Cancel any previous late-ACK watcher when a new TX starts.
    lateAckWatchTokenRef.current += 1;

    const protocol = await ensurePpiProtocol();
    const txReady = await waitForPpiTxSendable(protocol);
    if (!txReady) {
      const lastTxFrameRaw = protocol.getLastTxPacketRaw();
      const lastTxFrameHex = Buffer.from(lastTxFrameRaw).toString("hex");
      const lastTxPktType = lastTxFrameRaw.length > 8 ? lastTxFrameRaw[8] : null;
      const waitingForSyncAck = !manualNrfMode && lastTxPktType === 3;
      setLastPpiTxPreview({
        ...basePreview,
        fullFrameHex: lastTxFrameHex,
        fullFrameBase64: lastTxFrameHex ? Buffer.from(lastTxFrameHex, "hex").toString("base64") : "",
        status: waitingForSyncAck ? "WAITING_SYNC_ACK" : "TX_BUSY",
        sentAt: new Date().toLocaleTimeString(),
      });
      const blockedStatus = waitingForSyncAck ? "WAITING_SYNC_ACK" : "TX_BUSY";
      setFlowStatusByAction((prev) => ({ ...prev, [actionName]: blockedStatus }));
      addLog(`[PPI][${actionName}] Message protocol is not ready yet.`, {
        reason: waitingForSyncAck
          ? "Waiting for SYNC_ACK from peripheral for SYNC_START."
          : "TX busy / not sendable.",
        hint: manualNrfMode
          ? "Manual nRF mode: notify ACK for each DATA packet."
          : "Notify SYNC_ACK for SYNC_START, then ACK for each DATA packet.",
        lastTxFrameHex: lastTxFrameHex || null,
      });
      return blockedStatus;
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
    const txCompletionWaitMs = manualNrfMode
      ? PPI_NRF_TX_COMPLETION_WAIT_MS
      : PPI_TX_COMPLETION_WAIT_MS;
    const txOutcome = await waitForPpiTxCompletion(protocol, txCompletionWaitMs);
    const status =
      txOutcome === "COMPLETED"
        ? "SENT_ACKED"
        : txOutcome === "ABANDONED"
          ? "TX_ABANDONED"
          : "SENT_WAITING_ACK";

    setLastPpiTxPreview({
      ...basePreview,
      fullFrameHex,
      fullFrameBase64,
      status,
      sentAt: new Date().toLocaleTimeString(),
    });
    setFlowStatusByAction((prev) => ({ ...prev, [actionName]: status }));

    addLog(`[PPI][${actionName}] ${status}`, {
      ppi,
      type,
      payloadHex,
      fullFrameHex: fullFrameHex || null,
      note:
        txOutcome === "COMPLETED"
          ? "ACK received."
          : "If testing with nRF virtual peripheral, send ACK for this DATA packet (same session_id + pkt_counter).",
    });

    if (status === "SENT_WAITING_ACK") {
      void watchForLateTxCompletion(protocol, actionName, ppi, type, payloadHex);
    }

    return status;
  }

  async function onConnect() {
    await withBusy("connect", async () => {
      const targetDevice = deviceId.trim();
      if (!targetDevice) {
        addLog("[CONNECT][ERR] Enter a device id/name first.");
        return;
      }

      const hasPermissions = await ensureAndroidBlePermissions("connect");
      if (!hasPermissions) {
        addLog(
          "[CONNECT][ERR] Bluetooth permission is required. Enable Nearby devices and Location for this app."
        );
        return;
      }

      try {
        try {
          const connectedResponse = await isDeviceConnected();
          if (resolveConnectionState(connectedResponse)) {
            const current = await getConnectedDevice().catch(() => null);
            const currentLabel =
              extractConnectedDeviceLabel(current) ||
              extractConnectedDeviceLabel(connectedResponse) ||
              targetDevice;
            setIsConnected(true);
            setConnectedDeviceLabel(currentLabel);
            addLog("[CONNECT][INFO] Already connected.", {
              connectedDevice: currentLabel,
            });
            await ensureGattDiscovered("connect-existing");
            return;
          }
        } catch {
          // Continue with connect attempt.
        }

        try {
          // Some native stacks require a fresh scan before connect by name/id.
          await scanLeDevice(2);
        } catch (scanError) {
          addLog("[PRE-SCAN][WARN] Scan failed, attempting direct connect.", String(scanError));
        }

        const res = await connect(targetDevice);
        addLog("[CONNECT]", res);
        if (resolveConnectionState(res)) {
          setIsConnected(true);
          setConnectedDeviceLabel(extractConnectedDeviceLabel(res) || targetDevice);
        }
        await ensureGattDiscovered("connect");
        await refreshConnectionBanner(targetDevice);
      } catch (error) {
        const message = String(error);
        if (message.toLowerCase().includes("already connected")) {
          addLog("[CONNECT][INFO] Device already connected.");
          setIsConnected(true);
          setConnectedDeviceLabel(targetDevice);
          await ensureGattDiscovered("connect-already");
          return;
        }
        addLog(`[CONNECT][ERR] ${message}`);
        await refreshConnectionBanner(targetDevice);
      }
    });
  }

  async function onDisconnect() {
    await withBusy("disconnect", async () => {
      try {
        stopPpiProtocol();
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

  async function onPpiTimeRequest() {
    setSelectedFlowAction("TIME_RQ");
    let status = "";
    await withBusy("ppi-time-rq", async () => {
      const payload = new Uint8Array(0);
      status = await sendPpi("TIME_RQ", PpiId.AD_TIME, PpiType.RQ, payload);
    });
    return status;
  }

  async function onPpiTimeResponse() {
    setSelectedFlowAction("TIME_RE");
    let status = "";
    await withBusy("ppi-time-re", async () => {
      const unixTime = Math.floor(Date.now() / 1000);
      const payload = encodeUint32LE(unixTime);
      status = await sendPpi("TIME_RE", PpiId.AD_TIME, PpiType.RE, payload);
    });
    return status;
  }

  async function onPpiTimePush() {
    setSelectedFlowAction("TIME_PUSH");
    let status = "";
    await withBusy("ppi-time-push", async () => {
      const unixTime = Math.floor(Date.now() / 1000);
      const payload = encodeUint32LE(unixTime);
      status = await sendPpi("TIME_PUSH", PpiId.AD_TIME, PpiType.PUSH, payload);
    });
    return status;
  }

  function buildDemoDoseSchedulePayload() {
    return encodeDoseSchedulePpi({
      medication_type: 0,
      dosage_mg: 2,
      temp_upper_limit_deg_c: 60,
      temp_lower_limit_deg_c: 0,
      temp_avg_window_duration_sec: 1800,
      dose_days_bitfield: 0x7f, // Monday-Sunday
      dose_window_duration_minutes: 30,
      dose_window_count: 4,
      dose_window_start_times_minutes: [630, 840, 1050, 1260].slice(
        0,
        MAX_DOSES_PER_DAY
      ),
    });
  }

  async function onPpiDoseScheduleRequest() {
    setSelectedFlowAction("DOSE_SCHEDULE_RQ");
    let status = "";
    await withBusy("ppi-dose-schedule-rq", async () => {
      status = await sendPpi(
        "DOSE_SCHEDULE_RQ",
        PpiId.AD_DOSE_SCHEDULE,
        PpiType.RQ,
        new Uint8Array(0)
      );
    });
    return status;
  }

  async function onPpiDoseScheduleResponse() {
    setSelectedFlowAction("DOSE_SCHEDULE_RE");
    let status = "";
    await withBusy("ppi-dose-schedule-re", async () => {
      const payload = buildDemoDoseSchedulePayload();
      status = await sendPpi("DOSE_SCHEDULE_RE", PpiId.AD_DOSE_SCHEDULE, PpiType.RE, payload);
    });
    return status;
  }

  async function onPpiDoseSchedulePush() {
    setSelectedFlowAction("DOSE_SCHEDULE_PUSH");
    let status = "";
    await withBusy("ppi-dose-schedule-push", async () => {
      const payload = buildDemoDoseSchedulePayload();
      status = await sendPpi("DOSE_SCHEDULE_PUSH", PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, payload);
    });
    return status;
  }

  async function onSubscribe() {
    await withBusy("subscribe", async () => {
      try {
        await ensurePpiProtocol();
        const unsub = await subscribeToCharacteristic(
          characteristicUuid.trim(),
          serviceUuid.trim(),
          ({ uuid, fullUuid, hex, deviceId: eventDeviceId }) => {
            addLog("[NOTIFY]", {
              uuid,
              fullUuid,
              deviceId: eventDeviceId,
              hex,
            });
          }
        );

        subscriptionsRef.current.push(unsub);
        addLog("[SUBSCRIBE]", { serviceUuid, characteristicUuid });
      } catch (error) {
        addLog(`[SUBSCRIBE][ERR] ${String(error)}`);
      }
    });
  }

  async function onClearSubscriptions() {
    await withBusy("clear-subs", async () => {
      subscriptionsRef.current.forEach((fn) => fn());
      subscriptionsRef.current = [];
      stopPpiProtocol();
      addLog("[SUBSCRIBE] Cleared all listeners");
    });
  }

  function onCarouselScrollEnd(event: NativeSyntheticEvent<NativeScrollEvent>) {
    const x = event.nativeEvent.contentOffset.x;
    let nextIndex = 0;
    let minDistance = Number.POSITIVE_INFINITY;

    carouselSnapOffsets.forEach((offset, index) => {
      const distance = Math.abs(x - offset);
      if (distance < minDistance) {
        minDistance = distance;
        nextIndex = index;
      }
    });

    setActiveCarouselCard(nextIndex);
  }

  useEffect(() => {
    let mounted = true;

    async function hydrateInputs() {
      try {
        const raw = await AsyncStorage.getItem(DEBUG_INPUTS_STORAGE_KEY);
        if (!raw) return;
        const parsed = JSON.parse(raw) as Partial<PersistedDebugInputs>;
        if (!mounted) return;

        if (typeof parsed.deviceId === "string") setDeviceId(parsed.deviceId);
        if (typeof parsed.serviceUuid === "string") setServiceUuid(parsed.serviceUuid);
        if (typeof parsed.characteristicUuid === "string") {
          setCharacteristicUuid(parsed.characteristicUuid);
        }
      } catch (error) {
        addLog("[PERSIST][ERR] Failed to load debug inputs", String(error));
      } finally {
        if (mounted) setInputsHydrated(true);
      }
    }

    hydrateInputs();

    return () => {
      mounted = false;
    };
  }, []);

  useEffect(() => {
    if (!inputsHydrated) return;

    const payload: PersistedDebugInputs = {
      deviceId,
      serviceUuid,
      characteristicUuid,
    };

    AsyncStorage.setItem(DEBUG_INPUTS_STORAGE_KEY, JSON.stringify(payload)).catch((error) => {
      addLog("[PERSIST][ERR] Failed to save debug inputs", String(error));
    });
  }, [
    inputsHydrated,
    deviceId,
    serviceUuid,
    characteristicUuid,
  ]);

  useEffect(() => {
    return () => {
      lateAckWatchTokenRef.current += 1;
      subscriptionsRef.current.forEach((fn) => fn());
      subscriptionsRef.current = [];
      if (ppiProtocolRef.current) {
        ppiProtocolRef.current.stop();
        ppiProtocolRef.current = null;
      }
      setProtocolRunning(false);
    };
  }, []);

  useEffect(() => {
    void refreshConnectionBanner();
  }, [refreshConnectionBanner]);

  useEffect(() => {
    setLogCount(getBleDebugLogs().length);
    return subscribeBleDebugLogs(() => {
      setLogCount(getBleDebugLogs().length);
    });
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <KeyboardAvoidingView
        style={styles.keyboardContainer}
        behavior={Platform.OS === "ios" ? "padding" : "height"}
      >
        <TouchableWithoutFeedback onPress={Keyboard.dismiss} accessible={false}>
          <ScrollView
            contentContainerStyle={styles.content}
            keyboardShouldPersistTaps="handled"
          >
            <View style={styles.topInfoWrap}>
              <View style={styles.header}>
                <Pressable style={styles.backButton} onPress={() => router.back()}>
                  <Text style={styles.backButtonText}>{"<"}</Text>
                </Pressable>
                <Text style={styles.title}>BLE Debug</Text>
                <Pressable
                  style={styles.logsButton}
                  onPress={() => router.push("/home/ble-debug-logs")}
                >
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
                  {isConnected
                    ? connectedDeviceLabel || deviceId.trim() || "Unknown Device"
                    : "No active device"}
                </Text>
              </View>
            </View>

            <View style={styles.panel}>
            <View style={styles.carouselContainer}>
              <ScrollView
                horizontal
                showsHorizontalScrollIndicator={false}
                decelerationRate="fast"
                disableIntervalMomentum
                snapToOffsets={carouselSnapOffsets}
                snapToAlignment="start"
                contentContainerStyle={styles.carouselContent}
                onMomentumScrollEnd={onCarouselScrollEnd}
              >
                <View
                  style={[
                    styles.carouselCard,
                    styles.carouselStackCard,
                    { width: carouselCardWidth },
                  ]}
                >
                  <View style={styles.section}>
                    <Text style={styles.sectionTitle}>Device</Text>
                    <Text style={styles.inputLabel}>Device ID / Name</Text>
                    <TextInput
                      value={deviceId}
                      onChangeText={setDeviceId}
                      style={styles.input}
                      placeholder="VAL-OP ..."
                      placeholderTextColor={validoseGrey}
                    />
                    <View style={styles.deviceActionRow}>
                      <ActionButton
                        label="Connect"
                        onPress={onConnect}
                        disabled={isConnected || !deviceId.trim() || isBlockedByOtherAction("connect")}
                        loading={loadingAction === "connect"}
                        style={styles.deviceActionButton}
                      />
                      <ActionButton
                        label="Disconnect"
                        onPress={onDisconnect}
                        disabled={!isConnected || isBlockedByOtherAction("disconnect")}
                        loading={loadingAction === "disconnect"}
                        tone="danger"
                        style={styles.deviceActionButton}
                      />
                    </View>
                  </View>

                  <View style={styles.section}>
                    <Text style={styles.sectionTitle}>Message Protocol</Text>
                    <Text style={styles.inputLabel}>Service UUID</Text>
                    <TextInput
                      value={serviceUuid}
                      onChangeText={setServiceUuid}
                      style={styles.input}
                      placeholderTextColor={validoseGrey}
                    />
                    <Text style={styles.inputLabel}>Characteristic UUID</Text>
                    <TextInput
                      value={characteristicUuid}
                      onChangeText={setCharacteristicUuid}
                      style={styles.input}
                      placeholderTextColor={validoseGrey}
                    />
                    <View style={styles.deviceActionRow}>
                      <ActionButton
                        label="Subscribe"
                        onPress={onSubscribe}
                        disabled={!canRun || !isConnected || isBlockedByOtherAction("subscribe")}
                        loading={loadingAction === "subscribe"}
                        style={styles.deviceActionButton}
                      />
                      <ActionButton
                        label="Clear Subs"
                        onPress={onClearSubscriptions}
                        disabled={!isConnected || isBlockedByOtherAction("clear-subs")}
                        loading={loadingAction === "clear-subs"}
                        style={styles.deviceActionButton}
                      />
                    </View>
                    <View style={styles.ppiModeRow}>
                      <Text style={styles.ppiModeLabel}>Manual nRF Mode</Text>
                      <Pressable
                        style={[
                          styles.ppiModeToggle,
                          manualNrfMode ? styles.ppiModeToggleActive : styles.ppiModeToggleInactive,
                          !isConnected && styles.ppiModeToggleDisabled,
                        ]}
                        disabled={!isConnected || isActionInProgress}
                        onPress={onToggleManualNrfMode}
                      >
                        <Text
                          style={[
                            styles.ppiModeToggleText,
                            manualNrfMode && styles.ppiModeToggleTextActive,
                            !isConnected && styles.ppiModeToggleTextDisabled,
                          ]}
                        >
                          {manualNrfMode ? "ON (Skip Sync)" : "OFF (Full Sync)"}
                        </Text>
                      </Pressable>
                    </View>
                    <Text style={styles.sectionHint}>
                      {manualNrfMode
                        ? "Manual nRF mode sends DATA directly; ACK is still required."
                        : "Full sync mode sends SYNC_START then DATA and waits for ACKs."}
                    </Text>
                    <Text style={styles.sectionHint}>{statusText}</Text>
                  </View>
                </View>

                <View style={[styles.section, styles.carouselCard, { width: carouselCardWidth }]}>
                  <Text style={styles.sectionTitle}>Quick Actions</Text>
                  <View style={styles.quickPpiGrid}>
                    <QuickPpiButton
                      title="Time Request"
                      subtitle="AD_TIME (RQ, no payload)"
                      onPress={onPpiTimeRequest}
                      onInfoPress={() => openFlowHelp("TIME_RQ")}
                      disabled={!canRun || !isConnected || isBlockedByOtherAction("ppi-time-rq")}
                      loading={loadingAction === "ppi-time-rq"}
                    />
                    <QuickPpiButton
                      title="Time Response"
                      subtitle="AD_TIME (RE, uint32 unix)"
                      onPress={onPpiTimeResponse}
                      onInfoPress={() => openFlowHelp("TIME_RE")}
                      disabled={!canRun || !isConnected || isBlockedByOtherAction("ppi-time-re")}
                      loading={loadingAction === "ppi-time-re"}
                    />
                    <QuickPpiButton
                      title="Time Push"
                      subtitle="AD_TIME (PUSH, uint32 unix)"
                      onPress={onPpiTimePush}
                      onInfoPress={() => openFlowHelp("TIME_PUSH")}
                      disabled={!canRun || !isConnected || isBlockedByOtherAction("ppi-time-push")}
                      loading={loadingAction === "ppi-time-push"}
                    />
                    <QuickPpiButton
                      title="Dose Schedule Request"
                      subtitle="AD_DOSE_SCHEDULE (RQ, no payload)"
                      onPress={onPpiDoseScheduleRequest}
                      onInfoPress={() => openFlowHelp("DOSE_SCHEDULE_RQ")}
                      disabled={!canRun || !isConnected || isBlockedByOtherAction("ppi-dose-schedule-rq")}
                      loading={loadingAction === "ppi-dose-schedule-rq"}
                    />
                    <QuickPpiButton
                      title="Dose Schedule Response"
                      subtitle="AD_DOSE_SCHEDULE (RE, dose_schedule_t)"
                      onPress={onPpiDoseScheduleResponse}
                      onInfoPress={() => openFlowHelp("DOSE_SCHEDULE_RE")}
                      disabled={!canRun || !isConnected || isBlockedByOtherAction("ppi-dose-schedule-re")}
                      loading={loadingAction === "ppi-dose-schedule-re"}
                    />
                    <QuickPpiButton
                      title="Dose Schedule Push"
                      subtitle="AD_DOSE_SCHEDULE (PUSH, dose_schedule_t)"
                      onPress={onPpiDoseSchedulePush}
                      onInfoPress={() => openFlowHelp("DOSE_SCHEDULE_PUSH")}
                      disabled={!canRun || !isConnected || isBlockedByOtherAction("ppi-dose-schedule-push")}
                      loading={loadingAction === "ppi-dose-schedule-push"}
                    />
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
                          PPI: {lastPpiTxPreview.ppiName} ({lastPpiTxPreview.ppi}) · Type:{" "}
                          {lastPpiTxPreview.typeName} ({lastPpiTxPreview.type}) · Len:{" "}
                          {lastPpiTxPreview.pktPayloadLen}
                        </Text>
                        <Text style={styles.ppiPreviewSectionLabel}>Structure</Text>
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
                        <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiTxPreview.payloadHex || "(empty)"}
                        </Text>
                        <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiTxPreview.payloadBase64 || "(empty)"}
                        </Text>
                        <Text style={styles.ppiPreviewSectionLabel}>Parsed Payload</Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiTxHumanReadable || "(not available)"}
                        </Text>
                        <Text style={styles.ppiPreviewSectionLabel}>
                          Full Frame Hex (CRC + header + PPI + payload)
                        </Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiTxPreview.fullFrameHex || "(not captured yet)"}
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
                            PPI: {lastPpiRxPreview.ppiName} ({lastPpiRxPreview.ppi}) · Type:{" "}
                            {lastPpiRxPreview.typeName} ({lastPpiRxPreview.type}) · Len:{" "}
                            {lastPpiRxPreview.pktPayloadLen ?? 0}
                          </Text>
                        ) : null}
                        <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiRxPreview.payloadHex || "(empty)"}
                        </Text>
                        <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiRxPreview.payloadBase64 || "(empty)"}
                        </Text>
                        <Text style={styles.ppiPreviewSectionLabel}>Payload UTF-8</Text>
                        <Text style={styles.ppiPreviewCode} selectable>
                          {lastPpiRxPreview.payloadUtf8 || "(empty/non-utf8)"}
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
                </View>

              </ScrollView>

              <View style={styles.carouselDots}>
                {[0, 1].map((index) => (
                  <View
                    key={index}
                    style={[
                      styles.carouselDot,
                      activeCarouselCard === index && styles.carouselDotActive,
                    ]}
                  />
                ))}
              </View>
            </View>

            <View style={styles.protocolInfoCard}>
              <Text style={styles.protocolInfoTitle}>Protocol Info</Text>
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
                  <Text style={styles.protocolInfoLabel}>Debug events</Text>
                  <Text style={styles.protocolInfoValue}>{protocolInfo.logCount}</Text>
                </View>
                <View style={styles.protocolInfoRow}>
                  <Text style={styles.protocolInfoLabel}>Mode</Text>
                  <Text style={styles.protocolInfoValue}>{protocolInfo.mode}</Text>
                </View>
                <View style={styles.protocolInfoRow}>
                  <Text style={styles.protocolInfoLabel}>isMaster / relaxedAck</Text>
                  <Text style={styles.protocolInfoValue}>
                    {protocolInfo.isMaster} / {protocolInfo.relaxedAckMatching}
                  </Text>
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
                  <Text style={styles.protocolInfoLabel}>Sync retry interval</Text>
                  <Text style={styles.protocolInfoValue}>{protocolInfo.syncRetryIntervalMs} ms</Text>
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
                  <Text style={styles.protocolInfoValue}>{serviceUuid || "(empty)"}</Text>
                </View>
                <View style={styles.protocolInfoRow}>
                  <Text style={styles.protocolInfoLabel}>Characteristic UUID</Text>
                  <Text style={styles.protocolInfoValue}>{characteristicUuid || "(empty)"}</Text>
                </View>
              </View>
            </View>

            </View>
          </ScrollView>
        </TouchableWithoutFeedback>
      </KeyboardAvoidingView>
      <Modal
        visible={flowHelpVisible}
        animationType="slide"
        transparent
        onRequestClose={() => setFlowHelpVisible(false)}
      >
        <View style={styles.flowModalBackdrop}>
          <Pressable
            style={styles.flowModalDismissArea}
            onPress={() => setFlowHelpVisible(false)}
          />
          <View style={styles.flowModalCard}>
            <View style={styles.flowModalHandle} />
            <View style={styles.flowModalHeader}>
              <View style={styles.flowModalHeaderTextWrap}>
                <Text style={styles.flowModalTitle}>Protocol Steps</Text>
                <Text style={styles.flowModalSubtitle}>
                  {selectedFlowMeta.title} · {selectedFlowModeText}
                </Text>
              </View>
              <Pressable
                style={styles.flowModalClose}
                onPress={() => setFlowHelpVisible(false)}
              >
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
                } else if (selectedFlowStatus === "SENT_ACKED") {
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
