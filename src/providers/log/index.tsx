import React, {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useSyncExternalStore,
} from "react";

export type LogEntry = { message: string; timestamp: string };

const MAX_LOG_ENTRIES = 200;
const LOG_UPDATE_BATCH_MS = 80;
const MAX_LOG_MESSAGE_CHARS = 2400;

type LogStoreApi = {
  getSnapshot: () => LogEntry[];
  subscribe: (listener: () => void) => () => void;
};

const LogContext = createContext<LogStoreApi | undefined>(undefined);

let logBuffer: LogEntry[] = [];
const listeners = new Set<() => void>();
let emitTimeout: ReturnType<typeof setTimeout> | null = null;
let consolePatched = false;

function formatStamp(d = new Date()) {
  const pad = (n: number, w = 2) => String(n).padStart(w, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

function toLogString(value: unknown): string {
  if (typeof value === "string") return value;
  if (value instanceof Error) return `${value.name}: ${value.message}`;

  try {
    const serialized = JSON.stringify(value);
    if (typeof serialized === "string") {
      return serialized;
    }
  } catch {
    // Fall through to String(value).
  }

  return String(value);
}

function sanitizeLogMessage(message: string): string {
  if (message.length <= MAX_LOG_MESSAGE_CHARS) return message;
  return `${message.slice(0, MAX_LOG_MESSAGE_CHARS)}... [truncated]`;
}

function scheduleEmit() {
  if (!listeners.size) return;
  if (emitTimeout) return;

  emitTimeout = setTimeout(() => {
    emitTimeout = null;
    listeners.forEach((listener) => listener());
  }, LOG_UPDATE_BATCH_MS);
}

function appendLog(args: unknown[]) {
  const message = sanitizeLogMessage(args.map((arg) => toLogString(arg)).join(" "));

  const nextEntry: LogEntry = {
    message,
    timestamp: formatStamp(),
  };

  const nextBuffer = [...logBuffer, nextEntry];
  logBuffer =
    nextBuffer.length > MAX_LOG_ENTRIES
      ? nextBuffer.slice(nextBuffer.length - MAX_LOG_ENTRIES)
      : nextBuffer;

  scheduleEmit();
}

function patchConsoleIfNeeded() {
  if (consolePatched) return;
  consolePatched = true;

  const originalLog = console.log.bind(console);
  const originalWarn = console.warn.bind(console);
  const originalError = console.error.bind(console);
  const originalInfo = console.info.bind(console);

  console.log = (...args: unknown[]) => {
    appendLog(args);
    originalLog(...args);
  };
  console.warn = (...args: unknown[]) => {
    appendLog(args);
    originalWarn(...args);
  };
  console.error = (...args: unknown[]) => {
    appendLog(args);
    originalError(...args);
  };
  console.info = (...args: unknown[]) => {
    appendLog(args);
    originalInfo(...args);
  };
}

function subscribe(listener: () => void) {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}

function getSnapshot() {
  return logBuffer;
}

export function LogProvider({ children }: { children: React.ReactNode }) {
  useEffect(() => {
    patchConsoleIfNeeded();
  }, []);

  const value = useMemo<LogStoreApi>(
    () => ({
      getSnapshot,
      subscribe,
    }),
    []
  );

  return <LogContext.Provider value={value}>{children}</LogContext.Provider>;
}

export function useLogs() {
  const context = useContext(LogContext);
  if (!context) {
    throw new Error("useLogs must be used within a LogProvider");
  }

  return useSyncExternalStore(
    context.subscribe,
    context.getSnapshot,
    context.getSnapshot
  );
}
