import * as SecureStore from "expo-secure-store";

export async function storeToken(refreshToken: string, accessToken?: object) {
  await SecureStore.setItemAsync("authToken", refreshToken);
  if (accessToken) {
    await SecureStore.setItemAsync("accessToken", JSON.stringify(accessToken));
  }
}

export async function getToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("authToken");
}

export async function clearToken() {
  await SecureStore.deleteItemAsync("authToken");
}