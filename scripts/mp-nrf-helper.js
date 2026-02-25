#!/usr/bin/env node
"use strict";

const PACKET_TYPES = {
  DATA: 0,
  ACK: 1,
  NAK: 2,
  SYNC_START: 3,
  SYNC_ACK: 4,
};

const PPI_TYPE_PUSH = 2;

const PPIS = {
  dose: 9,
  dock_batt: 16,
  ring_batt: 17,
  dock_dbg: 18,
  ring_dbg: 19,
};

function crc16Update(data, seed = 0xffff) {
  let crc = seed;
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

function cleanHex(value) {
  return String(value || "")
    .replace(/0x/gi, "")
    .replace(/\s+/g, "")
    .trim()
    .toLowerCase();
}

function hexToBytes(hex) {
  const cleaned = cleanHex(hex);
  if (!cleaned || cleaned.length % 2 !== 0 || !/^[0-9a-f]+$/.test(cleaned)) {
    throw new Error(`Invalid hex input: ${hex}`);
  }
  return Buffer.from(cleaned, "hex");
}

function toHex(bytes) {
  return Buffer.from(bytes).toString("hex");
}

function toHexSpaced(bytes) {
  return toHex(bytes).replace(/(..)/g, "$1 ").trim();
}

function toBase64(bytes) {
  return Buffer.from(bytes).toString("base64");
}

function parseIntArg(value, fallback) {
  if (value === undefined || value === null || value === "") return fallback;
  const normalized = String(value).toLowerCase();
  if (normalized.startsWith("0x")) return parseInt(normalized, 16);
  return parseInt(normalized, 10);
}

function parseFlags(args) {
  const flags = {};
  for (let i = 0; i < args.length; i += 1) {
    const token = args[i];
    if (!token.startsWith("--")) continue;
    const key = token.slice(2);
    const next = args[i + 1];
    if (!next || next.startsWith("--")) {
      flags[key] = "true";
      continue;
    }
    flags[key] = next;
    i += 1;
  }
  return flags;
}

function buildFrame({
  counter,
  sessionId,
  pktType,
  status = 1,
  payloadType = 0,
  ppi = 0,
  payload = Buffer.alloc(0),
}) {
  const payloadLen = payload.length;
  const body = Buffer.alloc(12 + payloadLen);
  let o = 0;

  body.writeUInt16LE(counter & 0xffff, o);
  o += 2;
  body.writeUInt32LE(sessionId >>> 0, o);
  o += 4;
  body.writeUInt8(pktType & 0xff, o);
  o += 1;
  body.writeUInt8(status & 0xff, o);
  o += 1;
  body.writeUInt8(payloadType & 0xff, o);
  o += 1;
  body.writeUInt8(ppi & 0xff, o);
  o += 1;
  body.writeUInt16LE(payloadLen, o);
  o += 2;
  if (payloadLen > 0) {
    payload.copy(body, o);
  }

  const crc = crc16Update(body);
  const frame = Buffer.alloc(2 + body.length);
  frame.writeUInt16LE(crc, 0);
  body.copy(frame, 2);
  return frame;
}

function parseFrame(hex) {
  const bytes = hexToBytes(hex);
  if (bytes.length < 14) {
    throw new Error(`Frame too short: ${bytes.length} bytes`);
  }

  const pktCrc = bytes.readUInt16LE(0);
  const pktCounter = bytes.readUInt16LE(2);
  const sessionId = bytes.readUInt32LE(4);
  const pktType = bytes.readUInt8(8);
  const status = bytes.readUInt8(9);
  const payloadType = bytes.readUInt8(10);
  const ppi = bytes.readUInt8(11);
  const payloadLen = bytes.readUInt16LE(12);
  const expectedLength = 14 + payloadLen;
  if (bytes.length < expectedLength) {
    throw new Error(`Truncated frame: expected ${expectedLength} bytes, got ${bytes.length}`);
  }

  const raw = bytes.subarray(0, expectedLength);
  const payload = raw.subarray(14);
  const calcCrc = crc16Update(raw.subarray(2));

  return {
    raw,
    payload,
    pktCrc,
    calcCrc,
    crcOk: pktCrc === calcCrc,
    pktCounter,
    sessionId,
    pktType,
    status,
    payloadType,
    ppi,
    payloadLen,
  };
}

function encodeDosePayload(flags) {
  const days = parseIntArg(flags.days, 20500);
  const eventCtr = parseIntArg(flags.event, 1);
  const start = parseIntArg(flags.start, 1735689600);
  const duration = parseIntArg(flags.duration, 120);
  const inTime = parseIntArg(flags.inTime, 1);
  const tilt = parseIntArg(flags.tilt, 3);

  const buf = Buffer.alloc(12);
  buf.writeUInt16LE(days & 0xffff, 0);
  buf.writeUInt8(eventCtr & 0xff, 2);
  buf.writeUInt32LE(start >>> 0, 3);
  buf.writeUInt16LE(duration & 0xffff, 7);
  buf.writeUInt8(inTime & 0xff, 9);
  buf.writeUInt16LE(tilt & 0xffff, 10);
  return buf;
}

function encodeBatteryPayload(flags) {
  const ts = parseIntArg(flags.ts, Math.floor(Date.now() / 1000));
  const level = parseIntArg(flags.level, 90);
  const buf = Buffer.alloc(5);
  buf.writeUInt32LE(ts >>> 0, 0);
  buf.writeUInt8(level & 0xff, 4);
  return buf;
}

function encodeDebugPayload(flags) {
  const msg = flags.msg || "ERR:Debug test";
  return Buffer.from(String(msg), "utf8");
}

function buildNotify(kind, flags) {
  const sessionId = parseIntArg(flags.session, 0x78563412);
  const counter = parseIntArg(flags.counter, 2);

  let ppi;
  let payload;

  switch (kind) {
    case "dose":
      ppi = PPIS.dose;
      payload = encodeDosePayload(flags);
      break;
    case "dock_batt":
      ppi = PPIS.dock_batt;
      payload = encodeBatteryPayload(flags);
      break;
    case "ring_batt":
      ppi = PPIS.ring_batt;
      payload = encodeBatteryPayload(flags);
      break;
    case "dock_dbg":
      ppi = PPIS.dock_dbg;
      payload = encodeDebugPayload(flags);
      break;
    case "ring_dbg":
      ppi = PPIS.ring_dbg;
      payload = encodeDebugPayload(flags);
      break;
    default:
      throw new Error(`Unknown notify kind: ${kind}`);
  }

  return buildFrame({
    counter,
    sessionId,
    pktType: PACKET_TYPES.DATA,
    status: 1,
    payloadType: PPI_TYPE_PUSH,
    ppi,
    payload,
  });
}

function usage() {
  console.log(`
Message Protocol nRF helper

Usage
  node scripts/mp-nrf-helper.js ack <incoming_frame_hex>
  node scripts/mp-nrf-helper.js notify <dose|dock_batt|ring_batt|dock_dbg|ring_dbg> [flags]

Notify flags
  --session <num|0xhex>      default: 0x78563412
  --counter <num>            default: 2

Dose flags
  --days <num>               default: 20500
  --event <num>              default: 1
  --start <unix_sec>         default: 1735689600
  --duration <sec>           default: 120
  --inTime <0|1>             default: 1
  --tilt <num>               default: 3

Battery flags
  --ts <unix_sec>            default: now
  --level <0-100>            default: 90

Debug flags
  --msg "<text>"             default: ERR:Debug test

Examples
  node scripts/mp-nrf-helper.js ack f2f2010012345678030100000000
  node scripts/mp-nrf-helper.js notify dose --session 0x78563412 --counter 2
  node scripts/mp-nrf-helper.js notify dock_batt --session 0x78563412 --counter 3 --level 77
  node scripts/mp-nrf-helper.js notify dock_dbg --session 0x78563412 --counter 5 --msg "ERR:Dock overtemp"
`.trim());
}

function main() {
  const [, , cmd, ...args] = process.argv;
  if (!cmd || cmd === "--help" || cmd === "-h") {
    usage();
    return;
  }

  if (cmd === "ack") {
    const incomingHex = args[0];
    if (!incomingHex) {
      usage();
      process.exitCode = 1;
      return;
    }
    const parsed = parseFrame(incomingHex);
    const responseType =
      parsed.pktType === PACKET_TYPES.SYNC_START
        ? PACKET_TYPES.SYNC_ACK
        : parsed.pktType === PACKET_TYPES.DATA
          ? PACKET_TYPES.ACK
          : null;

    if (responseType === null) {
      throw new Error(`Incoming pkt_type ${parsed.pktType} is not SYNC_START(3) or DATA(0).`);
    }

    const response = buildFrame({
      counter: parsed.pktCounter,
      sessionId: parsed.sessionId,
      pktType: responseType,
      status: 1,
      payloadType: parsed.payloadType,
      ppi: parsed.ppi,
      payload: Buffer.alloc(0),
    });

    console.log(`incoming_crc_ok=${parsed.crcOk}`);
    console.log(`response_hex=${toHex(response)}`);
    console.log(`response_hex_spaced=${toHexSpaced(response)}`);
    console.log(`response_base64=${toBase64(response)}`);
    return;
  }

  if (cmd === "notify") {
    const kind = args[0];
    if (!kind) {
      usage();
      process.exitCode = 1;
      return;
    }
    const flags = parseFlags(args.slice(1));
    const frame = buildNotify(kind, flags);
    const parsed = parseFrame(toHex(frame));
    console.log(`notify_kind=${kind}`);
    console.log(`frame_crc_ok=${parsed.crcOk}`);
    console.log(`frame_hex=${toHex(frame)}`);
    console.log(`frame_hex_spaced=${toHexSpaced(frame)}`);
    console.log(`frame_base64=${toBase64(frame)}`);
    return;
  }

  throw new Error(`Unknown command: ${cmd}`);
}

try {
  main();
} catch (error) {
  console.error(`[mp-nrf-helper] ${error.message}`);
  process.exitCode = 1;
}
