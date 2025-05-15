import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { useState } from "react";
import { Pressable, StyleSheet, View } from "react-native";
import Toast from "react-native-toast-message";
import {
  validoseMedication,
  validoseMedication1,
  validoseMedicationError,
} from "@/constants/Colors";
import { Device } from "@/store/useDeviceStore";
import { VDoseItem } from "./VDoseItem";
import { VDoseLine } from "./VDoseLine";
import { VText } from "./VText";
import { showToast } from "@/utils/toastUtils";

interface VMedicationItemProps {
  item: Device;
  state: string;
  color: string;
}

export function VMedicationItem(props: VMedicationItemProps) {
  const [assets] = useAssets([
    require("./../../assets/images/link-broken.png"),
    require("./../../assets/images/alert-diamond.png"),
  ]);

  const stylesComputed = StyleSheet.create({
    medSection: {
      borderTopLeftRadius: 12,
      borderBottomLeftRadius: 12,
      width: 65,
      backgroundColor: props.color,
      justifyContent: "center",
    },
  });
  const [medicationState, setMedicationState] = useState<number>(0);

  function onPresMedicationItem() {
    if (medicationState > 5) {
      setMedicationState(0);
    } else {
      if (medicationState === 0) {
        showToast("success", "Successful dose", "Congratulations you made the first dose.");
      }
      if (medicationState === 1) {
        showToast("success", "Successful dose", "Congratulations you made the second dose.");
      }
      if (medicationState === 2) {
        showToast("success", "Successful dose", "Congratulations you made the third dose.");
      }
      if (medicationState === 3) {
        showToast("error", "Missed dose", "Please take your dose on time.");
      }
      if (medicationState === 4) {
        showToast("error", "No connection", "Please check your connection.");
      }
      if (medicationState === 5) {
        showToast("error", "Device error", "Contact support.");
      }
      setMedicationState((prev) => prev + 1);
    }
  }

  return (
    <Pressable onPress={onPresMedicationItem} style={styles.deviceItem}>
      {medicationState === 0 ? (
        <>
          <View style={stylesComputed.medSection}>
            <VText textVariant="LabelMedicine1">MED</VText>
            <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
          </View>
          <View style={styles.doseSection}>
            <VDoseItem color={props.color} doseNumber={1} state={0} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={2} state={1} />
            <VDoseLine color={props.color} state={1} />
            <VDoseItem color={props.color} doseNumber={3} state={1} />
          </View>
        </>
      ) : null}
      {medicationState === 1 ? (
        <>
          <View style={stylesComputed.medSection}>
            <VText textVariant="LabelMedicine1">MED</VText>
            <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
          </View>
          <View style={styles.doseSection}>
            <VDoseItem color={props.color} doseNumber={1} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={2} state={0} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={3} state={1} />
          </View>
        </>
      ) : null}
      {medicationState === 2 ? (
        <>
          <View style={stylesComputed.medSection}>
            <VText textVariant="LabelMedicine1">MED</VText>
            <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
          </View>
          <View style={styles.doseSection}>
            <VDoseItem color={props.color} doseNumber={1} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={2} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={3} state={0} />
          </View>
        </>
      ) : null}
      {medicationState === 3 ? (
        <>
          <View style={stylesComputed.medSection}>
            <VText textVariant="LabelMedicine1">MED</VText>
            <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
          </View>
          <View style={styles.doseSection}>
            <VDoseItem color={props.color} doseNumber={1} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={2} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={3} state={2} />
          </View>
        </>
      ) : null}
      {medicationState === 4 ? (
        <>
          <View style={stylesComputed.medSection}>
            <VText textVariant="LabelMedicine1">MED</VText>
            <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
          </View>
          <View style={styles.doseSection}>
            <VDoseItem color={props.color} doseNumber={1} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={2} state={2} />
            <VDoseLine color={props.color} state={0} />
            <VDoseItem color={props.color} doseNumber={4} state={0} />
          </View>
        </>
      ) : null}
      {medicationState === 5 ? (
        <>
          <View style={styles.medSectionError}>
            <VText textVariant="LabelMedicine1Dark">MED</VText>
            <VText textVariant="LabelMedicine2Dark">
              {props.item.medicine}
            </VText>
          </View>
          <View style={styles.doseSectionError}>
            {assets ? <Image source={assets[0]} style={styles.image} /> : null}
            <View>
              <VText
                textVariant="LabelMedicineBold"
                textAlign="left"
                style={{ color: "#252F3B", fontSize: 16, fontWeight: "600" }}
              >
                No connection
              </VText>
              <VText
                textVariant="LabelMedicine"
                textAlign="left"
                style={{ color: "#565F6B", fontSize: 16, fontWeight: "400" }}
              >
                Please check connection.
              </VText>
            </View>
          </View>
        </>
      ) : null}
      {medicationState === 6 ? (
        <>
          <View style={styles.medSectionError}>
            <VText textVariant="LabelMedicine1Dark">MED</VText>
            <VText textVariant="LabelMedicine2Dark">
              {props.item.medicine}
            </VText>
          </View>
          <View style={styles.doseSectionError}>
            {assets ? <Image source={assets[1]} style={styles.image} /> : null}
            <View>
              <VText
                textVariant="LabelMedicineBold"
                textAlign="left"
                style={{ color: "#252F3B", fontSize: 16, fontWeight: "600" }}
              >
                Device error
              </VText>
              <VText
                textVariant="LabelMedicine"
                textAlign="left"
                style={{ color: "#565F6B", fontSize: 16, fontWeight: "400" }}
              >
                Contact support.
              </VText>
            </View>
          </View>
        </>
      ) : null}
    </Pressable>
  );
}

const styles = StyleSheet.create({
  deviceItem: {
    marginTop: 20,
    flexDirection: "row",
    height: 80,
    backgroundColor: "#FFF",
    width: "100%",
  },
  medSection: {
    borderTopLeftRadius: 12,
    borderBottomLeftRadius: 12,
    gap: 5,
    width: 65,
    backgroundColor: validoseMedication1,
    justifyContent: "center",
    alignItems: "center",
  },
  medSectionError: {
    borderTopLeftRadius: 12,
    borderBottomLeftRadius: 12,
    gap: 5,
    width: 65,
    backgroundColor: validoseMedicationError,
    justifyContent: "center",
    alignItems: "center",
  },
  doseSection: {
    paddingLeft: 20,
    flexDirection: "row",
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
    height: 80,
    width: "80%",
    backgroundColor: validoseMedication,
    alignItems: "center",
  },
  doseSectionError: {
    gap: 15,
    paddingLeft: 20,
    flexDirection: "row",
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
    height: 80,
    width: "80%",
    backgroundColor: validoseMedication,
    justifyContent: "flex-start",
    alignItems: "center",
  },
  image: {
    height: 30,
    width: 30,
  },
});
