import { StyleSheet, View } from "react-native";
import { validoseWhite } from "@/constants/Colors";
import { VDoseText } from "./VDoseText";

interface VDoseItemProps {
  doseNumber: number;
  state: number;
  color: string;
}

export function VDoseItem(props: VDoseItemProps) {
  const styles = StyleSheet.create({
    doseItem: {
      borderRadius: "50%",
      height: 40,
      width: 40,
      borderWidth: props.state === 1 ? 1 : 2,
      justifyContent: "center",
      borderStyle: props.state === 1 ? "dashed" : "solid",
      borderColor: props.color,
      backgroundColor: props.state === 2 ? props.color : undefined,
    },
  });

  if (props.doseNumber === 4) {
    return (
      <View style={styles.doseItem}>
        <VDoseText color={props.color}>!</VDoseText>
      </View>
    );
  }

  return (
    <View style={styles.doseItem}>
      <VDoseText color={props.state === 2 ? validoseWhite : props.color}>
        {props.state === 2 ? "✓" : props.doseNumber}
      </VDoseText>
    </View>
  );
}
