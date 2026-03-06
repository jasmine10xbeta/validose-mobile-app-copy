import { toUtcISOString } from "@/utils/date";
import axiosInstance from "../axiosInstance";
import { encodeHexPayloadForIngest } from "./encoding";

export type IngestRawHardwareDataRequest = {
  payloadHex: string;
  deviceId: string;
  timestamp?: unknown;
};

export { encodeHexPayloadForIngest } from "./encoding";

export async function ingestRawHardwareData({
  payloadHex,
  deviceId,
  timestamp,
}: IngestRawHardwareDataRequest): Promise<void> {
  const body = {
    payload: encodeHexPayloadForIngest(payloadHex),
    timestamp: toUtcISOString(timestamp),
    device_id: deviceId,
  };

  await axiosInstance.post("/hardware/ingest", body);
}
