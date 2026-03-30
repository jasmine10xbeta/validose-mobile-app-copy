import AsyncStorage from "@react-native-async-storage/async-storage";

export type BleDebugLogEntry = {
  id: string;
  message: string;
  createdAtMs: number;
};

const MAX_LOG_ENTRIES = 200;
const BLE_DEBUG_LOGS_STORAGE_KEY = "ble-debug:logs:v1";
const PERSIST_BATCH_MS = 300;

let bleDebugLogs: BleDebugLogEntry[] = [];
const listeners = new Set<() => void>();
let hasHydrated = false;
let hydratePromise: Promise<void> | null = null;
let clearRequestedBeforeHydration = false;
let persistTimeout: ReturnType<typeof setTimeout> | null = null;

function emitUpdate() {
  listeners.forEach((listener) => listener());
}

function isLogEntry(value: unknown): value is BleDebugLogEntry {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }

  const candidate = value as Record<string, unknown>;
  return (
    typeof candidate.id === "string" &&
    typeof candidate.message === "string" &&
    typeof candidate.createdAtMs === "number"
  );
}

function capLogs(logs: BleDebugLogEntry[]) {
  return logs.length > MAX_LOG_ENTRIES
    ? logs.slice(logs.length - MAX_LOG_ENTRIES)
    : logs;
}

function mergeLogs(existing: BleDebugLogEntry[], incoming: BleDebugLogEntry[]) {
  const seen = new Set<string>();
  const merged: BleDebugLogEntry[] = [];

  [...existing, ...incoming].forEach((entry) => {
    if (seen.has(entry.id)) {
      return;
    }
    seen.add(entry.id);
    merged.push(entry);
  });

  return capLogs(merged);
}

function parseStoredLogs(serialized: string | null): BleDebugLogEntry[] {
  if (!serialized) {
    return [];
  }

  try {
    const parsed = JSON.parse(serialized);
    if (!Array.isArray(parsed)) {
      return [];
    }
    return capLogs(parsed.filter(isLogEntry));
  } catch {
    return [];
  }
}

async function persistBleDebugLogs() {
  try {
    await AsyncStorage.setItem(BLE_DEBUG_LOGS_STORAGE_KEY, JSON.stringify(bleDebugLogs));
  } catch {
    // Ignore persistence failures and keep in-memory logs functional.
  }
}

function schedulePersistBleDebugLogs() {
  if (!hasHydrated) {
    return;
  }

  if (persistTimeout) {
    return;
  }

  persistTimeout = setTimeout(() => {
    persistTimeout = null;
    void persistBleDebugLogs();
  }, PERSIST_BATCH_MS);
}

function ensureHydrated() {
  if (hasHydrated) {
    return;
  }

  if (hydratePromise) {
    return;
  }

  hydratePromise = (async () => {
    try {
      const serialized = await AsyncStorage.getItem(BLE_DEBUG_LOGS_STORAGE_KEY);
      const persistedLogs = clearRequestedBeforeHydration ? [] : parseStoredLogs(serialized);
      bleDebugLogs = mergeLogs(persistedLogs, bleDebugLogs);
      hasHydrated = true;
      emitUpdate();
      await persistBleDebugLogs();
    } catch {
      hasHydrated = true;
    } finally {
      hydratePromise = null;
    }
  })();
}

export function addBleDebugLog(message: string) {
  const entry: BleDebugLogEntry = {
    id: `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`,
    message,
    createdAtMs: Date.now(),
  };

  // Keep logs bounded with a rolling window to avoid UI/memory pressure.
  bleDebugLogs = capLogs([...bleDebugLogs, entry]);
  emitUpdate();
  ensureHydrated();
  schedulePersistBleDebugLogs();
}

export function getBleDebugLogs() {
  ensureHydrated();
  return bleDebugLogs;
}

export function clearBleDebugLogs() {
  if (!hasHydrated) {
    clearRequestedBeforeHydration = true;
  }
  bleDebugLogs = [];
  emitUpdate();
  ensureHydrated();
  schedulePersistBleDebugLogs();
}

export function subscribeBleDebugLogs(listener: () => void) {
  ensureHydrated();
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}

ensureHydrated();
