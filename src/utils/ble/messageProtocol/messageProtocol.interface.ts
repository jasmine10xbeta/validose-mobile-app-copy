export enum MsgProtTxPacketStatus {
  NONE = 0,
  NEW,
  COMPLETED,
  WAITING_FOR_ACK,
  ERROR,
  ABANDONED,
  MAX,
}

export enum MsgProtRxPacketStatus {
  NONE = 0,
  NEW,
  PROCESSED,
  MAX,
}

export enum MsgProtError {
  NONE = 0,
  NULL_PTR,
  INVALID_ARG,
  TIMEOUT,
  BUSY,
  TX_QUEUE_FULL,
  PAYLOAD_TOO_BIG,
  OUT_OF_RANGE,
  BUFFER_OVERFLOW,
  SWITCH_DEFAULT,
  INVALID_PACKET_STATUS,
  NO_NEW_PACKET,
  RNG_FAILURE,
  ERROR_MAX,
}

export type PacketTypeMap = {
  ACK: number;
  NAK: number;
  DATA: number;
  SYNC_START: number;
  SYNC_ACK: number;
  SYNC_MISMATCH: number;
  MAX: number;
};

export type Logger = {
  debug: (...args: unknown[]) => void;
  info: (...args: unknown[]) => void;
  warn: (...args: unknown[]) => void;
  error: (...args: unknown[]) => void;
};

export interface MessageProtocolTransport {
  sendPacket: (bytes: Uint8Array) => Promise<boolean>;
  subscribe: (handler: (bytes: Uint8Array) => void) => Promise<() => void>;
}

export interface MpPacketPayload {
  type: number;
  ppi: number;
  pktPayloadLen: number;
  payload: Uint8Array;
}

export interface AckedRxDataPacket {
  pktCounter: number;
  sessionId: number;
  payload: MpPacketPayload;
}

export type RxHandler = (payload: MpPacketPayload) => void;
export type RxDataAckedHandler = (packet: AckedRxDataPacket) => void;

export interface MessageProtocolInterface {
  send(payload: MpPacketPayload): MsgProtError;
  process(): Promise<MsgProtError>;
  startSync(): Promise<MsgProtError>;
  getRxPacketStatus(): MsgProtRxPacketStatus;
  getTxPacketStatus(): MsgProtTxPacketStatus;
  getCurrentSessionId(): number;
  getRxPacket(): { result: MsgProtError; packet?: MpPacketPayload };
  getMaxPayloadLength(): number;
  getLastRxPacketRaw(): Uint8Array;
  setMtu(mtu: number): void;
  setMaxPacketLength(maxPacketLength: number): void;
  registerRxHandler(ppi: number, type: number | "*", handler: RxHandler): void;
}

export interface BleMessageProtocolOptions {
  txCharacteristicUUID: string;
  rxCharacteristicUUID: string;
  serviceUUID?: string;
  processIntervalMs?: number;
  ackTimeoutMs?: number;
  maxRetries?: number;
  maxPacketLength?: number;
  mtu?: number;
  packetTypes?: Partial<PacketTypeMap>;
  onRxPacket?: RxHandler;
  onRxDataAcked?: RxDataAckedHandler;
  logger?: Partial<Logger>;
  autoConsumeRx?: boolean;
  transport?: MessageProtocolTransport;
  sessionIdProvider?: () => number;
  nowProvider?: () => number;
  relaxedAckMatching?: boolean;
  isMaster?: boolean;
  enableSyncControl?: boolean;
  sendAckNak?: boolean;
}
