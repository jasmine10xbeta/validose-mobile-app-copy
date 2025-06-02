import * as Localization from "expo-localization";
import * as Notifications from "expo-notifications";
import moment from "moment-timezone";

export const scheduleMedNotifications = async (protocol: any) => {
  if (!protocol || !protocol.medications) return;

  await Notifications.cancelAllScheduledNotificationsAsync();

  const tz = protocol.timezone || Localization.timezone;
  const now = moment().tz(tz);
  const today = now.format("dddd"); // e.g., "Monday"

  for (const med of protocol.medications) {
    const days = med.days_of_week || [
      // default to daily
      "Monday",
      "Tuesday",
      "Wednesday",
      "Thursday",
      "Friday",
      "Saturday",
      "Sunday",
    ];

    if (!days.includes(today)) continue;

    for (const time of med.dose_times) {
      const [hour, minute] = time.split(":").map(Number);
      const doseTime = now.clone().hour(hour).minute(minute).second(0);

      // If dose time has passed for today, schedule for tomorrow
      if (doseTime.isBefore(now)) {
        doseTime.add(1, "day");
      }

      // Optional: allow window +/- 15 min (just add note, or schedule twice if needed)
      const earlyTime = doseTime.clone().subtract(15, "minutes");
      const lateTime = doseTime.clone().add(15, "minutes");

      await Notifications.scheduleNotificationAsync({
        content: {
          title: `Time to take ${med.name}`,
          body: `Scheduled dose at ${time}.`,
          data: {
            medication: med.name,
            dose_time: time,
            protocol_id: protocol.protocol_id,
          },
        },
        trigger: {
          date: doseTime.toDate(), // exact time scheduling
        },
      });

      // Optionally: Add "early reminder" 15 mins before (commented out by default)
      await Notifications.scheduleNotificationAsync({
        content: {
          title: `Upcoming dose: ${med.name}`,
          body: `Take your medication at ${time}. Reminder 15 mins early.`,
        },
        trigger: {
          date: earlyTime.toDate(),
        },
      });

      // Optionally: Add "late reminder" 15 mins after (commented out by default)
      await Notifications.scheduleNotificationAsync({
        content: {
          title: `Upcoming dose: ${med.name}`,
          body: `Take your medication at ${time}. Reminder 15 mins late.`,
        },
        trigger: {
          date: lateTime.toDate(),
        },
      });
    }
  }
};
