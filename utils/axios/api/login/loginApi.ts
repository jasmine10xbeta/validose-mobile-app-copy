import { invokeSignIn } from "@/utils/provider/authManager";
import axiosInstance from "../../axiosInstance";
import { AuthTokenResponse } from "../onboarding";
import { getRefreshToken } from "../token";

/**
 * Refreshes the user's access token by using the stored refresh token.
 *
 * Retrieves the stored refresh token, uses it to request a new access token
 * from the backend, and then uses the new access token to sign in the user.
 *
 * If the refresh token is not valid, the user is signed out. If the refresh
 * token is not present, the user is signed out.
 */
export async function refreshAccessToken() {
  const refreshToken = await getRefreshToken();
  const newSession = await axiosInstance.post<AuthTokenResponse>("/auth/refresh", {
    refresh_token: refreshToken,
  });

  await invokeSignIn(newSession.data);
  return newSession.data;
}

/**
 * Logs in the user by making a request to the backend login endpoint.
 *
 * Sends a POST request to the "/auth/login" endpoint to obtain a new session
 * with access and refresh tokens. Once the tokens are received, it invokes the
 * sign-in process with the new session data.
 */
export async function login() {
  const newSession = await axiosInstance.post<AuthTokenResponse>("/auth/login");
  await invokeSignIn(newSession.data);
}