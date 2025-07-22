import { getUniqueId, getSystemName } from "react-native-device-info";
import axiosInstance from "../../axiosInstance";

export interface AuthTokenResponse {
  access_token: string,
  refresh_token: string
}

export interface DeviceList {
  device_ids: string[]
}

/**
 * Sends onboarding code and mobile device ID to the backend to pair the mobile device with the patient.
 * @param code - The unique onboarding code scanned from the QR.
 * @returns The complete response data from the backend (e.g., token, device/user metadata).
 */
export const onboardWithCode = async (code: string): Promise<AuthTokenResponse> => {
  const mobile_id = await getUniqueId();
  const platform = getSystemName();
  const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;

  console.log("Payload data", mobile_id, platform, timezone);
  const response = await axiosInstance.post<AuthTokenResponse>("/auth/register", {
    onboarding_code: code,
    mobile_id,
    platform,
    timezone 
  });

  return response.data;
};

/**
 * Fetches all validose devices associated with the user.
 * @returns An array of device IDs.
 */
export const getValidoseDevices = async (): Promise<DeviceList> => {
  const response = await axiosInstance.get<DeviceList>("/devices");
  return response.data;
}
