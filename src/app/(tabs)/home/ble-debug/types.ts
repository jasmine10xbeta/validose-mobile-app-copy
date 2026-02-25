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
  decoded: unknown;
};

export type FlowStepState = "pending" | "active" | "done" | "error";

export type QuickFlowAction =
  | "TIME_RQ"
  | "TIME_RE"
  | "TIME_PUSH"
  | "DOSE_SCHEDULE_RQ"
  | "DOSE_SCHEDULE_RE"
  | "DOSE_SCHEDULE_PUSH";

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
