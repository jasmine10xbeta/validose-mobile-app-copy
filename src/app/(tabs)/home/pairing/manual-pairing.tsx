import { useRouter } from "expo-router";
import { useState } from "react";
import { StyleSheet, View, Dimensions, FlatList, Platform } from "react-native";
import { TextInput } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import { connectAndSetupDevice } from "@/utils/ble";

export default function ManualPairingScreen() {
  const router = useRouter();

  const [deviceId, setDeviceId] = useState("");
  const authorizedDevices = useDeviceStore((state) => state.authorizedDevices);
  const { isMockBleModeEnabled, matchesBypassKey, enableMockBleMode, disableMockBleMode } = useDevStore();
  const [reconnectingDeviceId, setReconnectingDeviceId] = useState<string | null>(null);

  const [contentHeight, setContentHeight] = useState(0);
  const screenHeight = Dimensions.get("window").height;
  const devices = useDeviceStore((s) => s.devices);
  const isDevicesConnected = devices.length > 0;

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

  async function validateDevice(address: string) {
    try {
      if (isMockBleModeEnabled()) return true;

      console.log("\n");
      console.log("[APP] Received device address:", address);

      if (Array.isArray(authorizedDevices) && !authorizedDevices.some((device) => device.deviceId === address)) {
        console.log("[APP] Device not found in list");
        showToast("error", "Device not assigned to this patient");
        return false;
      }

      console.log(
        `[BLE] Connecting with deviceId: ${deviceId} (${Platform.OS})`
      );
      return true;
    } catch (err) {
      console.log(err);
      showToast("error", "Invalid QR Code", `${err}`);
      return false;
    }
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
          {`Let's connect\nyour device`}
        </VText>
        {isDevicesConnected ? (
          <View
            style={{
              maxHeight: screenHeight * 0.35,
              width: "100%",
              marginTop: 50,
              marginBottom: 40,
            }}
          >
            <FlatList
              data={devices}
              renderItem={({ item }) => (
                <VDeviceItem
                  item={item}
                  state={item?.connected || false}
                  reconnect={() => reconnectDevice(item.deviceId)}
                  isReconnecting={reconnectingDeviceId === item.deviceId}
                />
              )}
              keyExtractor={(item) => item.deviceId}
              showsVerticalScrollIndicator={false}
            />
          </View>
        ) : (
          <View style={{ marginBottom: 4 }} />
        )}
        <TextInput
          style={styles.textInput}
          value={deviceId}
          onChangeText={setDeviceId}
          placeholder="Enter device code"
          placeholderTextColor="#888"
        />
        <VButton
          onPress={async () => {
            const normalized = deviceId.trim();
            if (!normalized) {
              showToast("error", "No device code provided");
              return;
            }

            if (matchesBypassKey(normalized)) {
              if (isMockBleModeEnabled()) {
                disableMockBleMode();
                showToast("success", "Pairing mode reset");
                return;
              }

              enableMockBleMode();
              const connected = await connectAndSetupDevice("VAL-OP DEMO");
              if (connected?.error) {
                showToast("error", "Connection failed", String(connected.error));
                return;
              }
              router.push("/home/dashboard");
              return;
            }

            const isValid = await validateDevice(normalized);
            if (!isValid) return;

            const connected = await connectAndSetupDevice(normalized);
            if (connected?.error) showToast("error", connected?.error.toString());
          }}
          label="Link"
        />
        {isDevicesConnected && (
          <VButton
            onPress={() => router.push("/home/dashboard")}
            label="Continue"
            style={[styles.continueButton, { marginBottom: 5 }]}
            labelStyle={{ color: "#252F3B", fontWeight: "500" }}
          />
        )}
        <VButton
          onPress={() => {
            router.push("/home/pairing");
          }}
          style={{ borderWidth: 0, marginTop: 10 }}
          label="Scan QR code on the device"
          labelStyle={styles.manualPairingLabel}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
  },

  textInput: {
    width: "100%",
    // height: 44,
    paddingVertical: 18,
    borderColor: "#E6E7E8",
    borderWidth: 1,
    borderRadius: 6,
    paddingHorizontal: 12,
    marginBottom: 12,
    marginTop: 30,
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
  manualPairingLabel: {
    color: "#000",
    fontSize: 16,
    fontWeight: "300",
    fontStyle: "italic",
    textDecorationColor: "#000",
    textDecorationLine: "underline",
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
