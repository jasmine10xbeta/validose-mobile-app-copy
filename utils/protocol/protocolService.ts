import AsyncStorage from "@react-native-async-storage/async-storage";
import { getBackendProtocol } from "./mockProtocol"; // TODO: Simulated API

const PROTOCOL_KEY = "treatment_protocol";

export const syncProtocol = async () => {
  const local = await AsyncStorage.getItem(PROTOCOL_KEY);
  const localProtocol = local ? JSON.parse(local) : null;

  // TODO: Replace with real API
  const backendProtocol = await getBackendProtocol();

  if (!localProtocol || backendProtocol.version > localProtocol.version) {
    await AsyncStorage.setItem(PROTOCOL_KEY, JSON.stringify(backendProtocol));
    return { protocol: backendProtocol, updated: true };
  }

  return { protocol: localProtocol, updated: false };
};