import {
  BATTERY_LEVEL_T_SIZE_BYTES,
  DOSE_SCHEDULE_T_SIZE_BYTES,
  PpiId,
  PpiType,
} from "@/utils/ble/messageProtocolPpi";

import type { QuickFlowAction, QuickFlowMeta } from "./types";

export const QUICK_FLOW_ACTIONS: { key: QuickFlowAction; label: string }[] = [
  { key: "TIME_RQ", label: "Time RQ" },
  { key: "TIME_PUSH", label: "Time PUSH" },
  { key: "DOSE_SCHEDULE_RQ", label: "Dose RQ" },
  { key: "DOSE_SCHEDULE_PUSH", label: "Dose PUSH" },
  { key: "DOCK_STATUS_RQ", label: "Dock Status" },
  { key: "RING_STATUS_RQ", label: "Ring Status" },
  { key: "DOCK_BATTERY_RQ", label: "Dock Battery" },
  { key: "RING_BATTERY_RQ", label: "Ring Battery" },
];

export const QUICK_FLOW_META: Record<QuickFlowAction, QuickFlowMeta> = {
  TIME_RQ: {
    title: "Time Request",
    actionName: "TIME_RQ",
    busyKey: "ppi-time-rq",
    ppiName: "AD_TIME",
    ppiId: PpiId.AD_TIME,
    typeName: "RQ",
    typeId: PpiType.RQ,
    payloadHint: "No payload",
    lenHint: "len=0",
  },
  TIME_PUSH: {
    title: "Time Push",
    actionName: "TIME_PUSH",
    busyKey: "ppi-time-push",
    ppiName: "AD_TIME",
    ppiId: PpiId.AD_TIME,
    typeName: "PUSH",
    typeId: PpiType.PUSH,
    payloadHint: "uint32 unix time",
    lenHint: "len=4",
  },
  DOSE_SCHEDULE_RQ: {
    title: "Dose Schedule Request",
    actionName: "DOSE_SCHEDULE_RQ",
    busyKey: "ppi-dose-schedule-rq",
    ppiName: "AD_DOSE_SCHEDULE",
    ppiId: PpiId.AD_DOSE_SCHEDULE,
    typeName: "RQ",
    typeId: PpiType.RQ,
    payloadHint: "No payload",
    lenHint: "len=0",
  },
  DOSE_SCHEDULE_PUSH: {
    title: "Dose Schedule Push",
    actionName: "DOSE_SCHEDULE_PUSH",
    busyKey: "ppi-dose-schedule-push",
    ppiName: "AD_DOSE_SCHEDULE",
    ppiId: PpiId.AD_DOSE_SCHEDULE,
    typeName: "PUSH",
    typeId: PpiType.PUSH,
    payloadHint: "dose_schedule_t (demo)",
    lenHint: `len=${DOSE_SCHEDULE_T_SIZE_BYTES}`,
  },
  DOCK_STATUS_RQ: {
    title: "Dock Status Request",
    actionName: "DOCK_STATUS_RQ",
    busyKey: "ppi-dock-status-rq",
    ppiName: "AD_DOCK_STATUS",
    ppiId: PpiId.AD_DOCK_STATUS,
    typeName: "RQ",
    typeId: PpiType.RQ,
    payloadHint: "No payload",
    lenHint: "len=0",
  },
  RING_STATUS_RQ: {
    title: "Ring Status Request",
    actionName: "RING_STATUS_RQ",
    busyKey: "ppi-ring-status-rq",
    ppiName: "AD_RING_STATUS",
    ppiId: PpiId.AD_RING_STATUS,
    typeName: "RQ",
    typeId: PpiType.RQ,
    payloadHint: "No payload",
    lenHint: "len=0",
  },
  DOCK_BATTERY_RQ: {
    title: "Dock Battery Request",
    actionName: "DOCK_BATTERY_RQ",
    busyKey: "ppi-dock-battery-rq",
    ppiName: "AD_DOCK_BATT_LEVEL_LOG",
    ppiId: PpiId.AD_DOCK_BATT_LEVEL_LOG,
    typeName: "RQ",
    typeId: PpiType.RQ,
    payloadHint: "No payload",
    lenHint: `RE/PUSH expected len=${BATTERY_LEVEL_T_SIZE_BYTES}`,
  },
  RING_BATTERY_RQ: {
    title: "Ring Battery Request",
    actionName: "RING_BATTERY_RQ",
    busyKey: "ppi-ring-battery-rq",
    ppiName: "AD_RING_BATT_LEVEL_LOG",
    ppiId: PpiId.AD_RING_BATT_LEVEL_LOG,
    typeName: "RQ",
    typeId: PpiType.RQ,
    payloadHint: "No payload",
    lenHint: `RE/PUSH expected len=${BATTERY_LEVEL_T_SIZE_BYTES}`,
  },
};

export const PPI_TX_READY_POLL_MS = 100;
// Firmware parity:
// - ACK_TIMEOUT_MS = 1000
// - MSG_PROT_MAX_RETRIES = 20
// - MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS = 0
export const PPI_ACK_TIMEOUT_MS = 1000;
export const PPI_MAX_RETRIES = 20;
// In firmware, ABANDONED is reached after the initial wait + retries:
// (maxRetries + 1) * ackTimeout = 21 * 1000 = 21000 ms.
// Keep a small guard band in debug UI waiters.
export const PPI_TX_COMPLETION_WAIT_MS = 23000;
export const PPI_TX_READY_TIMEOUT_MS = 23000;
export const PPI_LATE_ACK_WATCH_TIMEOUT_MS = 23000;
export const PPI_LATE_ACK_WATCH_POLL_MS = 120;
