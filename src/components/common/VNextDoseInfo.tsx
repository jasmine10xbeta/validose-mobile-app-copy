import { useEffect, useState } from "react";
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

type UpcomingEntry = {
  schedule: Schedule;
  deviceName: string;
  eventTime: Date;
  windowStart: Date;
  windowEnd: Date;
};

export function VNextDoseInfo({ todaySchedulesByDevice }: VNextDoseInfoProps) {
  const [timestamp, setTimestamp] = useState(() => Date.now());

  useEffect(() => {
    const interval = setInterval(() => setTimestamp(Date.now()), 60_000);
    return () => clearInterval(interval);
  }, []);

  const { mainLabel, timeLabel, detailsLabel, state } = getDoseLabels(
    todaySchedulesByDevice,
    timestamp
  );

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
    marginTop: 24,
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

function parseDate(value?: string | null): Date | null {
  if (!value) return null;
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? null : date;
}

function resolveWindowBounds(schedule: Schedule, eventTime: Date, fallbackWindowMs: number) {
  const windowStart = parseDate(schedule.window_starts_at_local);
  const windowEnd = parseDate(schedule.window_ends_at_local);

  if (windowStart && windowEnd) {
    return { windowStart, windowEnd };
  }

  if (windowStart && !windowEnd) {
    return {
      windowStart,
      windowEnd: new Date(windowStart.getTime() + fallbackWindowMs),
    };
  }

  if (!windowStart && windowEnd) {
    return {
      windowStart: new Date(windowEnd.getTime() - fallbackWindowMs),
      windowEnd,
    };
  }

  const halfWindow = fallbackWindowMs / 2;
  return {
    windowStart: new Date(eventTime.getTime() - halfWindow),
    windowEnd: new Date(eventTime.getTime() + halfWindow),
  };
}

function formatDurationLabel(diffMs: number): string {
  const totalMinutes = Math.max(0, Math.round(diffMs / 60000));
  if (totalMinutes >= 60) {
    const hours = Math.floor(totalMinutes / 60);
    const mins = totalMinutes % 60;
    if (mins === 0) {
      return `In ${hours} ${hours === 1 ? "hour" : "hours"}`;
    }
    return `In ${hours} ${hours === 1 ? "hour" : "hours"} ${mins} mins`;
  }
  const minutes = Math.max(totalMinutes, 1);
  return `In ${minutes} ${minutes === 1 ? "min" : "mins"}`;
}

function formatMedicationList(codes: string[]): string {
  const sanitized = codes.filter((code) => code.length > 0);
  if (sanitized.length === 0) return "your medication";
  if (sanitized.length === 1) return sanitized[0];
  if (sanitized.length === 2) return `${sanitized[0]} and ${sanitized[1]}`;
  const head = sanitized.slice(0, -1).join(", ");
  const tail = sanitized[sanitized.length - 1];
  return `${head} and ${tail}`;
}

function getDoseLabels(
  schedulesByDevice: Record<string, Schedule[]>,
  timestamp: number
): LabelInfo {
  const DEFAULT_WINDOW_MS = 15 * 60 * 1000;
  const now = new Date(timestamp);
  const nowMs = now.getTime();

  const upcoming: UpcomingEntry[] = Object.entries(schedulesByDevice)
    .flatMap(([deviceName, list]) =>
      (list || []).map((schedule) => ({
        schedule,
        deviceName,
        eventTime: new Date(schedule.event_at_local),
      }))
    )
    .map(({ schedule, deviceName, eventTime }) => {
      const windowDurationMs =
        typeof schedule.dosing_window_min === "number" && schedule.dosing_window_min > 0
          ? schedule.dosing_window_min * 60 * 1000
          : DEFAULT_WINDOW_MS;
      const bounds = resolveWindowBounds(schedule, eventTime, windowDurationMs);

      return {
        schedule,
        deviceName,
        eventTime,
        windowStart: bounds.windowStart,
        windowEnd: bounds.windowEnd,
      };
    })
    .filter(({ windowEnd }) => windowEnd.getTime() >= now.getTime())
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

  // Group doses that fall within DEFAULT_WINDOW_MS of each other
  upcoming.forEach((entry) => {
    const currentGroup = groups[groups.length - 1];

    if (!currentGroup) {
      groups.push({ items: [entry] });
      return;
    }

    const groupStartTime = currentGroup.items[0].eventTime.getTime();
    if (entry.eventTime.getTime() - groupStartTime <= DEFAULT_WINDOW_MS) {
      currentGroup.items.push(entry);
    } else {
      groups.push({ items: [entry] });
    }
  });

  type GroupInfo = {
    items: UpcomingEntry[];
    earliestEvent: Date;
    windowStartMs: number;
    windowEndMs: number;
    windowDurationMs: number;
    pendingItems: UpcomingEntry[];
    allCodes: string[];
    pendingCodes: string[];
  };

  const groupsInfo: GroupInfo[] = groups.map(({ items }) => {
    const earliestEvent = items[0].eventTime;
    const windowStartMs = Math.min(
      ...items.map((entry) => entry.windowStart.getTime())
    );
    const windowEndMs = Math.max(
      ...items.map((entry) => entry.windowEnd.getTime())
    );
    const windowDurationMs = Math.max(
      windowEndMs - windowStartMs,
      DEFAULT_WINDOW_MS
    );

    const pendingItems = items.filter(
      (entry) => !entry.schedule.firmware_acknowledged
    );

    const allCodes = Array.from(
      new Set(
        items
          .map((entry) => entry.schedule.medication_code?.trim())
          .filter((code): code is string => !!code && code.length > 0)
      )
    );
    const pendingCodes = Array.from(
      new Set(
        pendingItems
          .map((entry) => entry.schedule.medication_code?.trim())
          .filter((code): code is string => !!code && code.length > 0)
      )
    );

    return {
      items,
      earliestEvent,
      windowStartMs,
      windowEndMs,
      windowDurationMs,
      pendingItems,
      allCodes,
      pendingCodes,
    };
  });

  if (groupsInfo.length === 0) {
    return {
      mainLabel: "No Upcoming Dose",
      timeLabel: "You're all done for today.",
      detailsLabel: "",
      state: 0,
    };
  }

  let activeGroup =
    groupsInfo.find(
      (group) =>
        group.windowStartMs <= nowMs &&
        nowMs <= group.windowEndMs &&
        group.pendingItems.length > 0
    ) ??
    groupsInfo.find(
      (group) =>
        group.pendingItems.length > 0 && group.windowStartMs > nowMs
    ) ??
    groupsInfo.find((group) => group.windowStartMs > nowMs) ??
    groupsInfo[0];

  // If selected group is fully acknowledged and already active, try to look ahead.
  if (
    activeGroup.pendingItems.length === 0 &&
    activeGroup.windowStartMs <= nowMs
  ) {
    const nextGroup = groupsInfo.find((group) => group.windowStartMs > nowMs);
    if (nextGroup) {
      activeGroup = nextGroup;
    }
  }

  const inWindow =
    activeGroup.windowStartMs <= nowMs && nowMs <= activeGroup.windowEndMs;
  const allTaken = activeGroup.pendingItems.length === 0;
  const halfWindowMs = activeGroup.windowDurationMs / 2;
  const elapsedMs = nowMs - activeGroup.windowStartMs;
  const halfWindowReached = inWindow && elapsedMs >= halfWindowMs;

  const codesForMessages =
    activeGroup.pendingCodes.length > 0
      ? activeGroup.pendingCodes
      : activeGroup.allCodes;
  const formattedCodes = formatMedicationList(codesForMessages);

  let mainLabel: string;
  let timeLabel: string;

  if (!inWindow || allTaken) {
    const diffMs =
      activeGroup.windowStartMs > nowMs
        ? activeGroup.windowStartMs - nowMs
        : Math.max(activeGroup.earliestEvent.getTime() - nowMs, 0);
    mainLabel = formatDurationLabel(diffMs);
    timeLabel = "from now";
  } else {
    mainLabel = "Take dose now";
    const remainingMs = Math.max(activeGroup.windowEndMs - nowMs, 0);
    const remainingMinutes = Math.ceil(remainingMs / 60000);
    if (remainingMinutes > 60) {
      timeLabel = "within the hour";
    } else {
      timeLabel = `within ${Math.max(1, remainingMinutes)} mins`;
    }
  }

  let detailsLabel: string;
  let state: number;

  if (allTaken) {
    detailsLabel = "Thank you for logging a successful dose";
    state = 4;
  } else if (halfWindowReached) {
    detailsLabel = `You are about to miss a scheduled dose for ${formattedCodes}. Take the dose(s) now.`;
    state = 3;
  } else {
    const medicationPrompt =
      formattedCodes === "your medication"
        ? "your medication"
        : `medication ${formattedCodes}`;
    detailsLabel = `Take ${medicationPrompt}.`;
    state = 6;
  }

  return {
    mainLabel,
    timeLabel,
    detailsLabel,
    state,
  };
}
