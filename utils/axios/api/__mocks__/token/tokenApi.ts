import * as SecureStore from "expo-secure-store";
import { jwtDecode } from "jwt-decode";

/**
 * Stores the provided access token and optional refresh token in SecureStore.
 * @param accessToken - The access token to be stored.
 * @param refreshToken - Optional refresh token to be stored. If provided, it is stringified and stored in SecureStore.
 */
export async function storeTokens(accessToken: string, refreshToken?: string) {
  await SecureStore.setItemAsync("authToken", accessToken);
  if (refreshToken) {
    await SecureStore.setItemAsync("refreshToken", refreshToken);
  }
}

/**
 * Retrieves the stored access token from the SecureStore.
 * @returns The stored access token if present, null otherwise.
 */
export async function getToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("authToken");
}

/**
 * Retrieves the stored refresh token from the SecureStore.
 * @returns The stored refresh token if present, null otherwise.
 */
export async function getRefreshToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("refreshToken");
}

/**
 * Deletes the stored access and refresh tokens from the SecureStore.
 * Used when user logs out.
 */
export async function clearTokens() {
  await SecureStore.deleteItemAsync("authToken");
  await SecureStore.deleteItemAsync("refreshToken");
}

/**
 * Checks if the given string is in valid JWT format.
 * A JWT should have 3 parts separated by dots.
 */
const isProperJwtFormat = (token: string | null | undefined): boolean => {
  return typeof token === "string" && token.split(".").length === 3;
};

/**
 * Validates whether a JWT is not expired and has a valid format.
 * @param token - JWT access token
 * @returns true if valid, false if expired or malformed
 */
export const isTokenValid = (token: string): boolean => {
  if (!isProperJwtFormat(token)) {
    console.log("Invalid token format");
    return false;
  }

  try {
    const decoded: any = jwtDecode(token);
    const currentTime = Math.floor(Date.now() / 1000); // seconds
    return decoded.exp && decoded.exp > currentTime;
  } catch (e) {
    console.log("Error decoding token:", e);
    return false;
  }
};