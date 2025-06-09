import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { StyleSheet, View } from "react-native";
import { validoseSuccess, validoseWarning } from "@/constants/colors";
import { VText } from "./VText";

interface VMedicationInfoProps {
  infoState: number;
}

export function VMedicationInfo(props: VMedicationInfoProps) {
  const [assets] = useAssets([
    require("./../../assets/images/pill-timer-alert.png"),
    require("./../../assets/images/alert-diamond.png"),
    require("./../../assets/images/check-circle.png"),
    require("./../../assets/images/alert-diamond-orange.png"),
  ]);

  return (
    <View style={styles.medicationInfo}>
      {props.infoState === 0 ? (
        <View style={styles.info}>
          {assets ? <Image source={assets[0]} style={styles.image} /> : null}
          <VText textVariant="LabelMedicine">Take medication A.</VText>
        </View>
      ) : null}
      {props.infoState === 1 ? (
        <View style={styles.info}>
          {assets ? <Image source={assets[1]} style={styles.image} /> : null}
          <VText textVariant="LabelMedicine" textAlign="left">
            You are about to miss a scheduled dose for A. Take the dose now.
          </VText>
        </View>
      ) : null}
      {props.infoState === 2 ? (
        <View style={styles.success}>
          {assets ? <Image source={assets[2]} style={styles.image} /> : null}
          <VText textVariant="LabelMedicine" textAlign="left">
            Thank you for logging a successful dose.
          </VText>
        </View>
      ) : null}
      {props.infoState === 3 ? (
        <View style={styles.warning}>
          {assets ? <Image source={assets[3]} style={styles.image} /> : null}
          <VText textVariant="LabelMedicine" textAlign="left">
            You missed a dose. Please take your dose on time.
          </VText>
        </View>
      ) : null}
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
