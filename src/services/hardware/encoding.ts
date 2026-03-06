import { Buffer } from "buffer";

function normalizeHex(value: string): string {
  return value.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "").toLowerCase();
}

export function encodeHexPayloadForIngest(payloadHex: string): string {
  const normalizedHex = normalizeHex(payloadHex);
  return Buffer.from(normalizedHex, "utf8").toString("base64");
}
