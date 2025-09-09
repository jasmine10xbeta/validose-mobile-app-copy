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
  { primary: "#102651", secondary: "#1026511A", error: "#10265144" },
];

export function VMedicationItem({ item, schedule }: VMedicationItemProps) {
  const { treatments } = useTreatmentStore();
  const getDeviceTreatmentId = useTreatmentStore((s) => s.getDeviceTreatmentId);
  const getTreatmentById = useTreatmentStore((s) => s.getTreatmentById);

  const isNetworkConnected = useNetworkStore((s) => s.isConnected);

  const deviceId = (item as any).deviceId ?? (item as any).device_id ?? item.deviceId;

  // Find the treatment for this device, then compute the first letter of its medication code.
  const medLabel = useMemo(() => {
    try {
      const treatmentId = getDeviceTreatmentId?.(deviceId);
      const treatment   = treatmentId ? getTreatmentById?.(treatmentId) : undefined;
      const code        = (treatment as any)?.medication_code as string | undefined;

      const first = code?.trim()?.[0];
      return first ? first.toUpperCase() : "M"; // fallback if missing
    } catch {
      return "M";
    }
  }, [deviceId, getDeviceTreatmentId, getTreatmentById, treatments]);

  const statusFlags = {
    connected: item.connected === true,
    batteryLow: item.batteryLevel !== undefined && item.batteryLevel < 20,
    error: item.error,
  };

  // determine status precedence
  const getStatusType = (): StatusType | null => {
    if (!statusFlags.connected) return "connection";
    if (!isNetworkConnected) return "network";
    if (statusFlags.error) return "error";
    if (statusFlags.batteryLow) return "battery";
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
        </View>
        <TouchableOpacity
          onPress={async () => {
            if (type === "connection") {
              const connected = await connectAndSetupDevice(item.deviceName, treatments);
              if (connected.error) showToast("error", connected.error.toString());
            }
          }}
          activeOpacity={0.7}
          style={[
            styles.doseSectionError,
            { backgroundColor: randomColors.secondary },
          ]}
        >
          {image && <Image source={image} style={styles.image} />}
          <View style={{ width: "100%" }}>
            <VText textVariant="LabelMedicineBold">{cfg.title}</VText>
            <VText textVariant="LabelMedicine">{cfg.message}</VText>
          </View>
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
      {statusType ? renderStatusBlock(statusType) : renderDoseProgress()}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flexDirection: "row", marginTop: 20 },
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
});

export function getDoseState(dose: Schedule): number {
  const now = new Date();
  const start = new Date(dose.window_starts_at_local);
  const end = new Date(dose.window_ends_at_local);
  const event = new Date(dose.event_at_local);

  if (dose.firmware_acknowledged) return 3;
  if (now < start) return 0;
  if (now >= start && now < end) return 1;
  if (now >= end) return 2;

  return 0;
}
