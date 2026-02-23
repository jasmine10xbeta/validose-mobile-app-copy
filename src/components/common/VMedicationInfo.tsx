import { StyleSheet, View } from "react-native";
import { VText } from "./VText";

interface VMedicationInfoProps {
  detailsLabel: string;
  state: number;
}

const getBackgroundColor = (state: number) => {
  switch (state) {
    case 3:
      return "#F9E2E4";
    case 4:
      return "#DDEFE4";
    case 5:
      return "#F6E8D8";
    case 6:
      return "#D4E4E8";
    default:
      return "#ECEFF3";
  }
};

const getTextColor = (state: number) => {
  switch (state) {
    case 3:
      return "#B91C1C";
    case 4:
      return "#127A4B";
    case 5:
      return "#9A5A10";
    case 6:
      return "#245B69";
    default:
      return "#5A6574";
  }
};


export function VMedicationInfo(props: VMedicationInfoProps) {
  const backgroundColor = getBackgroundColor(props.state);
  const textColor = getTextColor(props.state);

  return (
    <View style={[styles.medicationInfo, { backgroundColor }]}>
      <VText
        textVariant="LabelMedicineBold"
        style={[styles.detailsText, { color: textColor }]}
      >
        {props.detailsLabel}
      </VText>
    </View>
  );
}

const styles = StyleSheet.create({
  medicationInfo: {
    marginTop: 14,
    maxWidth: "92%",
    borderRadius: 11,
    paddingHorizontal: 16,
    paddingVertical: 10,
    alignItems: "center",
    justifyContent: "center",
  },
  detailsText: {
    width: "auto",
    textAlign: "center",
    fontSize: 17,
    fontWeight: "500",
  },
});
