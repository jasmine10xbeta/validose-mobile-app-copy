import { CameraType, CameraView, useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useState } from "react";
import { StyleSheet, View, Text, Pressable } from "react-native";
import { Button } from "react-native-paper";
import { SafeAreaView } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";
import { VButton } from "@/components/common/VButton";
import { VText } from "@/components/common/VText";
import { validoseMedication1, validoseMedication2 } from "@/constants/Colors";
import useDeviceStore from "@/store/useDeviceStore";

export default function QRCodeScannerScreen() {
  const router = useRouter();
  const { addDevice } = useDeviceStore();
  const [facing, setFacing] = useState<CameraType>("back");
  const [permission, requestPermission] = useCameraPermissions();

  if (!permission) {
    return <View />;
  }

  if (!permission.granted) {
    return (
      <View style={styles.container}>
        <Text style={styles.message}>
          We need your permission to show the camera
        </Text>
        <Button onPress={requestPermission}>Grant permission</Button>
      </View>
    );
  }

  function toggleCameraFacing() {
    setFacing((current) => (current === "back" ? "front" : "back"));
  }

  function onBackPress() {
    if (router.canGoBack()) {
      router.back();
    }
  }

  function onScannSuccess() {
    if (router.canGoBack()) {
      router.back();
    }
  }

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.pairingContainer}>
        <VText textVariant="Body">Scann QR Code on Bluetooth device</VText>
        <Pressable onPress={toggleCameraFacing}>
          <CameraView
            style={styles.camera}
            barcodeScannerSettings={{
              barcodeTypes: ["qr"],
            }}
            onBarcodeScanned={(scanningResult) => {
              if (scanningResult.data.includes("ValidoseDevice")) {
                addDevice({
                  id: scanningResult.data,
                  name: scanningResult.data,
                  medicine: scanningResult.data.slice(
                    scanningResult.data.length - 1
                  ),
                  modicineState: 0,
                  color: scanningResult.data.includes("ValidoseDevice1")
                    ? validoseMedication1
                    : validoseMedication2,
                });
                Toast.show({
                  type: "success",
                  text1: "Success",
                  text2: "Device QR code 📱",
                });
                onScannSuccess();
              } else {
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
        <VButton label="Back to Setup" onPress={onBackPress} />
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
  loginContainer: {
    alignContent: "center",
    justifyContent: "center",
    flexDirection: "column",
    alignItems: "center",
    gap: 10,
    height: "100%",
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
