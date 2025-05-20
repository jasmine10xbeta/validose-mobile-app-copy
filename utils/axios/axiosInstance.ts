import { fetchAuthSession } from "aws-amplify/auth";
import axios from "axios";

const axiosInstance = axios.create({
  baseURL: process.env.API_URL,
  headers: {
    "Content-Type": "application/json",
  },
});

axiosInstance.interceptors.request.use(
  async (config) => {
    try {
      const session = await fetchAuthSession();
      const jwtToken = session?.tokens?.idToken?.toString();

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
