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
  // Placeholder logic for grouping & labeling
  // You would replace this with proper grouping and status determination logic

  const allSchedules = Object.values(schedulesByDevice).flat();
  const now = new Date();

  if (allSchedules.length === 0) {
    return {
      mainLabel: "No Scheduled Dose",
      timeLabel: "There are no doses scheduled for the remainder of the day.",
      detailsLabel: "",
      state: 0,
    };
  }

  const next = allSchedules.find(s => new Date(s.event_at_local) > now);

  if (!next) {
    return {
      mainLabel: "No Upcoming Dose",
      timeLabel: "You're all done for today.",
      detailsLabel: "",
      state: 0,
    };
  }

  const eventTime = new Date(next.event_at_local);
  const isMissed = eventTime.getTime() + 15 * 60 * 1000 < now.getTime();
  const isAboutToMiss = eventTime.getTime() < now.getTime();

  const labels = {
    mainLabel: isMissed
      ? "Missed Dose"
      : isAboutToMiss
      ? "About to be Missed Dose"
      : "Upcoming Dose",
    timeLabel: eventTime.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
    // timeLabel: "from now",
    detailsLabel: next.medication_code,
    state: isMissed ? 2 : isAboutToMiss ? 1 : 0,
  };

  return labels;
}
