import * as Notifications from "expo-notifications";
import { Platform } from "react-native";
import { Schedule } from "@/types/schedule";

const formatTime = (d: Date) =>
  d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });

/**
 * Format a list of strings into a human-readable string.
 * @param items The list of strings
 * @returns A string that can be used to display the list
 * @example
 * formatList(["foo", "bar"]) // "foo and bar"
 * formatList(["foo", "bar", "baz"]) // "foo, bar, and baz"
 */
function formatList(items: string[]) {
  if (items.length <= 1) return items[0] ?? "";
  if (items.length === 2) return `${items[0]} and ${items[1]}`;
  const head = items.slice(0, -1).join(", ");
  const last = items[items.length - 1];
  return `${head}, and ${last}`;
}

/**
 * Given an array of medication codes, returns a single string instruction
 * indicating what medications should be taken.
 *
 * @param medCodes - array of medication codes
 * @returns instruction string, e.g. "Take medication A and B"
 */
function buildInstruction(medCodes: string[]) {
  const unique = Array.from(new Set(medCodes)).filter(Boolean);
  const plural = unique.length > 1;
  const list = formatList(unique);
  return `Take medication${plural ? "s" : ""} ${list}`;
}

/**
 * Given an array of schedules and a getter function that returns a date string
 * for each schedule, returns a single Date object that represents the average
 * of the dates returned by the getter.
 *
 * @param group - array of Schedules
 * @param getter - function that takes a Schedule and returns a date string
 * @returns a single Date object representing the average date of the input
 */
const avgDate = (group: Schedule[], getter: (s: Schedule) => string) => {
  const timestamps = group.map((s) => new Date(getter(s)).getTime());
  const avgTime = timestamps.reduce((sum, t) => sum + t, 0) / timestamps.length;
  return new Date(avgTime);
};

/**
 * Schedules three notifications for the given group of schedules. The first notification occurs at the average start of the dosing window, the second at the average event time, and the third at the average end of the dosing window.
 * @param group Array of Schedule objects to schedule notifications for
 * @returns An array of notification IDs, which can be used to cancel the scheduled notifications.
 */
export const scheduleNotificationsForGroup = async (group: Schedule[]) => {
  if (!group || group.length === 0) return;

  const ids: string[] = [];

  const medCodes = group.map((s) => s.medication_code);
  const instruction = buildInstruction(medCodes);

  const avgWindowStart = avgDate(group, (s) => s.window_starts_at_local);
  const fifteenMinutesBeforeStart = new Date(avgWindowStart.getTime() - 15 * 60 * 1000);

  const avgEvent = avgDate(group, (s) => s.event_at_local);
  const avgWindowEnd = avgDate(group, (s) => s.window_ends_at_local);

  const plural = new Set(medCodes.filter(Boolean)).size > 1;
  const titleBase = `Time to take your medication${plural ? "s" : ""}`;

  const now = Date.now();
  const safe = (d: Date) => (d.getTime() > now ? d : new Date(now + 5_000));

  // 1) 15 mins before avg window start
  ids.push(
    await Notifications.scheduleNotificationAsync({
      content: {
        title: "Upcoming dose" + (plural ? "s" : ""),
        body: `${instruction} at ${formatTime(avgEvent)}.`,
      },
      trigger: {
        type: Notifications.SchedulableTriggerInputTypes.DATE,
        date: safe(fifteenMinutesBeforeStart),
      },
    })
  );

  // 2) At avg event time
  ids.push(
    await Notifications.scheduleNotificationAsync({
      content: {
        title: titleBase,
        body: `${instruction} now.`,
      },
      trigger: {
        type: Notifications.SchedulableTriggerInputTypes.DATE,
        date: safe(avgEvent),
      },
    })
  );

  // 3) At avg window end
  ids.push(
    await Notifications.scheduleNotificationAsync({
      content: {
        title: plural ? "Almost missed doses" : "Almost missed dose",
        body: `${instruction} now.`,
      },
      trigger: {
        type: Notifications.SchedulableTriggerInputTypes.DATE,
        date: safe(avgWindowEnd),
      },
    })
  );

  return ids;
};

