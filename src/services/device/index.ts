import { AuthorizedDevice } from "../../types/device";
import axiosInstance from "../axiosInstance";

/**
 * Fetches the list of devices that are authorized to connect to the user's account
 * @returns The list of authorized devices
 */
export const getValidoseDevices = async (): Promise<AuthorizedDevice[]> => {
  const response = await axiosInstance.get<AuthorizedDevice[]>("/devices");
  return response.data;
}
