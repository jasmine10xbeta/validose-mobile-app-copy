import { storeToken } from "../token";

/**
 * Simulates sending onboarding code and mobile device ID to the backend to pair the mobile device with the patient.
 * @param code - The unique onboarding code scanned from the QR.
 * @param deviceId - The unique identifier for the current mobile device.
 * @returns The simulated response data (e.g., token, device/user metadata).
 */
export const onboardWithCode = async (code: string, deviceId: string) => {
  await new Promise((res) => setTimeout(res, 500)); // Simulate delay

  const response = {
    access_token: {
      sub: "USR123",
      role: "clinician",
      device_id: "DEVICE-123",
      exp: 1710000000,
      iat: 1709996400,
      aud: "validose-app",
      iss: "validose-api",
    },
    refresh_token: {
      token: "UUID",
      user_id: "USR123",
      device_id: "DEVICE-123",
      expires_at: "2025-05-01T00:00:00Z",
      revoked: false,
    },
  };

  if (response?.refresh_token?.token) {
    await storeToken(response.refresh_token.token, response.access_token);
  }

  return response;
};
