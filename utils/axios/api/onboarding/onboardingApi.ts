import axiosInstance from "../../axiosInstance";
import { storeToken } from "../token";

/**
 * Sends onboarding code and mobile device ID to the backend to pair the mobile device with the patient.
 * @param code - The unique onboarding code scanned from the QR.
 * @param deviceId - The unique identifier for the current mobile device.
 * @returns The complete response data from the backend (e.g., token, device/user metadata).
 */
export const onboardWithCode = async (code: string, deviceId: string) => {
  const response = await axiosInstance.post("/auth/pair", {
    code,
    device_id: deviceId,
  });

  if (response?.data?.token) {
    await storeToken(response.data.token, response.data?.access_token);
  }

  return response?.data;
};
