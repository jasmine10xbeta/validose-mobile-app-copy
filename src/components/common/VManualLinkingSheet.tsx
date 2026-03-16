import { useRouter } from "expo-router";
import { useMemo, useState } from "react";
import {
  Dimensions,
  FlatList,
  Keyboard,
  Platform,
  StyleSheet,
  Text,
  TextInput,
  TouchableWithoutFeedback,
  TouchableOpacity,
  View,
} from "react-native";
import { useSafeAreaInsets } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import { connectAndSetupDevice } from "@/utils/ble";

const SCREEN_HEIGHT = Dimensions.get("window").height;
const SHEET_HEIGHT = SCREEN_HEIGHT * 0.9;

interface VManualLinkingSheetProps {
  onClose: () => void;
  onBack?: () => void;
}

export function VManualLinkingSheet({
  onClose,
  onBack,
}: VManualLinkingSheetProps) {
  const router = useRouter();
  const insets = useSafeAreaInsets();
  const [deviceId, setDeviceId] = useState("");
  const [isInputFocused, setIsInputFocused] = useState(false);
  const [reconnectingDeviceId, setReconnectingDeviceId] = useState<
    string | null
  >(null);
  const devices = useDeviceStore((s) => s.devices);
  const authorizedDevices = useDeviceStore((state) => state.authorizedDevices);
  const isDevicesConnected = devices.length > 0;
  const {
    isMockBleModeEnabled,
    matchesBypassKey,
    enableMockBleMode,
    disableMockBleMode,
  } = useDevStore();
  const listMaxHeight = useMemo(() => SCREEN_HEIGHT * 0.30, []);

  async function reconnectDevice(deviceIdentifier: string) {
    if (reconnectingDeviceId) {
      showToast("info", "Another connection in progress", "Please wait..");
      return;
    }

    setReconnectingDeviceId(deviceIdentifier);
    try {
      const connected = await connectAndSetupDevice(deviceIdentifier);
      if (connected.error) showToast("error", connected.error.toString());
    } catch (error) {
      showToast(
        "error",
        "Connection failed",
        error instanceof Error ? error.message : String(error),
      );
    } finally {
      setReconnectingDeviceId(null);
    }
  }

  async function validateDevice(address: string) {
    try {
      if (isMockBleModeEnabled()) return true;

      console.log("[APP] Received device address:", address);

      if (
        Array.isArray(authorizedDevices) &&
        !authorizedDevices.some((device) => device.deviceId === address)
      ) {
        console.log("[APP] Device not found in list");
        showToast("error", "Device not assigned to this patient");
        return false;
      }

      console.log(`[BLE] Connecting with deviceId: ${deviceId} (${Platform.OS})`);
      return true;
    } catch (err) {
      console.log(err);
      showToast("error", "Invalid QR Code", `${err}`);
      return false;
    }
  }

  async function handleLinkPress() {
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
      onClose();
      router.push("/home/dashboard");
      return;
    }

    const isValid = await validateDevice(normalized);
    if (!isValid) return;

    const connected = await connectAndSetupDevice(normalized);
    if (connected?.error) {
      showToast("error", connected?.error.toString());
    }
  }

  return (
    <TouchableWithoutFeedback
      accessible={false}
      onPress={() => Keyboard.dismiss()}
    >
      <View
        style={[
          styles.sheetContainer,
          { paddingBottom: Math.max(insets.bottom, 12) },
        ]}
      >
        <View style={styles.headerRow}>
          {onBack ? (
            <TouchableOpacity style={styles.headerAction} onPress={onBack}>
              <Text style={styles.headerBackText}>Back</Text>
            </TouchableOpacity>
          ) : (
            <View style={styles.headerAction} />
          )}
          <Text style={styles.headerTitle}>Connect</Text>
          <TouchableOpacity style={styles.headerAction} onPress={onClose}>
            <Text style={styles.headerCancelText}>Cancel</Text>
          </TouchableOpacity>
        </View>

        <View style={styles.content}>
          <VText style={styles.setupMessage} textVariant="Label">
            {"Enter Device ID \nManually"}
          </VText>

          {isDevicesConnected ? (
            <View style={[styles.devicesListContainer, { maxHeight: listMaxHeight }]}>
              <FlatList
                data={devices}
                renderItem={({ item, index }) => (
                  <VDeviceItem
                    item={item}
                    state={item?.connected || false}
                    reconnect={() => reconnectDevice(item.deviceId)}
                    isReconnecting={reconnectingDeviceId === item.deviceId}
                    index={index}
                  />
                )}
                keyExtractor={(item) => item.deviceId}
                showsVerticalScrollIndicator={false}
              />
            </View>
          ) : (
            <View style={styles.emptyStateSpacer} />
          )}

          <TextInput
            style={[
              styles.textInput,
              isInputFocused ? styles.textInputFocused : null,
            ]}
            value={deviceId}
            onChangeText={setDeviceId}
            placeholder="Enter the Device ID"
            placeholderTextColor="#888"
            onFocus={() => setIsInputFocused(true)}
            onBlur={() => setIsInputFocused(false)}
          />
          <Text style={styles.inputHelperText}>
            Device ID is visible on clinician dashboard
          </Text>
          <View style={styles.bottomActions}>
            {isDevicesConnected && (
              <VButton
                onPress={() => {
                  onClose();
                  router.push("/home/dashboard");
                }}
                label="Continue"
                style={[styles.continueButton, { marginBottom: 12 }]}
                labelStyle={styles.continueButtonLabel}
              />
            )}

            <VButton onPress={handleLinkPress} label="Link" />
          </View>
        </View>
      </View>
    </TouchableWithoutFeedback>
  );
}

