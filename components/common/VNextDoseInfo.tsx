import { StyleSheet, View } from "react-native";
import { VMedicationInfo } from "./VMedicationInfo";
import { VText } from "./VText";

interface VNextDoseInfoProps {
  mainLabel: string;
  timeLabel: string;
  infoState: number;
}

export function VNextDoseInfo(props: VNextDoseInfoProps) {
  return (
    <View style={styles.nextDoseInfo}>
      <VText textVariant="LabelDose">Next Dose</VText>
      {props.infoState === 0 ? (
        <View style={styles.nextDoseInfoSecond}>
          <VText textVariant="Body">{props.mainLabel}</VText>
          <VText textVariant="LabelDose">{props.timeLabel}</VText>
        </View>
      ) : null}
      {props.infoState === 1 ? (
        <View style={styles.nextDoseInfoSecond}>
          <VText textVariant="Body">{props.mainLabel}</VText>
          <VText textVariant="LabelDose">within 15min</VText>
        </View>
      ) : null}
      {props.infoState === 2 ? (
        <View style={styles.nextDoseInfoSecond}>
          <VText textVariant="Body">In 4 hours</VText>
          <VText textVariant="LabelDose">from now</VText>
        </View>
      ) : null}
      {props.infoState === 3 ? (
        <View style={styles.nextDoseInfoSecond}>
          <VText textVariant="Body">In 4 hours</VText>
          <VText textVariant="LabelDose">from now</VText>
        </View>
      ) : null}

      <VMedicationInfo infoState={props.infoState} />
    </View>
  );
}

const styles = StyleSheet.create({
  nextDoseInfo: {
    alignItems: "center",
    backgroundColor: "#FFF",
    width: "100%",
    height: 210,
    gap: 20,
  },
  nextDoseInfoSecond: {
    alignItems: "center",
    backgroundColor: "#FFF",
    width: "100%",
    gap: 5,
  },
});
