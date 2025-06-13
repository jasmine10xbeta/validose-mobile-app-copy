import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState, useRef } from "react";
import { StyleSheet, View, PermissionsAndroid, Platform } from "react-native";
import { getUniqueId } from "react-native-device-info";
import { SafeAreaView } from "react-native-safe-area-context";

import { showToast } from "@/components/common/VToast";
import { VButton } from "@/components/common/VButton";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { onboardWithCode } from "@/utils/axios/api/__mocks__/onboarding";
import { useAuth } from "@/utils/provider/AuthenticationProvider";

function LoginMessageBlock({
  message,
  buttonLabel,
  onPress,
}: {
  message: string;
  buttonLabel: string;
  onPress: () => void;
}) {
  return <SafeAreaView style={styles.alignContent}>
    <View style={styles.loginContainer}>
      <VText style={styles.loginMessage} textVariant="Label">
        {message}
      </VText>
      <VButton onPress={onPress} label={buttonLabel} />
    </View>
  </SafeAreaView>
}

export default function LoginScreen() {
  const router = useRouter();

  const hasScannedRef = useRef(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();
  const { isSignedOut, signIn } = useAuth();

  async function requestBluetoothPermissions(): Promise<boolean> {
    if (Platform.OS !== "android" || Platform.Version < 31) {
      return true; // Not required for iOS or older Android
    }

    try {
      const granted = await PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
        PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
      ]);

      const allGranted = Object.values(granted).every(
        (status) => status === PermissionsAndroid.RESULTS.GRANTED
      );

      return allGranted;
    } catch (err) {
      console.log("Bluetooth permission error", err);
      return false;
    }
  }

  if (!permission) return <View />;

  if (!permission.granted) {
    return (
      <LoginMessageBlock
        message="We need your permission to show the camera"
        buttonLabel="Grant permission"
        onPress={requestPermission}
      />
    );
  }

  if (isSignedOut) {
    return (
      <LoginMessageBlock
        message="Let's get you reconnected"
        buttonLabel="Reconnect"
        onPress={() => {}}
      />
    );
  }

  const handlePatientQrScan = async (scanningResult: { data: string }) => {
    if (hasScannedRef.current) return;

    hasScannedRef.current = true;
    try {
      // TODO: Update parsing logic.
      // Assuming QR contains plain JSON {"code":"xyz"}
      const { code: onboardingCode } = JSON.parse(scanningResult?.data);

      console.log("\n");
      console.log("Scanned onboarding code:", onboardingCode);

      if (onboardingCode) {
        const mobileDeviceId = await getUniqueId();
        const response = await onboardWithCode(onboardingCode, mobileDeviceId);

        console.log("\n");
        console.log(`Pairing API reponse for mobile device with unique id ${mobileDeviceId}:\n ${JSON.stringify(response)}`);

        if (response?.access_token) {
          await signIn({
            token: response.access_token,
            refreshToken: response.refresh_token,
          });
          router.replace("/pairing");
          setShowCamera(false);
        } else {
          showToast("error", "Onboarding failed", "No token received.");
        }
      } else {
        showToast("error", "Invalid QR Code");
      }
    } catch (err) {
      showToast("error", "Invalid QR Code", `${err}`);
    } finally {
      setTimeout(() => {
        hasScannedRef.current = false;
      }, 2000);
    }
  };

  if (showCamera) {
    return (
      <QRCodeScanner
        onBarcodeScanned={handlePatientQrScan}
        onClose={() => !hasScannedRef.current}
      />
    );
  }

  return (
    <LoginMessageBlock
      message="Scan QR code to link mobile device"
      buttonLabel="Link"
      onPress={async () => {
        const granted = await requestBluetoothPermissions();
        if (!granted) {
          showToast("error", "Bluetooth permission denied", "Bluetooth features may not work.");
          return;
        }

        const cameraGranted = await requestPermission();
        if (!cameraGranted?.granted) {
          showToast("error", "Camera permission denied", "Cannot proceed without camera.");
          return;
        }

        setShowCamera(true);
      }}
      // onPress={() => setShowCamera(true)}
    />
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
