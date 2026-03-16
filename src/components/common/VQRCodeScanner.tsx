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
const SHEET_HEIGHT = SCREEN_HEIGHT * 0.90;

interface VQRCodeScannerProps {
  facing?: CameraType;
  onBarcodeScanned: (result: { data: string }) => void;
  onClose: () => void;
  variant?: "full-screen" | "overlay";
  headline?: string;
  helperText?: string;
  cancelLabel?: string;
  disableTransitions?: boolean;
  inline?: boolean;
  topBarMode?: "cancel-only" | "back-title-cancel";
  backLabel?: string;
  onBack?: () => void;
  showManualEntryFooter?: boolean;
  manualEntryLabel?: string;
  onPressManualEntry?: () => void;
  manualEntryDisabled?: boolean;
}

export function QRCodeScanner({
  facing = "back",
  onBarcodeScanned,
  onClose,
  variant = "full-screen",
  headline,
  helperText = "Open the three-dot menu in Participant Management and choose Onboarding to view the QR code.",
  cancelLabel = "Cancel",
  disableTransitions = false,
  inline = false,
  topBarMode = "cancel-only",
  backLabel = "Back",
  onBack,
  showManualEntryFooter = false,
  manualEntryLabel = "Enter device ID manually",
  onPressManualEntry,
  manualEntryDisabled = false,
}: VQRCodeScannerProps) {
  const [isProcessing, setIsProcessing] = useState(false);
  const sheetTranslateY = useRef(new Animated.Value(disableTransitions ? 0 : 1)).current;

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
    if (disableTransitions) {
      sheetTranslateY.setValue(0);
      return;
    }

    Animated.timing(sheetTranslateY, {
      toValue: 0,
      duration: 260,
      useNativeDriver: true,
      easing: Easing.out(Easing.ease),
    }).start();
  }, [disableTransitions, sheetTranslateY, variant]);

  if (variant === "overlay") {
    const dismiss = () => {
      if (disableTransitions) {
        onClose();
        return;
      }

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

    const overlayContent = (
      <>
        <StatusBar
          barStyle="light-content"
          translucent
          backgroundColor="rgba(12, 17, 25, 0.95)"
        />
        <SafeAreaView style={styles.overlayRoot}>
          <TouchableOpacity
            style={StyleSheet.absoluteFill}
            activeOpacity={1}
            onPress={dismiss}
          />
          <Animated.View style={[styles.sheetContainer, { transform: [{ translateY }] }]}>
            {topBarMode === "back-title-cancel" ? (
              <View style={styles.overlayHeaderRow}>
                <TouchableOpacity
                  accessibilityRole="button"
                  accessibilityLabel="Go back"
                  onPress={onBack ?? dismiss}
                  style={styles.overlayHeaderAction}
                >
                  <VText textVariant="Body" style={styles.overlayHeaderBackText}>
                    {backLabel}
                  </VText>
                </TouchableOpacity>
                <VText textVariant="Body" style={styles.overlayHeaderTitle}>
                  {headline || "Connect"}
                </VText>
                <TouchableOpacity
                  accessibilityRole="button"
                  accessibilityLabel="Cancel scanner"
                  onPress={dismiss}
                  style={styles.overlayHeaderAction}
                >
                  <VText textVariant="Body" style={styles.overlayHeaderCancelText}>
                    {cancelLabel}
                  </VText>
                </TouchableOpacity>
              </View>
            ) : (
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
            )}
            <View
              style={[
                styles.overlayTextGroup,
                topBarMode === "back-title-cancel"
                  ? styles.overlayTextGroupWithHeader
                  : null,
              ]}
            >
              <VText textVariant="LabelDose" style={styles.overlayHelperText}>
                {helperText}
              </VText>
            </View>
            <View
              style={[
                styles.overlayCameraShell,
                showManualEntryFooter ? styles.overlayCameraShellWithFooter : null,
              ]}
            >
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
                    {"Linking...\nKeep Validose app open"}
                  </VText>
                </View>
              )}
            </View>
            {showManualEntryFooter && (
              <View style={styles.overlayManualFooter}>
                <TouchableOpacity
                  style={styles.overlayManualFooterAction}
                  accessibilityRole="button"
                  accessibilityLabel={manualEntryLabel}
                  activeOpacity={manualEntryDisabled ? 1 : 0.75}
                  disabled={manualEntryDisabled || !onPressManualEntry}
                  onPress={onPressManualEntry}
                >
                  <VText textVariant="Body" style={styles.overlayManualFooterText}>
                    {manualEntryLabel}
                  </VText>
                </TouchableOpacity>
              </View>
            )}
          </Animated.View>
        </SafeAreaView>
      </>
    );

    if (inline) {
      return overlayContent;
    }

    return (
      <Modal
        animationType={disableTransitions ? "none" : "fade"}
        transparent
        visible
        onRequestClose={dismiss}
      >
        {overlayContent}
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
  overlayHeaderRow: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  overlayHeaderAction: {
    minWidth: 84,
    paddingHorizontal: 24,
    paddingVertical: 4,
  },
  overlayHeaderBackText: {
    color: "#505A66",
    fontSize: 16,
    textAlign: "left",
    fontWeight: "400",
  },
  overlayHeaderTitle: {
    flex: 1,
    color: "#252F3B",
    textAlign: "center",
    fontSize: 17,
    fontWeight: "600",
  },
  overlayHeaderCancelText: {
    color: "#505A66",
    fontSize: 16,
    textAlign: "right",
    fontWeight: "400",
  },
  overlayTextGroup: {
    marginTop: 40,
    marginBottom: 24,
    paddingHorizontal: 12,
  },
  overlayTextGroupWithHeader: {
    marginTop: 24,
  },
  overlayHelperText: {
    fontWeight: "500",
    color: "#252F3B",
    textAlign: "center",
    fontSize: 17,
    marginTop: 18
  },
  overlayCameraShell: {
    flex: 1,
    marginTop: 24,
    overflow: "hidden",
    position: "relative",
    backgroundColor: "#000000",
  },
  overlayCameraShellWithFooter: {
    flex: 0,
    height: SHEET_HEIGHT * 0.63,
    minHeight: 260,
  },
  overlayManualFooter: {
    flex: 1,
    width: "100%",
    backgroundColor: "#FFFFFF",
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: "#DFE5ED",
    alignItems: "center",
    justifyContent: "center",
  },
  overlayManualFooterAction: {
    width: "100%",
    height: "100%",
    alignItems: "center",
    justifyContent: "center",
  },
  overlayManualFooterText: {
    width: "auto",
    alignSelf: "center",
    color: "#255F6C",
    fontSize: 16,
    fontWeight: "500",
    borderBottomWidth: 1,
    borderBottomColor: "#255F6C",
    paddingBottom: 2,
    textAlign: "center",
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
