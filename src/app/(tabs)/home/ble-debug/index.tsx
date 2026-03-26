import { Buffer } from "buffer";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { useFocusEffect } from "@react-navigation/native";
import { useRouter } from "expo-router";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  type NativeScrollEvent,
  type NativeSyntheticEvent,
  type Permission,
  PermissionsAndroid,
  Platform,
  ScrollView,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "@/constants/ble";
import { getAccessToken, invokeSignIn } from "@/providers/auth";
import { login as loginWithMobileId } from "@/services/auth";
import { ingestRawHardwareData } from "@/services/hardware";
import { connectAndSetupDevice } from "@/utils/ble";
import {
  getMessageProtocolInstance,
  stopAndClearMessageProtocol,
  subscribeMessageProtocolRxPackets,
} from "@/utils/ble/connectionHandling/state";
import {
  addBleDebugLog,
  getBleDebugLogs,
  subscribeBleDebugLogs,
} from "@/utils/ble/debugLogStore";
import {
  BleMessageProtocol,
  MpPacketPayload,
  MessageProtocolInterface,
  MsgProtError,
  MsgProtTxPacketStatus,
} from "@/utils/ble/messageProtocol";
import {
  PpiId,
  PpiType,
  buildPpiPayload,
  decodePpiPayload,
  encodeBool,
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
} from "../../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { DebugHeaderCard } from "./components/DebugHeaderCard";
import { FlowHelpModal } from "./components/FlowHelpModal";
import { PayloadDetailsCard } from "./components/PayloadDetailsCard";
import { ProtocolInfoCard } from "./components/ProtocolInfoCard";
import { QuickActionsCarouselCard } from "./components/QuickActionsCarouselCard";
import {
  PPI_ACK_TIMEOUT_MS,
  DOSE_SCHEDULE_PUSH_PAYLOADS,
  PPI_LATE_ACK_WATCH_POLL_MS,
  PPI_LATE_ACK_WATCH_TIMEOUT_MS,
  PPI_MAX_RETRIES,
  PPI_TX_COMPLETION_WAIT_MS,
  PPI_TX_READY_POLL_MS,
  PPI_TX_READY_TIMEOUT_MS,
  QUICK_FLOW_META,
} from "./constants";
import {
  extractConnectedDeviceLabel,
  normalizeDecodedValue,
  prettifyForLog,
  resolveConnectionState,
} from "./helpers";
import {
  BASELINING_QUICK_ACTION_KEY_SET,
  buildQuickActionPages,
  CALIBRATION_QUICK_ACTION_KEY_SET,
  getQuickActionSubtitle,
} from "./quickActions";
import {
  decodeHexPayload,
  describeBaseliningState,
  encodeStartCalibrationParamPayload,
  extractBaseliningFeedbackSnapshot,
  extractCalibrationDataSnapshot,
  extractCalibrationFeedbackSnapshot,
  formatPpiHumanReadable,
  getCalibrationInstructionForState,
  isQuickFlowAction,
  isResponseTypeForRequest,
  parseMpFrameBytes,
  parseMpFrameHex,
  resolveMessageProtocolUuidsFromDiscovery,
  summarizeCalibrationResponseValue,
  summarizeFeedbackValue,
} from "./screenUtils";
import { styles } from "./styles";
import type {
  BaseliningFeedbackSnapshot,
  BaseliningGuideStage,
  CalibrationDataSnapshot,
  CalibrationFeedbackSnapshot,
  CalibrationGuideStage,
  CompactFeedbackPreview,
  IncomingDetailsTab,
  MpFramePreview,
  PendingResponseMatcher,
  PpiRxPreview,
  PpiTxPreview,
  PushAckPreview,
  QuickFlowAction,
} from "./types";

const MP_SERVICE_UUID = SERVICE_UUIDS.MESSAGE_PROTOCOL_SERVICE;
const MP_TX_UUID = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_TX;
const MP_RX_UUID = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_RX;
const MP_SERVICE_SHORT_UUID = "1500";
const MP_TX_SHORT_UUID = "1508";
const MP_RX_SHORT_UUID = "1509";
// Firmware parity: MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS = 0 (process on demand).
const MP_PROCESS_INTERVAL_MS = 0;
const MP_MAX_PACKET_LEN = 244;
const BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION = 4;
const BASELINING_STATE_COMPLETE = 6;
const BASELINING_STATE_ERROR = 7;
const BASELINING_INITIAL_INSTRUCTION = "Press Start Baselining to begin.";
const CALIBRATION_STATE_COMPLETE = 5;
const CALIBRATION_STATE_ERROR = 6;
const CALIBRATION_INITIAL_INSTRUCTION = "Press Start Calibration to begin.";
const MP_MIN_FRAME_LEN_BYTES = 14;
const MP_PACKET_TYPE_DATA = 0;
const CALIBRATION_WEIGHT_STORAGE_KEY = "ble_debug_calibration_weight_mg";
// const FILTER_ONLY_DOSE_EVENT_PPI_LOGS = true;
const FILTER_ONLY_DOSE_EVENT_PPI_LOGS = false; // Uncomment and disable the line above to allow all PPI types.
const DOSE_EVENT_LOG_PPI = PpiId.AD_DOSE_EVENT_REPORT;
const CALIBRATION_WEIGHT_MG_FALLBACK = 5000;
const CAP_DETECTION_SAMPLE_PERIOD_MS_DEFAULT = 1000;
const CAP_DETECTION_THRESHOLD_DEFAULT = 1000;
const CAP_DETECTION_HYSTERESIS_DEFAULT = 200;
const UINT16_MAX = 0xffff;
const UINT32_MAX = 0xffffffff;

function coerceCalibrationWeightMg(rawInput: string): number | null {
  const value = rawInput.trim();
  if (!value || !/^\d+$/.test(value)) {
    return null;
  }

  const parsed = Number.parseInt(value, 10);
  if (!Number.isSafeInteger(parsed) || parsed <= 0 || parsed > UINT32_MAX) {
    return null;
  }

  return parsed;
}

