import { Buffer } from "buffer";

import { SERVICE_UUIDS } from "@/constants/ble";

export const MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN = 256;
export const MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE = 1 + 1 + 2;
export const DEFAULT_ACK_TIMEOUT_MS = 1000;
export const DEFAULT_PROCESS_INTERVAL_MS = 250;
export const DEFAULT_MAX_RETRIES = 20;
export const DEFAULT_BLE_MTU = 23; // Safe default; effective ATT payload = MTU - 3
export const MAX_TIME_BEFORE_SYNC_RETRY_MS = 1000;

const HEADER_LEN = 2 + 2 + 4 + 1 + 1; // pkt_crc + pkt_counter + session_id + pkt_type + status
const MIN_PACKET_LEN = HEADER_LEN + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
const ATT_HEADER_LEN = 3; // BLE ATT header size

// TX Packet status enum (mirrors firmware ordering)
export enum MsgProtTxPacketStatus {
  NONE = 0,
  NEW,
  COMPLETED,
  WAITING_FOR_ACK,
  ERROR,
  ABANDONED,
  MAX,
}

// RX Packet status enum (mirrors firmware ordering)
export enum MsgProtRxPacketStatus {
  NONE = 0,
  NEW,
  PROCESSED,
  MAX,
}

// Error codes specific to the module (mirrors firmware ordering)
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

