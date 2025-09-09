import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { StyleSheet, View } from "react-native";
import { validoseSuccess, validoseWarning } from "@/constants/colors";
import { VText } from "./VText";

interface VMedicationInfoProps {
  detailsLabel: string;
  state: number;
}

const getBackgroundColor = (state: number) => {
  switch (state) {
    case 3:
      return "#FC9E9E33"; // Red (in window and not taken)
    case 4:
      return "#65BB8D33"; // Green (taken)
    case 5:
      return "#FFC88399"; // Orange (missed)
    default:
      return "#F3F3F3"; // Gray (no dose or future)
  }
};

// Icon index per state in assets[]
const getIconIndex = (state: number) => {
  switch (state) {
    case 3:
      return 0; // alert-diamond-red
    case 4:
      return 1; // check-circle
    case 5:
      return 2; // alert-diamond-orange
    default:
      return 3; // pill-timer-alert (default/gray/future)
  }
};


export function VMedicationInfo(props: VMedicationInfoProps) {
  const [assets] = useAssets([
    require("./../../assets/images/png/alert-diamond-red.png"),
    require("./../../assets/images/png/check-circle.png"),
    require("./../../assets/images/png/alert-diamond-orange.png"),
    require("./../../assets/images/png/pill-timer-alert.png"),
  ]);

  const backgroundColor = getBackgroundColor(props.state);
  const iconIndex = getIconIndex(props.state);
  const icon = assets?.[iconIndex];

  return (
    <View style={[styles.medicationInfo, { backgroundColor }]}>
      {assets ? <Image source={icon} style={styles.image} /> : null}
      <VText textVariant="LabelMedicine">{props.detailsLabel}</VText>
    </View>
  );
}

const styles = StyleSheet.create({
  medicationInfo: {
    marginTop: 20,
    width: "90%",
    backgroundColor: "#F3F3F3",
    borderRadius: 8,
    padding: 15,
    gap: 15,
    flexDirection: "row",
    alignItems: "center",
  },
  image: {
    height: 30,
    width: 30,
  },
  success: {
    gap: 15,
    height: "100%",
    width: "100%",
    backgroundColor: validoseSuccess,
    borderRadius: 10,
    padding: 15,
    flexDirection: "row",
    alignItems: "center",
  },
  warning: {
    gap: 15,
    height: "100%",
    backgroundColor: validoseWarning,
    borderRadius: 10,
    padding: 15,
    flexDirection: "row",
    alignItems: "center",
  },
});
