import axiosInstance from "../../axiosInstance";

/**
 * Attempts to refresh the user's session by sending the current token to the backend.
 * @param token - The current token to refresh.
 * @returns The new token and access token data if the refresh succeeds, null otherwise.
 */
export const refreshSession = async (token: string) => {
  try {
    console.log("Making API call to backend to refresh session..");
    
    const response = await axiosInstance("/auth/refresh", {     // TODO: Update endpoint
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
      },
    });

    const data = response?.data;
    console.log("Session refresh response:", data);

    if (!data) return null;
    return data;
  } catch (e) {
    return null;
  }
};