export const DEFAULT_PACKET_TYPES: PacketTypeMap = {
  DATA: 0,
  ACK: 1,
  NAK: 2,
  SYNC_START: 3,
  SYNC_ACK: 4,
  SYNC_MISMATCH: 5,
  MAX: 6,
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

export interface MessageProtocolInterface {
  send(payload: MpPacketPayload): MsgProtError;
  process(): Promise<MsgProtError>;
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

export type RxHandler = (payload: MpPacketPayload) => void;

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
  logger?: Partial<Logger>;
  // If true, RX packets are auto-marked as PROCESSED after callbacks run.
  // Use this if you prefer push-style handlers over polling getRxPacket().
  autoConsumeRx?: boolean;
  // Optional in-memory transport for tests; defaults to BLE characteristics.
  transport?: MessageProtocolTransport;
  // Optional deterministic session ID provider for tests.
  sessionIdProvider?: () => number;
  // Optional time provider for tests.
  nowProvider?: () => number;
  // Debug/testing aid: when true, ACK handling is tolerant to counter mismatch
  // and can recover a TX packet from ABANDONED -> COMPLETED if a valid-session ACK arrives.
  relaxedAckMatching?: boolean;
  // Firmware parity: true means this instance behaves as the master endpoint.
  // App runtime should use true when it owns session ID generation.
  isMaster?: boolean;
  // When false, SYNC control frames are ignored and no SYNC_START flow is initiated.
  // App runtime should disable sync control when firmware owns synchronization.
  enableSyncControl?: boolean;
  // When false, this endpoint will not transmit ACK/NAK packets for incoming DATA.
  // App runtime should disable ACK/NAK TX.
  sendAckNak?: boolean;
}

type MpPacketHeader = {
  pktCrc: number;
  pktCounter: number;
  sessionId: number;
  pktType: number;
  status: number;
};

type MpPacket = {
  header: MpPacketHeader;
  payload: MpPacketPayload;
};

type ParsedPacketResult = { result: MsgProtError; packet?: MpPacket; raw?: Uint8Array };

const defaultLogger: Logger = {
  debug: () => undefined,
  info: () => undefined,
  warn: () => undefined,
  error: () => undefined,
};

type BleModule = {
  subscribeToCharacteristic: (
    characteristicUUID: string,
    serviceUUID: string,
    callback: (data: { uuid: string; fullUuid: string; hex: string; deviceId: string }) => void
  ) => Promise<() => void>;
  writeCharacteristic: (characteristicUUID: string, value: any) => Promise<boolean>;
};

let bleModuleCache: BleModule | null = null;

function getBleModule(): BleModule {
  if (bleModuleCache) {
    return bleModuleCache;
  }

  // Lazy require to keep tests from importing native modules at file-load time.
  // eslint-disable-next-line @typescript-eslint/no-var-requires
  const mod = require("../../../modules/tenx-mdk-ble-rn-library/src/index") as BleModule;
  bleModuleCache = mod;
  return mod;
}

export class BleMessageProtocol implements MessageProtocolInterface {
  private readonly txCharacteristicUUID: string;
  private readonly rxCharacteristicUUID: string;
  private readonly serviceUUID: string;
  private readonly ackTimeoutMs: number;
  private readonly maxRetries: number;
  private readonly processIntervalMs: number;
  private readonly packetTypes: PacketTypeMap;
  private readonly logger: Logger;
  private readonly onRxPacket?: RxHandler;
  private readonly autoConsumeRx: boolean;
  private readonly transport?: MessageProtocolTransport;
  private readonly sessionIdProvider?: () => number;
  private readonly nowProvider?: () => number;
  private readonly relaxedAckMatching: boolean;
  private readonly isMaster: boolean;
  private readonly enableSyncControl: boolean;
  private readonly sendAckNak: boolean;

  private processTimer: ReturnType<typeof setInterval> | null = null;
  private unsubscribe: (() => void) | null = null;
  private processInFlight = false;

  private maxPacketLength = 0;
  private maxPacketPayloadLen = 0;

  private rxPacket: MpPacket;
  private txPacket: MpPacket;
  private lastPacketSent: MpPacket;

  private lastRxPacketRaw = new Uint8Array(0);
  private lastRxPacketLen = 0;
  private lastRxPacketValid = false;
  private lastRxPacketDeferred = false;
  private lastTxPacketRaw = new Uint8Array(0);
  private lastTxPacketLen = 0;
  private lastTxPacketValid = false;

  private nextPacketId = 0;
  private pendingId = 0;
  private retriesLeft = 0;
  private deadlineMs = 0;
  private currentSessionId = 0;
  private isSyncing = false;
  private hasAttemptedSync = false;
  private lastResyncTimeMs = 0;
  private hasGeneratedSessionId = false;
  private rxHandlers = new Map<string, RxHandler>();

  constructor(options: BleMessageProtocolOptions) {
    this.txCharacteristicUUID = options.txCharacteristicUUID;
    this.rxCharacteristicUUID = options.rxCharacteristicUUID;
    this.serviceUUID = options.serviceUUID ?? SERVICE_UUIDS.CUSTOM_SERVICE;
    this.ackTimeoutMs = options.ackTimeoutMs ?? DEFAULT_ACK_TIMEOUT_MS;
    this.maxRetries = options.maxRetries ?? DEFAULT_MAX_RETRIES;
    this.processIntervalMs = options.processIntervalMs ?? DEFAULT_PROCESS_INTERVAL_MS;
    this.packetTypes = { ...DEFAULT_PACKET_TYPES, ...(options.packetTypes ?? {}) };
    this.logger = { ...defaultLogger, ...(options.logger ?? {}) };
    this.onRxPacket = options.onRxPacket;
    this.autoConsumeRx = options.autoConsumeRx ?? false;
    this.transport = options.transport;
    this.sessionIdProvider = options.sessionIdProvider;
    this.nowProvider = options.nowProvider;
    this.relaxedAckMatching = options.relaxedAckMatching ?? false;
    this.isMaster = options.isMaster ?? false;
    this.enableSyncControl = options.enableSyncControl ?? true;
    this.sendAckNak = options.sendAckNak ?? true;

    if (options.maxPacketLength !== undefined) {
      this.setMaxPacketLength(options.maxPacketLength);
    } else if (options.mtu !== undefined) {
      this.setMtu(options.mtu);
    } else {
      this.setMtu(DEFAULT_BLE_MTU);
    }

    this.rxPacket = this.createEmptyPacket();
    this.txPacket = this.createEmptyPacket();
    this.lastPacketSent = this.createEmptyPacket();

    this.initializeState();
  }

  async start(): Promise<void> {
    if (this.unsubscribe) {
      return;
    }

    if (this.transport) {
      this.unsubscribe = await this.transport.subscribe((bytes) => this.handleIncomingRaw(bytes));
    } else {
      const { subscribeToCharacteristic } = getBleModule();
      this.unsubscribe = await subscribeToCharacteristic(
        this.rxCharacteristicUUID,
        this.serviceUUID,
        ({ hex }) => this.handleIncomingHex(hex)
      );
    }

    if (this.processIntervalMs > 0) {
      this.processTimer = setInterval(() => {
        void this.process();
      }, this.processIntervalMs);
    }

    if (this.isMaster && !this.enableSyncControl && !this.hasGeneratedSessionId) {
      this.generateSessionIdForMaster();
    }
  }

  stop(): void {
    if (this.processTimer) {
      clearInterval(this.processTimer);
      this.processTimer = null;
    }

    if (this.unsubscribe) {
      this.unsubscribe();
      this.unsubscribe = null;
    }
    this.initializeState();
  }

  /**
   * Queue a payload for transmission. Prefer checking getTxPacketStatus() first
   * and only sending when status is COMPLETED or ABANDONED.
   */
  send(payload: MpPacketPayload): MsgProtError {
    if (!payload) {
      return MsgProtError.NULL_PTR;
    }

    const payloadLen = payload.payload?.length ?? 0;

    if (!payload.payload) {
      return MsgProtError.NULL_PTR;
    }

    if (payload.pktPayloadLen !== payloadLen) {
      // Prefer actual buffer length if caller passed a mismatched pktPayloadLen.
      this.logger.warn("[MP] pktPayloadLen mismatch; using payload buffer length.");
    }

    // Validate against negotiated max payload length (header/overhead excluded).
    if (payloadLen + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE > this.maxPacketPayloadLen) {
      return MsgProtError.PAYLOAD_TOO_BIG;
    }

    if (
      this.txPacket.header.status !== MsgProtTxPacketStatus.ABANDONED &&
      this.txPacket.header.status !== MsgProtTxPacketStatus.COMPLETED &&
      this.txPacket.header.status !== MsgProtTxPacketStatus.NEW
    ) {
      this.logger.error("[MP] Cannot send new packet while another is being processed.");
      return MsgProtError.BUSY;
    }

    if (this.isSyncing) {
      this.logger.error("[MP] Cannot send new packet while syncing.");
      return MsgProtError.BUSY;
    }

    if (this.isMaster && !this.enableSyncControl && !this.hasGeneratedSessionId) {
      if (!this.generateSessionIdForMaster()) {
        return MsgProtError.RNG_FAILURE;
      }
    }

    this.txPacket.header.pktCounter = this.nextPacketId;
    this.txPacket.header.sessionId = this.currentSessionId;
    this.txPacket.header.pktType = this.packetTypes.DATA;
    this.txPacket.header.status = MsgProtTxPacketStatus.NEW;

    this.txPacket.payload = {
      type: payload.type,
      ppi: payload.ppi,
      pktPayloadLen: payloadLen,
      payload: new Uint8Array(payload.payload),
    };

    return MsgProtError.NONE;
  }

  /**
   * Process outbound state machine (retries/timeouts). Should be called often.
   */
  async process(): Promise<MsgProtError> {
    if (this.processInFlight) {
      return MsgProtError.BUSY;
    }

    this.processInFlight = true;
    try {
      this.refreshMaxPayloadLength();
      return await this.processOutboundPackets();
    } finally {
      this.processInFlight = false;
    }
  }

  /**
   * Get the current RX packet status. When NEW, call getRxPacket() to consume it.
   */
  getRxPacketStatus(): MsgProtRxPacketStatus {
    return this.rxPacket.header.status as MsgProtRxPacketStatus;
  }

  /**
   * Get the current TX packet status (WAITING_FOR_ACK, COMPLETED, etc.).
   */
  getTxPacketStatus(): MsgProtTxPacketStatus {
    if (this.isSyncing) {
      return MsgProtTxPacketStatus.ERROR;
    }

    return this.txPacket.header.status as MsgProtTxPacketStatus;
  }

  /**
   * Get current app-side session_id used when sending DATA frames.
   */
  getCurrentSessionId(): number {
    return this.currentSessionId >>> 0;
  }

  /**
   * Retrieve the latest RX packet if available. Marks internal RX slot as PROCESSED.
   */
  getRxPacket(): { result: MsgProtError; packet?: MpPacketPayload } {
    if (this.rxPacket.header.status !== MsgProtRxPacketStatus.NEW) {
      this.logger.error("[MP] No new RX packet available");
      return { result: MsgProtError.NO_NEW_PACKET };
    }

    const packet: MpPacketPayload = {
      type: this.rxPacket.payload.type,
      ppi: this.rxPacket.payload.ppi,
      pktPayloadLen: this.rxPacket.payload.pktPayloadLen,
      payload: new Uint8Array(this.rxPacket.payload.payload),
    };

    this.rxPacket.header.status = MsgProtRxPacketStatus.PROCESSED;

    return { result: MsgProtError.NONE, packet };
  }

  /**
   * Get the maximum payload length (bytes) based on negotiated MTU/link layer size.
   */
  getMaxPayloadLength(): number {
    return this.maxPacketPayloadLen;
  }

  /**
   * Returns raw bytes of the last packet written to the link layer.
   * Exposed for debug tooling and tests.
   */
  getLastTxPacketRaw(): Uint8Array {
    return new Uint8Array(this.lastTxPacketRaw);
  }

  /**
   * Returns raw bytes of the last valid packet received from the link layer.
   * Exposed for debug tooling and tests.
   */
  getLastRxPacketRaw(): Uint8Array {
    return new Uint8Array(this.lastRxPacketRaw);
  }

  /**
   * Update negotiated MTU; updates max packet/payload lengths.
   * For BLE, max packet length is MTU - 3 (ATT header).
   */
  setMtu(mtu: number): void {
    if (!Number.isFinite(mtu) || mtu <= ATT_HEADER_LEN) {
      return;
    }

    const maxPacketLength = Math.max(0, mtu - ATT_HEADER_LEN);
    this.setMaxPacketLength(maxPacketLength);
  }

  /**
   * Update max packet length directly (bytes). Overrides MTU-derived length.
   */
  setMaxPacketLength(maxPacketLength: number): void {
    if (!Number.isFinite(maxPacketLength) || maxPacketLength <= 0) {
      return;
    }

    this.maxPacketLength = maxPacketLength;
    this.refreshMaxPayloadLength();
  }

  /**
   * Optional registration point for PPI/type-specific dispatch.
   * Use type="*" to match any type for a given PPI.
   */
  registerRxHandler(ppi: number, type: number | "*", handler: RxHandler): void {
    const key = `${ppi}:${type}`;
    this.rxHandlers.set(key, handler);
  }

  private refreshMaxPayloadLength(): void {
    if (this.maxPacketLength <= 0) {
      return;
    }

    let maxPayload = this.maxPacketLength - HEADER_LEN - MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
    if (maxPayload < 0) {
      maxPayload = 0;
    }
    if (maxPayload > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN) {
      maxPayload = MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN;
    }

    this.maxPacketPayloadLen = maxPayload;
  }

  private async processOutboundPackets(): Promise<MsgProtError> {
    const now = this.getNow();
    const timerExpired = this.deadlineMs > 0 && now > this.deadlineMs;

    // Handle timeout / retries for current TX packet.
    if (
      timerExpired &&
      (this.txPacket.header.status === MsgProtTxPacketStatus.WAITING_FOR_ACK ||
        this.txPacket.header.status === MsgProtTxPacketStatus.ERROR)
    ) {
      if (this.retriesLeft > 0) {
        this.retriesLeft -= 1;
        this.logger.warn(`[MP] Message timeout. Retries left: ${this.retriesLeft}`);
      } else {
        this.logger.error("[MP] Message timeout.");
        this.txPacket.header.status = MsgProtTxPacketStatus.ABANDONED;
        return MsgProtError.TIMEOUT;
      }
    }

    if (timerExpired && this.txPacket.header.status === MsgProtTxPacketStatus.WAITING_FOR_ACK) {
      // Resend the last packet as-is.
      const resendResult = await this.sendPktToLinkLayer(this.lastPacketSent);
      if (resendResult !== MsgProtError.NONE) {
        this.logger.error("[MP] Failed to resend packet over link layer.");
      }
      this.startTimer(now);
      return MsgProtError.NONE;
    }

    if (
      this.txPacket.header.status === MsgProtTxPacketStatus.NEW ||
      (this.txPacket.header.status === MsgProtTxPacketStatus.ERROR && timerExpired)
    ) {
      if (this.txPacket.header.status === MsgProtTxPacketStatus.NEW) {
        this.retriesLeft = this.maxRetries;
      }

      // Send packet over link layer (BLE characteristic).
      const sendResult = await this.sendPktToLinkLayer(this.txPacket);
      if (sendResult === MsgProtError.NONE) {
        this.lastPacketSent = this.clonePacket(this.txPacket);
        this.pendingId = this.nextPacketId;
        this.nextPacketId += 1;
        if (this.sendAckNak) {
          this.txPacket.header.status = MsgProtTxPacketStatus.WAITING_FOR_ACK;
        } else {
          // App-side simple TX mode: send DATA and return immediately to sendable state.
          this.txPacket.header.status = MsgProtTxPacketStatus.ABANDONED;
        }
      } else {
        this.txPacket.header.status = MsgProtTxPacketStatus.ERROR;
        this.logger.error("[MP] Failed to send data over link layer.");
      }

      if (this.sendAckNak) {
        this.startTimer(now);
      }
      return MsgProtError.NONE;
    }

    return MsgProtError.NONE;
  }

  private async sendPktToLinkLayer(packet: MpPacket): Promise<MsgProtError> {
    if (!packet) {
      return MsgProtError.NULL_PTR;
    }

    const payloadLen = packet.payload.pktPayloadLen;
    const packetLength = HEADER_LEN + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + payloadLen;

    if (this.maxPacketLength > 0 && packetLength > this.maxPacketLength) {
      return MsgProtError.BUFFER_OVERFLOW;
    }

    if (payloadLen > this.maxPacketPayloadLen) {
      return MsgProtError.OUT_OF_RANGE;
    }

    const buffer = new Uint8Array(packetLength);
    let offset = 0;

    // Reserve CRC space (2 bytes) at the beginning.
    offset += 2;

    // Header (little-endian, explicit to avoid padding/endianness issues).
    buffer[offset++] = packet.header.pktCounter & 0xff;
    buffer[offset++] = (packet.header.pktCounter >> 8) & 0xff;
    buffer[offset++] = packet.header.sessionId & 0xff;
    buffer[offset++] = (packet.header.sessionId >> 8) & 0xff;
    buffer[offset++] = (packet.header.sessionId >> 16) & 0xff;
    buffer[offset++] = (packet.header.sessionId >> 24) & 0xff;
    buffer[offset++] = packet.header.pktType & 0xff;
    buffer[offset++] = packet.header.status & 0xff;

    // Payload header (little-endian).
    buffer[offset++] = packet.payload.type & 0xff;
    buffer[offset++] = packet.payload.ppi & 0xff;
    buffer[offset++] = payloadLen & 0xff;
    buffer[offset++] = (payloadLen >> 8) & 0xff;

    if (payloadLen > 0) {
      buffer.set(packet.payload.payload, offset);
    }

    // CRC covers everything after the CRC field itself.
    const crcOffset = 2;
    const crc = crc16Update(buffer.subarray(crcOffset, packetLength));
    buffer[0] = crc & 0xff;
    buffer[1] = (crc >> 8) & 0xff;

    this.logger.debug(
      "Writing packet.",
      this.toDebugPacketShape(packet, buffer)
    );

    if (this.transport) {
      const success = await this.transport.sendPacket(buffer);
      if (!success) {
        return MsgProtError.BUSY;
      }

      this.cacheLastTxPacket(buffer);
      return MsgProtError.NONE;
    }

    // BLE writes accept base64 string in the native module.
    const base64Value = Buffer.from(buffer).toString("base64");

    try {
      const { writeCharacteristic } = getBleModule();
      const success = await writeCharacteristic(this.txCharacteristicUUID, base64Value);
      if (!success) {
        return MsgProtError.BUSY;
      }

      this.cacheLastTxPacket(buffer);
    } catch (error) {
      this.logger.error("[MP] Link layer send failed", error);
      return MsgProtError.BUSY;
    }

    return MsgProtError.NONE;
  }

  private handleIncomingHex(hex: string): void {
    if (!hex) {
      return;
    }

    // Incoming data from BLE subscription arrives as hex string.
    // Normalize common user-entered formats: 0x prefix, spaces, angle brackets.
    const cleanedHex = hex.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "").trim();
    if (!cleanedHex || cleanedHex.length % 2 !== 0) {
      this.logger.warn("[MP] Ignoring invalid hex payload from BLE notification.", {
        raw: hex,
        cleanedHex,
      });
      return;
    }

    let raw: Uint8Array;
    try {
      raw = Buffer.from(cleanedHex, "hex");
    } catch (error) {
      this.logger.error("[MP] Failed to parse hex payload", error);
      return;
    }

    if (raw.length === 0) {
      this.logger.warn("[MP] Ignoring empty parsed payload from BLE notification.", {
        raw: hex,
      });
      return;
    }

    this.handleIncomingRaw(raw);
  }

  private handleIncomingRaw(raw: Uint8Array): void {
    this.logger.debug("Received incoming value.", {
      rawLen: raw.length,
      rawHex: Buffer.from(raw).toString("hex"),
      rawBase64: Buffer.from(raw).toString("base64"),
    });

    // Mirror firmware behavior for blank mailbox reads.
    if (raw.length > 0 && raw.every((value) => value === 0)) {
      this.logger.debug("Ignoring all-zero mailbox payload.");
      return;
    }

    let parsed = this.parseIncomingPacket(raw);
    if (parsed.result !== MsgProtError.NONE || !parsed.packet || !parsed.raw) {
      const asciiDecoded = this.tryDecodeAsciiHexFrame(raw);
      if (asciiDecoded) {
        const reparsed = this.parseIncomingPacket(asciiDecoded);
        if (reparsed.result === MsgProtError.NONE && reparsed.packet && reparsed.raw) {
          this.logger.warn("[MP] Incoming payload was ASCII-hex; auto-decoded packet.", {
            originalHex: Buffer.from(raw).toString("hex"),
            decodedHex: Buffer.from(asciiDecoded).toString("hex"),
          });
          parsed = reparsed;
        }
      }
    }

    if (parsed.result !== MsgProtError.NONE || !parsed.packet || !parsed.raw) {
      this.logger.warn("[MP] Dropping invalid MP frame.", {
        parseResult: parsed.result,
        rawLen: raw.length,
        rawHex: Buffer.from(raw).toString("hex"),
      });
      return;
    }

    const packetLength = parsed.raw.length;
    if (this.isSelfTxEcho(parsed.raw)) {
      this.logger.debug("[MP] Dropping self TX echo packet.");
      return;
    }

    // Suppress duplicates (same raw bytes) to avoid re-processing after NAK/ACK retries.
    let isDuplicate = false;

    if (
      this.lastRxPacketValid &&
      packetLength === this.lastRxPacketLen &&
      buffersEqual(parsed.raw, this.lastRxPacketRaw)
    ) {
      if (this.lastRxPacketDeferred) {
        if (this.rxPacket.header.status !== MsgProtRxPacketStatus.PROCESSED) {
          isDuplicate = true;
        }
      } else {
        isDuplicate = true;
      }
    }

    if (isDuplicate) {
      this.logger.debug("Duplicate packet.");
      void this.handleDuplicatePacket(parsed.packet);
      return;
    }

    const rxBusy =
      parsed.packet.header.pktType === this.packetTypes.DATA &&
      this.rxPacket.header.status !== MsgProtRxPacketStatus.PROCESSED;

    this.logger.debug(
      "Parsed incoming value.",
      this.toDebugPacketShape(parsed.packet, parsed.raw)
    );

    this.lastRxPacketRaw = new Uint8Array(parsed.raw);
    this.lastRxPacketLen = packetLength;
    this.lastRxPacketValid = true;

    void this.onLinkLayerPacket(parsed.packet);

    this.lastRxPacketDeferred = rxBusy;
  }

  private tryDecodeAsciiHexFrame(raw: Uint8Array): Uint8Array | null {
    if (raw.length < MIN_PACKET_LEN * 2) {
      return null;
    }

    const isTextLike = raw.every(
      (value) =>
        // Printable ASCII, tabs/newlines, and carriage return.
        (value >= 0x20 && value <= 0x7e) || value === 0x09 || value === 0x0a || value === 0x0d
    );
    if (!isTextLike) {
      return null;
    }

    const text = Buffer.from(raw).toString("utf8");
    const cleanedHex = text.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "").trim();
    if (!cleanedHex || cleanedHex.length % 2 !== 0 || cleanedHex.length < MIN_PACKET_LEN * 2) {
      return null;
    }

    try {
      const decoded = Buffer.from(cleanedHex, "hex");
      if (decoded.length < MIN_PACKET_LEN) {
        return null;
      }
      return decoded;
    } catch {
      return null;
    }
  }

  private async handleDuplicatePacket(packet: MpPacket): Promise<void> {
    if (packet.header.pktType !== this.packetTypes.DATA) {
      return;
    }

    if (packet.header.sessionId === this.currentSessionId) {
      if (this.sendAckNak) {
        await this.sendAckOrNak(packet, true);
      }
      return;
    }

    if (!this.isMaster) {
      await this.sendSyncMismatchPacket(packet);
      return;
    }

    // this.logger.warn("[MP] Not ACKing duplicate stale-session DATA packet.", {
    //   expectedSessionId: this.currentSessionId,
    //   receivedSessionId: packet.header.sessionId,
    // });
  }

  private parseIncomingPacket(raw: Uint8Array): ParsedPacketResult {
    if (raw.length < MIN_PACKET_LEN) {
      return { result: MsgProtError.BUFFER_OVERFLOW };
    }

    const buffer = Buffer.from(raw);
    let offset = 0;

    // Deserialize header (little-endian).
    const pktCrc = buffer.readUInt16LE(offset);
    offset += 2;
    const pktCounter = buffer.readUInt16LE(offset);
    offset += 2;
    const sessionId = buffer.readUInt32LE(offset);
    offset += 4;
    const pktType = buffer.readUInt8(offset);
    offset += 1;
    const status = buffer.readUInt8(offset);
    offset += 1;

    // Deserialize payload header.
    const payloadType = buffer.readUInt8(offset);
    offset += 1;
    const payloadPpi = buffer.readUInt8(offset);
    offset += 1;
    const payloadLen = buffer.readUInt16LE(offset);
    offset += 2;

    if (
      payloadLen > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN ||
      (this.maxPacketPayloadLen > 0 && payloadLen > this.maxPacketPayloadLen)
    ) {
      return { result: MsgProtError.OUT_OF_RANGE };
    }

    const packetLength = MIN_PACKET_LEN + payloadLen;
    if (
      packetLength > raw.length ||
      (this.maxPacketLength > 0 && packetLength > this.maxPacketLength)
    ) {
      return { result: MsgProtError.BUFFER_OVERFLOW };
    }

    // Validate CRC (skip the CRC field itself).
    const calcCrc = crc16Update(raw.subarray(2, packetLength));
    if (calcCrc !== pktCrc) {
      this.logger.warn("[MP] CRC mismatch; dropping packet.", {
        packetHex: Buffer.from(raw.subarray(0, packetLength)).toString("hex"),
        expectedCrc: pktCrc,
        calculatedCrc: calcCrc,
      });
      return { result: MsgProtError.INVALID_ARG };
    }

    const payload = new Uint8Array(payloadLen);
    if (payloadLen > 0) {
      payload.set(raw.subarray(offset, offset + payloadLen));
    }

    const packet: MpPacket = {
      header: {
        pktCrc,
        pktCounter,
        sessionId,
        pktType,
        status,
      },
      payload: {
        type: payloadType,
        ppi: payloadPpi,
        pktPayloadLen: payloadLen,
        payload,
      },
    };

    return { result: MsgProtError.NONE, packet, raw: raw.subarray(0, packetLength) };
  }

  private async onLinkLayerPacket(packet: MpPacket): Promise<void> {
    switch (packet.header.pktType) {
      case this.packetTypes.ACK:
        await this.handleAckPacket(packet);
        break;
      case this.packetTypes.NAK:
        await this.handleNakPacket(packet);
        break;
      case this.packetTypes.DATA:
        await this.handleDataPacket(packet);
        break;
      case this.packetTypes.SYNC_START:
        await this.handleSyncStartPacket(packet);
        break;
      case this.packetTypes.SYNC_MISMATCH:
        await this.handleSyncMismatchPacket();
        break;
      case this.packetTypes.SYNC_ACK:
        await this.handleSyncAckPacket(packet);
        break;
      default: {
        const payloadHex = Buffer.from(packet.payload.payload).toString("hex");
        this.logger.warn("[MP] Ignoring unsupported packet type.", {
          pktType: packet.header.pktType,
          receivedPacket: {
            header: { ...packet.header },
            payload: {
              type: packet.payload.type,
              ppi: packet.payload.ppi,
              pktPayloadLen: packet.payload.pktPayloadLen,
              payloadLen: packet.payload.payload.length,
              payloadHex,
            },
          },
        });
        break;
      }
    }
  }

  private async onSessionMismatch(packet: MpPacket, context: string): Promise<void> {
    if (!this.enableSyncControl) {
      // this.logger.warn("[MP] Session mismatch ignored because sync control is disabled.", {
      //   context,
      //   expectedSessionId: this.currentSessionId,
      //   receivedSessionId: packet.header.sessionId,
      // });
      return;
    }

    if (this.isMaster) {
      this.logger.error("[MP] Session ID mismatch.", {
        context,
        expectedSessionId: this.currentSessionId,
        receivedSessionId: packet.header.sessionId,
      });
      await this.mpSyncStart();
      return;
    }

    await this.sendSyncMismatchPacket(packet);
  }

  private async sendSyncMismatchPacket(packet: MpPacket): Promise<void> {
    const responsePacket = this.clonePacket(packet);
    responsePacket.header.sessionId = this.currentSessionId >>> 0;
    responsePacket.header.pktType = this.packetTypes.SYNC_MISMATCH;
    responsePacket.header.status = MsgProtTxPacketStatus.NEW;
    responsePacket.payload.type = 0;
    responsePacket.payload.ppi = 0;
    responsePacket.payload.pktPayloadLen = 0;
    responsePacket.payload.payload = new Uint8Array(0);

    await this.sendPktToLinkLayer(responsePacket);
    this.logger.info("[MP] Sent SYNC_MISMATCH packet.");
  }

  private async handleAckPacket(packet: MpPacket): Promise<void> {
    if (!this.sendAckNak) {
      return;
    }

    if (packet.header.sessionId !== this.currentSessionId) {
      await this.onSessionMismatch(packet, "ACK");
      return;
    }

    const txStatus = this.txPacket.header.status;
    const waitingForAck = txStatus === MsgProtTxPacketStatus.WAITING_FOR_ACK;
    const abandoned = txStatus === MsgProtTxPacketStatus.ABANDONED;
    const counterMatches = packet.header.pktCounter === this.pendingId;
    const canRelaxedComplete = this.relaxedAckMatching && (waitingForAck || abandoned);

    if ((waitingForAck && counterMatches) || canRelaxedComplete) {
      if (canRelaxedComplete && !counterMatches) {
        this.logger.warn("[MP] Relaxed ACK accept: counter mismatch.", {
          pendingCounter: this.pendingId,
          receivedCounter: packet.header.pktCounter,
          txStatus,
        });
      }
      if (canRelaxedComplete && abandoned) {
        this.logger.warn("[MP] Relaxed ACK accept: recovering ABANDONED TX packet.", {
          pendingCounter: this.pendingId,
          receivedCounter: packet.header.pktCounter,
        });
      }
      this.txPacket.header.status = MsgProtTxPacketStatus.COMPLETED;
      return;
    }

    this.logger.warn("[MP] ACK ignored: counter/status mismatch.", {
      txStatus: this.txPacket.header.status,
      pendingCounter: this.pendingId,
      receivedCounter: packet.header.pktCounter,
      sessionId: packet.header.sessionId,
    });
  }

  private async handleNakPacket(packet: MpPacket): Promise<void> {
    if (!this.sendAckNak) {
      return;
    }

    if (packet.header.sessionId !== this.currentSessionId) {
      await this.onSessionMismatch(packet, "NAK");
      return;
    }

    const result = await this.sendPktToLinkLayer(this.lastPacketSent);
    if (result === MsgProtError.NONE) {
      this.startTimer(this.getNow());
    }
  }

  private async handleDataPacket(packet: MpPacket): Promise<void> {
    if (packet.header.sessionId !== this.currentSessionId) {
      await this.onSessionMismatch(packet, "DATA");
      return;
    }

    const rxBusy = this.rxPacket.header.status !== MsgProtRxPacketStatus.PROCESSED;
    const delivered = this.deliverPayload(packet);
    if (delivered === MsgProtError.NONE && !rxBusy && this.sendAckNak) {
      await this.sendAckOrNak(packet, true);
    }
  }

  private async handleSyncStartPacket(packet: MpPacket): Promise<void> {
    if (!this.enableSyncControl) {
      // this.logger.warn("[MP] Ignoring SYNC_START because sync control is disabled.");
      return;
    }

    if (this.isMaster) {
      this.logger.error("[MP] Received SYNC_START from another master instance.");
      return;
    }

    this.resetMpState();
    this.currentSessionId = packet.header.sessionId >>> 0;

    const responsePacket = this.clonePacket(packet);
    responsePacket.header.pktType = this.packetTypes.SYNC_ACK;
    responsePacket.header.status = MsgProtTxPacketStatus.NEW;
    responsePacket.header.sessionId = this.currentSessionId;
    responsePacket.payload.type = 0;
    responsePacket.payload.ppi = 0;
    responsePacket.payload.pktPayloadLen = 0;
    responsePacket.payload.payload = new Uint8Array(0);

    await this.sendPktToLinkLayer(responsePacket);
    this.logger.info("[MP] Sent SYNC_ACK packet.");
  }

  private async handleSyncMismatchPacket(): Promise<void> {
    if (!this.enableSyncControl) {
      // this.logger.warn("[MP] Ignoring SYNC_MISMATCH because sync control is disabled.");
      return;
    }

    if (this.isMaster) {
      await this.mpSyncStart();
      return;
    }

    this.logger.error("[MP] Slave instance received SYNC_MISMATCH.");
  }

  private async handleSyncAckPacket(packet: MpPacket): Promise<void> {
    if (!this.enableSyncControl) {
      // this.logger.warn("[MP] Ignoring SYNC_ACK because sync control is disabled.");
      return;
    }

    if (!this.isMaster) {
      this.logger.error("[MP] Slave instance received SYNC_ACK.");
      return;
    }

    if (packet.header.sessionId !== this.currentSessionId) {
      this.logger.error("[MP] Session ID mismatch for SYNC_ACK.", {
        expectedSessionId: this.currentSessionId,
        receivedSessionId: packet.header.sessionId,
      });
      await this.mpSyncStart();
      return;
    }

    if (this.isSyncing) {
      this.isSyncing = false;
      this.logger.info("[MP] Sync process completed.", {
        sessionId: packet.header.sessionId >>> 0,
      });
      return;
    }

    this.logger.error("[MP] Received unexpected SYNC_ACK while not syncing.");
    await this.mpSyncStart();
  }

  private deliverPayload(packet: MpPacket): MsgProtError {
    if (this.rxPacket.header.status === MsgProtRxPacketStatus.PROCESSED) {
      this.rxPacket.header = { ...packet.header };
      this.rxPacket.payload = {
        type: packet.payload.type,
        ppi: packet.payload.ppi,
        pktPayloadLen: packet.payload.pktPayloadLen,
        payload: new Uint8Array(packet.payload.payload),
      };
      this.rxPacket.header.status = MsgProtRxPacketStatus.NEW;

      if (this.onRxPacket) {
        this.onRxPacket(this.rxPacket.payload);
      }

      this.dispatchRxHandlers(this.rxPacket.payload);
      // Optional auto-consume to keep RX slot free when using callbacks.
      if (this.autoConsumeRx) {
        this.rxPacket.header.status = MsgProtRxPacketStatus.PROCESSED;
      }
      return MsgProtError.NONE;
    }

    if (this.sendAckNak) {
      void this.sendAckOrNak(packet, false);
    }
    return MsgProtError.BUSY;
  }

  private dispatchRxHandlers(payload: MpPacketPayload): void {
    const directKey = `${payload.ppi}:${payload.type}`;
    const wildcardKey = `${payload.ppi}:*`;

    const directHandler = this.rxHandlers.get(directKey);
    if (directHandler) {
      directHandler(payload);
      return;
    }

    const wildcardHandler = this.rxHandlers.get(wildcardKey);
    if (wildcardHandler) {
      wildcardHandler(payload);
    }
  }

  private async sendAckOrNak(packet: MpPacket, isAck: boolean): Promise<void> {
    const ackPacket = this.clonePacket(packet);
    ackPacket.header.pktType = isAck ? this.packetTypes.ACK : this.packetTypes.NAK;
    ackPacket.payload.pktPayloadLen = 0;
    ackPacket.payload.payload = new Uint8Array(0);

    await this.sendPktToLinkLayer(ackPacket);
  }

  private getPacketTypeLabel(pktType: number): string {
    if (pktType === this.packetTypes.DATA) return "DATA";
    if (pktType === this.packetTypes.ACK) return "ACK";
    if (pktType === this.packetTypes.NAK) return "NAK";
    return "UNKNOWN";
  }

  private generateSessionIdForMaster(): boolean {
    if (!this.isMaster) {
      return false;
    }

    const providedSessionId = this.sessionIdProvider ? this.sessionIdProvider() : null;
    const sessionId = providedSessionId ?? generateSessionId();
    if (sessionId === null || !Number.isFinite(sessionId)) {
      this.logger.error("[MP] RNG failure; cannot generate session_id.");
      this.currentSessionId = 0;
      return false;
    }

    this.currentSessionId = sessionId >>> 0;
    this.hasGeneratedSessionId = true;
    return true;
  }

  private toDebugPacketShape(packet: MpPacket, raw: Uint8Array): Record<string, unknown> {
    return {
      frameLength: raw.length,
      rawHex: Buffer.from(raw).toString("hex"),
      rawBase64: Buffer.from(raw).toString("base64"),
      header: {
        pktCrc: packet.header.pktCrc,
        pktCounter: packet.header.pktCounter,
        sessionId: packet.header.sessionId >>> 0,
        pktType: packet.header.pktType,
        pktTypeLabel: this.getPacketTypeLabel(packet.header.pktType),
        status: packet.header.status,
      },
      payload: {
        type: packet.payload.type,
        ppi: packet.payload.ppi,
        pktPayloadLen: packet.payload.pktPayloadLen,
        payloadLen: packet.payload.payload.length,
        payloadHex: Buffer.from(packet.payload.payload).toString("hex"),
        payloadBase64: Buffer.from(packet.payload.payload).toString("base64"),
      },
    };
  }

  private startTimer(now: number): void {
    this.deadlineMs = now + this.ackTimeoutMs;
  }

  private async mpSyncStart(): Promise<void> {
    if (!this.isMaster) {
      return;
    }

    if (!this.enableSyncControl) {
      return;
    }

    const currentMs = this.getNow();
    const timeSinceLastSync = currentMs - this.lastResyncTimeMs;
    const isFirstSyncAttempt = !this.hasAttemptedSync;

    if (!isFirstSyncAttempt && timeSinceLastSync <= MAX_TIME_BEFORE_SYNC_RETRY_MS) {
      return;
    }

    this.resetMpState();

    const hasSessionId = this.generateSessionIdForMaster();
    if (!hasSessionId) {
      return;
    }

    this.lastResyncTimeMs = currentMs;
    this.hasAttemptedSync = true;
    this.logger.info("[MP] Generated new session ID.", {
      sessionId: this.currentSessionId >>> 0,
    });

    const packet = this.createEmptyPacket();
    packet.header.pktCounter = this.nextPacketId;
    this.nextPacketId += 1;
    packet.header.sessionId = this.currentSessionId;
    packet.header.pktType = this.packetTypes.SYNC_START;
    packet.header.status = MsgProtTxPacketStatus.NEW;
    packet.payload.type = 0;
    packet.payload.ppi = 0;
    packet.payload.pktPayloadLen = 0;
    packet.payload.payload = new Uint8Array(0);

    // Mirror firmware behavior: once SYNC_START is initiated, report syncing immediately.
    this.isSyncing = true;
    const result = await this.sendPktToLinkLayer(packet);
    if (result === MsgProtError.NONE) {
      this.logger.info("[MP] Sent SYNC_START packet.");
    }
  }

  private initializeState(): void {
    this.rxPacket = this.createEmptyPacket();
    this.rxPacket.header.status = MsgProtRxPacketStatus.PROCESSED;

    this.txPacket = this.createEmptyPacket();
    this.txPacket.header.status = MsgProtTxPacketStatus.ABANDONED;

    this.lastPacketSent = this.createEmptyPacket();

    this.lastRxPacketRaw = new Uint8Array(0);
    this.lastRxPacketLen = 0;
    this.lastRxPacketValid = false;
    this.lastRxPacketDeferred = false;
    this.lastTxPacketRaw = new Uint8Array(0);
    this.lastTxPacketLen = 0;
    this.lastTxPacketValid = false;

    this.nextPacketId = 0;
    this.pendingId = 0;
    this.retriesLeft = 0;
    this.deadlineMs = 0;
    this.currentSessionId = 0;
    this.isSyncing = false;
    this.hasAttemptedSync = false;
    this.lastResyncTimeMs = 0;
    this.hasGeneratedSessionId = false;
  }

  private resetMpState(): void {
    this.rxPacket = this.createEmptyPacket();
    this.rxPacket.header.status = MsgProtRxPacketStatus.PROCESSED;

    this.txPacket = this.createEmptyPacket();
    this.txPacket.header.status = MsgProtTxPacketStatus.ABANDONED;

    this.lastPacketSent = this.createEmptyPacket();

    this.lastRxPacketRaw = new Uint8Array(0);
    this.lastRxPacketLen = 0;
    this.lastRxPacketValid = false;
    this.lastRxPacketDeferred = false;
    this.lastTxPacketRaw = new Uint8Array(0);
    this.lastTxPacketLen = 0;
    this.lastTxPacketValid = false;

    this.nextPacketId = 1;
    this.pendingId = 0;
    this.retriesLeft = this.maxRetries;
    this.deadlineMs = 0;
    this.currentSessionId = 0;
    this.hasGeneratedSessionId = false;
  }

  private createEmptyPacket(): MpPacket {
    return {
      header: {
        pktCrc: 0,
        pktCounter: 0,
        sessionId: 0,
        pktType: this.packetTypes.DATA,
        status: MsgProtTxPacketStatus.NONE,
      },
      payload: {
        type: 0,
        ppi: 0,
        pktPayloadLen: 0,
        payload: new Uint8Array(0),
      },
    };
  }

  private clonePacket(packet: MpPacket): MpPacket {
    return {
      header: { ...packet.header },
      payload: {
        type: packet.payload.type,
        ppi: packet.payload.ppi,
        pktPayloadLen: packet.payload.pktPayloadLen,
        payload: new Uint8Array(packet.payload.payload),
      },
    };
  }

  private cacheLastTxPacket(raw: Uint8Array): void {
    this.lastTxPacketRaw = new Uint8Array(raw);
    this.lastTxPacketLen = raw.length;
    this.lastTxPacketValid = true;
  }

  private isSelfTxEcho(raw: Uint8Array): boolean {
    if (!this.lastTxPacketValid) {
      return false;
    }

    if (this.lastTxPacketLen === raw.length && buffersEqual(raw, this.lastTxPacketRaw)) {
      return true;
    }

    this.lastTxPacketValid = false;
    return false;
  }

  private getNow(): number {
    return this.nowProvider ? this.nowProvider() : Date.now();
  }
}

function buffersEqual(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) {
    return false;
  }

  for (let i = 0; i < a.length; i += 1) {
    if (a[i] !== b[i]) {
      return false;
    }
  }

  return true;
}

function crc16Update(data: Uint8Array, seed?: number): number {
  let crc = seed ?? 0xffff;

  for (let i = 0; i < data.length; i += 1) {
    crc = ((crc >> 8) & 0xff) | ((crc << 8) & 0xffff);
    crc ^= data[i] & 0xff;
    crc ^= (crc & 0xff) >> 4;
    crc ^= (crc << 8) << 4;
    crc ^= ((crc & 0xff) << 4) << 1;
    crc &= 0xffff;
  }

  return crc & 0xffff;
}

function generateSessionId(): number | null {
  try {
    const cryptoObj = (globalThis as unknown as { crypto?: { getRandomValues?: (arr: Uint32Array) => void } }).crypto;
    if (cryptoObj && typeof cryptoObj.getRandomValues === "function") {
      const buffer = new Uint32Array(1);
      cryptoObj.getRandomValues(buffer);
      return buffer[0];
    }
  } catch {
    // Fall through to Math.random below.
  }

  // Fallback for environments without secure RNG.
  return Math.floor(Math.random() * 0xffffffff);
}
