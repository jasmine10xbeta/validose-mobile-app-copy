import { Camera } from "expo-camera";
import * as Notifications from "expo-notifications";
import { Permission, PermissionsAndroid, Platform } from "react-native";

function getAndroidApiLevel(): number {
  const rawVersion = Platform.Version;
  if (typeof rawVersion === "number") return rawVersion;
  const parsed = Number.parseInt(String(rawVersion), 10);
  return Number.isFinite(parsed) ? parsed : 0;
}

function getRequiredAndroidBlePermissions(): Permission[] {
  if (Platform.OS !== "android") return [];

  const androidApi = getAndroidApiLevel();
  const required: Permission[] = [];

  if (androidApi >= 31) {
    required.push(
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
    );
  }
  if (androidApi >= 23) {
    required.push(PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION);
  }

  return Array.from(new Set(required));
}

async function getMissingAndroidBlePermissions(): Promise<Permission[]> {
  if (Platform.OS !== "android") return [];

  const required = getRequiredAndroidBlePermissions();
  if (!required.length) return [];

  const missing: Permission[] = [];
  for (const permission of required) {
    const granted = await PermissionsAndroid.check(permission);
    if (!granted) {
      missing.push(permission);
    }
  }

  return missing;
}

export async function hasAndroidBlePermissions(): Promise<boolean> {
  if (Platform.OS !== "android") return true;

  try {
    const missing = await getMissingAndroidBlePermissions();
    return missing.length === 0;
  } catch (error) {
    console.warn("[Permissions] Failed to check Android BLE permissions.", error);
    return false;
  }
}

export async function ensureAndroidBlePermissions(): Promise<boolean> {
  if (Platform.OS !== "android") return true;

  try {
    const missing = await getMissingAndroidBlePermissions();
    if (!missing.length) return true;

    const result = await PermissionsAndroid.requestMultiple(missing);
    return missing.every(
      (permission) => result[permission] === PermissionsAndroid.RESULTS.GRANTED,
    );
  } catch (error) {
    console.warn("[Permissions] Failed to request Android BLE permissions.", error);
    return false;
  }
}

export async function hasCameraPermission(): Promise<boolean> {
  try {
    const current = await Camera.getCameraPermissionsAsync();
    return current.granted;
  } catch (error) {
    console.warn("[Permissions] Failed to check camera permission.", error);
    return false;
  }
}

export async function ensureCameraPermission(): Promise<boolean> {
  try {
    const granted = await hasCameraPermission();
    if (granted) return true;

    const requested = await Camera.requestCameraPermissionsAsync();
    return requested.granted;
  } catch (error) {
    console.warn("[Permissions] Failed to request camera permission.", error);
    return false;
  }
}

export async function hasNotificationPermission(): Promise<boolean> {
  try {
    const current = await Notifications.getPermissionsAsync();
    return current.granted;
  } catch (error) {
    console.warn("[Permissions] Failed to check notification permission.", error);
    return false;
  }
}

export async function ensureNotificationPermission(): Promise<boolean> {
  try {
    const current = await Notifications.getPermissionsAsync();
    if (current.granted) return true;
    if (current.canAskAgain === false) return false;

    const requested = await Notifications.requestPermissionsAsync();
    return requested.granted;
  } catch (error) {
    console.warn("[Permissions] Failed to request notification permission.", error);
    return false;
  }
}

export async function requestOnboardingPermissions(): Promise<{
  cameraGranted: boolean;
  bluetoothGranted: boolean;
  notificationGranted: boolean;
}> {
  const cameraGranted = await ensureCameraPermission();
  const bluetoothGranted = await ensureAndroidBlePermissions();
  const notificationGranted = await ensureNotificationPermission();

  return { cameraGranted, bluetoothGranted, notificationGranted };
}