/**
 * Given a list of Schedule objects, group together those that are within 15 minutes
 * of each other by event_at_local time.
 *
 * @param {Schedule[]} schedules The list of schedules to group.
 * @return {Schedule[][]} A list of lists of Schedule objects, where each inner list
 *   contains Schedules that are close to each other in time.
 */
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

async function configureNotificationChannel() {
  if (Platform.OS !== "android") return;

  await Notifications.setNotificationChannelAsync("default", {
    name: "Default",
    importance: Notifications.AndroidImportance.DEFAULT,
  });
}

async function scheduleDoseNotifications(schedules: Schedule[]) {
  console.log("\n[Notifications] Updating based on schedule..");

  await configureNotificationChannel();

  await Notifications.cancelAllScheduledNotificationsAsync();
  await Notifications.dismissAllNotificationsAsync();

  const sorted = [...schedules].sort(
    (a, b) => +new Date(a.event_at_local) - +new Date(b.event_at_local)
  );
  const groups = groupNearbyDoses(sorted);

  console.log(`\n[Notifications] Found ${groups.length} groups`);

  await Promise.all(groups.map((g) => scheduleNotificationsForGroup(g)));
}

/**
 * Given a list of Schedule objects, update the notifications scheduled/delivered
 * for them. This will:
 *  1. Clear any previously scheduled/delivered notifications
 *  2. Group the schedules by event_at_local time, up to 15 minutes apart
 *  3. Schedule notifications for each group, at window start, event time, and
 *     window end
 *
 * @param {Schedule[]} schedules The list of schedules to update notifications for
 */
export const updateNotificationsForSchedules = async (
  schedules: Schedule[]
) => {
  const { status, canAskAgain } = await Notifications.getPermissionsAsync();

  if (status === "granted") {
    await scheduleDoseNotifications(schedules);
    return;
  }

  if (canAskAgain) {
    Notifications.requestPermissionsAsync()
      .then(async ({ status: newStatus }) => {
        if (newStatus === "granted") {
          try {
            await scheduleDoseNotifications(schedules);
          } catch (err) {
            console.warn("[Notifications] Failed to schedule after permission granted", err);
          }
        }
      })
      .catch((err) => {
        console.warn("[Notifications] Permission prompt failed", err);
      });
  }
};

/**
 * Cancels future notifications related to a taken dose.
 * Only cancels notifications for the group that includes the given dose,
 * and only if the notifications are still scheduled for the future.
 *
 * @param dose The dose that was taken
 * @param allSchedules All current schedules (used to group doses)
 */
export const cancelFutureNotificationsForTakenDose = async (
  dose: Schedule,
  allSchedules: Schedule[]
) => {
  const now = Date.now();

  // Sort and group all schedules to match the same logic as scheduling
  const sorted = [...allSchedules].sort(
    (a, b) => +new Date(a.event_at_local) - +new Date(b.event_at_local)
  );
  const groups = groupNearbyDoses(sorted);

  // Find the group that contains the taken dose
  const group = groups.find((g) =>
    g.some((s) => s.id === dose.id)
  );

  if (!group) {
    console.warn("[Notifications] Dose not found in any group.");
    return;
  }

  // Recalculate average times for the group
  const avgWindowStart = avgDate(group, (s) => s.window_starts_at_local);
  const avgEvent = avgDate(group, (s) => s.event_at_local);
  const avgWindowEnd = avgDate(group, (s) => s.window_ends_at_local);
  const fifteenBeforeStart = new Date(avgWindowStart.getTime() - 15 * 60 * 1000);

  const targetTimes = [
    fifteenBeforeStart,
    avgEvent,
    avgWindowEnd,
  ];

  // Fetch all scheduled notifications
  const allScheduled = await Notifications.getAllScheduledNotificationsAsync();

  let cancelled = 0;

  for (const notif of allScheduled) {
    const scheduledDate = (notif.trigger as any)?.date;
    if (!scheduledDate) continue;

    const triggerTime = new Date(scheduledDate).getTime();

    // If it's one of the notifications from this group and still in the future, cancel it
    if (
      targetTimes.some(
        (t) => Math.abs(t.getTime() - triggerTime) < 60 * 1000 && triggerTime > now
      )
    ) {
      await Notifications.cancelScheduledNotificationAsync(notif.identifier);
      cancelled++;
    }
  }

  console.log(`[Notifications] Cancelled ${cancelled} future notifications for taken dose.`);
};
