import { DoseRecord } from "@/store/useDoseHistoryStore";
import axiosInstance from "../../axiosInstance";

/**
 * Fetch dosage data for a specific device.
 * @param deviceId - The unique identifier for the validose device.
 * @returns Device dosage information from the backend.
 */
export const getLatestTreatmentProtocol = async (deviceId: string) => {
  try {
    const response = await axiosInstance.get(`/dosage/${deviceId}`); // TODO: Update endpoint
    return response.data;
  } catch (error: any) {
    console.error("Failed to fetch dosage", error.response?.data || error.message);
    // throw error;
  }
};

export const sendDoseRecordsToBackend = async (records: DoseRecord[]): Promise<void> => {
  // TODO: Implement the logic to send the records to the backend
}