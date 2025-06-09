import { StyleSheet, View } from "react-native";

interface VDoseLineProps {
  state: number;
  color: string;
}

export function VDoseLine(props: VDoseLineProps) {
  const styles = createStyles(props.state, props.color);
  return <View style={styles.doseItem} />;
}

const createStyles = (state: number, color: string) =>
  StyleSheet.create({
    doseItem: {
      width: 20,
      height: 0,
      alignSelf: "center",
      borderWidth: 1,
      borderStyle: state === 0 ? "solid" : "dashed",
      borderColor: color,
    },
  });
