import { Feather } from "@expo/vector-icons";
import { useRouter } from "expo-router";
import { useMemo, useRef, useEffect, useState } from "react";
import { StyleSheet, View, ScrollView, TouchableOpacity, Text } from "react-native";

import { shouldShowReplaceMedicineBanner } from "@/constants/dashboardHighlights";
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
type BannerType =
  | "replace-medicine"
  | "reconnect"
  | "charge-device"
  | "device-charging"
  | "take-missed-dose"
  | "wait-next-dose";

type BannerPresentation = "warning" | "danger" | "success" | "neutral";

const accentByMedicationLabel: Record<string, string> = {
  A: "#F29A2D",
  B: "#73BE95",
  C: "#72C4CE",
  M: "#D5DCE6",
};

const statusBannerConfig: Record<
  StatusType,
  { title: string; message: string; presentation: BannerPresentation }
> = {
  network: {
    title: "No internet",
    message: "Please check connection.",
    presentation: "danger",
  },
  connection: {
    title: "No connection",
    message: "Tap to reconnect",
    presentation: "danger",
  },
  battery: {
    title: "Battery low",
    message: "Please charge your device.",
    presentation: "warning",
  },
  error: {
    title: "Device error",
    message: "Please contact support.",
    presentation: "danger",
  },
};

