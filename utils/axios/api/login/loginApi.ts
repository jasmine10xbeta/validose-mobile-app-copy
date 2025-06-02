import axiosInstance from "../../axiosInstance";
import { storeToken } from "../token";

/**
 * Attempts to refresh the user's session by sending the current token to the backend.
 * @param token - The current token to refresh.
 * @returns The new token and access token data if the refresh succeeds, null otherwise.
 */
export const refreshSession = async (token: string) => {
  try {
    const response = await axiosInstance("/auth/refresh", {     // TODO: Update endpoint
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
      },
    });

    const data = response?.data;

    if (!data) return null;

    await storeToken(data?.token, data?.access_token);
    return data;
  } catch (e) {
    return null;
  }
};