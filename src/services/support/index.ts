import { SupportRequest } from "@/types/support";
import axiosInstance from "../axiosInstance";

/**
 * Creates a new support request.
 * @returns {Promise<any>} The newly created support request object.
 */

export const createSupportRequest = async (): Promise<SupportRequest> => {
  const response = await axiosInstance.post("/support-requests");
  return response.data;
}
