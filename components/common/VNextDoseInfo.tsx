import { StyleSheet, View } from "react-native";
import { VMedicationInfo } from "./VMedicationInfo";
import { VText } from "./VText";

interface VNextDoseInfoProps {
  state: number;
  mainLabel?: string;
  timeLabel?: string;
  detailsLabel?: string;
}

export function VNextDoseInfo(doseInfo: VNextDoseInfoProps) {
  const mainLabel = doseInfo?.mainLabel ?? "No Scheduled Dose";
  const timeLabel = doseInfo?.timeLabel ?? "There are no doses scheduled for the remainder of the day.";
  const detailsLabel = doseInfo?.detailsLabel ?? "";
  const state = doseInfo?.state ?? 0;

  return (
    <View style={styles.nextDoseInfo}>
      <VText textVariant="LabelDose" style={styles.label}>
        Next Dose
      </VText>
      <View style={styles.nextDoseInfoSecond}>
        <VText textVariant="Body">{mainLabel}</VText>
        <VText textVariant="LabelDose">{timeLabel}</VText>
      </View>
      {detailsLabel && <VMedicationInfo detailsLabel={detailsLabel} state={state} />}
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
