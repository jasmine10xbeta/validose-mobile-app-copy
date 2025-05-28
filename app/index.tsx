import { Amplify } from "aws-amplify";
import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import * as SecureStore from "expo-secure-store";
import { useState } from "react";
import { StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { showToast } from "@/components/common/Toast";
import { VButton } from "@/components/common/VButton";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { signInUser } from "@/utils/amplifyAWS/authService";
import awsconfig from "../utils/amplifyAWS/awsExports";

Amplify.configure(awsconfig);

export default function LoginScreen() {
  const router = useRouter();

  const [hasScanned, setHasScanned] = useState(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();

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
      // TODO: Update parsing logic. For now, assuming QR contains plain JSON {"userId":"abc123", "email":"abc123@validose.com", "password":"Temp@1234"}
      const parsed = JSON.parse(scanningResult.data);
      const { userId, email, password } = parsed;

      if (userId) {
        const isSignedIn = await signInUser(userId, email, password);

        if (isSignedIn) {
          // TODO: Get mobile device info and send to backend, if not already sent

          // TODO: Update logic to obtain and store login credentials
          await SecureStore.setItemAsync(
            "authUser",
            JSON.stringify({ userId, email, password })
          );
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
