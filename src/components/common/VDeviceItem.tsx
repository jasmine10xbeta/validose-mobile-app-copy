import { Pressable, StyleSheet, View, ActivityIndicator } from "react-native";
import { ValidoseDevice } from "@/types/device";
import { VText } from "./VText";

interface VDeviceItemProps {
  item: ValidoseDevice;
  state: boolean;
  reconnect: () => void;
  isReconnecting: boolean;
}

export function VDeviceItem(props: VDeviceItemProps) {
  return (
    <View style={styles.deviceItem}>
      <VText textVariant="DeviceItem">{props.item.deviceName}</VText>
      {props.isReconnecting ? (
        <ActivityIndicator size="small" color="#2EC4B6" />
      ) : props.state ? (
        <VText textVariant="DeviceItemState">
          {/* Connected */}
          {props.state ? "Connected" : "Disconnected"}
        </VText>
      ) : (
        <Pressable onPress={() => props.reconnect()}>
          <VText textVariant="DeviceItemState" style={{ color: "red" }}>
            {"Reconnect"}
          </VText>
        </Pressable>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  deviceItem: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignContent: "center",
    alignItems: "center",
    paddingHorizontal: 18,
    backgroundColor: "#F8F8F8",
    width: "100%",
    paddingVertical: 14,
    borderRadius: 8,
    marginTop: 12,
  },
});
