import * as Notifications from "expo-notifications";
import { Platform } from "react-native";
import { Schedule } from "@/types/schedule";

const formatTime = (d: Date) =>
  d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });

/** Turn ["A"] -> "A"; ["A","B"] -> "A and B"; ["A","B","C"] -> "A, B, and C" */
function formatList(items: string[]) {
  if (items.length <= 1) return items[0] ?? "";
  if (items.length === 2) return `${items[0]} and ${items[1]}`;
  const head = items.slice(0, -1).join(", ");
  const last = items[items.length - 1];
  return `${head}, and ${last}`;
}

/** Build an instruction like:
 *  - "Take medication A"
 *  - "Take medications A and B"
 *  - "Take medications A, B, and C"
 */
function buildInstruction(medCodes: string[]) {
  const unique = Array.from(new Set(medCodes)).filter(Boolean);
  const plural = unique.length > 1;
  const list = formatList(unique);
  return `Take medication${plural ? "s" : ""} ${list}`;
}

const avgDate = (group: Schedule[], getter: (s: Schedule) => string) => {
  const timestamps = group.map((s) => new Date(getter(s)).getTime());
  const avgTime = timestamps.reduce((sum, t) => sum + t, 0) / timestamps.length;
  return new Date(avgTime);
};

export const scheduleNotificationsForGroup = async (group: Schedule[]) => {
  if (!group || group.length === 0) return;

  const ids: string[] = [];

  const medCodes = group.map((s) => s.medication_code);
  const instruction = buildInstruction(medCodes);

  const avgWindowStart = avgDate(group, (s) => s.window_starts_at_local);
  const avgEvent = avgDate(group, (s) => s.event_at_local);
  const avgWindowEnd = avgDate(group, (s) => s.window_ends_at_local);

  const plural = new Set(medCodes.filter(Boolean)).size > 1;
  const titleBase = `Time to take your medication${plural ? "s" : ""}`;

  const now = Date.now();
  const safe = (d: Date) => (d.getTime() > now ? d : new Date(now + 5_000));

  // 1) At avg window start
  ids.push(
  await Notifications.scheduleNotificationAsync({
    content: {
      title: "Upcoming dose" + (plural ? "s" : ""),
      body: `${instruction} at ${formatTime(avgEvent)}.`,
    },
    trigger: {
      type: Notifications.SchedulableTriggerInputTypes.DATE,
      date: safe(avgWindowStart),
    },
  }));

  // 2) At avg event time
  ids.push(await Notifications.scheduleNotificationAsync({
    content: {
      title: titleBase,
      body: `${instruction} now (scheduled for ${formatTime(avgEvent)}).`,
    },
    trigger: {
      type: Notifications.SchedulableTriggerInputTypes.DATE,
      date: safe(avgEvent),
    },
  }));

  // 3) At avg window end
  ids.push(await Notifications.scheduleNotificationAsync({
    content: {
      title: plural ? "Almost missed doses" : "Almost missed dose",
      body: `${instruction} now. Window ends at ${formatTime(avgWindowEnd)}.`,
    },
    trigger: {
      type: Notifications.SchedulableTriggerInputTypes.DATE,
      date: safe(avgWindowEnd),
    },
  }));

  return ids;
};

function groupNearbyDoses(schedules: Schedule[]): Schedule[][] {
  return schedules.reduce((acc, s) => {
    const lastGroup = acc[acc.length - 1];
    if (
      lastGroup &&
      Math.abs(
        new Date(lastGroup[0].event_at_local).getTime() -
          new Date(s.event_at_local).getTime()
      ) <=
        15 * 60 * 1000
    ) {
      lastGroup.push(s);
    } else {
      acc.push([s]);
    }
    return acc;
  }, [] as Schedule[][]);
}

/** Ask for permissions and set Android channel */
async function ensureNotificationsReady() {
  const { status } = await Notifications.requestPermissionsAsync();
  if (status !== "granted") return false;

  if (Platform.OS === "android") {
    await Notifications.setNotificationChannelAsync("default", {
      name: "Default",
      importance: Notifications.AndroidImportance.DEFAULT,
    });
  }
  return true;
}

/**
 * Clears old notifications first, then (re)schedules.
 * - Cancels all scheduled (future) notifications
 * - Dismisses any delivered notifications from the tray
 * - Sorts schedules for stable grouping
 */
export const updateNotificationsForSchedules = async (schedules: Schedule[]) => {
  const ready = await ensureNotificationsReady();
  if (!ready) return;

  console.log("\n");
  console.log("[Notifications] Updating based on schedule..");

  // Clear anything previously scheduled/delivered
  await Notifications.cancelAllScheduledNotificationsAsync();
  await Notifications.dismissAllNotificationsAsync();

  // Keep grouping deterministic
  const sorted = [...schedules].sort(
    (a, b) => +new Date(a.event_at_local) - +new Date(b.event_at_local)
  );

  const groups = groupNearbyDoses(sorted);

  console.log("\n");
  console.log(`[Notifications] Found ${groups.length} groups`);

  await Promise.all(groups.map((g) => scheduleNotificationsForGroup(g)));
};
