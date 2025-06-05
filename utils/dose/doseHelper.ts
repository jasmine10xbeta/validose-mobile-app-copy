import dayjs from "dayjs";
import { DoseStatus } from "@/constants/enums";

export function getDoseStatus(
  expectedTime: string,
  taken: boolean | undefined,
  takenAt?: string
): DoseStatus {
  if (taken) return DoseStatus.Taken;

  const now = dayjs();
  const doseTime = dayjs().startOf("day").add(parseInt(expectedTime), "minute");

  const isDue =
    now.isAfter(doseTime.subtract(15, "minute")) &&
    now.isBefore(doseTime.add(15, "minute"));

  return isDue ? DoseStatus.Due : DoseStatus.NotTaken;
}

// Helper to find the next upcoming dose
export const getNextDoseInfoForDevices = (devices: any[]) => {
  console.log("devices", devices);

  const now = dayjs();
  const dayMap = [
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
  ];
  const todayStr = dayMap[now.day()];

  const upcomingDoses: {
    device: any;
    timeMin: number;
    mainLabel: string;
    timeLabel: string;
    detailsLabel: string;
  }[] = [];

  // Label logic
  let mainLabel = "No upcoming dose";
  let timeLabel = "";
  let detailsLabel = "";

  for (const device of devices) {
    if (!device.administrationDays?.includes(todayStr)) continue;

    for (const timeMin of device.administrationTimesMin || []) {
      const [hour, minute] = [Math.floor(timeMin / 60), timeMin % 60];
      const doseTime = now
        .startOf("day")
        .add(hour, "hour")
        .add(minute, "minute");
      const diffMin = doseTime.diff(now, "minute");

      // Only include doses within the valid range
      if (diffMin >= -device.dosingWindowMin) {
        console.log(`diffMin: ${diffMin}, device.dosingWindowMin: ${device.dosingWindowMin}`);
        if (diffMin <= 0 && diffMin >= -device.dosingWindowMin) {
          mainLabel = "Take dose now";
          timeLabel = `within ${device.dosingWindowMin} mins`;
          detailsLabel = `You are about to miss a scheduled dose for ${device.deviceName}. Take the dose now.`;
        } else if (diffMin >= 0 && diffMin <= device.dosingWindowMin) {
          mainLabel = "Take dose now";
          timeLabel = `within ${device.dosingWindowMin * 2} mins`;
          detailsLabel = `Take medication ${device.deviceName}`;
        } else if (diffMin > 0) {
          if (diffMin >= 60) {
            const hours = Math.floor(diffMin / 60);
            mainLabel = `In ${hours} hour${hours > 1 ? "s" : ""}`;
          } else {
            mainLabel = `In ${diffMin} minute${diffMin > 1 ? "s" : ""}`;
          }
          timeLabel = "from now";
          detailsLabel = `Take medication ${device.deviceName}`;
        }

        upcomingDoses.push({ device, timeMin: diffMin, mainLabel, timeLabel, detailsLabel });
      }
    }
  }

  if (upcomingDoses.length === 0) return null;

  // Sort by soonest dose time
  upcomingDoses.sort((a, b) => a.timeMin - b.timeMin);

  const next = upcomingDoses[0];
  console.log(
    `Next dose is for ${next.device.deviceName} at ${next.timeMin} min`
  );
  return next;
};

// Dose info label selector
export const getLabelsForDoseState = (state: number) => {
  switch (state) {
    case 0:
      return { mainLabel: "Next dose", timeLabel: "is coming up" };
    case 1:
      return { mainLabel: "Take dose now", timeLabel: "within dose window" };
    case 2:
      return {
        mainLabel: "Take dose now",
        timeLabel: "You're about to miss your dose!",
      };
    default:
      return { mainLabel: "All caught up 🎉", timeLabel: "No dose for now." };
  }
};
