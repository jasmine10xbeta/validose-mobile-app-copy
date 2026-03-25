import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { useEffect, useRef, useState } from "react";
import {
  Animated,
  Easing,
  FlatList,
  StyleSheet,
  View,
  Image,
  Dimensions,
  Text,
  Modal,
  TouchableOpacity,
  Pressable,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { VManualLinkingSheet } from "@/components/common/VManualLinkingSheet";
import { QRCodeScanner } from "@/components/common/VQRCodeScanner";
import { VText } from "@/components/common/VText";
import { showToast } from "@/components/common/VToast";
import { VTopActions } from "@/components/common/VTopActions";
import { useAuth } from "@/providers/auth";
import { getValidoseDevices } from "@/services/device";
import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import { connectAndSetupDevice } from "@/utils/ble";

const SCREEN_HEIGHT = Dimensions.get("window").height;

const SCAN_SLIDES = [
  {
    key: "locate",
    title: "Connect Your \nValidose Device",
    description: (
      <>
        <Text style={{ fontWeight: "600", color: "#505A66" }}>
          Make sure the device is charged.{" "}
        </Text>
        If not, place it on the charger until the green light appears.
      </>
    ),
    showAction: true,
    image: require("../../../../assets/images/png/connect-device.png"),
  },
  {
    key: "prepare",
    title: "Put the Validose device in \npairing mode",
    description: (
      <>
        <Text style={{ fontWeight: "600", color: "#505A66" }}>
          Press and hold the Validose device button{" "}
        </Text>
        {"until\nthe blue light on the Dock starts blinking\n(pairing mode)."}
      </>
    ),
    showAction: true,
    image: require("../../../../assets/images/png/pair-device.png"),
  },
  {
    key: "connect",
    title: "Connect your \nValidose device",
    description:
      (
      <>
        <Text style={{ fontWeight: "600", color: "#505A66" }}>
          Stay close to the device (within ~10m) and ensure Bluetooth is on.{" "}
        </Text>
        {"If it connects, you’ll see \"Connected\" in the app and a solid blue light on the device - you’re all set!"}
      </>
    ),
    showAction: true,
    image: require("../../../../assets/images/png/pair-phone.png"),
  },
];

export default function PairingScreen() {
  const router = useRouter();

  const { user, isLoading } = useAuth();
  const { authorizedDevices, setAuthorizedDevices } = useDeviceStore();
  const hasAccessToken = Boolean(user?.access_token);
  const handleHelpPress = () => router.push("/home/led-info");

  const hasScannedRef = useRef(false);
  const hasFetchedAuthorizedDevicesRef = useRef(false);
  const [showCamera, setShowCamera] = useState(false);
  const [permission, requestPermission] = useCameraPermissions();
  const [reconnectingDeviceId, setReconnectingDeviceId] = useState<
    string | null
  >(null);

  const [contentHeight, setContentHeight] = useState(0);
  const screenHeight = Dimensions.get("window").height;
  const devices = useDeviceStore((s) => s.devices);
  const isDevicesConnected = devices.length > 0;
  const activeConnectedDeviceCount = devices.filter((device) => device?.connected).length;
  const allListedDevicesConnected =
    devices.length > 0 && devices.every((device) => device?.connected === true);
  const isReconnectInProgress = reconnectingDeviceId !== null;
  const canShowGreenConnectedAction = allListedDevicesConnected && !isReconnectInProgress;
  const shouldContinueBeGreen = canShowGreenConnectedAction && activeConnectedDeviceCount >= 3;
  const forceConnectedButtonsWhite = !canShowGreenConnectedAction;
  const scanButtonIsWhite = forceConnectedButtonsWhite || shouldContinueBeGreen;
  const continueButtonIsWhite = forceConnectedButtonsWhite || !shouldContinueBeGreen;
  const {
    isMockBleModeEnabled,
    matchesBypassKey,
    enableMockBleMode,
    disableMockBleMode,
  } = useDevStore();
  const [showScanIntro, setShowScanIntro] = useState(false);
  const [isScanIntroMounted, setIsScanIntroMounted] = useState(false);
  const [showManualEntry, setShowManualEntry] = useState(false);
  const [manualEntrySource, setManualEntrySource] = useState<"camera" | "intro">(
    "camera",
  );
  const [introSlideIndex, setIntroSlideIndex] = useState(0);
  const introBackdropOpacity = useRef(new Animated.Value(0)).current;
  const introCardOpacity = useRef(new Animated.Value(0)).current;
  const introCardTranslateY = useRef(new Animated.Value(24)).current;
  const introAfterCloseActionRef = useRef<(() => void) | null>(null);
  const currentScanSlide = SCAN_SLIDES[introSlideIndex];
  const isLastScanSlide = introSlideIndex === SCAN_SLIDES.length - 1;

  const closeScanIntro = (afterClose?: () => void) => {
    introAfterCloseActionRef.current = afterClose ?? null;
    setShowScanIntro(false);
  };
  const closeScanIntroImmediately = (afterClose?: () => void) => {
    introAfterCloseActionRef.current = null;
    setShowScanIntro(false);
    setShowCamera(false);
    setShowManualEntry(false);
    setManualEntrySource("camera");
    setIsScanIntroMounted(false);
    introBackdropOpacity.setValue(0);
    introCardOpacity.setValue(0);
    introCardTranslateY.setValue(16);
    afterClose?.();
  };
  const closeActiveScanFlow = (afterClose?: () => void) => {
    if (showCamera || showManualEntry) {
      closeScanIntroImmediately(afterClose);
      return;
    }
    closeScanIntro(afterClose);
  };
  const openCameraFromScanIntro = () => {
    setShowManualEntry(false);
    setShowCamera(true);
  };
  const openManualEntryFromScanFlow = (source: "camera" | "intro") => {
    setManualEntrySource(source);
    setShowCamera(false);
    setShowManualEntry(true);
  };
  const returnFromManualEntry = () => {
    setShowManualEntry(false);
    if (manualEntrySource === "camera") {
      setShowCamera(true);
    }
  };

  const openScanIntro = () => {
    hasScannedRef.current = false;
    setIntroSlideIndex(0);
    setShowCamera(false);
    setShowManualEntry(false);
    setManualEntrySource("camera");
    setShowScanIntro(true);
  };

  useEffect(() => {
    if (showScanIntro) {
      setIsScanIntroMounted(true);
      introBackdropOpacity.setValue(0);
      introCardOpacity.setValue(0);
      introCardTranslateY.setValue(24);

      requestAnimationFrame(() => {
        Animated.parallel([
          Animated.timing(introBackdropOpacity, {
            toValue: 1,
            duration: 180,
            easing: Easing.out(Easing.quad),
            useNativeDriver: true,
          }),
          Animated.timing(introCardOpacity, {
            toValue: 1,
            duration: 180,
            easing: Easing.out(Easing.quad),
            useNativeDriver: true,
          }),
          Animated.timing(introCardTranslateY, {
            toValue: 0,
            duration: 220,
            easing: Easing.out(Easing.cubic),
            useNativeDriver: true,
          }),
        ]).start();
      });
      return;
    }

    if (!isScanIntroMounted) return;

    Animated.parallel([
      Animated.timing(introBackdropOpacity, {
        toValue: 0,
        duration: 140,
        easing: Easing.in(Easing.quad),
        useNativeDriver: true,
      }),
      Animated.timing(introCardOpacity, {
        toValue: 0,
        duration: 120,
        easing: Easing.in(Easing.quad),
        useNativeDriver: true,
      }),
      Animated.timing(introCardTranslateY, {
        toValue: 16,
        duration: 140,
        easing: Easing.in(Easing.quad),
        useNativeDriver: true,
      }),
    ]).start(({ finished }) => {
      if (!finished) return;
      setIsScanIntroMounted(false);
      const afterClose = introAfterCloseActionRef.current;
      introAfterCloseActionRef.current = null;
      afterClose?.();
    });
  }, [
    introBackdropOpacity,
    introCardOpacity,
    introCardTranslateY,
    isScanIntroMounted,
    showScanIntro,
  ]);

  useEffect(() => {
    if (!hasAccessToken || isLoading) {
      hasFetchedAuthorizedDevicesRef.current = false;
    }
  }, [hasAccessToken, isLoading]);

  useEffect(() => {
    async function fetchDevices() {
      if (isLoading || !hasAccessToken || isMockBleModeEnabled()) {
        return;
      }

      if (hasFetchedAuthorizedDevicesRef.current) {
        return;
      }

      // Prevent repeated /devices polling on token refreshes/re-renders.
      if (Array.isArray(authorizedDevices) && authorizedDevices.length > 0) {
        hasFetchedAuthorizedDevicesRef.current = true;
        return;
      }

      hasFetchedAuthorizedDevicesRef.current = true;
      try {
        const list = await getValidoseDevices();
        setAuthorizedDevices(list);
      } catch (error) {
        console.warn("[Pairing] Failed to fetch authorized devices:", error);
        setAuthorizedDevices([]);
      }
    }

    fetchDevices();
  }, [
    isLoading,
    hasAccessToken,
    authorizedDevices,
    setAuthorizedDevices,
    isMockBleModeEnabled,
  ]);

  async function reconnectDevice(deviceIdentifier: string) {
    if (reconnectingDeviceId) {
      showToast("info", "Another connection in progress", "Please wait..");
      return;
    }
    setReconnectingDeviceId(deviceIdentifier);
    try {
      const connected = await connectAndSetupDevice(deviceIdentifier);
      if (connected.error) showToast("error", connected.error.toString());
    } catch (error) {
      showToast(
        "error",
        "Connection failed",
        error instanceof Error ? error.message : String(error),
      );
    } finally {
      setReconnectingDeviceId(null);
    }
  }

  async function validateDeviceAddress(scanningResult: string) {
    try {
      const parsed = scanningResult;
      if (isMockBleModeEnabled()) return true;

      console.log("[APP] Scanned device name:", parsed);

      if (
        typeof parsed === "string" &&
        Array.isArray(authorizedDevices) &&
        !authorizedDevices.some((device) => device.deviceId === parsed)
      ) {
        console.log("[APP] Device not found in list");
        showToast("error", "Device not assigned to this patient");
        return false;
      }

      console.log(`[BLE] Connecting with device name: ${parsed}`);
      return true;
    } catch (err) {
      console.log(err);
      showToast("error", "Invalid QR Code", `${err}`);
      return false;
    }
  }

  // Handle QR code scanning result and connect device
  const handleDeviceQrScan = async (scanningResult: { data: string }) => {
    if (hasScannedRef.current) return;

    hasScannedRef.current = true;
    const deviceAddress = scanningResult.data?.trim();
    if (!deviceAddress) {
      hasScannedRef.current = false;
      closeActiveScanFlow();
      showToast("error", "Invalid QR Code");
      return;
    }

    if (matchesBypassKey(deviceAddress)) {
      if (isMockBleModeEnabled()) {
        disableMockBleMode();
        hasScannedRef.current = false;
        closeActiveScanFlow();
        showToast("success", "Pairing mode reset");
        return;
      }

      enableMockBleMode();
      const connected = await connectAndSetupDevice("VAL-OP DEMO");
      hasScannedRef.current = false;
      closeActiveScanFlow();

      if (connected?.error) {
        showToast("error", "Connection failed", String(connected.error));
        return;
      }

      router.push("/home/dashboard");
      return;
    }

    const isValid = await validateDeviceAddress(deviceAddress);
    if (isValid) {
      const connected = await connectAndSetupDevice(deviceAddress);
      if (connected?.error) showToast("error", connected?.error.toString());
    }

    hasScannedRef.current = false;
    closeActiveScanFlow();
  };

  if (!permission) return <View />;

  if (!permission.granted) {
    return (
      <SafeAreaView style={styles.alignContent}>
        <VTopActions
          style={styles.topActions}
          onPressHelp={handleHelpPress}
          personDisabled={!hasAccessToken}
        />
        <View style={styles.pairingContainer}>
          <VText textVariant="Body">
            We need your permission to show the camera
          </VText>
          <VButton onPress={requestPermission} label="Grant permission" />
        </View>
      </SafeAreaView>
    );
  }

  return (
    <>
      <SafeAreaView style={styles.alignContent}>
        <VTopActions
          style={styles.topActions}
          onPressHelp={handleHelpPress}
          personDisabled={!hasAccessToken}
        />
        <View
          style={[
            styles.setupContainer,
            contentHeight > screenHeight ? { height: screenHeight } : {},
          ]}
          onLayout={(e) => setContentHeight(e.nativeEvent.layout.height)}
        >
          <View
            style={{
              flexDirection: "column",
              width: "100%",
              alignItems: "center",
            }}
          >
            <VText textVariant="LabelDose">Setup</VText>
            <VText style={styles.setupMessage} textVariant="Label">
              {isDevicesConnected
                ? "Device Connection\nStatus"
                : "Let's connect\nyour device"}
            </VText>
            {!isDevicesConnected && (
              <>
                <Image
                  source={require("../../../../assets/images/png/device-qr.png")}
                  style={{
                    width: "55%",
                    height: "45%",
                    resizeMode: "contain",
                  }}
                />
                <VText textVariant="LabelDose">
                  {"Tap Scan device and scan the "}
                  <Text style={{ fontWeight: "600", color: "#505A66" }}>
                    QR sticker underneath the device.
                  </Text>
                </VText>
              </>
            )}
            {isDevicesConnected ? (
              <View
                style={{
                  marginTop: 16,
                  maxHeight: screenHeight * 0.35,
                  width: "100%",
                }}
              >
                <FlatList
                  data={devices}
                  renderItem={({ item, index }) => (
                    <VDeviceItem
                      item={item}
                      state={item?.connected}
                      reconnect={() => reconnectDevice(item.deviceId)}
                      isReconnecting={reconnectingDeviceId === item.deviceId}
                      index={index}
                    />
                  )}
                  keyExtractor={(item) => item.deviceId}
                  showsVerticalScrollIndicator={false}
                />
              </View>
            ) : (
              <View style={{ marginBottom: 18, marginTop: 21 }} />
            )}
            {!isDevicesConnected && (
              <>
              <VButton onPress={openScanIntro} label="Scan device sticker" />
                <VButton
                  onPress={() => {
                    router.push("/home/pairing/manual-pairing");
                  }}
                  style={{
                    borderWidth: 0,
                    marginTop: 18,
                  }}
                  label="Enter device ID manually"
                  labelStyle={styles.manualPairingLabel}
                />
              </>
            )}
          </View>
          <View style={styles.bottomActionsArea}>
            {isDevicesConnected && (
              <View style={styles.connectedBottomActions}>
                <VButton
                  onPress={openScanIntro}
                  label="Add next device"
                  style={[
                    styles.connectedActionButton,
                    scanButtonIsWhite
                      ? styles.connectedActionButtonWhite
                      : styles.connectedActionButtonGreen,
                  ]}
                  labelStyle={[
                    styles.connectedActionButtonLabel,
                    scanButtonIsWhite
                      ? styles.connectedActionButtonLabelDark
                      : styles.connectedActionButtonLabelLight,
                  ]}
                />
                <VButton
                  onPress={() => router.push("/home/dashboard")}
                  label="Continue to view doses"
                  style={[
                    styles.connectedActionButton,
                    styles.connectedActionButtonSpacing,
                    continueButtonIsWhite
                      ? styles.connectedActionButtonWhite
                      : styles.connectedActionButtonGreen,
                  ]}
                  labelStyle={[
                    styles.connectedActionButtonLabel,
                    continueButtonIsWhite
                      ? styles.connectedActionButtonLabelDark
                      : styles.connectedActionButtonLabelLight,
                  ]}
                />
              </View>
            )}
            <Pressable
              onPress={() => router.push("/home/ble-debug/console")}
              style={styles.debugTextAction}
            >
              <Text style={styles.debugText}>BLE Debug Console</Text>
            </Pressable>
          </View>
        </View>
      </SafeAreaView>
      {isScanIntroMounted && (
        <Modal
          animationType="none"
          transparent
          statusBarTranslucent
          visible={isScanIntroMounted}
          onRequestClose={() => {
            hasScannedRef.current = false;
            closeActiveScanFlow();
          }}
        >
          {showManualEntry ? (
            <Animated.View
              style={[styles.introOverlay, { opacity: introBackdropOpacity }]}
            >
              <Animated.View
                style={[
                  styles.introSheetWrapper,
                  {
                    opacity: introCardOpacity,
                    transform: [{ translateY: introCardTranslateY }],
                  },
                ]}
              >
                <VManualLinkingSheet
                  onBack={returnFromManualEntry}
                  onClose={() => {
                    hasScannedRef.current = false;
                    closeActiveScanFlow();
                  }}
                />
              </Animated.View>
            </Animated.View>
          ) : showCamera ? (
            <QRCodeScanner
              variant="overlay"
              inline
              disableTransitions
              topBarMode="back-title-cancel"
              headline="Connect"
              backLabel="Back"
              onBack={() => setShowCamera(false)}
              helperText={"Stay close to the device (within ~10m) and \nensure Bluetooth is on."}
              cancelLabel="Cancel"
              showManualEntryFooter
              manualEntryLabel="Enter device ID manually"
              onPressManualEntry={() => openManualEntryFromScanFlow("camera")}
              onBarcodeScanned={handleDeviceQrScan}
              onClose={() => {
                hasScannedRef.current = false;
                closeActiveScanFlow();
              }}
            />
          ) : (
            <Animated.View
              style={[styles.introOverlay, { opacity: introBackdropOpacity }]}
            >
              <Animated.View
                style={[
                  styles.introCard,
                  {
                    opacity: introCardOpacity,
                    transform: [{ translateY: introCardTranslateY }],
                  },
                ]}
              >
                <View style={styles.introHeaderRow}>
                  {introSlideIndex > 0 ? (
                    <TouchableOpacity
                      onPress={() => setIntroSlideIndex((prev) => prev - 1)}
                      style={styles.introTopAction}
                    >
                      <Text style={styles.introBackText}>Back</Text>
                    </TouchableOpacity>
                  ) : (
                    <View style={styles.introTopAction} />
                  )}
                  <Text style={styles.introHeaderTitle}>Connect</Text>
                  <TouchableOpacity
                    onPress={() => closeScanIntro()}
                    style={styles.introTopAction}
                  >
                    <Text style={styles.introCancelText}>Cancel</Text>
                  </TouchableOpacity>
                </View>

                <View key={currentScanSlide.key} style={styles.introSlide}>
                  <VText textVariant="Label" style={styles.introTitle}>
                    {currentScanSlide.title}
                  </VText>
                  <Image source={currentScanSlide.image} style={styles.introImage} />
                  <VText textVariant="LabelDose" style={styles.introDescription}>
                    {currentScanSlide.description}
                  </VText>
                </View>

                <View style={styles.introFooter}>
                  {isLastScanSlide ? (
                    <>
                      <VButton
                        label="Add next device"
                        onPress={openCameraFromScanIntro}
                      />
                      <VButton
                        onPress={() => openManualEntryFromScanFlow("intro")}
                        style={styles.introManualButton}
                        label="Enter device ID manually"
                        labelStyle={styles.manualPairingLabel}
                      />
                    </>
                  ) : (
                    <VButton
                      label="Next"
                      onPress={() =>
                        setIntroSlideIndex((prev) =>
                          Math.min(prev + 1, SCAN_SLIDES.length - 1),
                        )
                      }
                    />
                  )}
                </View>
              </Animated.View>
            </Animated.View>
          )}
        </Modal>
      )}
    </>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
  },
  topActions: {
    width: "100%",
    paddingHorizontal: 24,
    marginTop: 16,
  },

  // Setup Container Styles
  setupContainer: {
    flex: 1,
    flexDirection: "column",
    justifyContent: "space-between",
    position: "relative",
    alignItems: "center",
    paddingHorizontal: 18,
  },
  setupLabel: {
    position: "absolute",
    top: -10,
    paddingHorizontal: 25,
    color: "#505A66",
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
  },
  setupMessage: {
    textAlign: "center",
    fontSize: 30,
    color: "#252F3B",
    fontWeight: "600",
    marginTop: 8,
    paddingHorizontal: 10,
    // fontFamily: "Inter",
  },

  // Pairing Container Styles
  pairingContainer: {
    padding: 20,
    flexDirection: "column",
    alignItems: "center",
    gap: 25,
  },
  manualPairingLabel: {
    color: "#255F6C",
    fontSize: 16,
    fontWeight: "500",
    borderBottomWidth: 1,
    borderRadius: 1,
    borderColor: "#255F6C",
  },
  debugTextAction: {
    marginTop: 14,
    marginBottom: 44,
    paddingVertical: 4,
  },
  debugText: {
    color: "#997D84",
    fontSize: 18,
    fontWeight: "700",
    textDecorationLine: "underline",
  },
  bottomActionsArea: {
    width: "100%",
    alignItems: "center",
  },
  connectedBottomActions: {
    width: "100%",
    alignItems: "center",
    marginBottom: 10,
  },
  connectedActionButton: {
    width: "85%",
    borderWidth: 2,
    borderRadius: 25,
    padding: 5,
  },
  connectedActionButtonSpacing: {
    marginTop: 10,
  },
  connectedActionButtonGreen: {
    backgroundColor: "#255F6C",
    borderColor: "#255F6C",
  },
  connectedActionButtonWhite: {
    backgroundColor: "#FFFFFF",
    borderColor: "#E1E5EB",
    borderWidth: 1,
    shadowColor: "#252F3B",
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.12,
    shadowRadius: 1,
    elevation: 1,
  },
  connectedActionButtonLabel: {
    fontSize: 16,
    fontWeight: "600",
  },
  connectedActionButtonLabelLight: {
    color: "#FFFFFF",
  },
  connectedActionButtonLabelDark: {
    color: "#255F6C",
  },
  introOverlay: {
    flex: 1,
    justifyContent: "flex-end",
    backgroundColor: "rgba(8, 15, 26, 0.05)",
  },
  introSheetWrapper: {
    width: "100%",
  },
  introCard: {
    backgroundColor: "#FFFFFF",
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    paddingTop: 20,
    width: "100%",
    height: SCREEN_HEIGHT * 0.92,
  },
  introHeaderRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  introHeaderTitle: {
    flex: 1,
    textAlign: "center",
    fontSize: 17,
    fontWeight: "600",
    color: "#252F3B",
  },
  introTopAction: {
    minWidth: 84,
    paddingHorizontal: 24,
    paddingBottom: 12,
    paddingTop: 4,
  },
  introBackText: {
    fontSize: 16,
    color: "#505A66",
    fontWeight: "400",
  },
  introCancelText: {
    fontSize: 16,
    color: "#505A66",
    fontWeight: "400",
    textAlign: "right",
  },
  introSlide: {
    flex: 1,
    alignItems: "center",
    paddingHorizontal: 18,
  },
  introImage: {
    width: "55%",
    maxHeight: SCREEN_HEIGHT * 0.28,
    resizeMode: "contain",
  },
  introTitle: {
    color: "#252F3B",
    fontSize: 27,
    fontWeight: "700",
    marginTop: 100,
  },
  introDescription: {
    color: "#505A66",
    textAlign: "center",
    lineHeight: 23,
    paddingHorizontal: 10,
  },
  introFooter: {
    justifyContent: "center",
    alignItems: "center",
    marginBottom: 44,
    marginTop: 20,
    paddingHorizontal: 24,
  },
  introManualButton: {
    borderWidth: 0,
    marginTop: 21,
  },
});
