import { useEffect, useMemo, useRef } from "react";
import { Pressable, StyleSheet, View, Animated, Easing } from "react-native";
import useTreatmentStore from "@/store/treatment";
import { ValidoseDevice } from "@/types/device";
import { VText } from "./VText";

interface VDeviceItemProps {
  item: ValidoseDevice;
  state: boolean;
  reconnect: () => void;
  isReconnecting: boolean;
  index?: number;
}

function getMedicationInitial(medicationCode?: string): string {
  const normalized = (medicationCode || "").trim().toUpperCase();
  const match = normalized.match(/[A-Z0-9]/);
  return match ? match[0] : "M";
}

export function VDeviceItem(props: VDeviceItemProps) {
  const treatment = useTreatmentStore(
    (state) =>
      state.getDeviceTreatment(props.item.deviceId) ||
      state.getDeviceTreatment(props.item.deviceName)
  );
  const alphaLabel = useMemo(
    () => getMedicationInitial(treatment?.medication_code),
    [treatment?.medication_code]
  );
  const needsReconnect = !props.state && !props.isReconnecting;
  const reconnectLoaderSpin = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (!props.isReconnecting) {
      reconnectLoaderSpin.stopAnimation();
      reconnectLoaderSpin.setValue(0);
      return;
    }

    const spinnerAnimation = Animated.loop(
      Animated.timing(reconnectLoaderSpin, {
        toValue: 1,
        duration: 700,
        easing: Easing.linear,
        useNativeDriver: true,
      }),
    );
    spinnerAnimation.start();

    return () => spinnerAnimation.stop();
  }, [props.isReconnecting, reconnectLoaderSpin]);

  const reconnectLoaderRotate = reconnectLoaderSpin.interpolate({
    inputRange: [0, 1],
    outputRange: ["0deg", "360deg"],
  });

  return (
    <Pressable
      onPress={needsReconnect ? props.reconnect : undefined}
      disabled={!needsReconnect}
      style={styles.deviceItem}
    >
      <View style={styles.deviceRow}>
        <View style={styles.deviceInfoRow}>
          <View style={styles.deviceInitialPane}>
            <VText textVariant="DeviceItem" style={styles.deviceInitialText}>
              {alphaLabel}
            </VText>
          </View>
          <View
            style={[
              styles.devicePaneAccent,
              needsReconnect ? styles.devicePaneAccentReconnect : null,
            ]}
          />
          <VText textVariant="DeviceItem" style={styles.deviceNameText}>
            {props.item.deviceName}
          </VText>
        </View>

        {props.isReconnecting ? (
          <Animated.View
            style={[
              styles.reconnectLoader,
              { transform: [{ rotate: reconnectLoaderRotate }] },
            ]}
          />
        ) : props.state ? (
          <View style={styles.statusBadgeConnected}>
            <VText textVariant="DeviceItemState" style={styles.statusTextConnected}>
              Connected
            </VText>
          </View>
        ) : (
          <View style={styles.statusBadgeReconnect}>
            <VText textVariant="DeviceItemState" style={styles.statusTextReconnect}>
              {"Reconnect"}
            </VText>
          </View>
        )}
      </View>
      {needsReconnect ? (
        <View style={styles.reconnectBanner}>
          <VText textVariant="DeviceItemState" style={styles.reconnectBannerText}>
            Tap to reconnect
          </VText>
        </View>
      ) : null}
    </Pressable>
  );
}

const styles = StyleSheet.create({
  deviceItem: {
    width: "100%",
    marginTop: 12,
  },
  deviceRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignContent: "center",
    alignItems: "center",
    paddingHorizontal: 4,
    paddingVertical: 4,
    borderColor: "#FAFBFC",
    borderWidth: 2,
    width: "100%",
    height: 64,
    borderRadius: 14,
    backgroundColor: "#FFFFFF",
    shadowColor: "#000000",
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 1,
  },
  reconnectBanner: {
    borderBottomLeftRadius: 6,
    borderBottomRightRadius: 6,
    backgroundColor: "#FFEDED",
    paddingHorizontal: 12,
    paddingVertical: 9,
    marginHorizontal: 8,
    alignItems: "center",
    justifyContent: "center",
  },
  reconnectBannerText: {
    color: "#A60000",
    fontWeight: "400",
    fontSize: 17,
  },
  reconnectLoader: {
    width: 22,
    height: 22,
    borderRadius: 11,
    borderWidth: 3,
    borderColor: "#255F6C",
    borderTopColor: "rgba(46, 196, 182, 0.22)",
    marginRight: 18,
  },
  deviceInfoRow: {
    flexDirection: "row",
    alignItems: "center",
    height: "100%",
  },
  deviceInitialPane: {
    backgroundColor: "#F4F6F9",
    width: 50,
    height: "100%",
    borderTopLeftRadius: 7,
    borderBottomLeftRadius: 7,
    justifyContent: "center",
    alignItems: "center",
    marginRight: 3,
  },
  deviceInitialText: {
    color: "#252F3B",
    fontWeight: "700",
    fontSize: 26,
    textAlign: "center",
  },
  devicePaneAccent: {
    backgroundColor: "#F4F6F9",
    width: 8,
    height: 50,
    borderTopRightRadius: 21,
    borderBottomRightRadius: 21,
    marginRight: 8,
  },
  devicePaneAccentReconnect: {
    backgroundColor: "#F15050",
  },
  deviceNameText: {
    fontSize: 16,
    color: "#505A66",
    fontWeight: "500",
  },
  statusBadgeConnected: {
    paddingHorizontal: 12,
    paddingVertical: 10,
    borderRadius: 6,
    backgroundColor: "#E7F6EC",
    marginRight: 8,
  },
  statusTextConnected: {
    color: "#207245",
    fontWeight: "600",
  },
  statusBadgeReconnect: {
    flexDirection: "row",
    alignItems: "center",
    paddingHorizontal: 12,
    paddingVertical: 10,
    borderRadius: 6,
    backgroundColor: "#FFEDED",
    marginRight: 8,
  },
  statusTextReconnect: {
    color: "#C53939",
    fontWeight: "600",
  },
});