const styles = StyleSheet.create({
  sheetContainer: {
    width: "100%",
    height: SHEET_HEIGHT,
    backgroundColor: "#FFFFFF",
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    paddingTop: 20,
  },
  headerRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  headerAction: {
    minWidth: 84,
    paddingHorizontal: 24,
    paddingBottom: 12,
    paddingTop: 4,
  },
  headerBackText: {
    fontSize: 16,
    color: "#505A66",
    fontWeight: "400",
  },
  headerTitle: {
    flex: 1,
    textAlign: "center",
    fontSize: 17,
    fontWeight: "500",
    color: "#252F3B",
  },
  headerCancelText: {
    fontSize: 16,
    color: "#505A66",
    fontWeight: "400",
    textAlign: "right",
  },
  content: {
    flex: 1,
    alignItems: "center",
    paddingHorizontal: 24,
    paddingTop: 14,
    paddingBottom: 34,
  },
  setupMessage: {
    marginTop: 32,
    marginBottom: 10,
    textAlign: "center",
    fontSize: 30,
    color: "#252F3B",
    fontWeight: "600",
  },
  devicesListContainer: {
    width: "100%",
    marginTop: 20,
    marginBottom: 20,
  },
  emptyStateSpacer: {
    marginBottom: 4,
  },
  textInput: {
    width: "96%",
    paddingVertical: 18,
    borderColor: "#E6E7E8",
    borderWidth: 1,
    borderRadius: 6,
    paddingHorizontal: 12,
    marginBottom: 12,
    marginTop: 20,
  },
  textInputFocused: {
    borderColor: "#255F6C",
    shadowColor: "#255F6C",
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.2,
    shadowRadius: 6,
    elevation: 3,
  },
  inputHelperText: {
    width: "96%",
    color: "#252F3B",
    fontSize: 15,
    lineHeight: 18,
  },
  bottomActions: {
    width: "100%",
    marginTop: "auto",
    alignItems: "center",
    marginBottom: 12,
  },
  continueButton: {
    marginTop: 10,
    borderColor: "#252F3B",
    borderWidth: 2,
    width: "100%",
    padding: 5,
    borderRadius: 25,
  },
  continueButtonLabel: {
    color: "#252F3B",
    fontWeight: "500",
  },
});
