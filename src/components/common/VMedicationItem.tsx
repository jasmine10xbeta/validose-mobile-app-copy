import { useAssets } from "expo-asset";
import { Image } from "expo-image";
import { useMemo, useRef, useEffect } from "react";
import { StyleSheet, View, ScrollView, TouchableOpacity } from "react-native";

import useNetworkStore from "@/store/network";
import useTreatmentStore from "@/store/treatment";
import { ValidoseDevice } from "@/types/device";
import { Schedule } from "@/types/schedule";
import { connectAndSetupDevice } from "@/utils/ble";
import { VDoseItem } from "./VDoseItem";
import { VDoseLine } from "./VDoseLine";
import { VText } from "./VText";
import { showToast } from "./VToast";

interface VMedicationItemProps {
  item: ValidoseDevice;
  schedule: Schedule[];
}

type StatusType = "network" | "connection" | "error" | "battery";

const pastelColorPairs = [
  // { primary: "#5D9BFF", secondary: "#5D9BFF33", error: "#5D9BFF44" },
  // { primary: "#345DA0", secondary: "#345DA01A", error: "#345DA044" },
  // { primary: "#8044B8", secondary: "#8043B833", error: "#8043B844" },
  { primary: "#102651", secondary: "#1026511A", error: "#10265144" },
];

