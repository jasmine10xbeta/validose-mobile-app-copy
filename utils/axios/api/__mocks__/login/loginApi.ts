import { storeToken } from "../token";

/**
 * Simulates to refresh the user's session.
 * @param token - The current token to refresh.
 * @returns The new token and access token data.
 */
export const refreshSession = async (token: string) => {
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

  if (response.refresh_token.token) {
    await storeToken(response.refresh_token.token, response.access_token);
  }

  return response;
};