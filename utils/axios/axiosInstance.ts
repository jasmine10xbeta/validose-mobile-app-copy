import axios, { AxiosError, AxiosResponse } from "axios";
import { refreshAccessToken } from "./api/login";
import { getToken } from "./api/token/tokenApi";

// const BASE_URL = process.env.BASE_URL || '';
const BASE_URL = "https://api.dev.aws.validose.com";
const SUFFIX = '/api/mobile';

if (!BASE_URL) {
  console.warn("BASE_URL environment variable is not set. API requests may fail.");
}

// Create axios instance
const axiosInstance = axios.create({
  baseURL: `${BASE_URL || ""}${SUFFIX}`,
  headers: { "Content-Type": "application/json" },
  // timeout: 10000,
});

function printRequest(config: any) {
  const fullUrl = `${config.baseURL || ''}${config.url || ''}`;
  console.log("➡️ [Request]");
  console.log(`URL: ${fullUrl}`);
  console.log("Method:", config.method?.toUpperCase());
  console.log("Headers:", config.headers);
  console.log("Payload:", config?.data ? JSON.stringify(config?.data) : "-");
  console.log("\n");
}

function printResponse(response: AxiosResponse) {
  const fullUrl = `${response.config.baseURL || ''}${response.config.url || ''}`;
  console.log("✅ [Response]");
  console.log(`URL: ${fullUrl}`);
  console.log("Status:", response.status);
  console.log("Data:", JSON.stringify(response.data));
}

function printError(error: AxiosError) {
  console.log("❌ [Error]");
  console.log("Status:", error.response?.status);
  console.log("Data:", JSON.stringify(error.response?.data));
}

// Request Interceptor – attach token
axiosInstance.interceptors.request.use(
  async (config) => {
    const token = await getToken().catch((error) => {
      console.error("Failed to get token:", error);
      return null;
    });

    if (token) {
      config.headers.Authorization = `Bearer ${token}`;
    }

    printRequest(config);
    return config;
  },
  Promise.reject
);

// Response Interceptor – handle 5xx/4xx errors here
axiosInstance.interceptors.response.use(
  (response: AxiosResponse) => {
    printResponse(response);
    return response;
  },
  async (error: AxiosError) => {
    const status = error.response?.status;
    const originalRequest = error.config;
    const apiData = error.response?.data as { message?: string | undefined };

    printError(error);

    if (!error.response) {
      // Network error (no response at all)
      console.error("Network error:", apiData?.message);
      return Promise.reject(new Error("Network error, please try again."));
    }

    // Handle 401 Unauthorized by refreshing access token
    if (status === 401 && originalRequest) {
      try {
        const newSession = await refreshAccessToken();
        originalRequest.headers.Authorization = `Bearer ${newSession.access_token}`;

        console.warn("Retrying request with new token...");
        return axiosInstance(originalRequest); // retry original request
      } catch (refreshError) {
        console.error("Token refresh failed:", refreshError);
        // router.replace("/login");
        return Promise.reject(refreshError);
      }
    }

    return Promise.reject(error);
  }
);

export default axiosInstance;
