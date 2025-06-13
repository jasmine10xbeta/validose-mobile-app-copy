import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { useMemo, useRef, useEffect } from "react";
import { StyleSheet, View, ScrollView } from "react-native";
import { Device } from "@/store/useDeviceStore";
import useDoseScheduleStore from "@/store/useDoseScheduleStore";
import useNetworkStore from "@/store/useNetworkStore";
import useTreatmentProtocolStore from "@/store/useTreatmentProtocolStore";
import { getDoseState } from "@/utils/dose/doseHelper";
import { VDoseItem } from "./VDoseItem";
import { VDoseLine } from "./VDoseLine";
import { VText } from "./VText";

interface VMedicationItemProps {
  item: Device;
}

const pastelColorPairs = [
  { primary: "#5D9BFF", secondary: "#5D9BFF33", error: "#5D9BFF44" },
  { primary: "#345DA0", secondary: "#345DA01A", error: "#345DA044" },
  { primary: "#8044B8", secondary: "#8043B833", error: "#8043B844" },
  { primary: "#102651", secondary: "#1026511A", error: "#10265144" },
];

export function VMedicationItem(props: VMedicationItemProps) {
  const [assets] = useAssets([
    require("./../../assets/images/link-broken.png"),
    require("./../../assets/images/alert-diamond-red.png"),
  ]);

  const randomColors = useMemo(() => {
    const index = Math.floor(Math.random() * pastelColorPairs.length);
    return pastelColorPairs[index];
  }, []);

  const { getDoses } = useDoseScheduleStore();
  const { getProtocol } = useTreatmentProtocolStore();
  const isNetworkConnected = useNetworkStore((s) => s.isConnected);

  const DEFAULT_STATUS: "Connected" | "Disconnected" = "Disconnected";

  const devicePrimaryColor = props.item.color ?? randomColors.primary;
  const deviceSecondaryColor = props.item.color ?? randomColors.secondary;
  const deviceErrorColor = props.item.color ?? randomColors.error;

  const deviceStatus =
    props.item?.connected === true
      ? "Connected"
      : props.item?.connected === false
        ? "Disconnected"
        : DEFAULT_STATUS;

  const protocol = getProtocol(props.item.deviceId ?? "");
  const dosingWindowMin = protocol?.dosingWindowMin ?? 15; // fallback value
  const medicine = protocol?.medicine ?? "";

  const scrollRef = useRef<ScrollView>(null);
  const doses = getDoses(props.item.deviceId);

  const dosesForToday = doses.map((dose) =>
    getDoseState(props.item.deviceId, dose.expectedTime, dosingWindowMin)
  );

  // Find index to scroll to
  const scrollToIndex = (() => {
    const nextIndex = dosesForToday.findIndex((s) => s === 0); // upcoming
    if (nextIndex !== -1) return nextIndex;

    const lastTakenIndex = [...dosesForToday]
      .reverse()
      .findIndex((s) => s === 2); // taken
    return lastTakenIndex !== -1
      ? dosesForToday.length - 1 - lastTakenIndex
      : 0;
  })();

  useEffect(() => {
    setTimeout(() => {
      scrollRef.current?.scrollTo({
        x: scrollToIndex * 60, // approximate width of each dose block
        animated: true,
      });
    }, 300);
  }, []);

  function renderDoseProgress() {
    console.log("\n");
    console.log("Doses for today:", doses);
    console.log("Dose states:", dosesForToday);

    return (
      <>
        <View
          style={[styles.medSection, { backgroundColor: devicePrimaryColor }]}
        >
          <VText textVariant="LabelMedicine1">MED</VText>
          <VText textVariant="LabelMedicine2">
            {medicine.charAt(0).toUpperCase()}
          </VText>
        </View>
        <View
          style={[
            styles.doseSection,
            { backgroundColor: deviceSecondaryColor },
          ]}
        >
          {dosesForToday.length > 0 ? (
            <ScrollView
              ref={scrollRef}
              horizontal
              showsHorizontalScrollIndicator={false}
              contentContainerStyle={{ paddingHorizontal: 20 }}
            >
              {dosesForToday.map((state, index) => (
                <View style={{ flexDirection: "row" }} key={index}>
                  {index !== 0 && (
                    <VDoseLine
                      color={devicePrimaryColor}
                      state={state === 0 ? 0 : 1}
                    />
                  )}
                  <VDoseItem
                    color={devicePrimaryColor}
                    doseNumber={index + 1}
                    state={state}
                  />
                </View>
              ))}
            </ScrollView>
          ) : (
            <VText textVariant="LabelMedicineBold" marginLeft={20}>
              No doses for today
            </VText>
          )}
        </View>
      </>
    );
  }

  function renderMedicationState() {
    // Handle error states based on fallback logic (optional)
    const isDeviceConnected = deviceStatus === "Connected";

    if (!isDeviceConnected || !isNetworkConnected) {
      const imageSource = assets
        ? !isNetworkConnected
          ? assets[1]
          : assets[0]
        : null;
      const title = !isDeviceConnected ? "Device error" : "No connection";
      const message = !isDeviceConnected
        ? "Contact support."
        : "Please check connection.";

      return (
        <>
          <View
            style={[
              styles.medSectionError,
              { backgroundColor: deviceErrorColor },
            ]}
          >
            <VText textVariant="LabelMedicine1Dark">MED</VText>
            <VText textVariant="LabelMedicine2Dark">
              {medicine.charAt(0).toUpperCase()}
            </VText>
          </View>
          <View
            style={[
              styles.doseSectionError,
              { backgroundColor: deviceSecondaryColor },
            ]}
          >
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
  medSection: {
    borderTopLeftRadius: 12,
    borderBottomLeftRadius: 12,
    width: 65,
    justifyContent: "center",
    alignItems: "center",
  },
  medSectionError: {
    borderTopLeftRadius: 12,
    borderBottomLeftRadius: 12,
    gap: 5,
    width: 65,
    // backgroundColor: validoseMedicationError,
    justifyContent: "center",
    alignItems: "center",
  },
  doseSection: {
    flexDirection: "row",
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
    height: 80,
    width: "80%",
    // backgroundColor: validoseMedication,
    alignItems: "center",
    // paddingLeft: 20,
  },
  doseSectionError: {
    gap: 15,
    flexDirection: "row",
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
    height: 80,
    width: "80%",
    // backgroundColor: validoseMedication,
    justifyContent: "flex-start",
    alignItems: "center",
    paddingLeft: 20,
  },
  image: {
    height: 30,
    width: 30,
  },
});
