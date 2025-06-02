import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { useState } from "react";
import { Pressable, StyleSheet, View } from "react-native";

import { showToast, ToastType } from "@/components/common/Toast";
import {
  validoseMedication,
  validoseMedication1,
  validoseMedicationError,
} from "@/constants/Colors";
import { Device } from "@/store/useDeviceStore";
import { VDoseItem } from "./VDoseItem";
import { VDoseLine } from "./VDoseLine";
import { VText } from "./VText";

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

  const [medicationState, setMedicationState] = useState<number>(0);

  const stylesComputed = StyleSheet.create({
    medSection: {
      borderTopLeftRadius: 12,
      borderBottomLeftRadius: 12,
      width: 65,
      backgroundColor: props.color,
      justifyContent: "center",
    },
  });

  const messageMap = [
    {
      type: "success",
      title: "Successful dose",
      message: "Congratulations you made the first dose.",
    },
    {
      type: "success",
      title: "Successful dose",
      message: "Congratulations you made the second dose.",
    },
    {
      type: "success",
      title: "Successful dose",
      message: "Congratulations you made the third dose.",
    },
    {
      type: "error",
      title: "Missed dose",
      message: "Please take your dose on time.",
    },
    {
      type: "error",
      title: "No connection",
      message: "Please check your connection.",
    },
    { type: "error", title: "Device error", message: "Contact support." },
  ];

  function onPressMedicationItem() {
    if (medicationState > 5) {
      setMedicationState(0);
      return;
    }

    const currentMessage = messageMap[medicationState];
    showToast(
      currentMessage.type as ToastType,
      currentMessage.title,
      currentMessage.message
    );
    setMedicationState((prev) => prev + 1);
  }

  function renderMedicationState() {
    // Error States: 5 - No Connection, 6 - Device Error
    if (medicationState >= 5) {
      const isDeviceError = medicationState === 6;
      const imageSource = assets
        ? isDeviceError
          ? assets[1]
          : assets[0]
        : null;
      const title = isDeviceError ? "Device error" : "No connection";
      const message = isDeviceError
        ? "Contact support."
        : "Please check connection.";

      return (
        <>
          <View style={styles.medSectionError}>
            <VText textVariant="LabelMedicine1Dark">MED</VText>
            <VText textVariant="LabelMedicine2Dark">
              {props.item.medicine}
            </VText>
          </View>
          <View style={styles.doseSectionError}>
            {imageSource && <Image source={imageSource} style={styles.image} />}
            <View>
              <VText
                textVariant="LabelMedicineBold"
                textAlign="left"
                style={{ color: "#252F3B", fontSize: 16, fontWeight: "600" }}
              >
                {title}
              </VText>
              <VText
                textVariant="LabelMedicine"
                textAlign="left"
                style={{ color: "#565F6B", fontSize: 16, fontWeight: "400" }}
              >
                {message}
              </VText>
            </View>
          </View>
        </>
      );
    }

    // Dose progress states 0-4
    const doseStates: Record<number, [number, number, number]> = {
      0: [0, 1, 1],
      1: [2, 0, 1],
      2: [2, 2, 0],
      3: [2, 2, 2],
      4: [2, 2, 0], // TODO: Dose #4 used here for visual, to be adjusted
    };

    const currentDoseStates =
      doseStates[medicationState as keyof typeof doseStates];

    return (
      <>
        <View style={stylesComputed.medSection}>
          <VText textVariant="LabelMedicine1">MED</VText>
          <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
        </View>
        <View style={styles.doseSection}>
          <VDoseItem
            color={props.color}
            doseNumber={1}
            state={currentDoseStates[0]}
          />
          <VDoseLine color={props.color} state={0} />
          <VDoseItem
            color={props.color}
            doseNumber={medicationState === 4 ? 4 : 2}
            state={currentDoseStates[1]}
          />
          <VDoseLine color={props.color} state={0} />
          <VDoseItem
            color={props.color}
            doseNumber={3}
            state={currentDoseStates[2]}
          />
        </View>
      </>
    );
  }

  return (
    <Pressable onPress={onPressMedicationItem} style={styles.deviceItem}>
      {renderMedicationState()}
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
