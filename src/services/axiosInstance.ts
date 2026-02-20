import axios from "axios/dist/browser/axios.cjs";
import { getAccessToken, getRefreshToken, invokeSignIn, invokeSignOut } from "@/providers/auth";
import { AuthTokenResponse } from "@/types/auth";
import { logAPIRequest, logAPIResponse, logAPIError } from "@/utils/log";
import type { AxiosError, AxiosResponse } from "axios";

// Base URL and API suffix for requests
const BASE_URL = "https://api.stg.aws.validose.com";
const SUFFIX = "/api/mobile";

if (!BASE_URL) {
  console.warn(
    "BASE_URL environment variable is not set. API requests may fail."
  );
}

const MAX_REFRESH_ATTEMPTS = 3;

async function sleep(ms: number) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export async function refreshAccessToken(): Promise<AuthTokenResponse> {
  const refresh_token = await getRefreshToken();

  if (!refresh_token) {
    await invokeSignOut().catch((err) =>
      console.error("Failed to sign out after missing refresh token:", err)
    );
    throw new Error("No refresh token available");
  }

  let lastError: unknown;

  for (let attempt = 1; attempt <= MAX_REFRESH_ATTEMPTS; attempt++) {
    try {
      const { data } = await axios.post<AuthTokenResponse>(
        `${BASE_URL || ""}${SUFFIX}/auth/refresh`,
        { refresh_token },
        { headers: { "Content-Type": "application/json" } }
      );
      return data;
    } catch (error) {
      lastError = error;

      if (attempt < MAX_REFRESH_ATTEMPTS) {
        const backoffMs = attempt * 300;
        console.warn(
          `Refresh token attempt ${attempt} failed. Retrying in ${backoffMs}ms...`
        );
        await sleep(backoffMs);
        continue;
      }
    }
  }

  await invokeSignOut().catch((err) =>
    console.error("Failed to sign out after refresh retries:", err)
  );

  throw lastError instanceof Error
    ? lastError
    : new Error("Unable to refresh session");
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
        console.error("Token refresh failed after retries:", refreshError);
        return Promise.reject(refreshError);
      }
    }

    return Promise.reject(error);
  }
);

export default axiosInstance;
