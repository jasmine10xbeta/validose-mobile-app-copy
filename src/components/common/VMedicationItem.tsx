import { useRouter } from "expo-router";
import { useMemo, useRef, useEffect } from "react";
import { Pressable, ScrollView, StyleSheet, TouchableOpacity, View } from "react-native";

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
  index?: number;
}

const DOSE_PROGRESS_COLOR = "#255F6C";
const DOSE_ERROR_COLOR = "#A60000";
const LOW_BATTERY_THRESHOLD_PERCENT = 30;

function getMedicationInitial(medicationCode: string): string {
  const normalized = medicationCode.trim().toUpperCase();
  const match = normalized.match(/[A-Z0-9]/);
  return match ? match[0] : "M";
}

function shouldShowReplaceMedicationMock(): boolean {
  return true;
}

export function VMedicationItem({ item, schedule }: VMedicationItemProps) {
  const router = useRouter();
  const deviceId = (item as any).deviceId ?? (item as any).device_id ?? item.deviceId;
  const treatment = useTreatmentStore((state) => state.getDeviceTreatment(deviceId));
  const isNetworkConnected = useNetworkStore((s) => s.isConnected);

  const showReplaceMedicationTrigger = shouldShowReplaceMedicationMock();
  const showReplaceMedicationBanner = true;
  const hasErrorState = typeof item.error === "string" && item.error.trim().length > 0;
  const showErrorBanner = false && hasErrorState;
  const noConnection = item.connected !== true || !isNetworkConnected;
  const isDoseDueNow = useMemo(
    () => schedule.some((dose) => getDoseState(dose) === 1),
    [schedule]
  );
  const isReplaceMedicationState =
    showReplaceMedicationBanner &&
    showReplaceMedicationTrigger &&
    !noConnection &&
    !hasErrorState;

  const pipeColor = noConnection || hasErrorState
    ? "#F15050"
    : isDoseDueNow
      ? "#73D0D7"
      : isReplaceMedicationState
        ? "#F09525"
        : "#FAFBFC";

  const dockBatteryLevel =
    typeof item.dockBatteryLevel === "number" && item.dockBatteryLevel >= 0
      ? item.dockBatteryLevel
      : null;
  const ringBatteryLevel =
    typeof item.ringBatteryLevel === "number" && item.ringBatteryLevel >= 0
      ? item.ringBatteryLevel
      : null;
  const legacyBatteryLevel =
    typeof item.batteryLevel === "number" && item.batteryLevel >= 0
      ? item.batteryLevel
      : null;
  const lowBatteryLevels = useMemo(
    () => {
      const explicitLevels = [
        { key: "dock", label: "Dock", level: dockBatteryLevel },
        { key: "ring", label: "Ring", level: ringBatteryLevel },
      ].filter(
        (entry): entry is { key: string; label: string; level: number } =>
          typeof entry.level === "number" &&
          entry.level <= LOW_BATTERY_THRESHOLD_PERCENT
      );

      if (explicitLevels.length > 0) {
        return explicitLevels;
      }

      if (
        typeof legacyBatteryLevel === "number" &&
        legacyBatteryLevel <= LOW_BATTERY_THRESHOLD_PERCENT
      ) {
        return [{ key: "device", label: "Device", level: legacyBatteryLevel }];
      }

      return [];
    },
    [dockBatteryLevel, ringBatteryLevel, legacyBatteryLevel]
  );
  const showBatteryBanner = lowBatteryLevels.length > 0;

  const medicationCode = useMemo(() => {
    const scheduleCode = schedule.find((dose) =>
      typeof dose?.medication_code === "string" && dose.medication_code.trim().length > 0
    )?.medication_code;

    const code = scheduleCode ?? treatment?.medication_code;
    return code?.trim()?.toUpperCase() ?? "MED";
  }, [schedule, treatment?.medication_code]);
  const initialLabel = useMemo(
    () => getMedicationInitial(medicationCode),
    [medicationCode]
  );

  const scrollRef = useRef<ScrollView>(null);
  const hasAutoScrolledRef = useRef(false);

  useEffect(() => {
    if (hasAutoScrolledRef.current) return;

    const idx = schedule.findIndex((s) => getDoseState(s) === 0);
    const index = idx !== -1 ? idx : 0;

    setTimeout(() => {
      scrollRef.current?.scrollTo({ x: index * 60, animated: true });
      hasAutoScrolledRef.current = true;
    }, 300);
  }, [schedule]);

  async function onReconnectPress() {
    if (!noConnection) return;
    const connected = await connectAndSetupDevice(deviceId || item.deviceName);
    if (connected.error) {
      showToast("error", connected.error.toString());
    }
  }

  return (
    <View style={styles.container}>
      <Pressable
        onPress={noConnection ? onReconnectPress : undefined}
        disabled={!noConnection}
        style={styles.deviceRow}
      >
        <View style={styles.deviceInitialPane}>
          <VText textVariant="DeviceItem" style={styles.deviceInitialText}>
            {initialLabel}
          </VText>
        </View>

        <View style={styles.cardAccentTrack}>
          {Array.from({ length: 1 }).map((_, segmentIndex, all) => {
            const isFirst = segmentIndex === 0;
            const isLast = segmentIndex === all.length - 1;

            return (
              <View
                key={`med-pipe-${segmentIndex}`}
                style={[
                  styles.cardAccentSegment,
                  { backgroundColor: pipeColor },
                  isFirst ? styles.cardAccentSegmentFirst : null,
                  isLast ? styles.cardAccentSegmentLast : null,
                ]}
              />
            );
          })}
        </View>

        <View style={styles.doseSection}>
          {/* <VText textVariant="DeviceItem" style={styles.medicationTitleText}>
            {`Medication ${medicationCode}`}
          </VText> */}
          {schedule.length > 0 ? (
            <ScrollView
              ref={scrollRef}
              horizontal
              showsHorizontalScrollIndicator={false}
              contentContainerStyle={styles.timelineContent}
            >
              {schedule.map((dose: Schedule, i: number) => {
                const state = getDoseState(dose);
                const previousState = i > 0 ? getDoseState(schedule[i - 1]) : null;
                const isErrorState = state === 4 || state === 5;
                const hasErrorConnection =
                  isErrorState || previousState === 4 || previousState === 5;
                const timelineColor = hasErrorConnection
                  ? DOSE_ERROR_COLOR
                  : DOSE_PROGRESS_COLOR;
                return (
                  <View key={`${dose.window_starts_at_local}-${i}`} style={styles.timelineItem}>
                    {i > 0 ? (
                      <VDoseLine
                        color={timelineColor}
                        state={state === 0 ? 0 : 1}
                      />
                    ) : null}
                    <VDoseItem
                      color={isErrorState ? DOSE_ERROR_COLOR : DOSE_PROGRESS_COLOR}
                      doseNumber={i + 1}
                      state={state}
                    />
                  </View>
                );
              })}
            </ScrollView>
          ) : (
            <VText textVariant="LabelDose">No upcoming dose</VText>
          )}
        </View>
      </Pressable>

      {showReplaceMedicationBanner && showReplaceMedicationTrigger ? (
        <TouchableOpacity
          activeOpacity={0.75}
          onPress={() => router.push("/home/dashboard/replace-medication")}
          style={styles.replaceMedicationBanner}
        >
          <VText textVariant="DeviceItemState" style={styles.replaceMedicationText}>
            Tap to replace medication
          </VText>
        </TouchableOpacity>
      ) : null}
      {showErrorBanner ? (
        <View style={styles.errorBanner}>
          <VText textVariant="DeviceItemState" style={styles.errorBannerText}>
            ERROR
          </VText>
        </View>
      ) : null}
      {showBatteryBanner ? (
        <View style={styles.batteryBanner}>
          <VText textVariant="DeviceItemState" style={styles.batteryBannerLead}>
            Charge device
          </VText>
          <View style={styles.batteryBadgeGroup}>
            {lowBatteryLevels.map((entry) => (
              <View key={entry.key} style={styles.batteryBadgePair}>
                <View style={styles.batteryValuePill}>
                  <VText textVariant="DeviceItemState" style={styles.batteryValueText}>
                    {`${entry.level}%`}
                  </VText>
                </View>
                <View style={styles.batteryLabelPill}>
                  <VText textVariant="DeviceItemState" style={styles.batteryLabelText}>
                    {entry.label}
                  </VText>
                </View>
              </View>
            ))}
          </View>
        </View>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    width: "100%",
    marginTop: 24,
  },
  deviceRow: {
    flexDirection: "row",
    alignItems: "center",
    minHeight: 72,
    borderRadius: 14,
    backgroundColor: "#FFFFFF",
    borderWidth: 2,
    borderColor: "#FAFBFC",
    paddingHorizontal: 4,
    paddingVertical: 4,
    shadowColor: "#000000",
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 1,
  },
  deviceInitialPane: {
    backgroundColor: "#FAFBFC",
    width: 57,
    alignSelf: "stretch",
    borderTopLeftRadius: 8,
    borderBottomLeftRadius: 8,
    justifyContent: "center",
    alignItems: "center",
    marginRight: 3,
  },
  deviceInitialText: {
    color: "#252F3B",
    fontWeight: "700",
    fontSize: 28,
    textAlign: "center",
  },
  cardAccentTrack: {
    width: 8,
    alignSelf: "stretch",
    justifyContent: "space-between",
    paddingVertical: 1,
    marginRight: 12,
  },
  cardAccentSegment: {
    flex: 1,
  },
  cardAccentSegmentFirst: {
    borderTopRightRadius: 21,
  },
  cardAccentSegmentLast: {
    borderBottomRightRadius: 21,
  },
  cardAccentSegmentGap: {
    marginBottom: 2,
  },
  doseSection: {
    flex: 1,
    alignSelf: "stretch",
    justifyContent: "center",
  },
  medicationTitleText: {
    color: "#505A66",
    fontSize: 16,
    fontWeight: "500",
    width: "100%",
  },
  timelineContent: {
    paddingRight: 4,
    alignItems: "center",
  },
  timelineItem: {
    flexDirection: "row",
    alignItems: "center",
  },
  replaceMedicationBanner: {
    marginHorizontal: 8,
    backgroundColor: "#FAF2E8",
    paddingHorizontal: 12,
    paddingVertical: 10,
    alignItems: "center",
    justifyContent: "center",
    borderBottomEndRadius: 8,
    borderBottomStartRadius: 8,
  },
  replaceMedicationText: {
    color: "#7A4400",
    fontWeight: "500",
  },
  errorBanner: {
    backgroundColor: "#FFEDED",
    paddingHorizontal: 12,
    paddingVertical: 10,
    alignItems: "center",
    justifyContent: "center",
    borderBottomEndRadius: 8,
    borderBottomStartRadius: 8,
  },
  errorBannerText: {
    color: "#A60000",
    fontWeight: "600",
  },
  batteryBanner: {
    marginHorizontal: 8,
    backgroundColor: "#FAF2E8",
    paddingHorizontal: 10,
    paddingVertical: 10,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    gap: 8,
    borderBottomEndRadius: 8,
    borderBottomStartRadius: 8,
  },
  batteryBannerLead: {
    color: "#7A4A00",
    fontWeight: "500",
    fontSize: 15,
    width: "auto",
  },
  batteryBadgeGroup: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  batteryBadgePair: {
    flexDirection: "row",
    alignItems: "center",
  },
  batteryLabelPill: {
    backgroundColor: "#F095254D",
    borderTopLeftRadius: 6,
    borderBottomLeftRadius: 6,
    paddingHorizontal: 6,
    paddingVertical: 4,
  },
  batteryLabelText: {
    color: "#7A4400",
    fontSize: 15,
  },
  batteryValuePill: {
    backgroundColor: "#F09525",
    borderTopRightRadius: 6,
    borderBottomRightRadius: 6,
    paddingHorizontal: 6,
    paddingVertical: 4,
  },
  batteryValueText: {
    color: "#8C4F00",
    width: "auto",
    fontSize: 14,
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
