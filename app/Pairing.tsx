import { CameraType, CameraView, useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState } from "react";
import { Pressable, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";
import { VButton } from "@/components/common/VButton";
import { VText } from "@/components/common/VText";
import useDeviceStore from "@/store/useDeviceStore";
import { QRCodeReturnData } from "@/utils/qrcode/qrCodeValidation";

export default function PairingScreen() {
  const router = useRouter();
  const { removeAll } = useDeviceStore();
  const [qrCodeValidation, setQrCodeValidation] = useState<QRCodeReturnData>({
    validation: false,
  });
  const [facing, setFacing] = useState<CameraType>("back");
  const [permission, requestPermission] = useCameraPermissions();

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

  function toggleCameraFacing() {
    setFacing((current) => (current === "back" ? "front" : "back"));
    Toast.show({
      type: "success",
      text1: "Success",
      text2: "Change Camera 📷",
    });
  }

  function onPairingPress() {
    removeAll();
    //router.push("/SetupDeviceConnect");
  }

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.pairingContainer}>
        <VText textVariant="Body">Pairing with QR Code</VText>
        <Pressable onPress={toggleCameraFacing}>
          <CameraView
            style={styles.camera}
            barcodeScannerSettings={{
              barcodeTypes: ["qr"],
            }}
            onBarcodeScanned={(scanningResult) => {
              //TODO: QRcode validation with api call should be here to proper logged into app
              if (scanningResult.data === "ValidosePairing") {
                setQrCodeValidation({ validation: true });
                Toast.show({
                  type: "success",
                  text1: "Success",
                  text2: "Proper QR code 🂾",
                });
                onPairingPress();
              } else {
                setQrCodeValidation({ validation: false });
                Toast.show({
                  type: "error",
                  text1: "Error",
                  text2: "Wrong QR code 🂾",
                });
              }
            }}
            facing={facing}
          />
        </Pressable>
        <VButton label="Flip camera" onPress={toggleCameraFacing} />
        <VButton
          disabled={!qrCodeValidation.validation}
          label="Pair"
          onPress={onPairingPress}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    height: "100%",
    justifyContent: "center",
    alignContent: "center",
    backgroundColor: "#FFF",
  },
  pairingContainer: {
    padding: 20,
    flexDirection: "column",
    alignItems: "center",
    gap: 25,
  },
  textInput: {
    height: 50,
    width: 250,
  },
  camera: {
    height: 250,
    width: 250,
    borderRadius: 25,
  },
  container: {
    flex: 1,
    justifyContent: "center",
  },
  message: {
    textAlign: "center",
    paddingBottom: 10,
  },
  buttonContainer: {
    flexDirection: "row",
  },
  button: {
    flex: 1,
    alignSelf: "flex-end",
    alignItems: "center",
  },
  text: {
    fontSize: 24,
    fontWeight: "bold",
    color: "white",
  },
});
