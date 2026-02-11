import {
  BleMessageProtocol,
  DEFAULT_PACKET_TYPES,
  MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE,
  MsgProtError,
  MsgProtRxPacketStatus,
  MsgProtTxPacketStatus,
  MessageProtocolTransport,
} from "../messageProtocol";
import { PpiType } from "../messageProtocolPpi";

// This test suite mirrors the firmware MP tests using a minimal in-memory link layer.
// Each endpoint represents one side of a wire with a single-slot RX mailbox.

type RawPacket = {
  header: {
    pktCounter: number;
    sessionId: number;
    pktType: number;
    status: number;
  };
  payload: {
    type: number;
    ppi: number;
    payload: Uint8Array;
  };
};

const HEADER_LEN = 2 + 2 + 4 + 1 + 1;
const MIN_PACKET_LEN = HEADER_LEN + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;

class MockLinkEndpoint implements MessageProtocolTransport {
  peer?: MockLinkEndpoint;
  maxPacketLength: number;
  sendCount = 0;
  lastTx?: Uint8Array;
  queue: Uint8Array | null = null;
  private onReceive?: (bytes: Uint8Array) => void;

  constructor(maxPacketLength: number) {
    this.maxPacketLength = maxPacketLength;
  }

  async sendPacket(bytes: Uint8Array): Promise<boolean> {
    // Enforce MTU and single-slot mailbox backpressure.
    if (!this.peer) {
      return false;
    }
    if (bytes.length > this.maxPacketLength) {
      return false;
    }
    if (this.peer.queue) {
      return false;
    }

    this.sendCount += 1;
    this.lastTx = bytes.slice();
    this.peer.queue = bytes.slice();
    return true;
  }

  async subscribe(handler: (bytes: Uint8Array) => void): Promise<() => void> {
    // Register an RX handler (like BLE notification subscription).
    this.onReceive = handler;
    return () => {
      if (this.onReceive === handler) {
        this.onReceive = undefined;
      }
    };
  }

  flush(): void {
    // Deliver queued bytes to the registered handler (simulates a read/notify).
    if (!this.queue) {
      return;
    }
    if (!this.onReceive) {
      return;
    }
    const data = this.queue;
    this.queue = null;
    this.onReceive(data);
  }

  injectRaw(bytes: Uint8Array): void {
    // Directly inject raw bytes into this endpoint's RX mailbox.
    if (this.queue) {
      throw new Error("Queue already occupied");
    }
    this.queue = bytes.slice();
  }
}

class SinkTransport implements MessageProtocolTransport {
  async sendPacket(_bytes: Uint8Array): Promise<boolean> {
    // Accept sends but never deliver anything back (simulates no ACK).
    return true;
  }

  async subscribe(_handler: (bytes: Uint8Array) => void): Promise<() => void> {
    return () => undefined;
  }
}

function createLinkedEndpoints(maxPacketLength = 64) {
  const a = new MockLinkEndpoint(maxPacketLength);
  const b = new MockLinkEndpoint(maxPacketLength);
  a.peer = b;
  b.peer = a;
  return { a, b };
}

async function startLinked(
  mpA: BleMessageProtocol,
  mpB: BleMessageProtocol,
  linkA: MockLinkEndpoint,
  linkB: MockLinkEndpoint
) {
  // Start slave first so it can receive SYNC_START from master.
  await mpB.start();
  await mpA.start();
  // Deliver SYNC_START -> SYNC_ACK handshake.
  linkB.flush();
  linkA.flush();
}

