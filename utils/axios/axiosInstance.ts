import axios, { AxiosError, AxiosResponse } from "axios";
import { getToken } from "./api/token/tokenApi";

const BASE_URL = process.env.BASE_URL || '';
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

    return config;
  },
  Promise.reject
);

// Response Interceptor – handle 5xx/4xx errors here
axiosInstance.interceptors.response.use(
  (response: AxiosResponse) => {
    return response;
  },
  (error: AxiosError) => {
    const status = error.response?.status;
    const apiData = error.response?.data as { message?: string | undefined };

    if (!error.response) {
      // Network error (no response at all)
      console.error("Network error:", apiData?.message);
      return Promise.reject(new Error("Network error, please try again."));
    }

    // console.warn("API error:", status, apiData?.message);
    return Promise.reject(error);
  }
);

export default axiosInstance;