export function VMedicationItem({ item, schedule }: VMedicationItemProps) {
  const router = useRouter();
  const deviceId = (item as any).deviceId ?? (item as any).device_id ?? item.deviceId;
  const treatment = useTreatmentStore((state) => state.getDeviceTreatment(deviceId));
  const isNetworkConnected = useNetworkStore((s) => s.isConnected);
  const [dismissedBanners, setDismissedBanners] = useState<BannerType[]>([]);

  const medLabel = useMemo(() => {
    const scheduleCode = schedule.find((dose) =>
      typeof dose?.medication_code === "string" && dose.medication_code.trim().length > 0
    )?.medication_code;

    const code = scheduleCode ?? treatment?.medication_code;
    const first = code?.trim()?.[0];
    return first ? first.toUpperCase() : "M";
  }, [schedule, treatment?.medication_code]);

  const accentColor = accentByMedicationLabel[medLabel] ?? accentByMedicationLabel.M;

  const statusFlags = {
    connected: item.connected === true,
    batteryLow: item.batteryLevel !== undefined && item.batteryLevel >= 0 && item.batteryLevel < 20,
    error: Boolean(item.error),
  };

  const getStatusType = (): StatusType | null => {
    if (!statusFlags.connected) return "connection";
    if (!isNetworkConnected) return "network";
    if (statusFlags.error) return "error";
    if (statusFlags.batteryLow) return "battery";
    return null;
  };

  const statusType = getStatusType();
  const showReplaceMedicineBanner = shouldShowReplaceMedicineBanner(medLabel);

  const scrollRef = useRef<ScrollView>(null);
  const hasAutoScrolledRef = useRef(false);

  useEffect(() => {
    if (hasAutoScrolledRef.current) return;

    const idx = schedule.findIndex((dose) => getDoseState(dose) === 0);
    const index = idx !== -1 ? idx : 0;

    setTimeout(() => {
      scrollRef.current?.scrollTo({ x: index * 60, animated: true });
      hasAutoScrolledRef.current = true;
    }, 260);
  }, [schedule]);

  const doseStates = useMemo(() => schedule.map((dose) => getDoseState(dose)), [schedule]);
  const hasMissedDose = doseStates.includes(4);
  const firstUpcomingDoseIndex = doseStates.findIndex((state) => state === 0);
  const hasActiveDose = doseStates.includes(1);

  const availableBanners = useMemo(() => {
    const banners: {
      type: BannerType;
      text: string;
      priority: number;
      presentation: BannerPresentation;
      chips?: string[];
      leadingIcon?: React.ComponentProps<typeof Feather>["name"];
    }[] = [];

    if (hasMissedDose) {
      banners.push({
        type: "take-missed-dose",
        text: "Take missed dose",
        priority: 500,
        presentation: "danger",
      });
    }

    if (statusFlags.batteryLow) {
      banners.push({
        type: "charge-device",
        text: "Charge device",
        priority: 420,
        presentation: "warning",
        leadingIcon: "battery",
        chips: [`Ring ${Math.max(10, Math.min(100, 10))}%`, `Dock ${Math.max(1, item.batteryLevel)}%`],
      });
    }

    if (statusType === "connection") {
      banners.push({
        type: "reconnect",
        text: "Tap to reconnect",
        priority: 400,
        presentation: "danger",
      });
    }

    if (showReplaceMedicineBanner) {
      banners.push({
        type: "replace-medicine",
        text: "Tap to replace medicine",
        priority: 350,
        presentation: "warning",
      });
    }

    if (medLabel === "B" && !hasMissedDose && !statusType && !statusFlags.batteryLow) {
      banners.push({
        type: "device-charging",
        text: "Device B charging",
        priority: 280,
        presentation: "success",
      });
    }

    if (!hasActiveDose && firstUpcomingDoseIndex !== -1 && !hasMissedDose) {
      banners.push({
        type: "wait-next-dose",
        text: `Wait for dose ${firstUpcomingDoseIndex + 1}`,
        priority: 120,
        presentation: "neutral",
      });
    }

    if (statusType && statusType !== "connection") {
      const cfg = statusBannerConfig[statusType];
      banners.push({
        type: statusType === "battery" ? "charge-device" : "reconnect",
        text: cfg.message,
        priority: 300,
        presentation: cfg.presentation,
      });
    }

    return banners
      .filter((banner) => !dismissedBanners.includes(banner.type))
      .sort((a, b) => b.priority - a.priority);
  }, [
    dismissedBanners,
    firstUpcomingDoseIndex,
    hasActiveDose,
    hasMissedDose,
    item.batteryLevel,
    medLabel,
    showReplaceMedicineBanner,
    statusFlags.batteryLow,
    statusType,
  ]);

  const activeBanner = availableBanners[0] ?? null;

  async function handleBannerPress() {
    if (!activeBanner) return;

    if (activeBanner.type === "replace-medicine") {
      router.push("/home/dashboard/replace-medication");
      return;
    }

    if (activeBanner.type === "reconnect") {
      const connected = await connectAndSetupDevice(item.deviceName);
      if (connected.error) {
        showToast("error", connected.error.toString());
        return;
      }
    }

    setDismissedBanners((prev) => [...prev, activeBanner.type]);
  }

  const renderDoseProgress = () => (
    <>
      <View style={styles.medSection}>
        <VText textVariant="LabelMedicine2Dark" style={styles.medLabel}>
          {medLabel}
        </VText>
      </View>
      <View style={[styles.accentStrip, { backgroundColor: accentColor }]} />
      <View style={styles.doseSection}>
        {schedule.length > 0 ? (
          <ScrollView
            ref={scrollRef}
            horizontal
            showsHorizontalScrollIndicator={false}
          >
            {schedule.map((dose: Schedule, i: number) => {
              const state = getDoseState(dose);
              return (
                <View key={i} style={{ flexDirection: "row" }}>
                  {i > 0 && (
                    <VDoseLine
                      color="#1F6C83"
                      state={state === 0 ? 0 : 1}
                    />
                  )}
                  <VDoseItem
                    color="#1F6C83"
                    doseNumber={i + 1}
                    state={state}
                  />
                </View>
              );
            })}
          </ScrollView>
        ) : (
          <View style={styles.noDose}>
            <VText textVariant="LabelDose">No upcoming dose</VText>
          </View>
        )}
      </View>
    </>
  );

  return (
    <View style={styles.container}>
      <View style={styles.cardRow}>{renderDoseProgress()}</View>
      {activeBanner ? (
        <TouchableOpacity
          onPress={handleBannerPress}
          activeOpacity={0.82}
          style={[
            styles.highlightBanner,
            activeBanner.presentation === "warning" && styles.bannerWarning,
            activeBanner.presentation === "danger" && styles.bannerDanger,
            activeBanner.presentation === "success" && styles.bannerSuccess,
            activeBanner.presentation === "neutral" && styles.bannerNeutral,
          ]}
        >
          {activeBanner.type === "charge-device" ? (
            <View style={styles.bannerRow}>
              <View style={styles.bannerLeft}>
                <Feather name={activeBanner.leadingIcon ?? "battery"} size={15} color="#A35E0D" />
                <Text style={styles.bannerWarningText}>Charge device</Text>
              </View>
              <View style={styles.bannerChips}>
                {(activeBanner.chips ?? []).map((chip) => (
                  <View key={chip} style={styles.bannerChip}>
                    <Text style={styles.bannerChipText}>{chip}</Text>
                  </View>
                ))}
              </View>
            </View>
          ) : (
            <Text
              style={[
                styles.bannerTextBase,
                activeBanner.presentation === "danger" && styles.bannerDangerText,
                activeBanner.presentation === "success" && styles.bannerSuccessText,
                activeBanner.presentation === "neutral" && styles.bannerNeutralText,
                activeBanner.presentation === "warning" && styles.bannerWarningText,
              ]}
            >
              {activeBanner.text}
            </Text>
          )}
        </TouchableOpacity>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { marginTop: 12 },
  cardRow: {
    flexDirection: "row",
    borderRadius: 14,
    borderWidth: 1,
    borderColor: "#DFE3E8",
    backgroundColor: "#F8F9FB",
    overflow: "hidden",
    shadowColor: "#17202A",
    shadowOpacity: 0.07,
    shadowRadius: 10,
    shadowOffset: { width: 0, height: 3 },
    elevation: 2,
  },
  noDose: { marginLeft: 20 },
  medSection: {
    width: 62,
    backgroundColor: "#F4F5F7",
    justifyContent: "center",
    alignItems: "center",
  },
  medLabel: {
    width: "auto",
    fontSize: 24,
    textAlign: "center",
    color: "#2B3645",
    fontWeight: "700",
  },
  accentStrip: {
    width: 10,
    borderTopRightRadius: 10,
    borderBottomRightRadius: 10,
    marginVertical: 6,
  },
  doseSection: {
    flexDirection: "row",
    height: 84,
    flex: 1,
    backgroundColor: "#F8F9FB",
    alignItems: "center",
    paddingHorizontal: 12,
  },
  highlightBanner: {
    marginTop: 2,
    minHeight: 40,
    borderRadius: 7,
    alignItems: "center",
    justifyContent: "center",
    paddingHorizontal: 10,
    paddingVertical: 8,
  },
  bannerWarning: {
    backgroundColor: "#F6EBDD",
  },
  bannerDanger: {
    backgroundColor: "#F9E6E8",
  },
  bannerSuccess: {
    backgroundColor: "#D7EADF",
  },
  bannerNeutral: {
    backgroundColor: "#EDEFF3",
  },
  bannerTextBase: {
    fontSize: 30 / 2,
    fontWeight: "500",
    textAlign: "center",
  },
  bannerWarningText: {
    color: "#A35E0D",
  },
  bannerDangerText: {
    color: "#B91C1C",
  },
  bannerSuccessText: {
    color: "#0F7A4D",
  },
  bannerNeutralText: {
    color: "#5A6574",
  },
  bannerRow: {
    width: "100%",
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    gap: 10,
  },
  bannerLeft: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  bannerChips: {
    flexDirection: "row",
    gap: 4,
  },
  bannerChip: {
    backgroundColor: "#EDC58C",
    borderRadius: 6,
    paddingHorizontal: 6,
    paddingVertical: 3,
  },
  bannerChipText: {
    color: "#6A3B00",
    fontSize: 14,
    fontWeight: "500",
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
