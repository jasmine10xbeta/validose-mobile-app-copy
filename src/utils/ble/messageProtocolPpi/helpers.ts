import {
  MpPacketPayload,
  MsgProtRxPacketStatus,
  MsgProtTxPacketStatus,
} from "../messageProtocol";
import { PPI_DEFINITIONS } from "./definitions";
import { PpiId, PpiType } from "./types";

export function buildPpiPayload(ppi: PpiId, type: PpiType, payload: Uint8Array): MpPacketPayload {
  return {
    type,
    ppi,
    pktPayloadLen: payload.length,
    payload,
  };
}

export function isTxStatusSendable(status: number): boolean {
  return status === MsgProtTxPacketStatus.ABANDONED || status === MsgProtTxPacketStatus.COMPLETED;
}

export function isRxStatusReadable(status: number): boolean {
  return status === MsgProtRxPacketStatus.NEW;
}

export function getExpectedPayloadLength(ppi: PpiId, type: PpiType): number | null {
  const def = PPI_DEFINITIONS[ppi];
  if (!def) {
    return null;
  }

  switch (type) {
    case PpiType.RQ:
      return def.lengths.rq ?? null;
    case PpiType.RE:
      return def.lengths.re ?? null;
    case PpiType.PUSH:
      return def.lengths.push ?? null;
    default:
      return null;
  }
}

export function validatePayloadLength(ppi: PpiId, type: PpiType, payload: Uint8Array): boolean {
  const expected = getExpectedPayloadLength(ppi, type);
  if (expected === null || expected === undefined) {
    return true;
  }
  return payload.length === expected;
}

export function encodeBool(value: boolean): Uint8Array {
  return new Uint8Array([value ? 1 : 0]);
}

export function decodeBool(payload: Uint8Array): boolean | null {
  if (payload.length !== 1) {
    return null;
  }
  return payload[0] !== 0;
}

export function encodeUint32LE(value: number): Uint8Array {
  const buffer = new ArrayBuffer(4);
  const view = new DataView(buffer);
  view.setUint32(0, value >>> 0, true);
  return new Uint8Array(buffer);
}

export function decodeUint32LE(payload: Uint8Array): number | null {
  if (payload.length !== 4) {
    return null;
  }
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return view.getUint32(0, true);
}

export function decodeUint8(payload: Uint8Array): number | null {
  if (payload.length !== 1) {
    return null;
  }
  return payload[0];
}
