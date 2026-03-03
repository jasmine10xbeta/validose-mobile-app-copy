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
  frameBytesHex: string[];
  frameBytesIndexedHex: string[];
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
  ppi?: number;
  ppiName?: string;
  type?: number;
  typeName?: string;
  pktPayloadLen?: number;
  payloadHex: string;
  payloadBase64: string;
  payloadUtf8: string;
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
  | "DOCK_STATUS_RQ"
  | "RING_STATUS_RQ"
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
};
