import { SupportRequest } from "@/types/support";
import axiosInstance from "../axiosInstance";

/**
 * Creates a new support request.
 */
export const createSupportRequest = async (): Promise<SupportRequest> => {
  const response = await axiosInstance.post("/support-requests");
  return response.data;
};

/**
 * Fetches the latest open support requests for the inbox view.
 */
export const getSupportRequests = async (): Promise<SupportRequest[]> => {
  const response = await axiosInstance.get<any>(
    "/support-requests",
    {
      params: {
        limit: 50,
        offset: 0,
        sort: "created_at:desc",
      },
    }
  );

  return response?.data?.data || [];
};
