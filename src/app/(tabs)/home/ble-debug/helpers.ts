import { Buffer } from "buffer";

function normalizeHex(value: string): string {
  return value.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "").toLowerCase();
}

export function formatHexBytes(value: string): string {
  const normalized = normalizeHex(value);
  if (!normalized) return value;
  if (normalized.length % 2 !== 0) return value;

  const bytes = normalized.match(/.{2}/g);
  if (!bytes?.length) return value;
  return bytes.join(" ");
}

export function formatHexTokensInText(value: string): string {
  // First pass: JSON fields whose key includes "hex" (payloadHex/rawHex/frameHex/etc.).
  const withNamedHexValues = value.replace(
    /("(?:[^"\\]|\\.)*?hex(?:[^"\\]|\\.)*?"\s*:\s*")([0-9a-fA-F]+)"/gi,
    (_match, prefix: string, hex: string) => `${prefix}${formatHexBytes(hex)}"`
  );

  // Second pass: long standalone contiguous hex blobs in free-form log text.
  return withNamedHexValues.replace(
    /(^|[^0-9a-fA-F])([0-9a-fA-F]{8,})(?=[^0-9a-fA-F]|$)/g,
    (_match, prefix: string, hex: string) => {
      if (hex.length % 2 !== 0) {
        return `${prefix}${hex}`;
      }
      return `${prefix}${formatHexBytes(hex)}`;
    }
  );
}

export function prettifyForLog(value: unknown): string {
  if (value === null || value === undefined) return String(value);

  if (typeof value === "string") {
    const trimmed = value.trim();
    if (!trimmed) return "";

    try {
      const parsed = JSON.parse(trimmed);
      return JSON.stringify(parsed, null, 2);
    } catch {
      return value;
    }
  }

  if (typeof value === "object") {
    try {
      return JSON.stringify(value, null, 2);
    } catch {
      return String(value);
    }
  }

  return String(value);
}

export function formatDecodedValue(value: unknown): string {
  if (value instanceof Uint8Array) {
    const hex = Buffer.from(value).toString("hex");
    return hex ? formatHexBytes(hex) : "(empty)";
  }

  const pretty = prettifyForLog(value);
  return pretty || "(empty)";
}

function asRecord(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== "object") return null;
  return value as Record<string, unknown>;
}

export function extractConnectedDeviceLabel(value: unknown): string {
  if (typeof value === "string") return value.trim();

  const record = asRecord(value);
  if (!record) return "";

  const keys = ["deviceName", "name", "deviceId", "id", "address"] as const;
  for (const key of keys) {
    const maybe = record[key];
    if (typeof maybe === "string" && maybe.trim()) {
      return maybe.trim();
    }
  }

  return "";
}

export function resolveConnectionState(value: unknown): boolean {
  if (typeof value === "boolean") return value;
  if (typeof value === "number") return value !== 0;
  if (typeof value === "string") {
    const normalized = value.trim().toLowerCase();
    if (!normalized || normalized === "false" || normalized === "0") return false;
    return normalized !== "disconnected" && normalized !== "error";
  }

  const record = asRecord(value);
  if (!record) return false;

  const statusValue = record.status;
  if (typeof statusValue === "string") {
    const normalizedStatus = statusValue.trim().toLowerCase();
    if (normalizedStatus === "success" || normalizedStatus === "connected") return true;
    if (normalizedStatus === "error" || normalizedStatus === "disconnected") return false;
  }

  const connectedValue = record.connected;
  if (typeof connectedValue === "boolean") return connectedValue;
  if (typeof connectedValue === "number") return connectedValue !== 0;
  if (typeof connectedValue === "string") {
    const normalizedConnected = connectedValue.trim().toLowerCase();
    if (normalizedConnected === "true" || normalizedConnected === "1") return true;
    if (normalizedConnected === "false" || normalizedConnected === "0") return false;
  }

  return Boolean(extractConnectedDeviceLabel(value));
}
