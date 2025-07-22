import * as SecureStore from "expo-secure-store";

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