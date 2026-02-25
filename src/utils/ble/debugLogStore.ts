export type BleDebugLogEntry = {
  id: string;
  message: string;
  createdAtMs: number;
};

const MAX_LOG_ENTRIES = 1000;

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

  bleDebugLogs = [...bleDebugLogs, entry].slice(-MAX_LOG_ENTRIES);
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

