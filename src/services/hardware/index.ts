import { toUtcISOString } from "@/utils/date";
import axiosInstance from "../axiosInstance";
import { encodePacketBytesForIngest } from "./encoding";

export type IngestRawHardwareDataRequest = {
  packetBytes: Uint8Array;
  deviceId: string;
  timestamp?: unknown;
};

export { encodePacketBytesForIngest } from "./encoding";

export async function ingestRawHardwareData({
  packetBytes,
  deviceId,
  timestamp,
}: IngestRawHardwareDataRequest): Promise<void> {
  const body = {
    payload: encodePacketBytesForIngest(packetBytes),
    timestamp: toUtcISOString(timestamp),
    device_id: deviceId,
  };

  await axiosInstance.post("/hardware/ingest", body);
}
