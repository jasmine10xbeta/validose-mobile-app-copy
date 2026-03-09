import { Buffer } from "buffer";

export function encodePacketBytesForIngest(packetBytes: Uint8Array): string {
  const packetHex = Buffer.from(packetBytes).toString("hex");
  return Buffer.from(packetHex, "utf8").toString("base64");
}
