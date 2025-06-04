import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { StyleSheet, View } from "react-native";
import { validoseSuccess, validoseWarning } from "@/constants/colors";
import { VText } from "./VText";

interface VMedicationInfoProps {
  detailsLabel: string;
}

export function VMedicationInfo(props: VMedicationInfoProps) {
  const [assets] = useAssets([
    require("./../../assets/images/pill-timer-alert.png"),
    require("./../../assets/images/alert-diamond.png"),
    require("./../../assets/images/check-circle.png"),
    require("./../../assets/images/alert-diamond-orange.png"),
  ]);

  console.log("details label", props.detailsLabel);
  return (
    <View style={styles.medicationInfo}>
      <View style={styles.info}>
        {assets ? <Image source={assets[0]} style={styles.image} /> : null}
        <VText textVariant="LabelMedicine">{props.detailsLabel}</VText>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  medicationInfo: {
    marginTop: 20,
    width: "90%",
    height: 90,
    backgroundColor: "#F3F3F3",
    borderRadius: 8,
  },
  image: {
    height: 30,
    width: 30,
  },
  info: {
    padding: 15,
    gap: 15,
    height: "100%",
    flexDirection: "row",
    alignItems: "center",
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
