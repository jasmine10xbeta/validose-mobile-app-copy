import { getConnectedDevice, isDeviceConnected } from "../../../../modules/tenx-mdk-ble-rn-library/src/index";

import { connectAndSetupDevice } from "./connect";

export const DEFAULT_CONNECT_AND_SETUP_TIMEOUT_MS = 20000;

type ConnectAndSetupResult = Awaited<ReturnType<typeof connectAndSetupDevice>>;

function normalizeIdentifier(value: unknown): string {
  return typeof value === "string" ? value.trim().toLowerCase() : "";
}

function isConnectedState(value: unknown): boolean {
  if (typeof value === "boolean") return value;
  if (typeof value === "number") return value !== 0;
  if (typeof value === "string") {
    const normalized = value.trim().toLowerCase();
    return (
      normalized === "true" ||
      normalized === "1" ||
      normalized === "connected" ||
      normalized === "success"
    );
  }
  if (!value || typeof value !== "object") return false;

  const record = value as Record<string, unknown>;
  if ("connected" in record) {
    return isConnectedState(record.connected);
  }
  if ("status" in record) {
    return isConnectedState(record.status);
  }

  return false;
}

function extractConnectedDeviceIdentity(value: unknown): { deviceId: string; deviceName: string } {
  if (!value || typeof value !== "object") {
    return { deviceId: "", deviceName: "" };
  }

  const record = value as Record<string, unknown>;
  const deviceId =
    (typeof record.deviceId === "string" && record.deviceId.trim()) ||
    (typeof record.id === "string" && record.id.trim()) ||
    "";
  const deviceName =
    (typeof record.deviceName === "string" && record.deviceName.trim()) ||
    (typeof record.name === "string" && record.name.trim()) ||
    "";

  return { deviceId, deviceName };
}

async function recoverConnectedDeviceResult(
  targetIdentifier: string,
): Promise<ConnectAndSetupResult | null> {
  try {
    const connected = await isDeviceConnected();
    if (!isConnectedState(connected)) return null;

    const connectedDevice = await getConnectedDevice();
    const { deviceId, deviceName } = extractConnectedDeviceIdentity(connectedDevice);

    const normalizedTarget = normalizeIdentifier(targetIdentifier);
    const normalizedId = normalizeIdentifier(deviceId);
    const normalizedName = normalizeIdentifier(deviceName);
    const hasKnownIdentity = Boolean(normalizedId || normalizedName);
    const matchesTarget =
      !normalizedTarget ||
      !hasKnownIdentity ||
      normalizedTarget === normalizedId ||
      normalizedTarget === normalizedName;

    if (!matchesTarget) return null;

    return {
      status: "success",
      deviceId: deviceId || targetIdentifier,
      deviceName: deviceName || targetIdentifier,
    };
  } catch {
    return null;
  }
}

function createTimeoutError(timeoutMs: number): Error {
  const error = new Error(
    `Connection timed out after ${Math.round(timeoutMs / 1000)} seconds.`,
  );
  error.name = "BleConnectTimeoutError";
  return error;
}

function isTimeoutError(error: unknown): boolean {
  if (!(error instanceof Error)) return false;
  return error.name === "BleConnectTimeoutError";
}

export async function connectAndSetupDeviceWithTimeout(
  deviceIdentifier: string,
  options?: {
    timeoutMs?: number;
    recoverConnectedDeviceOnTimeout?: boolean;
  },
): Promise<ConnectAndSetupResult> {
  const timeoutMs = Math.max(1000, options?.timeoutMs ?? DEFAULT_CONNECT_AND_SETUP_TIMEOUT_MS);
  const shouldRecover = options?.recoverConnectedDeviceOnTimeout ?? true;
  let timeoutHandle: ReturnType<typeof setTimeout> | null = null;

  try {
    const result = await Promise.race<ConnectAndSetupResult>([
      connectAndSetupDevice(deviceIdentifier),
      new Promise<ConnectAndSetupResult>((_, reject) => {
        timeoutHandle = setTimeout(() => {
          reject(createTimeoutError(timeoutMs));
        }, timeoutMs);
      }),
    ]);

    return result;
  } catch (error) {
    if (shouldRecover && isTimeoutError(error)) {
      const recovered = await recoverConnectedDeviceResult(deviceIdentifier);
      if (recovered) return recovered;
    }

    return {
      status: "error",
      error: error instanceof Error ? error : new Error(String(error ?? "Connection failed")),
    };
  } finally {
    if (timeoutHandle) {
      clearTimeout(timeoutHandle);
    }
  }
}
