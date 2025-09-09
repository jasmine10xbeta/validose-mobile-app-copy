import { getUniqueId, getSystemName } from "react-native-device-info";
import { AuthTokenResponse } from "@/types/auth";
import axiosInstance from "../axiosInstance";

/**
 * Onboards a user with a given onboarding code.
 *
 * @param {string} code The onboarding code.
 * @returns {Promise<AuthTokenResponse>} A promise that resolves with an AuthTokenResponse.
 */
export const onboardWithCode = async (code: string): Promise<AuthTokenResponse> => {
  const mobile_id = await getUniqueId();
  const platform = getSystemName();
  const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;

  const { data } = await axiosInstance.post<AuthTokenResponse>("/auth/register", {
    onboarding_code: code,
    mobile_id,
    platform,
    timezone,
  });
  return data;
};

/**
 * Logs in an existing user, returning an access token for API calls.
 *
 * @returns {Promise<AuthTokenResponse>} A promise that resolves with an AuthTokenResponse.
 */
export async function login(): Promise<AuthTokenResponse> {
  const mobile_id = await getUniqueId();

  const { data } = await axiosInstance.post<AuthTokenResponse>("/auth/login", { mobile_id });
  return data;
}