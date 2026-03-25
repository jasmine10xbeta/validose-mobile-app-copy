export type BleDebugLogEntry = {
  id: string;
  message: string;
  createdAtMs: number;
};

const MAX_LOG_ENTRIES = 200;

let bleDebugLogs: BleDebugLogEntry[] = [];
const listeners = new Set<() => void>();

function emitUpdate() {
  listeners.forEach((listener) => listener());
}

export function addBleDebugLog(message: string) {
  const entry: BleDebugLogEntry = {
    id: `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`,
    message,
    createdAtMs: Date.now(),
  };

  // Keep logs bounded with a rolling window to avoid UI/memory pressure.
  bleDebugLogs.push(entry);
  if (bleDebugLogs.length > MAX_LOG_ENTRIES) {
    const overflow = bleDebugLogs.length - MAX_LOG_ENTRIES;
    bleDebugLogs = bleDebugLogs.slice(overflow);
  }
  emitUpdate();
}

export function getBleDebugLogs() {
  return bleDebugLogs;
}

export function clearBleDebugLogs() {
  bleDebugLogs = [];
  emitUpdate();
}

export function subscribeBleDebugLogs(listener: () => void) {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}
