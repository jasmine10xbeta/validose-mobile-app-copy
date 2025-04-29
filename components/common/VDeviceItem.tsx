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
    height: 50,
    backgroundColor: "#FFF",
    width: "100%",
  },
});
