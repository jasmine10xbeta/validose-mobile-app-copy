import * as SecureStore from "expo-secure-store";

export async function storeToken(accessToken: string, refreshToken?: object) {
  await SecureStore.setItemAsync("authToken", accessToken);
  if (refreshToken) {
    await SecureStore.setItemAsync("refreshToken", JSON.stringify(refreshToken));
  }
}

export async function getToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("authToken");
}

export async function clearToken() {
  await SecureStore.deleteItemAsync("authToken");
}