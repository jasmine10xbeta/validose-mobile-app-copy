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

async function flushAsyncWork() {
  await new Promise((resolve) => setTimeout(resolve, 0));
}

async function startLinked(
  mpA: BleMessageProtocol,
  mpB: BleMessageProtocol,
  linkA: MockLinkEndpoint,
  linkB: MockLinkEndpoint
) {
  // Start both endpoints and flush any queued link-layer bytes.
  await mpB.start();
  await mpA.start();
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

function getPacketType(raw?: Uint8Array): number | null {
  if (!raw || raw.length < 9) {
    return null;
  }
  return raw[8];
}

function getSessionId(raw?: Uint8Array): number | null {
  if (!raw || raw.length < 8) {
    return null;
  }
  return (
    (raw[4] ?? 0) |
    ((raw[5] ?? 0) << 8) |
    ((raw[6] ?? 0) << 16) |
    ((raw[7] ?? 0) << 24)
  ) >>> 0;
}

describe("BleMessageProtocol", () => {
  test("send rejects when TX busy", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
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

  test("send rejects while TX packet is still NEW (queued but not processed)", async () => {
    const now = { value: 0 };
    const transport = new SinkTransport();

    const mp = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x5a5a,
    });

    await mp.start();

    const payload = new Uint8Array([0xa5]);
    expect(
      mp.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.NONE);

    // Second send before process() should be rejected (firmware parity).
    expect(
      mp.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.BUSY);
  });

  test("send is allowed before control-plane initialization", async () => {
    const now = { value: 0 };
    const transport = new SinkTransport();

    const mp = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x1234,
    });

    await mp.start();

    expect(mp.getTxPacketStatus()).toBe(MsgProtTxPacketStatus.ABANDONED);

    const payload = new Uint8Array([0xa5]);
    expect(
      mp.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.NONE);
  });

  test("master generates session_id without initiating SYNC when sync control is disabled", async () => {
    const now = { value: 0 };
    const { a } = createLinkedEndpoints(96);

    const mp = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      enableSyncControl: false,
      sendAckNak: false,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0xa1b2c3d4,
    });

    await mp.start();
    expect(mp.getCurrentSessionId()).toBe(0xa1b2c3d4);

    const mismatchData = buildWirePacketFull({
      header: {
        pktCounter: 7,
        sessionId: 0x2222,
        pktType: DEFAULT_PACKET_TYPES.DATA,
        status: MsgProtTxPacketStatus.NEW,
      },
      payload: {
        type: PpiType.PUSH,
        ppi: 0,
        payload: new Uint8Array([0xde]),
      },
    });

    a.injectRaw(mismatchData);
    a.flush();

    expect(a.sendCount).toBe(0);
    expect(getPacketType(a.lastTx)).toBeNull();
    expect(mp.getRxPacketStatus()).toBe(MsgProtRxPacketStatus.PROCESSED);
  });

  test("endpoint configured with sendAckNak=false does not transmit ACK/NAK", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      sendAckNak: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x9001,
    });

    await startLinked(mpA, mpB, a, b);

    const firstPayload = new Uint8Array([0x01]);
    expect(
      mpA.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: firstPayload.length,
        payload: firstPayload,
      })
    ).toBe(MsgProtError.NONE);

    await mpA.process();
    b.flush();

    // First DATA should not be ACKed when sendAckNak is disabled.
    expect(b.sendCount).toBe(0);
    expect(mpA.getTxPacketStatus()).toBe(MsgProtTxPacketStatus.WAITING_FOR_ACK);

    const secondPayload = new Uint8Array([0x02]);
    expect(
      mpA.send({
        type: PpiType.PUSH,
        ppi: 0,
        pktPayloadLen: secondPayload.length,
        payload: secondPayload,
      })
    ).toBe(MsgProtError.BUSY);

    // Force resend from timeout path while RX slot is still busy to validate no NAK is emitted either.
    now.value += 1001;
    await mpA.process();
    b.flush();

    expect(b.sendCount).toBe(0);
  });

  test("send rejects payload too large", () => {
    const maxPacketLength = HEADER_LEN + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + 2;
    const { a } = createLinkedEndpoints(maxPacketLength);

    const mp = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      isMaster: true,
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
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
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

  test("autoConsumeRx keeps RX slot free when RX callback throws", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: true,
      isMaster: false,
      nowProvider: () => now.value,
      onRxPacket: () => {
        throw new Error("rx callback failure");
      },
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x2222,
    });

    await startLinked(mpA, mpB, a, b);

    const payload = new Uint8Array([0xab]);
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

    expect(getPacketType(b.lastTx)).toBe(DEFAULT_PACKET_TYPES.ACK);
    expect(mpA.getTxPacketStatus()).toBe(MsgProtTxPacketStatus.COMPLETED);
    expect(mpB.getRxPacketStatus()).toBe(MsgProtRxPacketStatus.PROCESSED);
  });

  test("onRxDataAcked fires after inbound DATA is ACKed", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);
    const onRxDataAcked = jest.fn();

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: true,
      isMaster: false,
      nowProvider: () => now.value,
      onRxDataAcked,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x9999,
    });

    await startLinked(mpA, mpB, a, b);

    const payload = new Uint8Array([0xab, 0xcd]);
    expect(
      mpA.send({
        type: PpiType.PUSH,
        ppi: 14,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.NONE);

    await mpA.process();
    b.flush();
    await flushAsyncWork();

    expect(onRxDataAcked).toHaveBeenCalledTimes(1);
    expect(onRxDataAcked).toHaveBeenCalledWith(
      expect.objectContaining({
        pktCounter: expect.any(Number),
        sessionId: expect.any(Number),
        payload: expect.objectContaining({
          ppi: 14,
          type: PpiType.PUSH,
          pktPayloadLen: 2,
          payload: new Uint8Array([0xab, 0xcd]),
        }),
      })
    );
  });

  test("onRxDataAcked does not fire if ACK send fails", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);
    const onRxDataAcked = jest.fn();

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: true,
      isMaster: false,
      nowProvider: () => now.value,
      onRxDataAcked,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0xaaaa,
    });

    await startLinked(mpA, mpB, a, b);

    // Occupy A's mailbox so B cannot enqueue ACK back to A.
    a.queue = new Uint8Array([0xff]);

    const payload = new Uint8Array([0x11]);
    expect(
      mpA.send({
        type: PpiType.PUSH,
        ppi: 14,
        pktPayloadLen: payload.length,
        payload,
      })
    ).toBe(MsgProtError.NONE);

    await mpA.process();
    b.flush();
    await flushAsyncWork();

    expect(onRxDataAcked).not.toHaveBeenCalled();
  });

  test("NAK triggers immediate resend", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
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
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
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
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
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

  test("duplicate active-session DATA is dropped but re-ACKed", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x5555,
    });

    await startLinked(mpA, mpB, a, b);

    const raw = buildWirePacketFull({
      header: {
        pktCounter: 1,
        sessionId: 0,
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

    // Clear peer queue in this single-slot transport so duplicate ACK can be sent.
    a.queue = null;

    b.injectRaw(raw);
    b.flush();

    expect(b.sendCount).toBe(sendAfterFirst + 1);
    expect(getPacketType(b.lastTx)).toBe(DEFAULT_PACKET_TYPES.ACK);
  });

  test("duplicate stale-session DATA on slave re-sends SYNC_MISMATCH", async () => {
    const now = { value: 0 };
    const { a, b } = createLinkedEndpoints(96);

    const mpB = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: b,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: false,
      nowProvider: () => now.value,
    });

    const mpA = new BleMessageProtocol({
      txCharacteristicUUID: "tx",
      rxCharacteristicUUID: "rx",
      transport: a,
      processIntervalMs: 0,
      autoConsumeRx: false,
      isMaster: true,
      nowProvider: () => now.value,
      sessionIdProvider: () => 0x7777,
    });

    await startLinked(mpA, mpB, a, b);

    const staleRaw = buildWirePacketFull({
      header: {
        pktCounter: 7,
        sessionId: 0x2222, // stale/mismatched session for slave.
        pktType: DEFAULT_PACKET_TYPES.DATA,
        status: MsgProtTxPacketStatus.NEW,
      },
      payload: {
        type: PpiType.PUSH,
        ppi: 0,
        payload: new Uint8Array([0xa5]),
      },
    });

    b.injectRaw(staleRaw);
    b.flush();
    expect(getPacketType(b.lastTx)).toBe(DEFAULT_PACKET_TYPES.SYNC_MISMATCH);
    const firstSessionId = getSessionId(b.lastTx);
    expect(firstSessionId).toBe(0);

    // Clear peer queue to avoid mailbox backpressure in this single-slot transport.
    a.queue = null;

    const sendAfterFirst = b.sendCount;
    b.injectRaw(staleRaw);
    b.flush();

    expect(b.sendCount).toBe(sendAfterFirst + 1);
    expect(getPacketType(b.lastTx)).toBe(DEFAULT_PACKET_TYPES.SYNC_MISMATCH);
    expect(getSessionId(b.lastTx)).toBe(firstSessionId);
  });

});
