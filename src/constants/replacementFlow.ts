import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "./ble";

export const REPLACEMENT_FLOW_SIGNAL = {
  serviceUuid: SERVICE_UUIDS.CUSTOM_SERVICE,
  characteristicUuid: CHARACTERISTIC_UUIDS.REPLACEMENT_STEP,
  // Placeholder command the app writes to firmware when replacement flow starts.
  processStartedWriteHex: "10",
  // Placeholder command when replacement flow times out/stops.
  processStoppedWriteHex: "11",
  // Placeholder command when user starts replacement again (restart).
  processRestartedWriteHex: "12",
  // Placeholder payload: when app receives hex "01", it moves from
  // "Remove bottle from dock" to "Checking dock".
  step1ToCheckingHex: "01",
  // Placeholder payload: when app receives hex "02", checking is complete
  // and flow advances to step 2.
  checkingToStep2Hex: "02",
  // Placeholder payload: when app receives hex "03", flow advances to step 3.
  step2ToStep3Hex: "03",
  // Placeholder payload: when app receives hex "04", flow advances to step 4 checking.
  step3ToStep4CheckingHex: "04",
  // Placeholder payload: when app receives hex "05", step 4 checking completes.
  step4CheckingToSuccessHex: "05",
};

export function doesMatchReplacementStep1Signal(hex: string): boolean {
  return hex.trim().toLowerCase() === REPLACEMENT_FLOW_SIGNAL.step1ToCheckingHex;
}

export function doesMatchReplacementCheckingCompleteSignal(hex: string): boolean {
  return hex.trim().toLowerCase() === REPLACEMENT_FLOW_SIGNAL.checkingToStep2Hex;
}

export function doesMatchReplacementStep2ToStep3Signal(hex: string): boolean {
  return hex.trim().toLowerCase() === REPLACEMENT_FLOW_SIGNAL.step2ToStep3Hex;
}

export function doesMatchReplacementStep3ToStep4CheckingSignal(hex: string): boolean {
  return hex.trim().toLowerCase() === REPLACEMENT_FLOW_SIGNAL.step3ToStep4CheckingHex;
}

export function doesMatchReplacementStep4CheckingToSuccessSignal(hex: string): boolean {
  return hex.trim().toLowerCase() === REPLACEMENT_FLOW_SIGNAL.step4CheckingToSuccessHex;
}
