import { StyleSheet, View } from "react-native";
import { Device } from "@/store/useDeviceStore";
import { VText } from "./VText";

interface VDeviceItemProps {
  item: Device;
  state: string;
}

export function VDeviceItem(props: VDeviceItemProps) {
  return (
    <View style={styles.deviceItem}>
      <VText textVariant="DeviceItem">{props.item.name}</VText>
      <VText textVariant="DeviceItemState">{props.state}</VText>
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
