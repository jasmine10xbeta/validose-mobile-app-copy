interface RefreshToken {
  token: string;
  user_id: string;
  device_id: string;
  expires_at: string;
  revoked: boolean;
}

interface RefreshResponse {
  access_token: string;
  refresh_token: RefreshToken;
}

/**
 * Simulates refreshing the user's session using a refresh token.
 * @param refreshToken - The current refresh token to refresh session.
 * @returns The refreshed token data including new access and refresh tokens.
 */
export const refreshSession = async (
  refreshToken: string
): Promise<RefreshResponse> => {
  console.log("Simulating API call to backend to refresh session..");

  // Simulate API delay
  await new Promise((res) => setTimeout(res, 500));

  const response: RefreshResponse = {
    access_token:
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." +
      "eyJzdWIiOiJVU1IxMjMiLCJyb2xlIjoiY2xpbmljaWFuIiwiZXhwIjoxNzEwMDAwMDAwLCJpYXQiOjE3MDk5OTY0MDAsImF1ZCI6InZhbGlkb3NlLWFwcCIsImlzcyI6InZhbGlkb3NlLWFwaSJ9." +
      "dummysignaturepart123456",
    refresh_token: {
      token: "refresh-token-uuid",
      user_id: "USR123",
      device_id: "DEVICE-123",
      expires_at: new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString(),
      revoked: false,
    },
  };

  console.log("Session refresh response:", response);
  return response;
};
