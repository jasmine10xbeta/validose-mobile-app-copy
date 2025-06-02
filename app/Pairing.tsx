import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState } from "react";
import { FlatList, StyleSheet, View, Dimensions } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { showToast } from "@/components/common/Toast";
import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import useDeviceStore from "@/store/useDeviceStore";
import { bondDevice } from "../modules/tenx-mdk-ble-rn-library/src/index";

export default function PairingScreen() {
  const router = useRouter();

  const [hasScanned, setHasScanned] = useState(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();

  const [contentHeight, setContentHeight] = useState(0);
  const screenHeight = Dimensions.get("window").height;

  const { addDevice } = useDeviceStore.getState();
  const devices = useDeviceStore((s) => s.devices);
  const isDevicesConnected = devices.length > 0;

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
      // Assuming QR contains plain JSON {"deviceId":"abc123"}
      const validoseDeviceId = JSON.parse(scanningResult?.data)?.deviceId;

      if (validoseDeviceId) {
        //TODO: Check if same deviceId as login response
        const connectResponse = await bondDevice(validoseDeviceId);

        if (connectResponse) {
          // TODO: Inform backend about attempted failed connections?

          const { deviceName: name, deviceId: id } = connectResponse;
          const added = addDevice({
            id,
            name,
            status: "Connected" as const,
            medicine: name.charAt(0),
            medicineState: 0, // default state
            color: "#5D9BFF",
            regimen_id: "",
            indication_code: "",
            dosage_amount: 0,
            administration_days: [],
            administration_times_min: [],
            frequency_count: 0,
            dosing_window_min: 0,
            active: false
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
    router.push("/dashboard");
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
        <VText style={styles.setupLabel} textVariant="Label">
          Setup
        </VText>
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
                <VDeviceItem item={item} state={item?.status || ""} />
              )}
              keyExtractor={(item) => item.id}
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
  setupLabel: {
    position: "absolute",
    top: -10,
    paddingHorizontal: 25,
    color: "#565F6B",
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
    backgroundColor: "#FFF",
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
