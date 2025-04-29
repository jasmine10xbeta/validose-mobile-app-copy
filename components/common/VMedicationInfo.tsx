import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { StyleSheet, View } from "react-native";
import { validoseSuccess, validoseWarning } from "@/constants/Colors";
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
          <VText textVariant="LabelMedicine">Take medication 1.</VText>
        </View>
      ) : null}
      {props.infoState === 1 ? (
        <View style={styles.info}>
          {assets ? <Image source={assets[1]} style={styles.image} /> : null}
          <VText textVariant="LabelMedicine" textAlign="left">
            You are about to miss a scheduled dose for 1.
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
    padding: 10,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-around",
    width: "80%",
    height: 100,
  },
  image: {
    height: 30,
    width: 30,
  },
  info: {
    gap: 10,
    borderRadius: 10,
    padding: 10,
    flexDirection: "row",
    alignItems: "center",
  },
  success: {
    gap: 10,
    backgroundColor: validoseSuccess,
    borderRadius: 10,
    padding: 10,
    flexDirection: "row",
    alignItems: "center",
  },
  warning: {
    gap: 10,
    backgroundColor: validoseWarning,
    borderRadius: 10,
    padding: 10,
    flexDirection: "row",
    alignItems: "center",
  },
});
