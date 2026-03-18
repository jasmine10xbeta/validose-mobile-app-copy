import { StyleSheet, Text, View } from "react-native";
import { validoseSuccess, validoseWarning } from "@/constants/colors";
import { VText } from "./VText";

interface VMedicationInfoProps {
  detailsLabel: string;
  state: number;
  isInDosingWindow?: boolean;
  isMissed?: boolean;
  highlightedMedication?: string;
}

const getBackgroundColor = (state: number) => {
  switch (state) {
    case 3:
      return "#FFEDED"; // Red (in window and not taken)
    case 4:
      return "#E8FAF0"; // Green (taken)
    case 5:
      return "#FFEDED"; // Orange (missed)
    case 6:
      return "#DFEFF0"; // Grey with alarm icon
    default:
      return "#FAFBFC"; // Gray (no dose or future)
  }
};

export function VMedicationInfo(props: VMedicationInfoProps) {
  const backgroundColor = getBackgroundColor(props.state);
  const detailsTextColor = props.isInDosingWindow
    ? "#A60000"
    : props.isMissed || props.state === 5
      ? "#A60000"
      : "#20535E";

  const getDetailsLabel = () => {
    if (!props.highlightedMedication) {
      return props.detailsLabel;
    }

    const matchIndex = props.detailsLabel.indexOf(props.highlightedMedication);
    if (matchIndex === -1) {
      return props.detailsLabel;
    }

    const before = props.detailsLabel.slice(0, matchIndex);
    const after = props.detailsLabel.slice(
      matchIndex + props.highlightedMedication.length
    );

    return (
      <>
        {before}
        <Text style={styles.medicationNameBold}>{props.highlightedMedication}</Text>
        {after}
      </>
    );
  };

  return (
    <View style={[styles.medicationInfo, { backgroundColor }]}>
      <VText textVariant="LabelMedicine" style={{ color: detailsTextColor }}>
        {getDetailsLabel()}
      </VText>
    </View>
  );
}

const styles = StyleSheet.create({
  medicationInfo: {
    marginTop: 20,
    backgroundColor: "#FAFBFC",
    borderRadius: 8,
    padding: 12,
  },
  medicationNameBold: {
    fontWeight: "700",
  },
  success: {
    gap: 15,
    height: "100%",
    width: "100%",
    backgroundColor: validoseSuccess,
    borderRadius: 10,
    padding: 12,
    flexDirection: "row",
    alignItems: "center",
  },
  warning: {
    gap: 15,
    height: "100%",
    backgroundColor: validoseWarning,
    borderRadius: 10,
    padding: 12,
    flexDirection: "row",
    alignItems: "center",
  },
});
