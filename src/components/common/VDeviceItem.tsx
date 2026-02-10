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
      <View style={{ flexDirection: "row", alignItems: "center" }}>
        <View
          style={{
            backgroundColor: "#F4F6F9",
            width: 50,
            height: 50,
            borderTopLeftRadius: 8,
            borderBottomLeftRadius: 8,
            justifyContent: "center",
            alignItems: "center",
            marginRight: 2,
          }}
        >
          <VText textVariant="DeviceItem" style={{color: "#255F6C", fontWeight: "700", fontSize: 26 }}>
            {props.item.deviceName.slice(0, 1)}
          </VText>
        </View>
        <View
          style={{
            backgroundColor: "#F4F6F9",
            width: 8,
            height: 50,
            borderTopRightRadius: 21,
            borderBottomRightRadius: 21,
            marginRight: 8,
          }}
        />
        <VText textVariant="DeviceItem" style={{ fontSize: 15, color: "#505A66", fontWeight: "500" }}>{props.item.deviceName}</VText>
      </View>

      {props.isReconnecting ? (
        <ActivityIndicator size="small" color="#2EC4B6" />
      ) : props.state ? (
        <VText textVariant="DeviceItemState">
          {/* Connected */}
          {props.state ? "Connected" : "Disconnected"}
        </VText>
      ) : (
        <Pressable onPress={() => props.reconnect()} style={{ flexDirection: "row", alignItems: "center", backgroundColor: "#FFEDED" }}>
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
    flex: 1,
    flexDirection: "row",
    justifyContent: "space-between",
    alignContent: "center",
    alignItems: "center",
    paddingHorizontal: 4,
    paddingVertical: 4,
    borderColor: "#FAFBFC",
    borderWidth: 2,
    width: "100%",
    height: 62,
    // paddingVertical: 14,
    borderRadius: 7,
    marginTop: 12,
  },
});