function coerceUint16FromDecimalInput(rawInput: string): number | null {
  const value = rawInput.trim();
  if (!value || !/^\d+$/.test(value)) {
    return null;
  }

  const parsed = Number.parseInt(value, 10);
  if (!Number.isSafeInteger(parsed) || parsed < 0 || parsed > UINT16_MAX) {
    return null;
  }

  return parsed;
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
  const [quickActionsPageIndex, setQuickActionsPageIndex] = useState(0);
  const [quickActionsViewportWidth, setQuickActionsViewportWidth] = useState(0);
  const [quickActionPageHeights, setQuickActionPageHeights] = useState<Record<string, number>>({});
  const [developmentCmdInput, setDevelopmentCmdInput] = useState("0");
  const [developmentCmdInputError, setDevelopmentCmdInputError] = useState("");
  const [calibrationWeightInput, setCalibrationWeightInput] = useState(
    String(CALIBRATION_WEIGHT_MG_FALLBACK)
  );
  const [calibrationWeightInputError, setCalibrationWeightInputError] = useState("");
  const [capDetectionThresholdInput, setCapDetectionThresholdInput] = useState(
    String(CAP_DETECTION_THRESHOLD_DEFAULT)
  );
  const [capDetectionHysteresisInput, setCapDetectionHysteresisInput] = useState(
    String(CAP_DETECTION_HYSTERESIS_DEFAULT)
  );
  const [capDetectionConfigInputError, setCapDetectionConfigInputError] = useState("");
  const [calibrationFeedbackPreview, setCalibrationFeedbackPreview] =
    useState<CompactFeedbackPreview | null>(null);
  const [calibrationResponsePreview, setCalibrationResponsePreview] =
    useState<CompactFeedbackPreview | null>(null);
  const [baseliningFeedbackPreview, setBaseliningFeedbackPreview] =
    useState<CompactFeedbackPreview | null>(null);
  const [baseliningResponsePreview, setBaseliningResponsePreview] =
    useState<CompactFeedbackPreview | null>(null);
  const [baseliningFeedbackSnapshot, setBaseliningFeedbackSnapshot] =
    useState<BaseliningFeedbackSnapshot | null>(null);
  const [baseliningGuideStage, setBaseliningGuideStage] = useState<BaseliningGuideStage>("IDLE");
  const [baseliningGuideInstruction, setBaseliningGuideInstruction] =
    useState(BASELINING_INITIAL_INSTRUCTION);
  const [calibrationGuideStage, setCalibrationGuideStage] = useState<CalibrationGuideStage>("IDLE");
  const [calibrationGuideInstruction, setCalibrationGuideInstruction] = useState(
    CALIBRATION_INITIAL_INSTRUCTION
  );
  const [calibrationCompletionMessage, setCalibrationCompletionMessage] = useState("");
  const [calibrationDataSnapshot, setCalibrationDataSnapshot] = useState<CalibrationDataSnapshot | null>(
    null
  );
  const [calibrationFeedbackSnapshot, setCalibrationFeedbackSnapshot] =
    useState<CalibrationFeedbackSnapshot | null>(null);

  const ppiProtocolRef = useRef<BleMessageProtocol | null>(null);
  const ownsProtocolRef = useRef(false);
  const sharedRxPacketUnsubscribeRef = useRef<(() => void) | null>(null);
  const ensurePpiProtocolRef = useRef<() => Promise<BleMessageProtocol>>(async () => {
    throw new Error("Message protocol not initialized.");
  });
  const connectedDeviceIdRef = useRef("");
  const pendingResponseMatcherRef = useRef<PendingResponseMatcher | null>(null);
  const resolvedMpTxUuidRef = useRef(MP_TX_UUID);
  const resolvedMpRxUuidRef = useRef(MP_RX_UUID);
  const resolvedMpServiceUuidRef = useRef(MP_SERVICE_UUID);
  const lateAckWatchTokenRef = useRef(0);
  const gattDiscoveryInFlightRef = useRef<Promise<void> | null>(null);
  const isGattDiscoveredRef = useRef(false);
  const calibrationCompletionAutoRequestRef = useRef(false);
  const calibrationStopRequestedRef = useRef(false);
  const baseliningValidationSentRef = useRef(false);
  const debugAuthBootstrapRef = useRef<Promise<boolean> | null>(null);

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
  const quickActionPages = useMemo(() => buildQuickActionPages(), []);
  const quickActionsViewportHeight = useMemo(() => {
    const activePageId = quickActionPages[quickActionsPageIndex]?.id;
    if (activePageId && quickActionPageHeights[activePageId]) {
      return quickActionPageHeights[activePageId];
    }

    for (const page of quickActionPages) {
      const measuredHeight = quickActionPageHeights[page.id];
      if (typeof measuredHeight === "number" && measuredHeight > 0) {
        return measuredHeight;
      }
    }

    return undefined;
  }, [quickActionPageHeights, quickActionPages, quickActionsPageIndex]);
  const calibrationGuideStageLabel = useMemo(() => {
    switch (calibrationGuideStage) {
      case "START_SENT":
        return "Waiting for Start RE";
      case "AWAITING_WEIGHT":
        return "Awaiting Weight";
      case "WEIGHT_PRESENT_SENT":
        return "Weight Sent";
      case "COMPLETED":
        return "Completed";
      case "ERROR":
        return "Error";
      default:
        return "Ready";
    }
  }, [calibrationGuideStage]);
  const isCalibrationWeightInputDisabled = useMemo(
    () =>
      calibrationGuideStage === "START_SENT" ||
      calibrationGuideStage === "AWAITING_WEIGHT" ||
      calibrationGuideStage === "WEIGHT_PRESENT_SENT",
    [calibrationGuideStage]
  );
  const isCalibrationActionEnabled = useCallback(
    (action: QuickFlowAction) => {
      if (!CALIBRATION_QUICK_ACTION_KEY_SET.has(action)) {
        return true;
      }

      switch (calibrationGuideStage) {
        case "IDLE":
          return action === "START_CALIBRATION_RQ" || action === "CALIBRATION_DATA_RQ";
        case "START_SENT":
          return action === "CALIBRATION_DATA_RQ";
        case "AWAITING_WEIGHT":
          return (
            action === "CALIBRATION_DATA_RQ" ||
            action === "STOP_CALIBRATION_RQ" ||
            action === "CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE"
          );
        case "WEIGHT_PRESENT_SENT":
          return (
            action === "CALIBRATION_DATA_RQ" ||
            action === "STOP_CALIBRATION_RQ" ||
            action === "CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE"
          );
        case "COMPLETED":
          return action === "START_CALIBRATION_RQ" || action === "CALIBRATION_DATA_RQ";
        case "ERROR":
          return action === "START_CALIBRATION_RQ" || action === "CALIBRATION_DATA_RQ";
        default:
          return false;
      }
    },
    [calibrationGuideStage]
  );
  const isBaseliningActionEnabled = useCallback(
    (action: QuickFlowAction) => {
      if (!BASELINING_QUICK_ACTION_KEY_SET.has(action)) {
        return true;
      }

      switch (baseliningGuideStage) {
        case "IDLE":
        case "COMPLETED":
        case "ERROR":
          return action === "START_BASELINING_RQ";
        case "START_SENT":
        case "RUNNING":
        case "VALIDATION_SENT":
          return action === "STOP_BASELINING_RQ";
        case "AWAITING_VALIDATION":
          return (
            action === "STOP_BASELINING_RQ" ||
            action === "VALIDATE_MED_RE_TRUE" ||
            action === "VALIDATE_MED_RE_FALSE"
          );
        default:
          return false;
      }
    },
    [baseliningGuideStage]
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
    if (selectedFlowAction === "START_CALIBRATION_RQ" || selectedFlowAction === "STOP_CALIBRATION_RQ") {
      const calibrationWeightMg = coerceCalibrationWeightMg(calibrationWeightInput);
      return {
        start: selectedFlowAction === "START_CALIBRATION_RQ",
        calibration_weight_mg: calibrationWeightMg ?? "(invalid input)",
      };
    }

    if (selectedFlowAction === "SET_CAP_DETECTION_CONFIG_PUSH") {
      const threshold = coerceUint16FromDecimalInput(capDetectionThresholdInput);
      const hysteresis = coerceUint16FromDecimalInput(capDetectionHysteresisInput);
      return {
        threshhold: threshold ?? "(invalid input)",
        hysteresis: hysteresis ?? "(invalid input)",
      };
    }

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
  }, [
    calibrationWeightInput,
    capDetectionHysteresisInput,
    capDetectionThresholdInput,
    selectedFlowAction,
    selectedFlowMeta,
  ]);

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

  function asRecord(value: unknown): Record<string, unknown> | null {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      return null;
    }

    return value as Record<string, unknown>;
  }

  function extractPpiFromLogPayload(payload: unknown): number | null {
    const record = asRecord(payload);
    if (!record) {
      return null;
    }

    if (typeof record.ppi === "number") {
      return record.ppi;
    }

    const nestedPayload = asRecord(record.payload);
    if (nestedPayload && typeof nestedPayload.ppi === "number") {
      return nestedPayload.ppi;
    }

    const receivedPacket = asRecord(record.receivedPacket);
    const receivedPacketPayload = asRecord(receivedPacket?.payload);
    if (receivedPacketPayload && typeof receivedPacketPayload.ppi === "number") {
      return receivedPacketPayload.ppi;
    }

    return null;
  }

  function shouldKeepPpiLogEntry(payload: unknown): boolean {
    const ppi = extractPpiFromLogPayload(payload);
    if (ppi === null) {
      return true;
    }

    if (!FILTER_ONLY_DOSE_EVENT_PPI_LOGS) {
      return true;
    }

    return ppi === DOSE_EVENT_LOG_PPI;
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

  async function ensureDebugAuthSession(): Promise<boolean> {
    const currentToken = await getAccessToken().catch((error) => {
      addLog("[PPI][INGEST][AUTH][WARN] Failed to read access token from secure storage.", {
        error: toErrorDetails(error),
      });
      return null;
    });

    if (currentToken) {
      return true;
    }

    if (debugAuthBootstrapRef.current) {
      return debugAuthBootstrapRef.current;
    }

    const bootstrapPromise = (async () => {
      try {
        addLog("[PPI][INGEST][AUTH] Access token missing. Calling /auth/login before ingest.");
        const newSession = await loginWithMobileId();
        if (!newSession?.access_token) {
          addLog("[PPI][INGEST][AUTH][ERR] /auth/login returned no access token.");
          return false;
        }

        await invokeSignIn(newSession);
        addLog("[PPI][INGEST][AUTH] Auth session restored for debug ingest.");
        return true;
      } catch (error) {
        addLog("[PPI][INGEST][AUTH][ERR] Failed to authenticate for debug ingest.", {
          error: toErrorDetails(error),
        });
        return false;
      } finally {
        debugAuthBootstrapRef.current = null;
      }
    })();

    debugAuthBootstrapRef.current = bootstrapPromise;
    return bootstrapPromise;
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

    const payloadBytes = decodeHexPayload(payloadHex);
    if (!payloadBytes) {
      return details;
    }

    try {
      const decoded = decodePpiPayload(
        ppi,
        type as PpiType,
        payloadBytes
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
        if (!shouldKeepPpiLogEntry(rest[0])) {
          return;
        }

        const payload =
          first === "Parsed incoming value."
            ? enrichParsedIncomingPayload(rest[0])
            : rest[0];
        addLog(`[PPI][MP][${level}] ${first}`, payload);
        return;
      }

      if (!shouldKeepPpiLogEntry(rest[0])) {
        return;
      }

      addLog(`[PPI][MP][${level}] ${first}`, rest);
      return;
    }

    if (!shouldKeepPpiLogEntry(args[0])) {
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
    detachSharedRxPacketSubscription();

    if (ppiProtocolRef.current) {
      if (ownsProtocolRef.current) {
        ppiProtocolRef.current.stop();
        addLog("[PPI] Message protocol stopped.");
      } else {
        addLog("[PPI] Detached from shared app message protocol.");
      }
      ppiProtocolRef.current = null;
      ownsProtocolRef.current = false;
    }
    setProtocolRunning(false);

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
          const resolved = resolveMessageProtocolUuidsFromDiscovery(response, {
            txUuid: MP_TX_UUID,
            rxUuid: MP_RX_UUID,
            serviceShortUuid: MP_SERVICE_SHORT_UUID,
            txShortUuid: MP_TX_SHORT_UUID,
            rxShortUuid: MP_RX_SHORT_UUID,
          });
          resolvedMpTxUuidRef.current = resolved.txUuid;
          resolvedMpRxUuidRef.current = resolved.rxUuid;
          resolvedMpServiceUuidRef.current = resolved.serviceUuid;
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

  function detachSharedRxPacketSubscription() {
    if (sharedRxPacketUnsubscribeRef.current) {
      sharedRxPacketUnsubscribeRef.current();
      sharedRxPacketUnsubscribeRef.current = null;
    }
  }

  function attachSharedRxPacketSubscription() {
    if (sharedRxPacketUnsubscribeRef.current) {
      return;
    }

    sharedRxPacketUnsubscribeRef.current = subscribeMessageProtocolRxPackets((packet) => {
      handleIncomingPpiPacket(packet);
    });
  }

  function handleIncomingPpiPacket(packet: MpPacketPayload) {
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

    const packetType = packet.type as PpiType;
    const pendingActionName = pendingResponseMatcherRef.current?.actionName ?? "";
    const compactBase = {
      updatedAt: receivedAt,
      ppiName: rxPreview.ppiName ?? `PPI_${packet.ppi}`,
      typeName: rxPreview.typeName ?? `TYPE_${packet.type}`,
    };

    if (
      (packetType === PpiType.PUSH || packetType === PpiType.RE) &&
      (packet.ppi === PpiId.AD_CALIBRATION_FEEDBACK ||
        (packet.ppi === PpiId.AD_START_CALIBRATION && packetType === PpiType.PUSH))
    ) {
      setCalibrationFeedbackPreview({
        ...compactBase,
        summary: summarizeFeedbackValue(normalizedDecodedValue),
      });

      const feedbackSnapshot = extractCalibrationFeedbackSnapshot(normalizedDecodedValue, compactBase.updatedAt);
      if (feedbackSnapshot) {
        setCalibrationFeedbackSnapshot(feedbackSnapshot);
        const stateCode = feedbackSnapshot.currentState;
        const stateInstruction = getCalibrationInstructionForState(stateCode);

        if (stateCode === 0 || stateCode === 1 || stateCode === 2) {
          setCalibrationGuideStage("AWAITING_WEIGHT");
        } else if (stateCode === 3 || stateCode === 4) {
          setCalibrationGuideStage("WEIGHT_PRESENT_SENT");
        } else if (stateCode === CALIBRATION_STATE_COMPLETE) {
          setCalibrationGuideStage("COMPLETED");
        } else if (stateCode === CALIBRATION_STATE_ERROR) {
          setCalibrationGuideStage("ERROR");
        }

        if (stateInstruction) {
          setCalibrationGuideInstruction(stateInstruction);
        }

        if (feedbackSnapshot.isComplete) {
          setCalibrationGuideStage("COMPLETED");
          setCalibrationCompletionMessage(
            `Calibration complete feedback received at ${compactBase.updatedAt}.`
          );
          if (!calibrationCompletionAutoRequestRef.current) {
            calibrationCompletionAutoRequestRef.current = true;
            setCalibrationGuideInstruction("Calibration complete. Requesting calibration data...");
            void requestCalibrationData();
          }
        }
      }
    }

    if (
      (packetType === PpiType.PUSH || packetType === PpiType.RE) &&
      (packet.ppi === PpiId.AD_BASELINING_FEEDBACK ||
        (packet.ppi === PpiId.AD_START_BASELINING && packetType === PpiType.PUSH))
    ) {
      const baseliningSnapshot = extractBaseliningFeedbackSnapshot(normalizedDecodedValue, compactBase.updatedAt);
      if (baseliningSnapshot) {
        setBaseliningFeedbackSnapshot(baseliningSnapshot);
        const baseliningSummaryParts = [
          describeBaseliningState(baseliningSnapshot.currentState),
          typeof baseliningSnapshot.avgWeightMg === "number"
            ? `avg ${baseliningSnapshot.avgWeightMg}mg`
            : null,
          baseliningSnapshot.isRingPresent === null ? null : `ring ${baseliningSnapshot.isRingPresent ? "on" : "off"}`,
        ].filter((part): part is string => Boolean(part));

        setBaseliningFeedbackPreview({
          ...compactBase,
          summary: baseliningSummaryParts.join(" · "),
        });

        if (baseliningSnapshot.currentState === BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION) {
          if (baseliningValidationSentRef.current) {
            setBaseliningGuideStage("VALIDATION_SENT");
            setBaseliningGuideInstruction("Validation response sent. Waiting for next baselining state...");
          } else {
            setBaseliningGuideStage("AWAITING_VALIDATION");
            setBaseliningGuideInstruction("Backend validation required. Send Validate Med OK or Validate Med Fail.");
          }
        } else {
          baseliningValidationSentRef.current = false;
          if (baseliningSnapshot.currentState === BASELINING_STATE_COMPLETE) {
            setBaseliningGuideStage("COMPLETED");
            setBaseliningGuideInstruction("Baselining complete. Start Baselining is enabled.");
          } else if (baseliningSnapshot.currentState === BASELINING_STATE_ERROR) {
            setBaseliningGuideStage("ERROR");
            setBaseliningGuideInstruction("Baselining entered ERROR state. Restart using Start Baselining.");
          } else {
            setBaseliningGuideStage("RUNNING");
            setBaseliningGuideInstruction(
              `Baselining in progress: ${describeBaseliningState(baseliningSnapshot.currentState)}.`
            );
          }
        }
      } else {
        setBaseliningFeedbackPreview({
          ...compactBase,
          summary: summarizeFeedbackValue(normalizedDecodedValue),
        });
      }
    }

    if (
      (packetType === PpiType.RE || packetType === PpiType.PUSH) &&
      (packet.ppi === PpiId.AD_START_CALIBRATION || packet.ppi === PpiId.AD_CALIBRATION_DATA)
    ) {
      setCalibrationResponsePreview({
        ...compactBase,
        summary: summarizeCalibrationResponseValue(packet.ppi, normalizedDecodedValue),
      });
    }

    if ((packetType === PpiType.RE || packetType === PpiType.PUSH) && packet.ppi === PpiId.AD_CALIBRATION_DATA) {
      const dataSnapshot = extractCalibrationDataSnapshot(normalizedDecodedValue, compactBase.updatedAt);
      if (dataSnapshot) {
        setCalibrationDataSnapshot(dataSnapshot);
      }
    }

    if (packetType === PpiType.RE && packet.ppi === PpiId.AD_START_CALIBRATION) {
      const accepted =
        typeof normalizedDecodedValue === "boolean"
          ? normalizedDecodedValue
          : typeof normalizedDecodedValue === "number"
            ? normalizedDecodedValue !== 0
            : null;
      if (pendingActionName === "START_CALIBRATION_RQ") {
        if (accepted === true) {
          setCalibrationGuideStage("AWAITING_WEIGHT");
          setCalibrationGuideInstruction(
            getCalibrationInstructionForState(0) ?? "Remove ring from dock. Place dock on a stable, level surface."
          );
        } else if (accepted === false) {
          setCalibrationGuideStage("IDLE");
          setCalibrationGuideInstruction("Firmware rejected start calibration.");
        }
      } else if (pendingActionName === "STOP_CALIBRATION_RQ" || calibrationStopRequestedRef.current) {
        if (accepted === true) {
          calibrationStopRequestedRef.current = false;
          setCalibrationGuideStage("IDLE");
          setCalibrationGuideInstruction("Calibration stopped.");
          pendingResponseMatcherRef.current = null;
          setPendingResponseMatcher(null);
        } else if (accepted === false) {
          calibrationStopRequestedRef.current = false;
          setCalibrationGuideInstruction("Firmware rejected stop calibration.");
          pendingResponseMatcherRef.current = null;
          setPendingResponseMatcher(null);
        }
      }
    }

    if (packetType === PpiType.RE && packet.ppi === PpiId.AD_START_BASELINING) {
      setBaseliningResponsePreview({
        ...compactBase,
        summary: summarizeCalibrationResponseValue(packet.ppi, normalizedDecodedValue),
      });

      const accepted =
        typeof normalizedDecodedValue === "boolean"
          ? normalizedDecodedValue
          : typeof normalizedDecodedValue === "number"
            ? normalizedDecodedValue !== 0
            : null;
      if (pendingActionName === "START_BASELINING_RQ") {
        if (accepted === true) {
          baseliningValidationSentRef.current = false;
          setBaseliningGuideStage("RUNNING");
          setBaseliningGuideInstruction("Baselining started. Follow live state updates below.");
        } else if (accepted === false) {
          baseliningValidationSentRef.current = false;
          setBaseliningGuideStage("IDLE");
          setBaseliningGuideInstruction("Firmware rejected start baselining.");
        }
      } else if (pendingActionName === "STOP_BASELINING_RQ") {
        if (accepted === true) {
          baseliningValidationSentRef.current = false;
          setBaseliningGuideStage("IDLE");
          setBaseliningGuideInstruction("Baselining stopped.");
        } else if (accepted === false) {
          setBaseliningGuideInstruction("Firmware rejected stop baselining.");
        }
      }
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
  }

  async function ensurePpiProtocol() {
    if (ppiProtocolRef.current) {
      setProtocolRunning(true);
      return ppiProtocolRef.current;
    }

    const sharedProtocol = getMessageProtocolInstance();
    if (sharedProtocol) {
      ppiProtocolRef.current = sharedProtocol;
      ownsProtocolRef.current = false;
      attachSharedRxPacketSubscription();
      setProtocolRunning(true);
      addLog("[PPI] Using shared app message protocol.");
      return sharedProtocol;
    }

    const reconnectIdentifier = connectedDeviceIdRef.current.trim();
    if (reconnectIdentifier) {
      addLog("[PPI] Shared protocol missing. Reinitializing app protocol setup.", {
        reconnectIdentifier,
      });

      const setupResult = await connectAndSetupDevice(reconnectIdentifier);
      if (setupResult.status === "error") {
        addLog("[PPI][WARN] Failed to reinitialize app protocol setup.", {
          reconnectIdentifier,
          error: toErrorDetails(setupResult.error),
        });
      } else {
        const protocolAfterSetup = getMessageProtocolInstance();
        if (protocolAfterSetup) {
          ppiProtocolRef.current = protocolAfterSetup;
          ownsProtocolRef.current = false;
          attachSharedRxPacketSubscription();
          setProtocolRunning(true);
          addLog("[PPI] Reattached to app message protocol after setup refresh.");
          return protocolAfterSetup;
        }
      }
    } else {
      addLog("[PPI][WARN] Shared protocol missing and no connected device identifier was available.");
    }

    const hasPermissions = await ensureAndroidBlePermissions("protocol-start");
    if (!hasPermissions) {
      throw new Error("Bluetooth permissions not granted.");
    }

    await ensureGattDiscovered("protocol-start");
    const txUuid = resolvedMpTxUuidRef.current || MP_TX_UUID;
    const rxUuid = resolvedMpRxUuidRef.current || MP_RX_UUID;
    const serviceUuid = resolvedMpServiceUuidRef.current || MP_SERVICE_UUID;

    const protocol = new BleMessageProtocol({
      txCharacteristicUUID: txUuid,
      rxCharacteristicUUID: rxUuid,
      serviceUUID: serviceUuid,
      processIntervalMs: MP_PROCESS_INTERVAL_MS,
      ackTimeoutMs: PPI_ACK_TIMEOUT_MS,
      maxRetries: PPI_MAX_RETRIES,
      maxPacketLength: MP_MAX_PACKET_LEN,
      isMaster: true,
      enableSyncControl: true,
      sendAckNak: true,
      autoConsumeRx: true,
      onRxDataAcked: (ackedPacket) => {
        const packetBytes = ppiProtocolRef.current?.getLastRxPacketRaw() ?? new Uint8Array(0);
        const deviceId = connectedDeviceIdRef.current || "unknown";
        if (!packetBytes.length || deviceId === "unknown") {
          if (!packetBytes.length) {
            addLog("[PPI][INGEST][WARN] Skipped ingest because raw packet bytes were unavailable.", {
              sessionId: ackedPacket.sessionId,
              pktCounter: ackedPacket.pktCounter,
              ppi: ackedPacket.payload.ppi,
              type: ackedPacket.payload.type,
            });
          }
          return;
        }
        if (packetBytes.length < MP_MIN_FRAME_LEN_BYTES) {
          addLog("[PPI][INGEST][WARN] Skipped ingest because packet is shorter than MP frame header.", {
            packetBytesLength: packetBytes.length,
            sessionId: ackedPacket.sessionId,
            pktCounter: ackedPacket.pktCounter,
          });
          return;
        }

        const pktType = packetBytes[8];
        const pktPayloadLen = packetBytes[12] | (packetBytes[13] << 8);
        if (pktType !== MP_PACKET_TYPE_DATA) {
          addLog("[PPI][INGEST][WARN] Skipped ingest because packet is not DATA (ACK/NAK/etc.).", {
            pktType,
            sessionId: ackedPacket.sessionId,
            pktCounter: ackedPacket.pktCounter,
          });
          return;
        }
        if (pktPayloadLen <= 0 || ackedPacket.payload.payload.length <= 0) {
          addLog("[PPI][INGEST][WARN] Skipped ingest because DATA packet has empty payload.", {
            pktPayloadLen,
            sessionId: ackedPacket.sessionId,
            pktCounter: ackedPacket.pktCounter,
            ppi: ackedPacket.payload.ppi,
            type: ackedPacket.payload.type,
          });
          return;
        }
        if (packetBytes.length < MP_MIN_FRAME_LEN_BYTES + pktPayloadLen) {
          addLog("[PPI][INGEST][WARN] Skipped ingest because DATA packet appears truncated.", {
            packetBytesLength: packetBytes.length,
            pktPayloadLen,
            sessionId: ackedPacket.sessionId,
            pktCounter: ackedPacket.pktCounter,
          });
          return;
        }

        void (async () => {
          const hasAuthSession = await ensureDebugAuthSession();
          if (!hasAuthSession) {
            addLog("[PPI][INGEST][WARN] Skipped ingest because debug auth session could not be established.");
            return;
          }

          try {
            await ingestRawHardwareData({
              packetBytes,
              timestamp: new Date(),
              deviceId,
            });
          } catch (error) {
            const status =
              typeof error === "object" && error !== null && "response" in error
                ? (error as { response?: { status?: number } }).response?.status
                : undefined;

            addLog("[PPI][INGEST][WARN] Failed to ingest ACKed packet.", {
              error: toErrorDetails(error),
              status,
              deviceId,
              sessionId: ackedPacket.sessionId,
              pktCounter: ackedPacket.pktCounter,
              ppi: ackedPacket.payload.ppi,
              type: ackedPacket.payload.type,
              packetBytesLength: packetBytes.length,
              packetBase64: Buffer.from(packetBytes).toString("base64"),
            });

            if (status === 401 || status === 403) {
              addLog(
                "[PPI][INGEST][WARN] Missing or expired auth token for /api/mobile/hardware/ingest."
              );
            }
          }
        })();
      },
      logger: {
        debug: (...args: unknown[]) => relayMessageProtocolLog("DEBUG", args),
        info: (...args: unknown[]) => relayMessageProtocolLog("INFO", args),
        warn: (...args: unknown[]) => relayMessageProtocolLog("WARN", args),
        error: (...args: unknown[]) => relayMessageProtocolLog("ERR", args),
      },
      onRxPacket: (packet) => {
        handleIncomingPpiPacket(packet);
      },
    });

    ppiProtocolRef.current = protocol;
    ownsProtocolRef.current = true;
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
      serviceUuid,
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
    const shouldTrackResponse = type !== PpiType.RE && actionName !== "STOP_CALIBRATION_RQ";
    let matcher: PendingResponseMatcher | null = null;
    lateAckWatchTokenRef.current += 1;
    setResolvedResponsePreview(null);
    setResolvedPushAckPreview(null);
    if (shouldTrackResponse) {
      matcher = {
        actionName,
        ppi,
        requestType: type,
        sentAtMs,
        sentAt,
      };
      pendingResponseMatcherRef.current = matcher;
      setPendingResponseMatcher(matcher);
    } else {
      pendingResponseMatcherRef.current = null;
      setPendingResponseMatcher(null);
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

    if (type === PpiType.PUSH && matcher) {
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

  function parseDevelopmentCmdValue(rawInput: string): number | null {
    const value = rawInput.trim().toLowerCase();
    if (!value) {
      setDevelopmentCmdInputError("Enter a command value (0-255).");
      return null;
    }

    let parsed: number | null = null;
    if (/^0x[0-9a-f]{1,2}$/i.test(value)) {
      parsed = Number.parseInt(value, 16);
    } else if (/^\d{1,3}$/.test(value)) {
      parsed = Number.parseInt(value, 10);
    }

    if (parsed === null || !Number.isInteger(parsed) || parsed < 0 || parsed > 255) {
      setDevelopmentCmdInputError("Command must be a uint8 (0-255).");
      return null;
    }

    setDevelopmentCmdInputError("");
    return parsed;
  }

  function parseCalibrationWeightInput(rawInput: string): number | null {
    const parsed = coerceCalibrationWeightMg(rawInput);
    if (parsed === null) {
      const trimmedValue = rawInput.trim();
      if (!trimmedValue) {
        setCalibrationWeightInputError("Enter calibration weight in mg.");
      } else if (!/^\d+$/.test(trimmedValue)) {
        setCalibrationWeightInputError("Weight must be decimal digits only.");
      } else {
        setCalibrationWeightInputError("Weight must be a uint32 value (1-4294967295).");
      }
      return null;
    }

    setCalibrationWeightInputError("");
    return parsed;
  }

  function parseCapDetectionConfigInput(
    thresholdRawInput: string,
    hysteresisRawInput: string
  ): { threshold: number; hysteresis: number } | null {
    const thresholdTrimmed = thresholdRawInput.trim();
    const hysteresisTrimmed = hysteresisRawInput.trim();

    if (!thresholdTrimmed) {
      setCapDetectionConfigInputError("Enter cap threshold.");
      return null;
    }
    if (!/^\d+$/.test(thresholdTrimmed)) {
      setCapDetectionConfigInputError("Threshold must be decimal digits only.");
      return null;
    }

    const threshold = coerceUint16FromDecimalInput(thresholdTrimmed);
    if (threshold === null) {
      setCapDetectionConfigInputError("Threshold must be a uint16 value (0-65535).");
      return null;
    }
    if (threshold <= 0) {
      setCapDetectionConfigInputError("Threshold must be greater than 0.");
      return null;
    }

    if (!hysteresisTrimmed) {
      setCapDetectionConfigInputError("Enter cap hysteresis.");
      return null;
    }
    if (!/^\d+$/.test(hysteresisTrimmed)) {
      setCapDetectionConfigInputError("Hysteresis must be decimal digits only.");
      return null;
    }

    const hysteresis = coerceUint16FromDecimalInput(hysteresisTrimmed);
    if (hysteresis === null) {
      setCapDetectionConfigInputError("Hysteresis must be a uint16 value (0-65535).");
      return null;
    }
    if (hysteresis > threshold) {
      setCapDetectionConfigInputError("Hysteresis must be less than or equal to threshold.");
      return null;
    }

    setCapDetectionConfigInputError("");
    return { threshold, hysteresis };
  }

  async function persistCalibrationWeight(weightMg: number) {
    try {
      await AsyncStorage.setItem(CALIBRATION_WEIGHT_STORAGE_KEY, String(weightMg));
    } catch (error) {
      addLog("[PPI][CALIBRATION_WEIGHT][WARN] Failed to persist calibration weight.", {
        error: toErrorDetails(error),
      });
    }
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

  async function onDevelopmentCommandRequest() {
    const command = parseDevelopmentCmdValue(developmentCmdInput);
    if (command === null) {
      addLog("[PPI][DEVELOPMENT_CMD_RQ][WARN] Invalid command input.", {
        input: developmentCmdInput,
      });
      return;
    }

    setSelectedFlowAction("DEVELOPMENT_CMD_RQ");
    await withBusy("ppi-development-cmd-rq", async () => {
      await sendPpi(
        "DEVELOPMENT_CMD_RQ",
        PpiId.AD_DEVELOPMENT_CMD,
        PpiType.RQ,
        new Uint8Array([command])
      );
    });
  }

  async function requestCalibrationData() {
    setSelectedFlowAction("CALIBRATION_DATA_RQ");
    await withBusy("ppi-calibration-data-rq", async () => {
      await sendPpi("CALIBRATION_DATA_RQ", PpiId.AD_CALIBRATION_DATA, PpiType.RQ, new Uint8Array(0));
    });
  }

  async function onCalibrationDataRequest() {
    await requestCalibrationData();
  }

  function buildStartCalibrationPayload(start: boolean, calibrationWeightMg: number): Uint8Array {
    return encodeStartCalibrationParamPayload(start, calibrationWeightMg);
  }

  function buildCapDetectionSampleRatePayload(samplePeriodMs: number): Uint8Array {
    const payload = new Uint8Array(2);
    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    view.setUint16(0, samplePeriodMs, true);
    return payload;
  }

  function buildCapDetectionConfigPayload(threshold: number, hysteresis: number): Uint8Array {
    const payload = new Uint8Array(4);
    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    view.setUint16(0, threshold, true);
    view.setUint16(2, hysteresis, true);
    return payload;
  }

  async function onStartCapCalibrationIntervalPush() {
    setSelectedFlowAction("START_CAP_CALIBRATION_INTERVAL_PUSH");
    await withBusy("ppi-start-cap-calibration-interval-push", async () => {
      await sendPpi(
        "START_CAP_CALIBRATION_INTERVAL_PUSH",
        PpiId.AD_CAP_DETECTION_SAMPLE_RATE,
        PpiType.PUSH,
        buildCapDetectionSampleRatePayload(CAP_DETECTION_SAMPLE_PERIOD_MS_DEFAULT)
      );
    });
  }

  async function onSetCapDetectionConfigPush() {
    const parsedConfig = parseCapDetectionConfigInput(
      capDetectionThresholdInput,
      capDetectionHysteresisInput
    );
    if (!parsedConfig) {
      addLog("[PPI][SET_CAP_DETECTION_CONFIG_PUSH][WARN] Invalid cap detection config input.", {
        threshold: capDetectionThresholdInput,
        hysteresis: capDetectionHysteresisInput,
      });
      return;
    }

    setSelectedFlowAction("SET_CAP_DETECTION_CONFIG_PUSH");
    await withBusy("ppi-set-cap-detection-config-push", async () => {
      await sendPpi(
        "SET_CAP_DETECTION_CONFIG_PUSH",
        PpiId.AD_CAP_DETECTION_CONFIG,
        PpiType.PUSH,
        buildCapDetectionConfigPayload(parsedConfig.threshold, parsedConfig.hysteresis)
      );
    });
  }

  async function onStartCalibrationRequest() {
    const calibrationWeightMg = parseCalibrationWeightInput(calibrationWeightInput);
    if (calibrationWeightMg === null) {
      addLog("[PPI][START_CALIBRATION_RQ][WARN] Invalid calibration weight input.", {
        input: calibrationWeightInput,
      });
      return;
    }
    await persistCalibrationWeight(calibrationWeightMg);
    calibrationCompletionAutoRequestRef.current = false;
    calibrationStopRequestedRef.current = false;
    setCalibrationGuideStage("START_SENT");
    setCalibrationGuideInstruction("Start request sent. Waiting for firmware response...");
    setCalibrationCompletionMessage("");
    setCalibrationDataSnapshot(null);
    setCalibrationFeedbackSnapshot(null);
    setSelectedFlowAction("START_CALIBRATION_RQ");
    await withBusy("ppi-start-calibration-rq", async () => {
      const status = await sendPpi(
        "START_CALIBRATION_RQ",
        PpiId.AD_START_CALIBRATION,
        PpiType.RQ,
        buildStartCalibrationPayload(true, calibrationWeightMg)
      );
      if (status !== "SENT_DATA") {
        setCalibrationGuideStage("IDLE");
        setCalibrationGuideInstruction("Start request failed. Retry Start Calibration.");
      }
    });
  }

  async function onStopCalibrationRequest() {
    const calibrationWeightMg = parseCalibrationWeightInput(calibrationWeightInput);
    if (calibrationWeightMg === null) {
      addLog("[PPI][STOP_CALIBRATION_RQ][WARN] Invalid calibration weight input.", {
        input: calibrationWeightInput,
      });
      return;
    }
    await persistCalibrationWeight(calibrationWeightMg);
    calibrationStopRequestedRef.current = true;
    setSelectedFlowAction("STOP_CALIBRATION_RQ");
    setCalibrationGuideInstruction("Stop request sent. Waiting for firmware response...");
    await withBusy("ppi-stop-calibration-rq", async () => {
      const status = await sendPpi(
        "STOP_CALIBRATION_RQ",
        PpiId.AD_START_CALIBRATION,
        PpiType.RQ,
        buildStartCalibrationPayload(false, calibrationWeightMg)
      );
      if (status !== "SENT_DATA") {
        calibrationStopRequestedRef.current = false;
        setCalibrationGuideInstruction("Stop request failed. Retry Stop Calibration.");
      }
    });
  }

  async function onCalibrationWeightPresentPush(isPresent: boolean) {
    const selectedAction = isPresent
      ? "CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE"
      : "CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE";
    const busyKey = isPresent
      ? "ppi-calibration-weight-present-push-true"
      : "ppi-calibration-weight-present-push-false";
    const actionName = isPresent
      ? "CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE"
      : "CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE";

    setSelectedFlowAction(selectedAction);
    if (isPresent) {
      setCalibrationGuideStage("WEIGHT_PRESENT_SENT");
      setCalibrationGuideInstruction("Weight present sent. Waiting for calibration completion feedback...");
    }
    await withBusy(busyKey, async () => {
      const status = await sendPpi(
        actionName,
        PpiId.AD_CALIBRATION_WEIGHT_PRESENT,
        PpiType.PUSH,
        encodeBool(isPresent)
      );
      if (status !== "SENT_DATA" && isPresent) {
        setCalibrationGuideStage("AWAITING_WEIGHT");
        setCalibrationGuideInstruction("Weight present send failed. Retry Weight Present.");
      }
    });
  }

  async function onCalibrationWeightPresentPushTrue() {
    await onCalibrationWeightPresentPush(true);
  }

  async function onCalibrationWeightPresentPushFalse() {
    await onCalibrationWeightPresentPush(false);
  }

  async function onStartBaseliningRequest() {
    baseliningValidationSentRef.current = false;
    setBaseliningGuideStage("START_SENT");
    setBaseliningGuideInstruction("Start request sent. Waiting for firmware response...");
    setBaseliningFeedbackSnapshot(null);
    setSelectedFlowAction("START_BASELINING_RQ");
    await withBusy("ppi-start-baselining-rq", async () => {
      const status = await sendPpi(
        "START_BASELINING_RQ",
        PpiId.AD_START_BASELINING,
        PpiType.RQ,
        encodeBool(true)
      );
      if (status !== "SENT_DATA") {
        setBaseliningGuideStage("IDLE");
        setBaseliningGuideInstruction("Start request failed. Retry Start Baselining.");
      }
    });
  }

  async function onStopBaseliningRequest() {
    setSelectedFlowAction("STOP_BASELINING_RQ");
    setBaseliningGuideInstruction("Stop request sent. Waiting for firmware response...");
    await withBusy("ppi-stop-baselining-rq", async () => {
      const status = await sendPpi(
        "STOP_BASELINING_RQ",
        PpiId.AD_START_BASELINING,
        PpiType.RQ,
        encodeBool(false)
      );
      if (status !== "SENT_DATA") {
        setBaseliningGuideInstruction("Stop request failed. Retry Stop Baselining.");
      }
    });
  }

  async function onValidateMedResponse(success: boolean) {
    const selectedAction = success ? "VALIDATE_MED_RE_TRUE" : "VALIDATE_MED_RE_FALSE";
    const busyKey = success ? "ppi-validate-med-re-true" : "ppi-validate-med-re-false";
    const actionName = success ? "VALIDATE_MED_RE_TRUE" : "VALIDATE_MED_RE_FALSE";

    setSelectedFlowAction(selectedAction);
    setBaseliningGuideStage("VALIDATION_SENT");
    setBaseliningGuideInstruction("Validation response sent. Waiting for next baselining state...");
    baseliningValidationSentRef.current = true;
    await withBusy(busyKey, async () => {
      const status = await sendPpi(actionName, PpiId.AD_VALIDATE_MED, PpiType.RE, encodeBool(success));
      if (status !== "SENT_DATA") {
        baseliningValidationSentRef.current = false;
        setBaseliningGuideStage("AWAITING_VALIDATION");
        setBaseliningGuideInstruction("Validation send failed. Retry Validate Med OK/Fail.");
      }
    });
  }

  async function onValidateMedResponseTrue() {
    await onValidateMedResponse(true);
  }

  async function onValidateMedResponseFalse() {
    await onValidateMedResponse(false);
  }

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

  async function onRepeatLastPacket() {
    const txPreview = lastPpiTxPreview;
    if (!txPreview) {
      addLog("[PPI][REPEAT_LAST_PACKET][WARN] No last packet available to replay.");
      return;
    }

    if (!isConnected) {
      addLog("[PPI][REPEAT_LAST_PACKET][WARN] Cannot replay while disconnected.");
      return;
    }

    const payloadBytes = decodeHexPayload(txPreview.payloadHex);
    if (!payloadBytes) {
      addLog("[PPI][REPEAT_LAST_PACKET][ERR] Last payload hex is invalid.", {
        action: txPreview.action,
        payloadHex: txPreview.payloadHex,
      });
      return;
    }

    if (!Number.isInteger(txPreview.ppi) || !Number.isInteger(txPreview.type)) {
      addLog("[PPI][REPEAT_LAST_PACKET][ERR] Last packet metadata is invalid.", {
        action: txPreview.action,
        ppi: txPreview.ppi,
        type: txPreview.type,
      });
      return;
    }

    if (isQuickFlowAction(txPreview.action)) {
      setSelectedFlowAction(txPreview.action);
    }

    await withBusy("ppi-repeat-last-packet", async () => {
      addLog("[PPI][REPEAT_LAST_PACKET] Replaying last packet.", {
        action: txPreview.action,
        ppi: txPreview.ppi,
        type: txPreview.type,
        payloadLen: payloadBytes.length,
      });
      await sendPpi(
        txPreview.action,
        txPreview.ppi as PpiId,
        txPreview.type as PpiType,
        payloadBytes
      );
    });
  }

  async function onDisconnect() {
    await withBusy("disconnect", async () => {
      try {
        stopPpiProtocol();
        stopAndClearMessageProtocol();
        resolvedMpTxUuidRef.current = MP_TX_UUID;
        resolvedMpRxUuidRef.current = MP_RX_UUID;
        resolvedMpServiceUuidRef.current = MP_SERVICE_UUID;
        isGattDiscoveredRef.current = false;
        gattDiscoveryInFlightRef.current = null;
        const res = await disconnect();
        addLog("[DISCONNECT]", res);
        setIsConnected(false);
        setConnectedDeviceLabel("");
        connectedDeviceIdRef.current = "";
        calibrationCompletionAutoRequestRef.current = false;
        calibrationStopRequestedRef.current = false;
        baseliningValidationSentRef.current = false;
        setCalibrationGuideStage("IDLE");
        setCalibrationGuideInstruction(CALIBRATION_INITIAL_INSTRUCTION);
        setCalibrationCompletionMessage("");
        setCalibrationFeedbackSnapshot(null);
        setBaseliningFeedbackSnapshot(null);
        setBaseliningGuideStage("IDLE");
        setBaseliningGuideInstruction(BASELINING_INITIAL_INSTRUCTION);
      } catch (error) {
        addLog(`[DISCONNECT][ERR] ${String(error)}`);
      }
    });
  }

  useEffect(() => {
    return () => {
      lateAckWatchTokenRef.current += 1;
      detachSharedRxPacketSubscription();
      if (ppiProtocolRef.current && ownsProtocolRef.current) {
        ppiProtocolRef.current.stop();
      }
      ppiProtocolRef.current = null;
      ownsProtocolRef.current = false;
    };
  }, []);

  useEffect(() => {
    setLogCount(getBleDebugLogs().length);
    return subscribeBleDebugLogs(() => {
      setLogCount(getBleDebugLogs().length);
    });
  }, []);

  useEffect(() => {
    let isActive = true;

    const hydrateCalibrationWeight = async () => {
      try {
        const storedWeight = await AsyncStorage.getItem(CALIBRATION_WEIGHT_STORAGE_KEY);
        if (!isActive || !storedWeight) {
          return;
        }

        const parsedWeight = coerceCalibrationWeightMg(storedWeight);
        if (parsedWeight === null) {
          return;
        }

        setCalibrationWeightInput(String(parsedWeight));
      } catch {
        // Ignore hydration failures in debug mode.
      }
    };

    void hydrateCalibrationWeight();

    return () => {
      isActive = false;
    };
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
    if (isConnected) {
      return;
    }
    calibrationCompletionAutoRequestRef.current = false;
    calibrationStopRequestedRef.current = false;
    baseliningValidationSentRef.current = false;
    setCalibrationGuideStage("IDLE");
    setCalibrationGuideInstruction(CALIBRATION_INITIAL_INSTRUCTION);
    setBaseliningFeedbackSnapshot(null);
    setBaseliningGuideStage("IDLE");
    setBaseliningGuideInstruction(BASELINING_INITIAL_INSTRUCTION);
  }, [isConnected]);

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
    DEVELOPMENT_CMD_RQ: onDevelopmentCommandRequest,
    CALIBRATION_DATA_RQ: onCalibrationDataRequest,
    START_CALIBRATION_RQ: onStartCalibrationRequest,
    STOP_CALIBRATION_RQ: onStopCalibrationRequest,
    CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE: onCalibrationWeightPresentPushTrue,
    CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE: onCalibrationWeightPresentPushFalse,
    START_CAP_CALIBRATION_INTERVAL_PUSH: onStartCapCalibrationIntervalPush,
    SET_CAP_DETECTION_CONFIG_PUSH: onSetCapDetectionConfigPush,
    START_BASELINING_RQ: onStartBaseliningRequest,
    STOP_BASELINING_RQ: onStopBaseliningRequest,
    VALIDATE_MED_RE_TRUE: onValidateMedResponseTrue,
    VALIDATE_MED_RE_FALSE: onValidateMedResponseFalse,
    DOCK_BATTERY_RQ: onDockBatteryRequest,
    RING_BATTERY_RQ: onRingBatteryRequest,
  };

  const onQuickActionsMomentumEnd = useCallback(
    (event: NativeSyntheticEvent<NativeScrollEvent>) => {
      const pageWidth = quickActionsViewportWidth || event.nativeEvent.layoutMeasurement.width;
      if (!pageWidth) return;

      const nextPageIndex = Math.round(event.nativeEvent.contentOffset.x / pageWidth);
      setQuickActionsPageIndex(nextPageIndex);
    },
    [quickActionsViewportWidth]
  );
  const onQuickActionPageLayout = useCallback((pageId: string, height: number) => {
    if (!height) return;
    setQuickActionPageHeights((previous) => {
      const existingHeight = previous[pageId];
      if (existingHeight && Math.abs(existingHeight - height) < 1) {
        return previous;
      }
      return {
        ...previous,
        [pageId]: height,
      };
    });
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
        <View style={styles.topInfoWrap}>
          <DebugHeaderCard
            isConnected={isConnected}
            connectedDeviceLabel={connectedDeviceLabel}
            onBack={() => router.back()}
            onOpenLogs={() => router.push("/home/ble-debug/logs")}
            onDisconnect={() => {
              void onDisconnect();
            }}
            disconnectDisabled={!isConnected || isBlockedByOtherAction("disconnect")}
            disconnectLoading={loadingAction === "disconnect"}
          />
        </View>

        <View style={styles.panel}>
          <QuickActionsCarouselCard
            quickActionPages={quickActionPages}
            quickActionsViewportHeight={quickActionsViewportHeight}
            quickActionsViewportWidth={quickActionsViewportWidth}
            quickActionsPageIndex={quickActionsPageIndex}
            loadingAction={loadingAction}
            isConnected={isConnected}
            developmentCmdInput={developmentCmdInput}
            developmentCmdInputError={developmentCmdInputError}
            calibrationWeightInput={calibrationWeightInput}
            calibrationWeightInputError={calibrationWeightInputError}
            capDetectionThresholdInput={capDetectionThresholdInput}
            capDetectionHysteresisInput={capDetectionHysteresisInput}
            capDetectionConfigInputError={capDetectionConfigInputError}
            isCalibrationWeightInputDisabled={isCalibrationWeightInputDisabled}
            calibrationGuideStage={calibrationGuideStage}
            calibrationGuideStageLabel={calibrationGuideStageLabel}
            calibrationGuideInstruction={calibrationGuideInstruction}
            calibrationCompletionMessage={calibrationCompletionMessage}
            calibrationDataSnapshot={calibrationDataSnapshot}
            calibrationFeedbackSnapshot={calibrationFeedbackSnapshot}
            calibrationFeedbackPreview={calibrationFeedbackPreview}
            calibrationResponsePreview={calibrationResponsePreview}
            baseliningGuideStage={baseliningGuideStage}
            baseliningGuideInstruction={baseliningGuideInstruction}
            baseliningFeedbackSnapshot={baseliningFeedbackSnapshot}
            baseliningFeedbackPreview={baseliningFeedbackPreview}
            baseliningResponsePreview={baseliningResponsePreview}
            onViewportWidthChange={setQuickActionsViewportWidth}
            onQuickActionsMomentumEnd={onQuickActionsMomentumEnd}
            onQuickActionPageLayout={onQuickActionPageLayout}
            onDevelopmentCmdInputChange={(text) => {
              setDevelopmentCmdInput(text);
              if (developmentCmdInputError) {
                setDevelopmentCmdInputError("");
              }
            }}
            onCalibrationWeightInputChange={(text) => {
              setCalibrationWeightInput(text);
              if (calibrationWeightInputError) {
                setCalibrationWeightInputError("");
              }

              const parsedWeight = coerceCalibrationWeightMg(text);
              if (parsedWeight !== null) {
                void persistCalibrationWeight(parsedWeight);
              }
            }}
            onCapDetectionThresholdInputChange={(text) => {
              setCapDetectionThresholdInput(text);
              if (capDetectionConfigInputError) {
                setCapDetectionConfigInputError("");
              }
            }}
            onCapDetectionHysteresisInputChange={(text) => {
              setCapDetectionHysteresisInput(text);
              if (capDetectionConfigInputError) {
                setCapDetectionConfigInputError("");
              }
            }}
            onQuickActionPress={(action) => {
              void quickActionMap[action]();
            }}
            onQuickActionInfoPress={openFlowHelp}
            quickActionSubtitle={getQuickActionSubtitle}
            isBlockedByOtherAction={isBlockedByOtherAction}
            isCalibrationActionEnabled={isCalibrationActionEnabled}
            isBaseliningActionEnabled={isBaseliningActionEnabled}
          />

          <PayloadDetailsCard
            lastPpiTxPreview={lastPpiTxPreview}
            lastPpiRxPreview={lastPpiRxPreview}
            lastPpiRxHumanReadable={lastPpiRxHumanReadable}
            incomingDetailsTab={incomingDetailsTab}
            resolvedResponsePreview={resolvedResponsePreview}
            resolvedResponseHumanReadable={resolvedResponseHumanReadable}
            resolvedPushAckPreview={resolvedPushAckPreview}
            pendingResponseMatcher={pendingResponseMatcher}
            onIncomingDetailsTabChange={setIncomingDetailsTab}
            onRepeatLastPacket={() => {
              void onRepeatLastPacket();
            }}
            repeatDisabled={
              !isConnected || !lastPpiTxPreview || isBlockedByOtherAction("ppi-repeat-last-packet")
            }
            repeatLoading={loadingAction === "ppi-repeat-last-packet"}
          />

          <ProtocolInfoCard
            protocolInfo={protocolInfo}
            serviceUuid={resolvedMpServiceUuidRef.current || MP_SERVICE_UUID}
            txUuid={resolvedMpTxUuidRef.current || MP_TX_UUID}
            rxUuid={resolvedMpRxUuidRef.current || MP_RX_UUID}
          />
        </View>
      </ScrollView>

      <FlowHelpModal
        visible={flowHelpVisible}
        selectedFlowMeta={selectedFlowMeta}
        selectedFlowAction={selectedFlowAction}
        selectedFlowStatus={selectedFlowStatus}
        flowStatusBadge={flowStatusBadge}
        selectedFlowPayloadValueText={selectedFlowPayloadValueText}
        flowSteps={flowSteps}
        flowProgress={flowProgress}
        onClose={() => setFlowHelpVisible(false)}
      />
    </SafeAreaView>
  );
}