export function VMedicationItem({ item, schedule }: VMedicationItemProps) {
  const deviceId = (item as any).deviceId ?? (item as any).device_id ?? item.deviceId;
  const treatment = useTreatmentStore((state) => state.getDeviceTreatment(deviceId));
  const isNetworkConnected = useNetworkStore((s) => s.isConnected);
  const dockBatteryLevel =
    typeof item.dockBatteryLevel === "number" && item.dockBatteryLevel >= 0
      ? item.dockBatteryLevel
      : null;
  const ringBatteryLevel =
    typeof item.ringBatteryLevel === "number" && item.ringBatteryLevel >= 0
      ? item.ringBatteryLevel
      : null;
  const showBatteryBanner = dockBatteryLevel !== null || ringBatteryLevel !== null;
  const showErrorBanner = typeof item.error === "string" && item.error.trim().length > 0;

  // Prefer the schedule's medication code, fall back to the treatment's code.
  const medLabel = useMemo(() => {
    const scheduleCode = schedule.find((dose) =>
      typeof dose?.medication_code === "string" && dose.medication_code.trim().length > 0
    )?.medication_code;

    const code = scheduleCode ?? treatment?.medication_code;
    const first = code?.trim()?.[0];

    return first ? first.toUpperCase() : "M";
  }, [schedule, treatment?.medication_code]);

  const statusFlags = {
    connected: item.connected === true,
    batteryLow: item.batteryLevel !== undefined && item.batteryLevel < 20,
    error: item.error,
  };

  // determine status precedence
  const getStatusType = (): StatusType | null => {
    if (!statusFlags.connected) return "connection";
    if (!isNetworkConnected) return "network";
    // if (statusFlags.error) return "error";
    // if (statusFlags.batteryLow) return "battery";
    return null;
  };

  const statusType = getStatusType();

  const statusConfig: Record<
    StatusType,
    { title: string; message: string; imageIndex: number }
  > = {
    network: {
      title: "No internet",
      message: "Please check connection.",
      imageIndex: 1,
    },
    connection: {
      title: "Device error",
      message: "Click to retry or contact support.",
      imageIndex: 0,
    },
    battery: {
      title: "Battery low",
      message: "Please charge your device.",
      imageIndex: 2,
    },
    error: { title: "Device error", message: item.error, imageIndex: 0 },
  };

  const [assets] = useAssets([
    require("./../../assets/images/png/link-broken.png"),
    require("./../../assets/images/png/alert-diamond-red.png"),
    require("./../../assets/images/png/alert-battery.png"),
  ]);

  const randomColors = useMemo(
    () => pastelColorPairs[Math.floor(Math.random() * pastelColorPairs.length)],
    []
  );

  const scrollRef = useRef<ScrollView>(null);
  const hasAutoScrolledRef = useRef(false);

  useEffect(() => {
    if (hasAutoScrolledRef.current) return;

    const idx = schedule.findIndex((s) => s === 0);
    const index = idx !== -1 ? idx : 0;

    setTimeout(() => {
      scrollRef.current?.scrollTo({ x: index * 60, animated: true });
      hasAutoScrolledRef.current = true;
    }, 300);
  }, [schedule]);

  const renderStatusBlock = (type: StatusType) => {
    const cfg = statusConfig[type];
    const image = assets?.[cfg.imageIndex];
    return (
      <>
        <View
          style={[
            styles.medSectionError,
            { backgroundColor: randomColors.error },
          ]}
        >
          <VText textVariant="LabelMedicine1Dark">MED</VText>
          <VText textVariant="LabelMedicine2Dark">{medLabel}</VText>
          {/* <VText textVariant="LabelMedicine2Dark">{medicine.charAt(0)}</VText> */}
        </View>
        <TouchableOpacity
          onPress={async () => {
            if (type === "connection") {
              const connected = await connectAndSetupDevice(deviceId || item.deviceName);
              if (connected.error) showToast("error", connected.error.toString());
            }
          }}
          activeOpacity={0.7}
          style={[
            styles.doseSectionError,
            { backgroundColor: randomColors.secondary },
          ]}
        >
          {/* <View
            style={[
              styles.doseSectionError,
              { backgroundColor: randomColors.secondary },
            ]}
          > */}
          {image && <Image source={image} style={styles.image} />}
          <View style={{ width: "100%" }}>
            <VText textVariant="LabelMedicineBold">{cfg.title}</VText>
            <VText textVariant="LabelMedicine">{cfg.message}</VText>
          </View>
          {/* </View> */}
        </TouchableOpacity>
      </>
    );
  };

  const renderDoseProgress = () => (
    <>
      <View
        style={[styles.medSection, { backgroundColor: randomColors.primary }]}
      >
        <VText textVariant="LabelMedicine1">MED</VText>
        <VText textVariant="LabelMedicine2">{medLabel}</VText>
        {/* <VText textVariant="LabelMedicine2">{medicine.charAt(0)}</VText> */}
      </View>
      <View
        style={[
          styles.doseSection,
          { backgroundColor: randomColors.secondary, paddingHorizontal: 18 },
        ]}
      >
        {schedule.length > 0 ? (
          <ScrollView
            ref={scrollRef}
            horizontal
            showsHorizontalScrollIndicator={false}
          >
            {schedule.map((dose: Schedule, i: any) => {
              const state = getDoseState(dose);
              return (
                <View key={i} style={{ flexDirection: "row" }}>
                  {i > 0 && (
                    <VDoseLine
                      color={randomColors.primary}
                      state={state === 0 ? 0 : 1}
                    />
                  )}
                  <VDoseItem
                    color={randomColors.primary}
                    doseNumber={i + 1}
                    state={state}
                  />
                </View>
              );
            })}
          </ScrollView>
        )
        : (
          <View style={styles.noDose}>
            <VText textVariant="LabelDose">No upcoming dose</VText>
          </View>
        )}
      </View>
    </>
  );

  return (
    <View style={styles.container}>
      <View style={styles.deviceRow}>{statusType ? renderStatusBlock(statusType) : renderDoseProgress()}</View>
      {showBatteryBanner ? (
        <View style={styles.batteryBanner}>
          <VText textVariant="DeviceItemState" style={styles.batteryBannerText}>
            {`Dock battery: ${dockBatteryLevel ?? "--"}%   Ring battery: ${ringBatteryLevel ?? "--"}%`}
          </VText>
        </View>
      ) : null}
      {showErrorBanner ? (
        <View style={styles.errorBanner}>
          <VText textVariant="DeviceItemState" style={styles.errorBannerText}>
            ERROR
          </VText>
        </View>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { width: "100%", marginTop: 20 },
  deviceRow: { flexDirection: "row" },
  noDose: { marginLeft: 20 },
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
    gap: 8,
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
  batteryBanner: {
    borderRadius: 8,
    backgroundColor: "#FAF2E8",
    paddingHorizontal: 12,
    paddingVertical: 7,
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 8,
  },
  batteryBannerText: {
    color: "#7A4A00",
    fontWeight: "500",
  },
  errorBanner: {
    marginTop: 4,
    borderRadius: 8,
    backgroundColor: "#FFEDED",
    paddingHorizontal: 12,
    paddingVertical: 7,
    alignItems: "center",
    justifyContent: "center",
  },
  errorBannerText: {
    color: "#A60000",
    fontWeight: "600",
  },
});

export function getDoseState(dose: Schedule): number {
  const now = new Date();
  const start = new Date(dose.window_starts_at_local);
  const end = new Date(dose.window_ends_at_local);

  if (dose.firmware_acknowledged) return 2;
  if (now < start) return 0;
  if (now >= start && now < end) return 1;
  if (now >= end) return 4;

  return 5;
}
