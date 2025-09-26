import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState, useRef } from "react";
import { StyleSheet, View, PermissionsAndroid, Platform } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import { useAuth } from "@/providers/auth";
import { onboardWithCode, login } from "@/services/auth";

// Reusable UI block for login messages and button
function LoginMessageBlock({
  message,
  buttonLabel,
  onPress,
}: {
  message: string;
  buttonLabel: string;
  onPress: () => void;
}) {
  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.loginContainer}>
        <VText style={styles.loginMessage} textVariant="Label">
          {message}
        </VText>
        <VButton onPress={onPress} label={buttonLabel} />
      </View>
    </SafeAreaView>
  );
}

export default function LoginScreen() {
  const router = useRouter();

  const hasScannedRef = useRef(false); // Prevents duplicate scans
  const { isSignedOut, signIn } = useAuth();
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();

  // Handles runtime permissions for Bluetooth on Android 12+
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

      // Check if all requested permissions are granted
      const allGranted = Object.values(granted).every(
        (status) => status === PermissionsAndroid.RESULTS.GRANTED
      );

      return allGranted;
    } catch (err) {
      console.log("Bluetooth permission error", err);
      return false;
    }
  }

  // UI state: show blank screen while checking permission
  if (!permission) return <View />;

  // UI state: prompt user to grant camera access
  if (!permission.granted) {
    return (
      <LoginMessageBlock
        message="We need your permission to show the camera"
        buttonLabel="Grant permission"
        onPress={requestPermission}
      />
    );
  }

  // UI state: show reconnect prompt if user is signed out
  if (isSignedOut) {
    return (
      <LoginMessageBlock
        message="Let's get you reconnected"
        buttonLabel="Reconnect"
        onPress={async () => {
          const session = await login();
          await signIn(session);
        }}
      />
    );
  }

  // Called when a patient QR code is scanned successfully
  const handlePatientQrScan = async (scanningResult: { data: string }) => {
    if (hasScannedRef.current) return;

    hasScannedRef.current = true;
    try {
      const onboardingCode = scanningResult?.data;

      console.log("\n");
      console.log("Scanned onboarding code:", onboardingCode);

      if (onboardingCode) {
        const response = await onboardWithCode(onboardingCode);

        if (response?.access_token) {
          await signIn(response);                         // Sign user in and redirect to pairing
          router.replace("/home/pairing");
          setShowCamera(false);
        } else {
          showToast("error", "Onboarding failed", "Please try again.");
        }
      } else {
        showToast("error", "Invalid QR Code");
      }
    } catch (err) {
      const errorMessage =
        (err as any)?.response?.data?.message ||            // Check for Axios error shape (err.response?.data?.message)
        (err as any)?.message ||                            // Fallback to err.message
        String(err);                                        // Fallback to stringified error
      showToast("error", "Invalid QR Code", `${errorMessage}`);
      setShowCamera(false);
    } finally {
      setShowCamera(false);
      setTimeout(() => hasScannedRef.current = false, 10); // Reset scanner lock after delay
    }
  };

  // UI state: show camera scanner when ready
  if (showCamera) {
    return (
      <QRCodeScanner
        onBarcodeScanned={handlePatientQrScan}
        onClose={() => {
          hasScannedRef.current = !hasScannedRef.current;
          setShowCamera(false);
        }}
      />
    );
  }

  // Default UI state: prompt to scan QR and request permissions
  return (
    <LoginMessageBlock
      message="Scan QR code to link mobile device"
      buttonLabel="Link"
      onPress={async () => {
        const granted = await requestBluetoothPermissions();
        if (!granted) {
          showToast(
            "error",
            "Bluetooth permission denied",
            "Bluetooth features may not work."
          );
          return;
        }

        const cameraGranted = await requestPermission();
        if (!cameraGranted?.granted) {
          showToast(
            "error",
            "Camera permission denied",
            "Cannot proceed without camera."
          );
          return;
        }

        setShowCamera(true); // Launch camera view
      }}
    />
  );
}

// Styles for layout and login view
const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
  },

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
    backgroundColor: "#FFF",
  },
  loginMessage: {
    marginBottom: 35,
    textAlign: "center",
    fontSize: 32,
    color: "#252F3B",
    fontWeight: "600",
  },
});
