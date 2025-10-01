import { StyleSheet, View } from "react-native";

import { Schedule } from "@/types/schedule";
import { VMedicationInfo } from "./VMedicationInfo";
import { VText } from "./VText";

interface VNextDoseInfoProps {
  todaySchedulesByDevice: Record<string, Schedule[]>;
}

interface LabelInfo {
  mainLabel: string;
  timeLabel: string;
  detailsLabel: string;
  state: number;
}

export function VNextDoseInfo({ todaySchedulesByDevice }: VNextDoseInfoProps) {
  const { mainLabel, timeLabel, detailsLabel, state } = getDoseLabels(todaySchedulesByDevice);

  return (
    <View style={styles.nextDoseInfo}>
      <VText textVariant="LabelDose" style={styles.label}>
        Next Dose
      </VText>
      <View style={styles.nextDoseInfoSecond}>
        <VText textVariant="Body">{mainLabel}</VText>
        <VText textVariant="LabelDose">{timeLabel}</VText>
      </View>
      {detailsLabel && <VMedicationInfo detailsLabel={detailsLabel} state={state} />}
    </View>
  );
}

const styles = StyleSheet.create({
  nextDoseInfo: {
    flexDirection: "column",
    alignItems: "center",
    marginTop: 40,
    paddingTop: 35,
    paddingBottom: 15,
    borderColor: "#E6E7E8",
    borderRadius: 12,
    borderWidth: 1,
    width: "100%",
  },
  nextDoseInfoSecond: {
    alignItems: "center",
    backgroundColor: "#FFF",
    width: "100%",
    gap: 5,
  },
  label: {
    position: "absolute",
    top: -10,
    paddingHorizontal: 25,
    color: "#565F6B",
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
    backgroundColor: "#FFF",
  },
});

// This should be imported from utils/labels.ts or similar
// TODO: Move this to utils & fix labels logic
function getDoseLabels(schedulesByDevice: Record<string, Schedule[]>): LabelInfo {
  const UPCOMING_WINDOW_MS = 15 * 60 * 1000;
  const now = new Date();

  const upcoming = Object.entries(schedulesByDevice)
    .flatMap(([deviceName, list]) =>
      (list || []).map((schedule) => ({
        schedule,
        deviceName,
        eventTime: new Date(schedule.event_at_local),
      }))
    )
    .filter(({ eventTime }) => eventTime.getTime() >= now.getTime())
    .sort((a, b) => a.eventTime.getTime() - b.eventTime.getTime());

  if (upcoming.length === 0) {
    return {
      mainLabel: "No Upcoming Dose",
      timeLabel: "You're all done for today.",
      detailsLabel: "",
      state: 0,
    };
  }

  const groups: Array<{
    items: typeof upcoming;
  }> = [];

  upcoming.forEach((entry) => {
    const currentGroup = groups[groups.length - 1];

    if (!currentGroup) {
      groups.push({ items: [entry] });
      return;
    }

    const groupStartTime = currentGroup.items[0].eventTime.getTime();
    if (entry.eventTime.getTime() - groupStartTime <= UPCOMING_WINDOW_MS) {
      currentGroup.items.push(entry);
    } else {
      groups.push({ items: [entry] });
    }
  });

  const nextGroup = groups[0];
  const groupedItems = nextGroup.items;
  const earliest = groupedItems[0].eventTime;
  const latest = groupedItems[groupedItems.length - 1].eventTime;

  const formatTime = (date: Date) =>
    date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });

  const timeLabel =
    latest.getTime() - earliest.getTime() > 0
      ? `${formatTime(earliest)} – ${formatTime(latest)}`
      : formatTime(earliest);

  const deviceNames = Array.from(
    new Set(groupedItems.map((entry) => entry.deviceName))
  ).join(", ");

  const medicationCodes = Array.from(
    new Set(
      groupedItems
        .map((entry) => entry.schedule.medication_code)
        .filter((code): code is string => !!code && code.trim().length > 0)
    )
  ).join(", ");

  const isMissed = earliest.getTime() + UPCOMING_WINDOW_MS < now.getTime();
  const isInWindow =
    earliest.getTime() <= now.getTime() &&
    now.getTime() <= earliest.getTime() + UPCOMING_WINDOW_MS;
  const isAboutToMiss =
    earliest.getTime() > now.getTime() &&
    earliest.getTime() - now.getTime() <= UPCOMING_WINDOW_MS;

  const state = isMissed ? 2 : isInWindow || isAboutToMiss ? 1 : 0;

  const doseCount = groupedItems.length;
  const countSuffix = doseCount > 1 ? `s (${doseCount})` : "";

  const mainLabel = isMissed
    ? `Missed Dose${doseCount > 1 ? "s" : ""}`
    : isInWindow || isAboutToMiss
    ? `Due Soon${doseCount > 1 ? ` (${doseCount})` : ""}`
    : `Upcoming Dose${countSuffix}`;

  const details: string[] = [];
  // if (deviceNames) details.push(deviceNames);
  if (medicationCodes) details.push(medicationCodes);

  return {
    mainLabel,
    timeLabel,
    detailsLabel: `Take medication ${details.join(", ")}`,
    state,
  };
}
