export function buffersEqual(a: Uint8Array, b: Uint8Array): boolean {
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

export function crc16Update(data: Uint8Array, seed?: number): number {
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

export function generateSessionId(): number | null {
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

  return Math.floor(Math.random() * 0xffffffff);
}
