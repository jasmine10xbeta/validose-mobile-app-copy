import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useEffect, useRef, useState } from "react";
import { FlatList, StyleSheet, View, Image, Dimensions, Text } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import { VTopActions } from "@/components/common/VTopActions";
import { useAuth } from "@/providers/auth";
import { getValidoseDevices } from "@/services/device";
import useDeviceStore from "@/store/device";
import { connectAndSetupDevice } from "@/utils/ble";

export default function PairingScreen() {
  const router = useRouter();

  const { user, isLoading } = useAuth();
  const { authorizedDevices, setAuthorizedDevices } = useDeviceStore();
  const hasAccessToken = Boolean(user?.access_token);
  const handleHelpPress = () => router.push("/home/led-info");

  const hasScannedRef = useRef(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();
  const [reconnectingDeviceId, setReconnectingDeviceId] = useState<
    string | null
  >(null);

  const [contentHeight, setContentHeight] = useState(0);
  const screenHeight = Dimensions.get("window").height;
  const devices = useDeviceStore((s) => s.devices);
  const isDevicesConnected = devices.length > 0;
  
  useEffect(() => {
    async function fetchDevices() {
      if (!isLoading && user?.access_token) {
        const list = await getValidoseDevices();
        setAuthorizedDevices(list);
      }
    }
    fetchDevices();
  }, []);

  async function reconnectDevice(deviceName: string) {
    if (reconnectingDeviceId) {
      showToast("info", "Another connection in progress", "Please wait..");
      return;
    }
    setReconnectingDeviceId(deviceName);
    try {
      const connected = await connectAndSetupDevice(deviceName);
      if (connected.error) showToast("error", connected.error.toString());
    } catch (error) {
      showToast("error", "Connection failed", error instanceof Error ? error.message : String(error));
    } finally {
      setReconnectingDeviceId(null);
    }
  }

  async function validateDeviceAddress(scanningResult: string) {
    try {
      const parsed = scanningResult;

      console.log("\n");
      console.log("[APP] Scanned device name:", parsed);

      if (typeof parsed === "string" && Array.isArray(authorizedDevices) && !authorizedDevices.some((device) => device.deviceId === parsed)) {
        console.log("[APP] Device not found in list");
        showToast("error", "Device not assigned to this patient");
        return false;
      }

      console.log(`[BLE] Connecting with device name: ${parsed}`);
      return true;
    } catch (err) {
      console.log(err);
      showToast("error", "Invalid QR Code", `${err}`);
      return false;
    }
  }

  // Handle QR code scanning result and connect device
  const handleDeviceQrScan = async (scanningResult: { data: string }) => {
    if (hasScannedRef.current) return;

    hasScannedRef.current = true;
    const deviceAddress = scanningResult.data;
    const isValid = await validateDeviceAddress(deviceAddress);
    if (isValid) {
      const connected = await connectAndSetupDevice(deviceAddress);
      hasScannedRef.current = false;
      setShowCamera(false);

      if (connected?.error) showToast("error", connected?.error.toString());
    }

    hasScannedRef.current = false;
    setShowCamera(false);
  };

  if (!permission) return <View />;

  if (!permission.granted) {
    return (
      <SafeAreaView style={styles.alignContent}>
        <VTopActions
          style={styles.topActions}
          onPressHelp={handleHelpPress}
          personDisabled={!hasAccessToken}
        />
        <View style={styles.pairingContainer}>
          <VText textVariant="Body">
            We need your permission to show the camera
          </VText>
          <VButton onPress={requestPermission} label="Grant permission" />
        </View>
      </SafeAreaView>
    );
  }

  if (showCamera) {
    return (
      <QRCodeScanner
        onBarcodeScanned={handleDeviceQrScan}
        onClose={() => setShowCamera(false)}
      />
    );
  }

  return (
    <SafeAreaView style={styles.alignContent}>
      <VTopActions
        style={styles.topActions}
        onPressHelp={handleHelpPress}
        personDisabled={!hasAccessToken}
      />
      <View
        style={[
          styles.setupContainer,
          contentHeight > screenHeight ? { height: screenHeight } : {},
        ]}
        onLayout={(e) => setContentHeight(e.nativeEvent.layout.height)}
      >
        <View
          style={{
            flexDirection: "column",
            width: "100%",
            alignItems: "center",
          }}
        >
          <VText textVariant="LabelDose">
            Setup
          </VText>
          <VText style={styles.setupMessage} textVariant="Label">
            {isDevicesConnected
              ? "Device Connection Status"
              : "Let's connect\nyour device"}
          </VText>
          {!isDevicesConnected && (
            <>
              <Image
                source={require("../../../../assets/images/png/device-qr.png")}
                style={{ width: "65%", height: "45%", resizeMode: "contain", marginTop: 24, marginBottom: 18 }}
              />
              <VText textVariant="LabelDose">
                {"Tap Scan device and scan the "}
                <Text style={{ fontWeight: "600", color: "#505A66" }}>
                  QR sticker underneath the device.
                </Text>
              </VText>
            </>
          )}
          {isDevicesConnected ? (
            <View
              style={{
                maxHeight: screenHeight * 0.35,
                width: "100%",
              }}
            >
              <FlatList
                data={devices}
                renderItem={({ item }) => (
                  <VDeviceItem
                    item={item}
                    state={item?.connected}
                    reconnect={() => reconnectDevice(item.deviceName)}
                    isReconnecting={reconnectingDeviceId === item.deviceName}
                  />
                )}
                keyExtractor={(item) => item.deviceId}
                showsVerticalScrollIndicator={false}
              />
            </View>
          ) : (
            <View style={{ marginBottom: 0 }} />
          )}
        </View>

        <View
          style={{
            flexDirection: "column",
            width: "100%",
            alignItems: "center",
          }}
        >
          {isDevicesConnected ? (
            <>
              <VButton
                onPress={() => router.push("/home/dashboard")}
                label="Continue"
                style={[styles.continueButton, { marginBottom: 10 }]}
                labelStyle={{ color: "#252F3B", fontWeight: "500" }}
              />
              <VButton
                onPress={() => setShowCamera(true)}
                label="Scan device sticker"
              />
            </>
          ) : (
            <VButton
              onPress={() => setShowCamera(true)}
              label="Scan device sticker"
            />
          )}
          <VButton
            onPress={() => {
              router.push("/home/pairing/manual-pairing");
            }}
            style={{
              borderWidth: 0,
              marginTop: 21,
            }}
            label="Enter device ID manually"
            labelStyle={styles.manualPairingLabel}
          />
        </View>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
  },
  topActions: {
    width: "100%",
    paddingHorizontal: 24,
    marginTop: 16,
  },

  // Setup Container Styles
  setupContainer: {
    flex: 1,
    flexDirection: "column",
    justifyContent: "space-between",
    position: "relative",
    alignItems: "center",
    marginHorizontal: 24,
    paddingHorizontal: 18,
  },
  setupLabel: {
    position: "absolute",
    top: -10,
    paddingHorizontal: 25,
    color: "#505A66",
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
  },
  setupMessage: {
    textAlign: "center",
    fontSize: 30,
    color: "#252F3B",
    fontWeight: "600",
    marginTop: 8
    // fontFamily: "Inter",
  },

  // Pairing Container Styles
  pairingContainer: {
    padding: 20,
    flexDirection: "column",
    alignItems: "center",
    gap: 25,
  },
  manualPairingLabel: {
    color: "#255F6C",
    fontSize: 16,
    fontWeight: "500",
    borderBottomWidth: 1,
    borderRadius: 1,
    borderColor: "#255F6C",
  },
  secretContainer: {
    position: "absolute",
    bottom: 0,
    height: "20%",
    width: "100%",
  },

  // Continue Button Styles
  continueButton: {
    marginTop: 10,
    borderColor: "#252F3B",
    borderWidth: 2,
    width: "100%",
    padding: 5,
    borderRadius: 25,
  },
});
