import { addBleDebugLog } from "./debugLogStore";

function toLogString(value: unknown): string {
  if (typeof value === "string") {
    return value;
  }

  if (value instanceof Error) {
    return `${value.name}: ${value.message}`;
  }

  try {
    return JSON.stringify(value, null, 2);
  } catch {
    return String(value);
  }
}

function formatTimestamp(date = new Date()): string {
  return date.toLocaleTimeString();
}

export function bleLog(message: string, payload?: unknown): void {
  const body = payload === undefined ? "" : `\n${toLogString(payload)}`;
  addBleDebugLog(`${formatTimestamp()}  ${message}${body}`);
}

export function bleLogInfo(message: string, payload?: unknown): void {
  bleLog(`[INFO] ${message}`, payload);
}

export function bleLogWarn(message: string, payload?: unknown): void {
  bleLog(`[WARN] ${message}`, payload);
}

export function bleLogError(message: string, payload?: unknown): void {
  bleLog(`[ERR] ${message}`, payload);
}
