import { QUICK_FLOW_ACTIONS } from "./constants";
import type { QuickActionPage, QuickFlowAction } from "./types";

export const CALIBRATION_QUICK_ACTION_KEY_SET = new Set<QuickFlowAction>([
  "CALIBRATION_DATA_RQ",
  "START_CALIBRATION_RQ",
  "STOP_CALIBRATION_RQ",
  "CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE",
  "CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE",
]);

export const BASELINING_QUICK_ACTION_KEY_SET = new Set<QuickFlowAction>([
  "START_BASELINING_RQ",
  "STOP_BASELINING_RQ",
  "VALIDATE_MED_RE_TRUE",
  "VALIDATE_MED_RE_FALSE",
]);

export const DEVELOPMENT_CMD_QUICK_ACTION_KEY_SET = new Set<QuickFlowAction>([
  "DEVELOPMENT_CMD_RQ",
]);

export function buildQuickActionPages(): QuickActionPage[] {
  const primaryActions = QUICK_FLOW_ACTIONS.filter(
    (action) =>
      !DEVELOPMENT_CMD_QUICK_ACTION_KEY_SET.has(action.key) &&
      !CALIBRATION_QUICK_ACTION_KEY_SET.has(action.key) &&
      !BASELINING_QUICK_ACTION_KEY_SET.has(action.key)
  );
  const developmentActions = QUICK_FLOW_ACTIONS.filter((action) =>
    DEVELOPMENT_CMD_QUICK_ACTION_KEY_SET.has(action.key)
  );
  const calibrationActions = QUICK_FLOW_ACTIONS.filter((action) =>
    CALIBRATION_QUICK_ACTION_KEY_SET.has(action.key)
  );
  const baseliningActions = QUICK_FLOW_ACTIONS.filter((action) =>
    BASELINING_QUICK_ACTION_KEY_SET.has(action.key)
  );

  return [
    { id: "general", title: "General", actions: primaryActions },
    { id: "developer", title: "Developer", actions: developmentActions },
    { id: "calibration", title: "Calibration", actions: calibrationActions },
    { id: "baselining", title: "Baselining", actions: baseliningActions },
  ].filter((page) => page.actions.length > 0) as QuickActionPage[];
}

export function getQuickActionSubtitle(action: QuickFlowAction): string {
  switch (action) {
    case "TIME_RQ":
      return "AD_TIME (RQ, no payload)";
    case "TIME_PUSH":
      return "AD_TIME (PUSH, uint32 unix)";
    case "DOSE_SCHEDULE_RQ":
      return "AD_DOSE_SCHEDULE (RQ, no payload)";
    case "DOSE_SCHEDULE_PUSH":
      return "AD_DOSE_SCHEDULE (PUSH, preset A)";
    case "DOSE_SCHEDULE_PUSH_ALT_1":
      return "AD_DOSE_SCHEDULE (PUSH, preset B)";
    case "DOSE_SCHEDULE_PUSH_ALT_2":
      return "AD_DOSE_SCHEDULE (PUSH, preset C)";
    case "DOCK_STATUS_RQ":
      return "AD_DOCK_STATUS (RQ, no payload)";
    case "RING_STATUS_RQ":
      return "AD_RING_STATUS (RQ, no payload)";
    case "DEVELOPMENT_CMD_RQ":
      return "AD_DEVELOPMENT_CMD (RQ, uint8 from input)";
    case "CALIBRATION_DATA_RQ":
      return "AD_CALIBRATION_DATA (RQ, no payload)";
    case "START_CALIBRATION_RQ":
      return "AD_START_CALIBRATION (RQ, start=true + weight mg)";
    case "STOP_CALIBRATION_RQ":
      return "AD_START_CALIBRATION (RQ, start=false + weight mg)";
    case "CALIBRATION_WEIGHT_PRESENT_PUSH_TRUE":
      return "AD_CALIBRATION_WEIGHT_PRESENT (PUSH, is_present=true)";
    case "CALIBRATION_WEIGHT_PRESENT_PUSH_FALSE":
      return "AD_CALIBRATION_WEIGHT_PRESENT (PUSH, is_present=false)";
    case "START_BASELINING_RQ":
      return "AD_START_BASELINING (RQ, start=true)";
    case "STOP_BASELINING_RQ":
      return "AD_START_BASELINING (RQ, start=false)";
    case "VALIDATE_MED_RE_TRUE":
      return "AD_VALIDATE_MED (RE, success=true)";
    case "VALIDATE_MED_RE_FALSE":
      return "AD_VALIDATE_MED (RE, success=false)";
    case "DOCK_BATTERY_RQ":
      return "AD_DOCK_BATT_LEVEL_LOG (RQ, no payload)";
    case "RING_BATTERY_RQ":
      return "AD_RING_BATT_LEVEL_LOG (RQ, no payload)";
    default:
      return "";
  }
}
