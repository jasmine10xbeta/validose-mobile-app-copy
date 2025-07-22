interface RefreshResponse {
  access_token: string;
  refresh_token: string;
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
    "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." +
      "eyJzdWIiOiJVU1IxMjMiLCJyb2xlIjoiY2xpbmljaWFuIiwiZXhwIjoxNzEwMDAwMDAwLCJpYXQiOjE3MDk5OTY0MDAsImF1ZCI6InZhbGlkb3NlLWFwcCIsImlzcyI6InZhbGlkb3NlLWFwaSJ9." +
      "dummysignaturepart123456",
    "refresh_token": "refresh-token-uuid"
  }

  console.log("Session refresh response:", response);
  return response;
};
