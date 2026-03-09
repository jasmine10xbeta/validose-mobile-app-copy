export type MpFramePreview = {
  frameLength: number;
  crc: number;
  pktCounter: number;
  sessionId: number;
  pktType: number;
  status: number;
  payloadType: number;
  payloadPpi: number;
  pktPayloadLen: number;
  payloadHex: string;
  payloadBase64: string;
  frameHex: string;
  frameBase64: string;
};

export type PersistedDebugInputs = {
  deviceId: string;
  serviceUuid: string;
  characteristicUuid: string;
};

export type PpiTxPreview = {
  action: string;
  ppi: number;
  ppiName: string;
  type: number;
  typeName: string;
  pktPayloadLen: number;
  payloadHex: string;
  payloadBase64: string;
  fullFrameHex: string;
  fullFrameBase64: string;
  mpFrame: MpFramePreview | null;
  status: string;
  sentAt: string;
};

export type PpiRxPreview = {
  source: string;
  receivedAt: string;
  receivedAtMs: number;
  ppi?: number;
  ppiName?: string;
  type?: number;
  typeName?: string;
  pktPayloadLen?: number;
  payloadHex: string;
  payloadBase64: string;
  fullFrameHex: string;
  fullFrameBase64: string;
  mpFrame: MpFramePreview | null;
  decoded: unknown;
};

export type FlowStepState = "pending" | "active" | "done" | "error";

export type QuickFlowAction =
  | "TIME_RQ"
  | "TIME_PUSH"
  | "DOSE_SCHEDULE_RQ"
  | "DOSE_SCHEDULE_PUSH"
  | "DOSE_SCHEDULE_PUSH_ALT_1"
  | "DOSE_SCHEDULE_PUSH_ALT_2"
  | "DOCK_STATUS_RQ"
  | "RING_STATUS_RQ"
  | "DEVELOPMENT_CMD_RQ"
  | "CALIBRATION_DATA_RQ"
  | "START_CALIBRATION_RQ"
  | "STOP_CALIBRATION_RQ"
  | "CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE"
  | "CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE"
  | "START_BASELINING_RQ"
  | "STOP_BASELINING_RQ"
  | "VALIDATE_MED_RE_TRUE"
  | "VALIDATE_MED_RE_FALSE"
  | "DOCK_BATTERY_RQ"
  | "RING_BATTERY_RQ";

export type QuickFlowMeta = {
  title: string;
  actionName: string;
  busyKey: string;
  ppiName: string;
  ppiId: number;
  typeName: string;
  typeId: number;
  payloadHint: string;
  lenHint: string;
  payloadPreview?: unknown;
};

export type QuickActionItem = {
  key: QuickFlowAction;
  label: string;
};

export type QuickActionPage = {
  id: "general" | "developer" | "calibration" | "baselining";
  title: string;
  actions: QuickActionItem[];
};

export type IncomingDetailsTab = "LATEST" | "RESOLVED";

export type PendingResponseMatcher = {
  actionName: string;
  ppi: number;
  requestType: number;
  sentAtMs: number;
  sentAt: string;
};

export type PushAckPreview = {
  actionName: string;
  ppi: number;
  type: number;
  ackedAt: string;
  ackedAtMs: number;
  sessionId: number | null;
  pktCounter: number | null;
};

export type CompactFeedbackPreview = {
  updatedAt: string;
  ppiName: string;
  typeName: string;
  summary: string;
};

export type CalibrationGuideStage =
  | "IDLE"
  | "START_SENT"
  | "AWAITING_WEIGHT"
  | "WEIGHT_PRESENT_SENT"
  | "COMPLETED";

export type CalibrationDataSnapshot = {
  updatedAt: string;
  zeroOffset: number | null;
  calibrationFactor: number | null;
  fullAssemblyWeightMg: number | null;
};

export type CalibrationFeedbackSnapshot = {
  updatedAt: string;
  currentState: number | null;
  motionState: number | null;
  isComplete: boolean;
};

export type BaseliningFeedbackSnapshot = {
  updatedAt: string;
  currentState: number | null;
  motionState: number | null;
  stdDev: number | null;
  avgWeightMg: number | null;
  isRingPresent: boolean | null;
  medicationNfcIdHex: string | null;
};

export type BaseliningGuideStage =
  | "IDLE"
  | "START_SENT"
  | "RUNNING"
  | "AWAITING_VALIDATION"
  | "VALIDATION_SENT"
  | "COMPLETED"
  | "ERROR";
