import React, { createContext, useContext, useRef, useState } from "react";

type LogEntry = { message: string; timestamp: string };
const LogContext = createContext<{ logs: LogEntry[] }>({ logs: [] });

function formatStamp(d = new Date()) {
  const pad = (n: number, w = 2) => String(n).padStart(w, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

export function LogProvider({ children }: { children: React.ReactNode }) {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const initialized = useRef(false);

  if (!initialized.current) {
    initialized.current = true;

    const attachLog = (args: any[]) => ({
      message: args.map(a => (typeof a === "string" ? a : JSON.stringify(a, null, 2))).join(" "),
      timestamp: formatStamp(),
    });

    const origLog = console.log;
    const origWarn = console.warn;
    const origError = console.error;
    const origInfo = console.info;

    console.log = (...args: any[]) => {
      setLogs(l => [...l.slice(-300), attachLog(args)]);
      origLog(...args);
    };
    console.warn = (...args: any[]) => {
      setLogs(l => [...l.slice(-300), attachLog(args)]);
      origWarn(...args);
    };
    console.error = (...args: any[]) => {
      setLogs(l => [...l.slice(-300), attachLog(args)]);
      origError(...args);
    };
    console.info = (...args: any[]) => {
      setLogs(l => [...l.slice(-300), attachLog(args)]);
      origInfo(...args);
    };
  }

  return <LogContext.Provider value={{ logs }}>{children}</LogContext.Provider>;
}

export function useLogs() {
  return useContext(LogContext).logs;
}
