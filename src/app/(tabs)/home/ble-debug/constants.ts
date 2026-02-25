import {
  DOSE_SCHEDULE_T_SIZE_BYTES,
  PpiId,
  PpiType,
} from "@/utils/ble/messageProtocolPpi";

import type { QuickFlowAction, QuickFlowMeta } from "./types";

export const QUICK_FLOW_ACTIONS: { key: QuickFlowAction; label: string }[] = [
  { key: "TIME_RQ", label: "Time RQ" },
  { key: "TIME_RE", label: "Time RE" },
  { key: "TIME_PUSH", label: "Time PUSH" },
  { key: "DOSE_SCHEDULE_RQ", label: "Dose RQ" },
  { key: "DOSE_SCHEDULE_RE", label: "Dose RE" },
  { key: "DOSE_SCHEDULE_PUSH", label: "Dose PUSH" },
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
  TIME_RE: {
    title: "Time Response",
    actionName: "TIME_RE",
    busyKey: "ppi-time-re",
    ppiName: "AD_TIME",
    ppiId: PpiId.AD_TIME,
    typeName: "RE",
    typeId: PpiType.RE,
    payloadHint: "uint32 unix time",
    lenHint: "len=4",
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
  DOSE_SCHEDULE_RE: {
    title: "Dose Schedule Response",
    actionName: "DOSE_SCHEDULE_RE",
    busyKey: "ppi-dose-schedule-re",
    ppiName: "AD_DOSE_SCHEDULE",
    ppiId: PpiId.AD_DOSE_SCHEDULE,
    typeName: "RE",
    typeId: PpiType.RE,
    payloadHint: "dose_schedule_t (demo)",
    lenHint: `len=${DOSE_SCHEDULE_T_SIZE_BYTES}`,
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
};

export const DEBUG_INPUTS_STORAGE_KEY = "validose_ble_debug_inputs_v1";
export const PPI_TX_READY_TIMEOUT_MS = 15000;
export const PPI_TX_READY_POLL_MS = 100;
export const PPI_MANUAL_ACK_TIMEOUT_MS = 6000;
export const PPI_MANUAL_SYNC_RETRY_MS = 6000;
export const PPI_MANUAL_MAX_RETRIES = 3;
export const PPI_NRF_MANUAL_ACK_TIMEOUT_MS = 30000;
export const PPI_NRF_MANUAL_MAX_RETRIES = 1;
export const PPI_TX_COMPLETION_WAIT_MS = 7000;
export const PPI_NRF_TX_COMPLETION_WAIT_MS = 60000;
export const PPI_LATE_ACK_WATCH_TIMEOUT_MS = 90000;
export const PPI_LATE_ACK_WATCH_POLL_MS = 120;
