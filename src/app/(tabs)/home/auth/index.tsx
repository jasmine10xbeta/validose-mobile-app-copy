import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState, useRef, type ReactNode } from "react";
import {
  StyleSheet,
  View,
  Image,
  PermissionsAndroid,
  Platform,
  Pressable,
  Text,
  type ImageSourcePropType,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import { VTopActions } from "@/components/common/VTopActions";
import { useAuth } from "@/providers/auth";
import { onboardWithCode, login } from "@/services/auth";
import useDevStore from "@/store/dev";
import { connectAndSetupDevice } from "@/utils/ble";

// Reusable UI block for login messages and button
function LoginMessageBlock({
  title,
  message,
  instructions,
  illustration,
  buttonLabel,
  onPress,
  onPressHelp,
  onPressDebug,
  personDisabled,
}: {
  title?: string;
  message: string;
  instructions?: ReactNode;
  illustration?: ImageSourcePropType;
  buttonLabel: string;
  onPress: () => void;
  onPressHelp: () => void;
  onPressDebug?: () => void;
  personDisabled?: boolean;
}) {
  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.topActions}>
        <VTopActions onPressHelp={onPressHelp} personDisabled={personDisabled} />
      </View>
      <View style={styles.loginContainer}>
        <View
          style={{
            flexDirection: "column",
            width: "100%",
            alignItems: "center",
            gap: 12,
          }}
        >
          {title && <VText textVariant={"LabelDose"}>{title}</VText>}
          <VText style={styles.loginMessage} textVariant="Label">
            {message}
          </VText>
          {illustration && (
            <Image
              source={illustration}
              style={{ width: "60%", height: "45%", resizeMode: "contain", marginVertical: 12 }}
            />
          )}
          {instructions && (
            <VText textVariant="LabelDose">{instructions}</VText>
          )}
        </View>

        <View style={styles.loginActions}>
          <VButton onPress={onPress} label={buttonLabel} />
          {onPressDebug && (
            <Pressable onPress={onPressDebug} style={styles.debugTextAction}>
              <Text style={styles.debugText}>BLE Debug Console</Text>
            </Pressable>
          )}
        </View>
      </View>
    </SafeAreaView>
  );
}

export default function LoginScreen() {
  const router = useRouter();

  const hasScannedRef = useRef(false);                      // Prevents duplicate scans
  const { user, isSignedOut, signIn } = useAuth();
  const { matchesBypassKey, enableMockBleMode } = useDevStore();
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();
  const hasAccessToken = Boolean(user?.access_token);
  const handleHelpPress = () => router.push("/home/led-info");

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
        onPressHelp={handleHelpPress}
        personDisabled={!hasAccessToken}
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
        onPressHelp={handleHelpPress}
        personDisabled={!hasAccessToken}
      />
    );
  }

  // Called when a patient QR code is scanned successfully
  const handlePatientQrScan = async (scanningResult: { data: string }) => {
    if (hasScannedRef.current) return;

    hasScannedRef.current = true;
    try {
      const onboardingCode = scanningResult?.data?.trim();

      console.log("Scanned onboarding code:", onboardingCode);

      if (onboardingCode) {
        if (matchesBypassKey(onboardingCode)) {
          enableMockBleMode();
          await signIn({
            access_token: "mock-access-token",
            refresh_token: "mock-refresh-token",
          });

          const connected = await connectAndSetupDevice("VAL-OP DEMO");
          if (connected?.error) {
            showToast("error", "Connection failed", String(connected.error));
            return;
          }

          router.replace("/home/dashboard");
          setShowCamera(false);
          return;
        }

        const response = await onboardWithCode(onboardingCode);

        if (response?.access_token) {
          await signIn(response);                           // Sign user in and redirect to pairing
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
      setTimeout(() => (hasScannedRef.current = false), 10); // Reset scanner lock after delay
    }
  };

  // Default UI state: prompt to scan QR and request permissions
  return (
    <>
      <LoginMessageBlock
        title="Setup"
        message="Scan QR Code to Link This Mobile Device"
        instructions={
          <>
            Your Site Manager should open the three-dot menu in{" "}
            <Text style={{ fontWeight: "600", color: "#252F3B" }}>Participant Management</Text>
            {" and choose Onboarding to view the QR code."}
          </>
        }
        illustration={require("../../../../assets/images/png/onboarding-qr.png")}
        buttonLabel="Scan QR code"
        onPressDebug={() => router.push("/home/ble-debug/console")}
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

          setShowCamera(true);                              // Launch camera overlay
        }}
        onPressHelp={handleHelpPress}
        personDisabled={!hasAccessToken}
      />
      {showCamera && (
        <QRCodeScanner
          variant="overlay"
          headline="Scan QR code"
          helperText="Open the three-dot menu in Participant Management and choose Onboarding to view the QR code."
          cancelLabel="Cancel"
          onBarcodeScanned={handlePatientQrScan}
          onClose={() => {
            hasScannedRef.current = false;
            setShowCamera(false);
          }}
        />
      )}
    </>
  );
}

// Styles for layout and login view
const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    height: "100%",
    backgroundColor: "#FFF",
  },
  topActions: {
    width: "100%",
    paddingHorizontal: 24,
    marginTop: 16,
  },

  loginContainer: {
    height: "100%",
    flexDirection: "column",
    alignItems: "center",
    justifyContent: "space-between",
    marginHorizontal: 24,
    paddingBottom: 100,
  },
  loginActions: {
    width: "100%",
    alignItems: "center",
  },
  debugTextAction: {
    marginTop: 14,
    marginBottom: 14,
    paddingVertical: 4,
  },
  debugText: {
    color: "#997D84",
    fontSize: 18,
    fontWeight: "700",
    textDecorationLine: "underline",
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
    textAlign: "center",
    fontSize: 30,
    color: "#252F3B",
    fontWeight: "600",
  },
});
