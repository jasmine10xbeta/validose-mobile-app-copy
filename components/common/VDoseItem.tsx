import { StyleSheet, View } from "react-native";
import { validoseWhite } from "@/constants/colors";
import { VDoseText } from "./VDoseText";

interface VDoseItemProps {
  doseNumber: number;
  state: number;
  color: string;
}

export function VDoseItem(props: VDoseItemProps) {
  // Validate state value
  const validState = [1, 2].includes(props.state) ? props.state : 1;

  const styles = StyleSheet.create({
    doseItem: {
      borderRadius: "50%",
      height: 40,
      width: 40,
      borderWidth: validState === 1 ? 1 : 2,
      justifyContent: "center",
      borderStyle: validState === 1 ? "dashed" : "solid",
      borderColor: props.color,
      backgroundColor: validState === 2 ? props.color : undefined,
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
