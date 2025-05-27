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
      <VText textVariant="LabelDose" style={styles.label}>
        Next Dose
      </VText>
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
    flexDirection: "column",
    alignItems: "center",
    marginTop: 40,
    paddingTop: 35,
    paddingBottom: 15,
    borderColor: "#E6E7E8",
    borderRadius: 12,
    borderWidth: 1,
    width: "100%",
  },
  nextDoseInfoSecond: {
    alignItems: "center",
    backgroundColor: "#FFF",
    width: "100%",
    gap: 5,
  },
  label: {
    position: "absolute",
    top: -10,
    paddingHorizontal: 25,
    color: "#565F6B",
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
    backgroundColor: "#FFF",
  },
});
