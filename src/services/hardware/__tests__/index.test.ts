import { encodePacketBytesForIngest } from "../encoding";

describe("hardware ingest encoding", () => {
  test("encodes packet bytes as base64 of packet hex", () => {
    const encoded = encodePacketBytesForIngest(Uint8Array.from([0x0a, 0xff, 0x10]));
    expect(encoded).toBe("MGFmZjEw");
  });
});
