import { CameraType, CameraView } from "expo-camera";
import { useCallback, useState } from "react";
import {
  ActivityIndicator,
  StatusBar,
  StyleSheet,
  View,
  Vibration,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "./VButton";
import { VText } from "./VText";

interface VQRCodeScannerProps {
  facing?: CameraType;
  onBarcodeScanned: (result: { data: string }) => void;
  onClose: () => void;
}

export function QRCodeScanner({
  facing = "back",
  onBarcodeScanned,
  onClose,
}: VQRCodeScannerProps) {
  const [isProcessing, setIsProcessing] = useState(false);

  const handleBarcodeScanned = useCallback(
    (result: { data: string }) => {
      if (isProcessing || !result?.data) return;

      setIsProcessing(true);
      Vibration.vibrate(5);
      onBarcodeScanned(result);
    },
    [isProcessing, onBarcodeScanned]
  );

  return (
    <SafeAreaView style={styles.dimBackground}>
      <StatusBar hidden />
      <View style={styles.camera}>
        <CameraView
          style={styles.camera}
          facing={facing}
          barcodeScannerSettings={{ barcodeTypes: ["qr"] }}
          onBarcodeScanned={handleBarcodeScanned}
        />
        <View style={styles.overlay}>
          <VText textVariant="Header" style={styles.overlayText}>
            Scan QR Code
          </VText>
          <View style={styles.cornerBracketTL} />
          <View style={styles.cornerBracketTR} />
          <View style={styles.cornerBracketBL} />
          <View style={styles.cornerBracketBR} />
        </View>
        <VButton
          onPress={onClose}
          label="✕"
          style={styles.closeButton}
          labelStyle={styles.closeButtonLabel}
        />
        {isProcessing && (
          <View style={styles.loaderOverlay}>
            {/* <View style={styles.loaderCard}> */}
              <ActivityIndicator size="large" color="#7ce3ff" />
              <VText textVariant="Body" style={styles.loaderText}>
                Validating QR Code…
              </VText>
            {/* </View> */}
          </View>
        )}
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  dimBackground: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#000",
  },

  // Camera & Overlay Styles
  camera: {
    width: "100%",
    height: "100%",
  },
  overlay: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    alignItems: "center",
    justifyContent: "center",
  },
  overlayText: {
    position: "absolute",
    top: 40,
    color: "#FFF",
    fontSize: 18,
    fontWeight: "500",
    fontFamily: "Inter",
  },
  cornerBracketTL: {
    position: "absolute",
    top: "25%",
    left: "15%",
    width: 30,
    height: 30,
    borderTopWidth: 2,
    borderLeftWidth: 2,
    borderColor: "#FFF",
  },
  cornerBracketTR: {
    position: "absolute",
    top: "25%",
    right: "15%",
    width: 30,
    height: 30,
    borderTopWidth: 2,
    borderRightWidth: 2,
    borderColor: "#FFF",
  },
  cornerBracketBR: {
    position: "absolute",
    bottom: "25%",
    right: "15%",
    width: 30,
    height: 30,
    borderBottomWidth: 2,
    borderRightWidth: 2,
    borderColor: "#FFF",
  },
  cornerBracketBL: {
    position: "absolute",
    bottom: "25%",
    left: "15%",
    width: 30,
    height: 30,
    borderBottomWidth: 2,
    borderLeftWidth: 2,
    borderColor: "#FFF",
  },

  // Close Button Styles
  closeButton: {
    position: "absolute",
    top: 35,
    right: 40,
    width: 40,
    height: 40,
    borderWidth: 0,
    justifyContent: "center",
    alignItems: "center",
  },
  closeButtonLabel: {
    fontSize: 18,
    fontWeight: "bold",
    color: "#FFF",
  },
  loaderOverlay: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: "rgba(0, 0, 0, 0.45)",
    alignItems: "center",
    justifyContent: "center",
  },
  loaderCard: {
    paddingHorizontal: 24,
    paddingVertical: 20,
    borderRadius: 18,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "rgba(20, 22, 36, 0.9)",
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "rgba(124, 227, 255, 0.4)",
    gap: 22,
  },
  loaderText: {
    color: "#E9F8FF",
    fontSize: 14,
    fontWeight: "500",
  },
});
