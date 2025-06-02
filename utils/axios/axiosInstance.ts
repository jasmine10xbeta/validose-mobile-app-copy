import axios, { AxiosError, AxiosResponse } from "axios";
import { getToken } from "./api/token/tokenApi";

const API_URL = process.env.API_URL || '';
const PREFIX = '/api/mobile';

if (!API_URL) {
  console.warn("API_URL environment variable is not set. API requests may fail.");
}

// Create axios instance
const axiosInstance = axios.create({
  baseURL: API_URL + PREFIX,
  headers: {
    "Content-Type": "application/json",
  },
});

// Request Interceptor – attach token
axiosInstance.interceptors.request.use(
  async (config) => {
    try {
      const jwtToken = await getToken();
      if (jwtToken) config.headers.Authorization = `Bearer ${jwtToken}`;

      return config;
    } catch (error) {
      console.error("Token fetch error:", error);
      return config;
    }
  },
  (error) => {
    return Promise.reject(error);
  }
);

// Response Interceptor – handle 5xx/4xx errors here
axiosInstance.interceptors.response.use(
  (response: AxiosResponse) => {
    return response;
  },
  (err: AxiosError) => {
    const status = err.response?.status || 500;
    switch (status) {
      // authentication (token related issues)
      case 401: {
        return {
          status: 401,
          message: "Unauthorized",
        };
      }

      // forbidden (permission related issues)
      case 403: {
        return {
          status: 403,
          message: "Forbidden",
        };
      }

      // bad request
      case 400: {
        return {
          status: 400,
          message: "Bad request",
        };
      }

      // not found
      case 404: {
        return {
          status: 404,
          message: "Not found",
        };
      }

      // conflict
      case 409: {
        return {
          status: 409,
          message: "Conflict",
        };
      }

      // unprocessable
      case 422: {
        return {
          status: 422,
          message: "Unprocessable",
        };
      }

      // generic api error (server related) unexpected
      default: {
        return {
          status: 500,
          message: "Server error",
        };
      }
    }
  }
);

export default axiosInstance;
