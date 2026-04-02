import AsyncStorage from "@react-native-async-storage/async-storage";
import { useFocusEffect } from "@react-navigation/native";
import { useRouter } from "expo-router";
import { useCallback, useMemo, useState } from "react";
import {
  ActivityIndicator,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import { SafeAreaView, useSafeAreaInsets } from "react-native-safe-area-context";

import { validoseAqua3, validoseDarkBlue, validoseGrey, validoseWhite } from "@/constants/colors";
import {
  connectAndSetupDeviceWithTimeout,
  DEFAULT_CONNECT_AND_SETUP_TIMEOUT_MS,
} from "@/utils/ble";
import { addBleDebugLog } from "@/utils/ble/debugLogStore";
import { hasAndroidBlePermissions } from "@/utils/permissions/androidRuntime";
import {
  getConnectedDevice,
  isDeviceConnected,
  scanLeDevice,
} from "../../../../../../modules/tenx-mdk-ble-rn-library/src/index";

type ScanDevice = {
  deviceName?: string;
  deviceAddress?: string;
  rssi?: number;
  isConnectable?: boolean;
};

type StoredLastDebugDevice = {
  deviceName: string;
  deviceAddress: string;
  connectedAtMs: number;
};

const LAST_DEBUG_DEVICE_STORAGE_KEY = "ble-debug:last-connected-device:v1";
const BUILD_INFO_BADGE_CLEARANCE_PX = 72;

function getSignalTone(rssi?: number) {
  if (typeof rssi !== "number") {
    return {
      label: "Unknown",
      textColor: validoseGrey,
      backgroundColor: "#F2F4F7",
    };
  }

  if (rssi >= -60) {
    return {
      label: "Excellent",
      textColor: "#087443",
      backgroundColor: "#ECFDF3",
    };
  }

  if (rssi >= -75) {
    return {
      label: "Good",
      textColor: "#0B5F71",
      backgroundColor: "#EAF6F8",
    };
  }

  if (rssi >= -90) {
    return {
      label: "Fair",
      textColor: "#9A6700",
      backgroundColor: "#FFFAEB",
    };
  }

  return {
    label: "Weak",
    textColor: "#B42318",
    backgroundColor: "#FEF3F2",
  };
}

function toLogPayload(value: unknown): string {
  try {
    return JSON.stringify(value, null, 2);
  } catch {
    return String(value);
  }
}

function logDebug(message: string, payload?: unknown) {
  const body = payload === undefined ? "" : `\n${toLogPayload(payload)}`;
  addBleDebugLog(`${new Date().toLocaleTimeString()}  ${message}${body}`);
}

async function saveLastDebugDevice(device: ScanDevice) {
  const record: StoredLastDebugDevice = {
    deviceName: device.deviceName ?? "",
    deviceAddress: device.deviceAddress ?? "",
    connectedAtMs: Date.now(),
  };

  await AsyncStorage.setItem(LAST_DEBUG_DEVICE_STORAGE_KEY, JSON.stringify(record));
}

function normalizeDevices(raw: unknown): ScanDevice[] {
  if (!Array.isArray(raw)) return [];

  const seen = new Set<string>();
  const mapped: ScanDevice[] = [];

  raw.forEach((item) => {
    if (!item || typeof item !== "object") return;
    const row = item as Record<string, unknown>;
    const deviceName = typeof row.deviceName === "string" ? row.deviceName.trim() : "";
    const deviceAddress = typeof row.deviceAddress === "string" ? row.deviceAddress.trim() : "";

    if (!deviceName || !deviceName.toUpperCase().startsWith("VAL")) return;

    const key = deviceAddress || deviceName;
    if (!key || seen.has(key)) return;
    seen.add(key);

    mapped.push({
      deviceName,
      deviceAddress,
      rssi: typeof row.rssi === "number" ? row.rssi : undefined,
      isConnectable: typeof row.isConnectable === "boolean" ? row.isConnectable : undefined,
    });
  });

  return mapped.sort((a, b) => {
    const aRssi = a.rssi ?? -999;
    const bRssi = b.rssi ?? -999;
    return bRssi - aRssi;
  });
}

export default function BleDebugConsoleScreen() {
  const router = useRouter();
  const insets = useSafeAreaInsets();
  const [isScanning, setIsScanning] = useState(false);
  const [devices, setDevices] = useState<ScanDevice[]>([]);
  const [error, setError] = useState("");
  const [connectingKey, setConnectingKey] = useState("");
  const [connectedLabel, setConnectedLabel] = useState("");

  const hasDevices = devices.length > 0;
  const listBottomPadding = 28 + Math.max(insets.bottom, 8) + BUILD_INFO_BADGE_CLEARANCE_PX;

  const isBusy = useMemo(() => isScanning || Boolean(connectingKey), [isScanning, connectingKey]);

  const ensureAndroidBlePermissions = useCallback(async (context: string): Promise<boolean> => {
    try {
      const granted = await hasAndroidBlePermissions();
      if (!granted) {
        logDebug(`[DEBUG-CONSOLE][PERMISSION][ERR] Missing permissions (${context}).`);
        return false;
      }

      return true;
    } catch (permissionError) {
      logDebug(`[DEBUG-CONSOLE][PERMISSION][ERR] Permission check failed (${context}).`, String(permissionError));
      return false;
    }
  }, []);

  const refreshConnectedBanner = useCallback(async () => {
    try {
      const connected = await isDeviceConnected();
      if (!connected) {
        setConnectedLabel("");
        return;
      }

      const device = await getConnectedDevice();
      const deviceName =
        (device && typeof device === "object" && "deviceName" in device && typeof (device as any).deviceName === "string"
          ? (device as any).deviceName
          : "") ||
        (device && typeof device === "object" && "deviceId" in device && typeof (device as any).deviceId === "string"
          ? (device as any).deviceId
          : "");
      setConnectedLabel(deviceName || "Connected device");
    } catch {
      setConnectedLabel("");
    }
  }, []);

  useFocusEffect(
    useCallback(() => {
      void refreshConnectedBanner();
    }, [refreshConnectedBanner])
  );

  async function onScan() {
    if (isBusy) return;

    setError("");
    setIsScanning(true);

    try {
      const hasPermissions = await ensureAndroidBlePermissions("scan");
      if (!hasPermissions) {
        setError("Bluetooth permissions are required to scan.");
        return;
      }

      logDebug("[DEBUG-CONSOLE][SCAN] Scanning for BLE devices...");
      const scanResult = await scanLeDevice(4);
      logDebug("[DEBUG-CONSOLE][SCAN] Raw scan result", scanResult);

      const filtered = normalizeDevices(scanResult);
      setDevices(filtered);
      if (!filtered.length) {
        setError('No devices found with names starting with "VAL".');
      }
    } catch (scanError) {
      setDevices([]);
      setError(String(scanError));
      logDebug("[DEBUG-CONSOLE][SCAN][ERR] Scan failed", String(scanError));
    } finally {
      setIsScanning(false);
    }
  }

  const connectToDevice = useCallback(async (
    device: ScanDevice,
    options: { navigateOnSuccess: boolean }
  ) => {
    const key = device.deviceAddress || device.deviceName || "";
    if (!key || isBusy) return;

    setConnectingKey(key);
    setError("");

    try {
      const hasPermissions = await ensureAndroidBlePermissions("connect");
      if (!hasPermissions) {
        setError("Bluetooth permissions are required to connect.");
        return;
      }

      const connectIdentifier =
        Platform.OS === "ios"
          ? device.deviceName || device.deviceAddress || ""
          : device.deviceAddress || device.deviceName || "";

      if (!connectIdentifier) {
        setError("Invalid device identifier.");
        return;
      }

      logDebug("[DEBUG-CONSOLE][CONNECT] Attempting connectAndSetupDevice", {
        source: "manual",
        connectIdentifier,
        device,
      });

      const setupResult = await connectAndSetupDeviceWithTimeout(connectIdentifier, {
        timeoutMs: DEFAULT_CONNECT_AND_SETUP_TIMEOUT_MS,
      });
      logDebug("[DEBUG-CONSOLE][CONNECT] connectAndSetupDevice result", setupResult);
      if (setupResult.status === "error") {
        const message =
          setupResult.error instanceof Error
            ? setupResult.error.message
            : String(setupResult.error ?? "Connection failed");
        setError(message);
        return;
      }

      await saveLastDebugDevice(device);
      await refreshConnectedBanner();
      if (options.navigateOnSuccess) {
        router.push("/home/ble-debug");
      }
    } catch (connectError) {
      const message = String(connectError);
      setError(message);
      logDebug("[DEBUG-CONSOLE][CONNECT][ERR] Connect failed", message);
    } finally {
      setConnectingKey("");
    }
  }, [ensureAndroidBlePermissions, isBusy, refreshConnectedBanner, router]);

  async function onConnect(device: ScanDevice) {
    await connectToDevice(device, { navigateOnSuccess: true });
  }

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.header}>
        <Pressable style={styles.backButton} onPress={() => router.back()}>
          <Text style={styles.backButtonText}>{"<"}</Text>
        </Pressable>
        <Text style={styles.title}>BLE Debug Console</Text>
      </View>

      {/* <View style={styles.introCard}>
        <Text style={styles.introTitle}>VAL Device Scanner</Text>
        <Text style={styles.subtitle}>
          Scan nearby BLE peripherals and connect to devices whose names begin with &quot;VAL&quot;.
        </Text>
      </View> */}

      {connectedLabel && (
        <View style={styles.connectedBanner}>
          <Text style={styles.connectedBannerText}>
            {connectedLabel ? `Connected: ${connectedLabel}` : "Not connected"}
          </Text>
          <Pressable
            style={styles.debugButton}
            onPress={() => router.push("/home/ble-debug")}
          >
            <Text style={styles.debugButtonText}>Open Debug Screen</Text>
          </Pressable>
        </View>
      )}

      <View style={styles.topActions}>
        <Pressable
          style={[styles.scanButton, isBusy && styles.scanButtonDisabled]}
          onPress={() => void onScan()}
          disabled={isBusy}
        >
          <View style={styles.scanButtonInner}>
            {isScanning ? (
              <ActivityIndicator size="small" color={validoseWhite} />
            ) : (
              <View style={styles.scanButtonDot} />
            )}
            <Text style={styles.scanButtonText}>
              {isScanning ? "Scanning..." : "Scan VAL Devices"}
            </Text>
          </View>
          {/* <Text style={styles.scanButtonHint}>Looks for nearby devices named VAL*</Text> */}
        </Pressable>
      </View>

      {error ? <Text style={styles.errorText}>{error}</Text> : null}

      <ScrollView contentContainerStyle={[styles.listContent, { paddingBottom: listBottomPadding }]}>
        <View style={styles.resultsHeader}>
          <Text style={styles.resultsTitle}>Scan Results</Text>
          <Text style={styles.resultsCount}>
            {devices.length} device{devices.length === 1 ? "" : "s"}
          </Text>
        </View>

        {!hasDevices && !isScanning ? (
          <View style={styles.emptyCard}>
            <Text style={styles.emptyTitle}>No scanned VAL devices yet</Text>
            <Text style={styles.emptyText}>
              Tap “Scan VAL Devices” to discover nearby docks and rings.
            </Text>
          </View>
        ) : null}

        {isScanning ? (
          <View style={styles.loadingRow}>
            <ActivityIndicator size="small" color={validoseAqua3} />
            <Text style={styles.loadingText}>Searching...</Text>
          </View>
        ) : null}

        {devices.map((device) => {
          const key =
            device.deviceAddress ||
            device.deviceName ||
            Math.random().toString(16);
          const isConnecting = connectingKey === key;
          const signal = getSignalTone(device.rssi);
          const connectable =
            device.isConnectable === true ? "Connectable" : "Unknown";

          return (
            <View key={key} style={styles.deviceCard}>
              <View style={styles.deviceInfo}>
                <View style={styles.deviceHeaderRow}>
                  <Text style={styles.deviceName}>
                    {device.deviceName || "Unknown Device"}
                  </Text>
                  <View
                    style={[
                      styles.signalBadge,
                      { backgroundColor: signal.backgroundColor },
                    ]}
                  >
                    <Text
                      style={[
                        styles.signalBadgeText,
                        { color: signal.textColor },
                      ]}
                    >
                      {signal.label}
                    </Text>
                  </View>
                </View>
                <Text style={styles.deviceMeta}>
                  {device.deviceAddress || "No address"}
                </Text>
                <View style={styles.metaRow}>
                  <View style={styles.metaPill}>
                    <Text style={styles.metaPillText}>
                      RSSI{" "}
                      {typeof device.rssi === "number" ? device.rssi : "N/A"}
                    </Text>
                  </View>
                  <View style={styles.metaPill}>
                    <Text style={styles.metaPillText}>{connectable}</Text>
                  </View>
                </View>
              </View>
              <Pressable
                style={[
                  styles.connectButton,
                  isConnecting && styles.connectButtonDisabled,
                ]}
                onPress={() => void onConnect(device)}
                disabled={isBusy}
              >
                <Text style={styles.connectButtonText}>
                  {isConnecting ? "Connecting..." : "Connect"}
                </Text>
              </Pressable>
            </View>
          );
        })}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: validoseWhite,
    paddingHorizontal: 18,
    paddingBottom: 20,
  },
  header: {
    flexDirection: "row",
    alignItems: "center",
    gap: 10,
    marginBottom: 34,
  },
  backButton: {
    width: 32,
    height: 32,
    borderRadius: 16,
    borderWidth: 1,
    borderColor: validoseAqua3,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: validoseWhite,
  },
  backButtonText: {
    color: validoseAqua3,
    fontSize: 18,
    lineHeight: 20,
    fontWeight: "700",
  },
  title: {
    fontSize: 24,
    fontWeight: "700",
    color: validoseDarkBlue,
  },
  subtitle: {
    fontSize: 12,
    color: validoseGrey,
    lineHeight: 18,
  },
  introCard: {
    borderWidth: 1,
    borderColor: "#D6E8EC",
    backgroundColor: "#F4FBFD",
    borderRadius: 14,
    paddingHorizontal: 14,
    paddingVertical: 12,
    gap: 6,
    marginBottom: 16,
  },
  introTitle: {
    color: validoseDarkBlue,
    fontSize: 15,
    fontWeight: "700",
  },
  connectedBanner: {
    borderWidth: 1,
    borderColor: "#A9E6BF",
    backgroundColor: "#EEFFF3",
    borderRadius: 12,
    padding: 12,
    marginBottom: 16,
    gap: 10,
  },
  connectedBannerText: {
    fontSize: 12,
    color: "#087443",
    fontWeight: "700",
  },
  debugButton: {
    alignSelf: "flex-start",
    borderWidth: 1,
    borderColor: validoseAqua3,
    borderRadius: 8,
    paddingHorizontal: 10,
    paddingVertical: 6,
    backgroundColor: validoseWhite,
  },
  debugButtonText: {
    color: validoseAqua3,
    fontWeight: "700",
    fontSize: 12,
  },
  topActions: {
    flexDirection: "row",
    marginBottom: 14,
  },
  scanButton: {
    borderRadius: 14,
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: validoseAqua3,
    width: "100%",
    borderWidth: 1,
    borderColor: "#34C3D9",
    shadowColor: "#0A2E3B",
    shadowOpacity: 0.14,
    shadowRadius: 10,
    shadowOffset: { width: 0, height: 4 },
    elevation: 3,
  },
  scanButtonDisabled: {
    opacity: 0.7,
  },
  scanButtonInner: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    gap: 8,
    marginBottom: 2,
  },
  scanButtonDot: {
    width: 9,
    height: 9,
    borderRadius: 999,
    backgroundColor: "#FFFFFF",
    opacity: 0.9,
  },
  scanButtonText: {
    color: validoseWhite,
    fontWeight: "700",
    fontSize: 15,
    letterSpacing: 0.2,
  },
  scanButtonHint: {
    color: "#E7FBFF",
    fontSize: 11,
    textAlign: "center",
    fontWeight: "500",
  },
  errorText: {
    color: "#B42318",
    fontSize: 12,
    marginBottom: 12,
  },
  listContent: {
    paddingBottom: 28,
    gap: 14,
  },
  resultsHeader: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    marginBottom: 2,
  },
  resultsTitle: {
    color: validoseDarkBlue,
    fontSize: 14,
    fontWeight: "700",
  },
  resultsCount: {
    color: validoseGrey,
    fontSize: 12,
    fontWeight: "600",
  },
  emptyCard: {
    borderWidth: 1,
    borderColor: "#E4E7EC",
    borderRadius: 14,
    backgroundColor: "#FAFBFC",
    paddingHorizontal: 16,
    paddingVertical: 14,
    gap: 6,
  },
  emptyTitle: {
    color: validoseDarkBlue,
    fontSize: 13,
    fontWeight: "700",
  },
  emptyText: {
    color: validoseGrey,
    fontSize: 12,
  },
  loadingRow: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
    paddingVertical: 4,
  },
  loadingText: {
    color: validoseGrey,
    fontSize: 12,
  },
  deviceCard: {
    borderWidth: 1,
    borderColor: "#E4E7EC",
    borderRadius: 14,
    backgroundColor: validoseWhite,
    padding: 14,
    flexDirection: "row",
    // justifyContent: "space-between",
    // gap: 14,
  },
  deviceInfo: {
    flex: 1,
    gap: 8,
  },
  deviceHeaderRow: {
    flexDirection: "row",
    alignItems: "center",
    // justifyContent: "space-between",
    // gap: 10,
  },
  deviceName: {
    color: validoseDarkBlue,
    fontSize: 14,
    fontWeight: "700",
    marginRight: 8
  },
  deviceMeta: {
    color: validoseGrey,
    fontSize: 11,
    fontWeight: "500",
  },
  metaRow: {
    flexDirection: "row",
    gap: 8,
    flexWrap: "wrap",
  },
  metaPill: {
    borderWidth: 1,
    borderColor: "#E4E7EC",
    borderRadius: 999,
    backgroundColor: "#F8FAFC",
    paddingHorizontal: 8,
    paddingVertical: 3,
  },
  metaPillText: {
    color: "#475467",
    fontSize: 10,
    fontWeight: "700",
  },
  signalBadge: {
    borderRadius: 999,
    paddingHorizontal: 8,
    paddingVertical: 3,
  },
  signalBadgeText: {
    fontSize: 10,
    fontWeight: "700",
  },
  connectButton: {
    borderWidth: 0,
    borderRadius: 10,
    paddingHorizontal: 14,
    paddingVertical: 9,
    alignSelf: "center",
    backgroundColor: validoseAqua3,
  },
  connectButtonDisabled: {
    opacity: 0.6,
  },
  connectButtonText: {
    color: validoseWhite,
    fontSize: 12,
    fontWeight: "700",
  },
});
