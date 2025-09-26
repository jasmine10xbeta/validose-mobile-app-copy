import { getLocalISOString } from "../../utils/schedule";
import axiosInstance from "../axiosInstance";

/**
 * Fetches the list of treatments from the API
 *
 * @returns {Promise<any>} A promise that resolves with the list of treatments
 */
export const getTreatments = async (): Promise<any> => {
  const response = await axiosInstance.get<any>("/treatments");
  return response.data;
};

/**
 * Fetches the list of schedules from the API, filtered by the given treatment ID
 * and optional filters. The `sort` parameter allows for sorting by any field.
 *
 * @param {string} treatmentId The ID of the treatment to fetch schedules for
 * @param {any} filters Optional filters to apply to the schedules query,
 *   as a JSON-serializable object
 * @param {string} [sort="event_at:asc"] The field to sort the schedules by,
 *   with optional ascending/descending order. e.g. "event_at:desc"
 * @returns {Promise<any>} A promise that resolves with the list of schedules
 */
export const getSchedules = async (
  treatmentId: string,
  filters: any,
  sort: string = "event_at:asc"
): Promise<any> => {
  const response = await axiosInstance.get<any>(
    `/schedules/${treatmentId}?sort=${sort}&filters=${encodeURIComponent(JSON.stringify(filters))}`
  );
  return response.data;
};

/**
 * Sends a dose event to the server.
 *
 * @param {any} dose The dose event received from the device, as a JSON-serializable object
 * @param {string} deviceId The ID of the device that sent the dose event
 * @param {string} medication_code The medication code associated with the dose event
 * @returns {Promise<any>} A promise that resolves with the server response
 */
export const sendDoseEvent = async (
  dose: any,
  deviceId: string,
  medication_code: string
): Promise<any> => {
  const payload = {
    // event_id: dose.id,
    medication_code: medication_code,
    device_id: deviceId,
    dose_id: `${dose?.event_id?.days_since_epoch}-${dose?.event_id?.event_ctr}`,
    dose_state: dose?.dose_state || 0,
    dose_amount_mg: dose?.dose_amount_mg || 0,
    dose_event_at: dose?.dose_event_at || getLocalISOString()
  };

  const res = await axiosInstance.post("/dose-events", payload);
  return res.data;
};
