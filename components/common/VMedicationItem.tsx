import dayjs from "dayjs";
import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { StyleSheet, View } from "react-native";

import {
  validoseMedication,
  validoseMedication1,
  validoseMedicationError,
} from "@/constants/Colors";
import { Device } from "@/store/useDeviceStore";
import useDoseStore from "@/store/useDoseStore";
import { VDoseItem } from "./VDoseItem";
import { VDoseLine } from "./VDoseLine";
import { VText } from "./VText";

interface VMedicationItemProps {
  item: Device;
}

export function VMedicationItem(props: VMedicationItemProps) {
  const [assets] = useAssets([
    require("./../../assets/images/link-broken.png"),
    require("./../../assets/images/alert-diamond.png"),
  ]);

  const DEFAULT_COLOR = "#5D9BFF";
  const DEFAULT_STATUS: "Connected" | "Disconnected" = "Disconnected";

  const deviceColor = props.item.color ?? DEFAULT_COLOR;
  const deviceStatus = props.item.status ?? DEFAULT_STATUS;

  const stylesComputed = StyleSheet.create({
    medSection: {
      borderTopLeftRadius: 12,
      borderBottomLeftRadius: 12,
      width: 65,
      backgroundColor: deviceColor,
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

  function getDoseStateForTime(expectedTime: string, taken?: boolean): number {
    if (taken) return 2;

    const [hour, minute] = expectedTime.split(":").map(Number);
    const doseTime = dayjs()
      .startOf("day")
      .add(hour, "hour")
      .add(minute, "minute");
    const now = dayjs();

    if (now.isAfter(doseTime.add(30, "minute"))) {
      return 1; // missed or later
    }
    return 0; // upcoming
  }

  function renderDoseProgress() {
    const doses = useDoseStore.getState().getDosesForToday(props.item.deviceId);

    console.log("\n");
    console.log("Doses for today:", doses);

    const dosesForToday = doses.map((dose) =>
      getDoseStateForTime(dose.expectedTime, dose.taken)
    );

    return (
      <>
        <View style={stylesComputed.medSection}>
          <VText textVariant="LabelMedicine1">MED</VText>
          <VText textVariant="LabelMedicine2">{props.item.medicine}</VText>
        </View>
        <View style={styles.doseSection}>
          {dosesForToday.map((state, index) => (
            <View style={{ flexDirection: "row" }} key={index}>
              <VDoseItem
                color={deviceColor}
                doseNumber={index + 1}
                state={state}
              />
              {index < dosesForToday.length - 1 && (
                <VDoseLine color={deviceColor} state={state === 2 ? 0 : 1} />
              )}
            </View>
          ))}
        </View>
      </>
    );
  }

  function renderMedicationState() {
    // Handle error states based on fallback logic (optional)
    const isDisconnected = deviceStatus === "Disconnected";
    const isDeviceError = props.item.medicineState === 6;

    if (isDisconnected || isDeviceError) {
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

    // Dose Progress View
    return renderDoseProgress();
  }

  return (
    <View style={{ flexDirection: "row", marginTop: 20 }}>
      {renderMedicationState()}
    </View>
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
    flexDirection: "row",
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
    height: 80,
    width: "80%",
    backgroundColor: validoseMedication,
    alignItems: "center",
    paddingLeft: 20,
  },
  doseSectionError: {
    gap: 15,
    flexDirection: "row",
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
    height: 80,
    width: "80%",
    backgroundColor: validoseMedication,
    justifyContent: "flex-start",
    alignItems: "center",
    paddingLeft: 20,
  },
  image: {
    height: 30,
    width: 30,
  },
});
