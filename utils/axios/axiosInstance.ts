import axios from "axios";
import { getValidToken } from "../amplifyAWS/authService";

// Validate API URL
if (!process.env.API_URL) {
  console.log('API_URL environment variable is not set. API requests may fail.');
}

const axiosInstance = axios.create({
  baseURL: process.env.API_URL || '',
  headers: {
    "Content-Type": "application/json",
  },
});

axiosInstance.interceptors.request.use(
  async (config) => {
    try {
      const jwtToken = await getValidToken();

      if (jwtToken) {
        config.headers.Authorization = `Bearer ${jwtToken}`;
      }

      return config;
    } catch (error) {
      console.error("Error getting Amplify session for Axios request:", error);
      return Promise.reject(error);
    }
  },
  (error) => {
    return Promise.reject(error);
  }
);

export default axiosInstance;
