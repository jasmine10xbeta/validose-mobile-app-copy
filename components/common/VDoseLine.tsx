import { StyleSheet, View } from "react-native";

interface VDoseLineProps {
  state: number;
  color: string;
}

export function VDoseLine(props: VDoseLineProps) {
  const styles = StyleSheet.create({
    doseItem: {
      width: 20,
      borderWidth: 1,
      borderStyle: props.state === 0 ? "solid" : "dashed",
      borderColor: props.color,
    },
  });

  return <View style={styles.doseItem} />;
}
