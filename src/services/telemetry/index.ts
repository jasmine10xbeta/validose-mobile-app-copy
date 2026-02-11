import { toUtcISOString } from "@/utils/date";
import axiosInstance from "../axiosInstance";

export type TelemetryCharacteristic = "BATTERY" | "ERROR" | "TEMPERATURE";

interface TelemetryPayload {
  device_id: string;
  created_at?: string;
  characteristic: TelemetryCharacteristic;
  value: number | string;
}

export async function sendTelemetry(
  treatmentId: string,
  payload: TelemetryPayload
): Promise<void> {
  const body = {
    ...payload,
    created_at: payload.created_at ?? toUtcISOString(),
  };

  await axiosInstance.post(`/telemetry/${treatmentId}`, body);
}
