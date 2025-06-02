import * as SecureStore from "expo-secure-store";
import { jwtDecode } from "jwt-decode";

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

export const isTokenValid = (token: string): boolean => {
  try {
    const decoded: any = jwtDecode(token);
    const currentTime = Math.floor(Date.now() / 1000);
    return decoded.exp > currentTime;
  } catch (e) {
    console.log("Error decoding token:", e);
    return false;
  }
};