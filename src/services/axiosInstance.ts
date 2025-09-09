import axios, { AxiosError, AxiosResponse } from "axios";
import { getAccessToken, getRefreshToken, invokeSignIn } from "@/providers/auth";
import { AuthTokenResponse } from "@/types/auth";
import { logAPIRequest, logAPIResponse, logAPIError } from "@/utils/log";

// Base URL and API suffix for requests
const BASE_URL = "https://api.stg.aws.validose.com";
const SUFFIX = "/api/mobile";

if (!BASE_URL) {
  console.warn(
    "BASE_URL environment variable is not set. API requests may fail."
  );
}

export async function refreshAccessToken(): Promise<AuthTokenResponse> {
  const refresh_token = await getRefreshToken();
  const { data } = await axiosInstance.post<AuthTokenResponse>("/auth/refresh", {
    refresh_token,
  });
  return data;
}

// Create axios instance with base URL and JSON headers
const axiosInstance = axios.create({
  baseURL: `${BASE_URL || ""}${SUFFIX}`,
  headers: { "Content-Type": "application/json" },
  // timeout: 10000, // optional timeout
});

// Request interceptor to add Authorization header with access token
axiosInstance.interceptors.request.use(async (config) => {
  const token = await getAccessToken().catch((error) => {
    console.error("Failed to get token:", error);
    return null;
  });

  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }

  logAPIRequest(config);
  return config;
}, Promise.reject);

// Response interceptor to handle responses and token refresh on 401
axiosInstance.interceptors.response.use(
  (response: AxiosResponse) => {
    logAPIResponse(response);
    return response;
  },
  async (error: AxiosError) => {
    const status = error.response?.status;
    const originalRequest = error.config;
    const apiData = error.response?.data as { message?: string | undefined };

    logAPIError(error);

    if (!error.response) {
      // Network error (no response at all)
      console.error("Network error:", apiData?.message);
      return Promise.reject(new Error("Network error, please try again."));
    }

    // Handle 401 Unauthorized by refreshing access token
    if (status === 401 || (status === 403 && originalRequest)) {
      try {
        const newSession = await refreshAccessToken();
        await invokeSignIn(newSession);
        
        if (originalRequest && originalRequest.headers && newSession.access_token) {
            // Update authorization header with new token
            originalRequest.headers.Authorization = `Bearer ${newSession.access_token}`;

            console.warn("Retrying request with new token..");
            return axiosInstance(originalRequest); // retry original request
        }
      } catch (refreshError) {
        console.error("Token refresh failed:", refreshError);
        return Promise.reject(refreshError);
      }
    }

    return Promise.reject(error);
  }
);

export default axiosInstance;
