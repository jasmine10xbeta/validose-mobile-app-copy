/**
 * Simulates sending onboarding code and mobile device ID to the backend to pair the mobile device with the patient.
 * @param code - The unique onboarding code scanned from the QR.
 * @param deviceId - The unique identifier for the current mobile device.
 * @returns The simulated response data (e.g., token, device/user metadata).
 */
export const onboardWithCode = async (code: string, deviceId: string) => {
  await new Promise((res) => setTimeout(res, 500)); // Simulate delay

  const response = {
    access_token:
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." +
      "eyJzdWIiOiJVU0VSX0lEIiwicm9sZSI6InBhdGllbnQiLCJleHAiOjE3MjAwMDAwMDAsImlhdCI6MTcxOTk5NjQwMCwiYXVkIjoidmFsaWRvc2UtYXBwIiwiaXNzIjoidmFsaWRvc2UtYXBpIn0." +
      "dummysignature", // this is a valid JWT-like format
    refresh_token: {
      token: "refresh-token-uuid-abc-123",
      user_id: "USER_ID",
      device_id: "DEVICE_456",
      expires_at: new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString(), // 30 days from now
      revoked: false,
    },
  };

  return response;
};
