import { PacketTypeMap } from "./messageProtocol.interface";

export const MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN = 256;
export const MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE = 1 + 1 + 2;
export const DEFAULT_ACK_TIMEOUT_MS = 1000;
export const DEFAULT_PROCESS_INTERVAL_MS = 250;
export const DEFAULT_MAX_RETRIES = 20;
export const DEFAULT_BLE_MTU = 23;
export const MAX_TIME_BEFORE_SYNC_RETRY_MS = 1000;

export const HEADER_LEN = 2 + 2 + 4 + 1 + 1;
export const MIN_PACKET_LEN = HEADER_LEN + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
export const ATT_HEADER_LEN = 3;

export const DEFAULT_PACKET_TYPES: PacketTypeMap = {
  DATA: 0,
  ACK: 1,
  NAK: 2,
  SYNC_START: 3,
  SYNC_ACK: 4,
  SYNC_MISMATCH: 5,
  MAX: 6,
};
