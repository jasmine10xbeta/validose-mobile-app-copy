import { StyleSheet, View } from "react-native";
import { VText } from "./VText";

interface VHeaderProps {
  label: string;
}

export function VHeader(props: VHeaderProps) {
  return (
    <View style={styles.header}>
      <VText textVariant="Header">{props.label}</VText>
    </View>
  );
}

const styles = StyleSheet.create({
  header: {
    backgroundColor: "#FFF",
    width: "100%",
  },
});
