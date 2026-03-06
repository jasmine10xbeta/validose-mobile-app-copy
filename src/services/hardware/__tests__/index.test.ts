import { encodeHexPayloadForIngest } from "../encoding";

describe("hardware ingest encoding", () => {
  test("encodes hex payload as base64 of hex text", () => {
    const encoded = encodeHexPayloadForIngest("0A ff 10");
    expect(encoded).toBe("MGFmZjEw");
  });
});
