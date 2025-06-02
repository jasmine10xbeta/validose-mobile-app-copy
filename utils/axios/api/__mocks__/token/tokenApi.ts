import * as SecureStore from "expo-secure-store";

export async function storeToken(token: string) {
  await SecureStore.setItemAsync("authToken", token);
}

export async function getToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("authToken");
}

export async function clearToken() {
  await SecureStore.deleteItemAsync("authToken");
}