function buildWirePacketFull(packet: RawPacket): Uint8Array {
  // Build a raw on-wire MP packet (with CRC) for injection tests.
  const payloadLen = packet.payload.payload.length;
  const packetLength = MIN_PACKET_LEN + payloadLen;
  const buffer = new Uint8Array(packetLength);

  let offset = 2;
  buffer[offset++] = packet.header.pktCounter & 0xff;
  buffer[offset++] = (packet.header.pktCounter >> 8) & 0xff;
  buffer[offset++] = packet.header.sessionId & 0xff;
  buffer[offset++] = (packet.header.sessionId >> 8) & 0xff;
  buffer[offset++] = (packet.header.sessionId >> 16) & 0xff;
  buffer[offset++] = (packet.header.sessionId >> 24) & 0xff;
  buffer[offset++] = packet.header.pktType & 0xff;
  buffer[offset++] = packet.header.status & 0xff;

  buffer[offset++] = packet.payload.type & 0xff;
  buffer[offset++] = packet.payload.ppi & 0xff;
  buffer[offset++] = payloadLen & 0xff;
  buffer[offset++] = (payloadLen >> 8) & 0xff;

  if (payloadLen > 0) {
    buffer.set(packet.payload.payload, offset);
  }

  // CRC covers everything after the CRC field itself.
  const crc = crc16Update(buffer.subarray(2, packetLength));
  buffer[0] = crc & 0xff;
  buffer[1] = (crc >> 8) & 0xff;

  return buffer;
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

describe("BleMessageProtocol", () => {
  test("send rejects when TX busy", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      isMaster: false,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      isMaster: true,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x1111,
    });

    await startLinked(mpA, mpB, a, b);

    const payload = new Uint8Array([0xaa]);
    expect(
      mpA.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.NONE);

    await mpA.process();
    b.flush();

    // ACK is queued for A but not delivered yet.
    expect(mpA.getTxPacketStatus()).toBe(MsgProtTxPacketStatus.WAITING_FOR_ACK);

    const second = mpA.send({
      type: PpiType.PUSH,
      ppi: 0,
      pktPayloadLen: payload.length,
      payload,
    });
    expect(second).toBe(MsgProtError.BUSY);
  });

  test("send rejects payload too large", () => {
    const maxPacketLength = HEADER_LEN + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + 2;
    const { a } = createLinkedEndpoints(maxPacketLength);

    const mp = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      isMaster: false,
      processIntervalMs: 0,
      maxPacketLength,
    });

    const payload = new Uint8Array([0x10, 0x11, 0x12]);
    const result = mp.send({
      type: PpiType.PUSH,
      ppi: 0,
      pktPayloadLen: payload.length,
      payload,
    });

    expect(result).toBe(MsgProtError.PAYLOAD_TOO_BIG);
  });

  test("zero-length payload transmits", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      isMaster: false,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      isMaster: true,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x2222,
    });

    await startLinked(mpA, mpB, a, b);

    const payload = new Uint8Array();
    expect(
      mpA.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.NONE);

    await mpA.process();
    b.flush();
    a.flush();

    expect(mpA.getTxPacketStatus()).toBe(MsgProtTxPacketStatus.COMPLETED);
    expect(mpB.getRxPacketStatus()).toBe(MsgProtRxPacketStatus.NEW);

    const { result, packet } = mpB.getRxPacket();
    expect(result).toBe(MsgProtError.NONE);
    expect(packet?.pktPayloadLen).toBe(0);
  });

  test("NAK triggers immediate resend", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      isMaster: false,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      isMaster: true,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x3333,
    });

    await startLinked(mpA, mpB, a, b);

    const payload1 = new Uint8Array([0x01]);
    mpA.send({ type: PpiType.PUSH, ppi: 0, pktPayloadLen: payload1.length, payload: payload1 });
    await mpA.process();
    b.flush();
    a.flush();

    expect(mpB.getRxPacketStatus()).toBe(MsgProtRxPacketStatus.NEW);

    const payload2 = new Uint8Array([0x02]);
    mpA.send({ type: PpiType.PUSH, ppi: 0, pktPayloadLen: payload2.length, payload: payload2 });
    await mpA.process();
    b.flush(); // delivers data -> NAK because RX still NEW
    a.flush(); // delivers NAK -> triggers resend (queued to B)

    expect(a.sendCount).toBeGreaterThanOrEqual(3);
  });

  test("timeout after retries abandons packet", async () => {
    const now = { value: 0 };
    const transport = new SinkTransport();

    const mp = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport,
      isMaster: false,
      processIntervalMs: 0,
      autoConsumeRx: false,
      ackTimeoutMs: 10,
      maxRetries: 2,
      nowProvider: () => now.value,
    });

    const payload = new Uint8Array([0x10]);
    mp.send({ type: PpiType.PUSH, ppi: 0, pktPayloadLen: payload.length, payload });

    await mp.process();

    let result = MsgProtError.NONE;
    for (let i = 0; i < 3; i += 1) {
      now.value += 11;
      result = await mp.process();
    }

    expect(result).toBe(MsgProtError.TIMEOUT);
    expect(mp.getTxPacketStatus()).toBe(MsgProtTxPacketStatus.ABANDONED);
  });

  test("corrupted frame is ignored", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      isMaster: false,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      isMaster: true,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x4444,
    });

    await startLinked(mpA, mpB, a, b);

    const raw = buildWirePacketFull({
      header: {
        pktCounter: 1,
        sessionId: 0x4444,
        pktType: DEFAULT_PACKET_TYPES.DATA,
        status: MsgProtTxPacketStatus.NEW,
      },
      payload: {
        type: PpiType.PUSH,
        ppi: 0,
        payload: new Uint8Array([0x55]),
      },
    });

    raw[0] ^= 0xff; // corrupt CRC
    b.injectRaw(raw);
    b.flush();

    expect(mpB.getRxPacketStatus()).toBe(MsgProtRxPacketStatus.PROCESSED);
  });

  test("duplicate frame is dropped", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      isMaster: false,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      isMaster: true,
      processIntervalMs: 0,
      autoConsumeRx: false,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x5555,
    });

    await startLinked(mpA, mpB, a, b);

    const raw = buildWirePacketFull({
      header: {
        pktCounter: 1,
        sessionId: 0x5555,
        pktType: DEFAULT_PACKET_TYPES.DATA,
        status: MsgProtTxPacketStatus.NEW,
      },
      payload: {
        type: PpiType.PUSH,
        ppi: 0,
        payload: new Uint8Array([0xaa, 0xbb]),
      },
    });

    b.injectRaw(raw);
    b.flush();
    const sendAfterFirst = b.sendCount;

    b.injectRaw(raw);
    b.flush();

    expect(b.sendCount).toBe(sendAfterFirst);
  });
});
