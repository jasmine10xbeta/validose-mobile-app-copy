import { CameraType, CameraView } from "expo-camera";
import { useCallback, useEffect, useRef, useState } from "react";
import {
  ActivityIndicator,
  Animated,
  Dimensions,
  Easing,
  Modal,
  StatusBar,
  StyleSheet,
  TouchableOpacity,
  View,
  Vibration,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "./VButton";
import { VText } from "./VText";

const SCREEN_HEIGHT = Dimensions.get("window").height;
const SHEET_HEIGHT = SCREEN_HEIGHT * 0.9;

interface VQRCodeScannerProps {
  facing?: CameraType;
  onBarcodeScanned: (result: { data: string }) => void;
  onClose: () => void;
  variant?: "full-screen" | "overlay";
  headline?: string;
  helperText?: string;
  cancelLabel?: string;
}

export function QRCodeScanner({
  facing = "back",
  onBarcodeScanned,
  onClose,
  variant = "full-screen",
  helperText = "Open the three-dot menu in Participant Management and choose Onboarding to view the QR code.",
  cancelLabel = "Cancel",
}: VQRCodeScannerProps) {
  const [isProcessing, setIsProcessing] = useState(false);
  const sheetTranslateY = useRef(new Animated.Value(1)).current;

  const handleBarcodeScanned = useCallback(
    (result: { data: string }) => {
      if (isProcessing || !result?.data) return;

      setIsProcessing(true);
      Vibration.vibrate(5);
      onBarcodeScanned(result);
    },
    [isProcessing, onBarcodeScanned]
  );

  useEffect(() => {
    if (variant !== "overlay") return;

    Animated.timing(sheetTranslateY, {
      toValue: 0,
      duration: 260,
      useNativeDriver: true,
      easing: Easing.out(Easing.ease),
    }).start();
  }, [variant, sheetTranslateY]);

  if (variant === "overlay") {
    const dismiss = () => {
      Animated.timing(sheetTranslateY, {
        toValue: 1,
        duration: 200,
        useNativeDriver: true,
        easing: Easing.in(Easing.ease),
      }).start(({ finished }) => {
        if (finished) onClose();
      });
    };

    const translateY = sheetTranslateY.interpolate({
      inputRange: [0, 1],
      outputRange: [0, SHEET_HEIGHT + 100],
    });

    return (
      <Modal
        animationType="fade"
        transparent
        visible
        onRequestClose={dismiss}
      >
        <StatusBar barStyle="light-content" translucent backgroundColor="rgba(12, 17, 25, 0.95)" />
        <SafeAreaView style={styles.overlayRoot}>
          <TouchableOpacity
            style={StyleSheet.absoluteFill}
            activeOpacity={1}
            onPress={dismiss}
          />
          <Animated.View style={[styles.sheetContainer, { transform: [{ translateY }] }]}
          >
            <TouchableOpacity
              accessibilityRole="button"
              accessibilityLabel="Cancel scanner"
              onPress={dismiss}
              style={styles.overlayCancelButton}
            >
              <VText textVariant="Body" style={styles.overlayCancelText}>
                {cancelLabel}
              </VText>
            </TouchableOpacity>
            <View style={styles.overlayTextGroup}>
              <VText textVariant="LabelDose" style={styles.overlayHelperText}>
                {helperText}
              </VText>
            </View>
            <View style={styles.overlayCameraShell}>
              <CameraView
                style={styles.overlayCamera}
                facing={facing}
                barcodeScannerSettings={{ barcodeTypes: ["qr"] }}
                onBarcodeScanned={handleBarcodeScanned}
              />
              {isProcessing && (
                <View style={styles.loaderOverlay}>
                  <ActivityIndicator size="large" color="#7ce3ff" />
                  <VText textVariant="Body" style={styles.loaderText}>
                    Validating QR Code…
                  </VText>
                </View>
              )}
            </View>
          </Animated.View>
        </SafeAreaView>
      </Modal>
    );
  }

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
              <ActivityIndicator size="large" color="#7ce3ff" />
              <VText textVariant="Body" style={styles.loaderText}>
                Validating QR Code…
              </VText>
          </View>
        )}
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  overlayRoot: {
    flex: 1,
    backgroundColor: "rgba(5, 12, 24, 0.15)",
    justifyContent: "flex-end",
    paddingHorizontal: 0,
  },
  sheetContainer: {
    height: SHEET_HEIGHT,
    backgroundColor: "#FFFFFF",
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    paddingTop: 20,
  },
  overlayCancelButton: {
    paddingVertical: 4,
    paddingHorizontal: 24,
    alignItems: "flex-start",
  },
  overlayCancelText: {
    color: "#505A66",
    fontSize: 18,
    textAlign: "left",
    width: "auto",
    fontWeight: "400",
  },
  overlayTextGroup: {
    marginTop: 40,
    marginBottom: 24,
    paddingHorizontal: 12,
  },
  overlayHelperText: {
    fontWeight: "600",
    color: "#272F3A",
    textAlign: "center",
    fontSize: 17
  },
  overlayCameraShell: {
    flex: 1,
    marginTop: 24,
    overflow: "hidden",
    position: "relative",
    backgroundColor: "#000000",
    minHeight: SHEET_HEIGHT * 0.9,
  },
  overlayCamera: {
    width: "100%",
    height: "100%",
  },
  overlayCorner: {
    borderColor: "#FFFFFF",
  },
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
