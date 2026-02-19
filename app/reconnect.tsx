import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState } from "react";
import { StyleSheet, View } from "react-native";
import { getUniqueId } from "react-native-device-info";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import useDeviceStore from "@/store/useDeviceStore";
import { onboardWithCode } from "@/utils/axios/api/__mocks__/onboarding";
import { useAuth } from "@/utils/provider/AuthenticationProvider";

export default function ReconnectScreen() {
  const router = useRouter();
  const { addDevice } = useDeviceStore();
  const { signIn } = useAuth();

  const [hasScanned, setHasScanned] = useState(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();
  const bypassEnabled =
    String(process.env.ENABLE_BLE_BYPASS ?? process.env.EXPO_PUBLIC_ENABLE_BLE_BYPASS ?? "")
      .trim()
      .toLowerCase() === "true";
  const bypassKey = String(
    process.env.BLE_BYPASS_KEY ?? process.env.EXPO_PUBLIC_BLE_BYPASS_KEY ?? ""
  )
    .trim()
    .toUpperCase();

  const extractQrCandidates = (rawInput: string): string[] => {
    const raw = (rawInput || "").trim();
    if (!raw) return [];

    const values = new Set<string>();
    values.add(raw);

    try {
      const parsed = JSON.parse(raw);
      if (typeof parsed === "string") values.add(parsed);
      if (parsed && typeof parsed === "object") {
        const obj = parsed as Record<string, unknown>;
        ["code", "onboarding_code", "key", "value"].forEach((k) => {
          const v = obj[k];
          if (typeof v === "string" && v.trim()) values.add(v.trim());
        });
      }
    } catch {
      // ignore non-json payload
    }

    return Array.from(values);
  };

  if (!permission) {
    return <View />;
  }

  if (!permission.granted) {
    return (
      <SafeAreaView style={styles.alignContent}>
        <View style={styles.loginContainer}>
          <VText textVariant="Body">
            We need your permission to show the camera
          </VText>
          <VButton onPress={requestPermission} label="Grant permission" />
        </View>
      </SafeAreaView>
    );
  }

  const handlePatientQrScan = async (scanningResult: { data: string }) => {
    if (hasScanned) return;

    setHasScanned(true);
    try {
      const raw = scanningResult?.data?.trim() || "";
      const candidates = extractQrCandidates(raw).map((v) => v.toUpperCase());

      if (
        bypassEnabled &&
        bypassKey &&
        candidates.some((candidate) => candidate === bypassKey)
      ) {
        await signIn({
          token: "mock-access-token",
          refreshToken: "mock-refresh-token",
        });

        const added = addDevice({
          deviceId: "VAL-OP-DEMO",
          deviceName: "VAL-OP DEMO",
          linkedProtocolId: "",
          connected: true,
          color: "",
        });

        if (!added) {
          useDeviceStore.getState().updateDevice("VAL-OP-DEMO", {
            connected: true,
            color: "",
          });
        }

        router.replace("/dashboard");
        return;
      }

      // TODO: Update parsing logic.
      // Assuming QR contains plain JSON {"code":"xyz"}
      const { code: onboardingCode } = JSON.parse(raw);

      if (onboardingCode) {
        const mobileDeviceId = await getUniqueId();
        const response = await onboardWithCode(onboardingCode, mobileDeviceId);

        if (response) {
          router.replace("/pairing");
        }
      } else {
        showToast("error", "Invalid QR Code");
        setHasScanned(false);
      }
    } catch (err) {
      showToast("error", "Invalid QR Code", `${err}`);
      setHasScanned(false);
    }
  };

  if (showCamera) {
    return (
      <QRCodeScanner
        onBarcodeScanned={handlePatientQrScan}
        onClose={() => setShowCamera(false)}
      />
    );
  }

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.loginContainer}>
        {/* TODO: Update label, message and button for onboardinng vs login */}
        <VText style={styles.loginLabel} textVariant="Label">
          Login
        </VText>
        <VText style={styles.loginMessage} textVariant="Label">
          Scan QR code to link mobile device
        </VText>
        <VButton onPress={() => setShowCamera(true)} label="Link" />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
  },

  // Login Container Styles
  loginContainer: {
    flexDirection: "column",
    alignItems: "center",
    marginHorizontal: 24,
    marginTop: 40,
    paddingHorizontal: 25,
    paddingVertical: 45,
    borderColor: "#E6E7E8",
    borderRadius: 12,
    borderWidth: 1,
  },
  loginLabel: {
    position: "absolute",
    top: -10,
    paddingHorizontal: 25,
    color: "#565F6B",
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
    backgroundColor: "#FFF",
  },
  loginMessage: {
    marginBottom: 35,
    textAlign: "center",
    fontSize: 32,
    color: "#252F3B",
    fontWeight: "600",
    // fontFamily: "Inter",
  },
});
