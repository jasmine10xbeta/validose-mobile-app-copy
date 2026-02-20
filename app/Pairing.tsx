import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useRef, useState } from "react";
import { FlatList, StyleSheet, View, Dimensions, Pressable, Text } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import useDeviceStore from "@/store/useDeviceStore";
import { bondDevice } from "../modules/tenx-mdk-ble-rn-library/src/index";

export default function PairingScreen() {
  const router = useRouter();
  const { addDevice } = useDeviceStore();
  const secretTapRef = useRef({ count: 0 });
  const resetTapTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const [hasScanned, setHasScanned] = useState(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();

  const [contentHeight, setContentHeight] = useState(0);
  const screenHeight = Dimensions.get("window").height;

  const devices = useDeviceStore((s) => s.devices);
  const isDevicesConnected = devices.length > 0;
  const debugGateEnabled =
    String(
      process.env.ENABLE_BLE_BYPASS ??
        process.env.EXPO_PUBLIC_ENABLE_BLE_BYPASS ??
        process.env.MOCK_BLE_BYPASS ??
        ""
    )
      .trim()
      .toLowerCase() === "true";

  const handleSecretOpenDebug = () => {
    if (!debugGateEnabled) {
      showToast("info", "Debug console disabled", "Set ENABLE_BLE_BYPASS=true");
      return;
    }

    secretTapRef.current.count += 1;

    if (resetTapTimeoutRef.current) {
      clearTimeout(resetTapTimeoutRef.current);
    }
    resetTapTimeoutRef.current = setTimeout(() => {
      secretTapRef.current.count = 0;
    }, 2500);

    if (secretTapRef.current.count >= 7) {
      secretTapRef.current.count = 0;
      if (resetTapTimeoutRef.current) {
        clearTimeout(resetTapTimeoutRef.current);
        resetTapTimeoutRef.current = null;
      }
      showToast("success", "Opening BLE debug console");
      router.push("/ble-debug");
      return;
    }

    if (secretTapRef.current.count >= 3) {
      showToast("info", `Debug unlock: ${secretTapRef.current.count}/7`, "Keep tapping Setup");
    }
  };

  if (!permission) {
    return <View />;
  }

  if (!permission.granted) {
    return (
      <SafeAreaView style={styles.alignContent}>
        <View style={styles.pairingContainer}>
          <VText textVariant="Body">
            We need your permission to show the camera
          </VText>
          <VButton onPress={requestPermission} label="Grant permission" />
        </View>
      </SafeAreaView>
    );
  }

  const handleDeviceQrScan = async (scanningResult: { data: string }) => {
    if (hasScanned) return;
    setHasScanned(true);

    try {
      // TODO: Update parsing logic. 
      // For now, assuming QR contains plain JSON {"deviceId":"abc123"}
      const parsed = JSON.parse(scanningResult.data);
      const deviceId = parsed?.deviceId;

      console.log("\n");
      console.log("Scanned device id:", deviceId);

      if (deviceId) {
        const connectResponse = await bondDevice(deviceId);
        console.log(`Connection response with ${deviceId}:\n`, connectResponse);

        if (connectResponse) {
          // TODO: Inform backend about attempted failed connections?
          const { deviceName, deviceId } = connectResponse;
          showToast("success", "Device connected", deviceName);
          const added = addDevice({
            deviceId,
            deviceName,
            linkedProtocolId: "",
            connected: true,
            color: ""
          });

          if (added) {
            showToast("success", "Device connected");
          } else {
            showToast("info", "Device already exists");
          }
          setHasScanned(false);
          setShowCamera(false);
        } else {
          showToast(
            "error",
            "Connection failed",
            "Either incorrect QR code or device is already bonded"
          );
        }
      } else {
        showToast("error", "Invalid QR Code");
      }
    } catch (err) {
      console.log(err);
      showToast("error", "Invalid QR Code", `${err}`);
    }

    setHasScanned(false);
  };

  if (showCamera) {
    return (
      <QRCodeScanner
        onBarcodeScanned={handleDeviceQrScan}
        onClose={() => setShowCamera(false)}
      />
    );
  }

  function onContinuePress() {
    router.push("/dashboard" as never);
  }

  return (
    <SafeAreaView style={styles.alignContent}>
      <View
        style={[
          styles.setupContainer,
          contentHeight > screenHeight ? { height: screenHeight } : {},
        ]}
        onLayout={(e) => setContentHeight(e.nativeEvent.layout.height)}
      >
        <Pressable onPress={handleSecretOpenDebug} style={styles.debugTapZone}>
          <Text style={styles.debugTapTitle}>Setup</Text>
          <Text style={styles.debugTapHint}>click here</Text>
        </Pressable>
        <VText style={styles.setupMessage} textVariant="Label">
          {isDevicesConnected
            ? "Device connection successful"
            : "Let's connect\nyour device"}
        </VText>
        <VButton onPress={() => setShowCamera(true)} label="+ Scan device" />
        {isDevicesConnected && (
          <VButton
            onPress={() => onContinuePress()}
            label="Continue"
            style={styles.continueButton}
            labelStyle={{ color: "#252F3B", fontWeight: "500" }}
          />
        )}
        <VButton
          onPress={() => {}} // TODO: Add manual pairing
          style={{ borderWidth: 0, marginTop: 5 }}
          label="Enter device ID manually"
          labelStyle={styles.manualPairingLabel}
        />
        <View
          style={{
            maxHeight: screenHeight * 0.35,
            width: "100%",
            marginTop: 70,
          }}
        >
          {isDevicesConnected ? (
            <FlatList
              data={devices}
              renderItem={({ item }) => (
                <VDeviceItem item={item} state={item?.connected || false} />
              )}
              keyExtractor={(item) => item.deviceId}
              showsVerticalScrollIndicator={false}
            />
          ) : null}
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

  // Setup Container Styles
  setupContainer: {
    flexDirection: "column",
    position: "relative",
    alignItems: "center",
    marginHorizontal: 24,
    marginTop: 40,
    paddingHorizontal: 22,
    paddingTop: 45,
    paddingBottom: 20,
    borderColor: "#E6E7E8",
    borderRadius: 12,
    borderWidth: 1,
  },
  debugTapZone: {
    position: "absolute",
    top: -22,
    minWidth: 170,
    minHeight: 56,
    zIndex: 10,
    backgroundColor: "#D92D20",
    borderRadius: 12,
    alignItems: "center",
    justifyContent: "center",
    paddingHorizontal: 14,
    paddingVertical: 8,
  },
  debugTapTitle: {
    color: "#FFFFFF",
    fontSize: 15,
    fontWeight: "700",
    lineHeight: 18,
  },
  debugTapHint: {
    color: "#FFECE9",
    fontSize: 11,
    lineHeight: 14,
  },
  setupMessage: {
    marginBottom: 20,
    textAlign: "center",
    fontSize: 32,
    color: "#252F3B",
    fontWeight: "600",
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
    color: "#000",
    fontSize: 16,
    fontWeight: "300",
    fontStyle: "italic",
    textDecorationColor: "#000",
    textDecorationLine: "underline",
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
