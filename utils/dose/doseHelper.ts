import dayjs from "dayjs";
import useDoseHistoryStore from "@/store/useDoseHistoryStore";
import { TreatmentProtocol } from "@/store/useTreatmentProtocolStore";

// Helper to find the next upcoming dose
export interface DoseLabelResult {
  mainLabel: string;
  timeLabel: string;
  detailsLabel: string;
  color?: string;
}

/**
 * Returns the UI labels for a given dose timing.
 */
export function getLabelsForNextDose(
  medicineName: string,
  state: number,
  diffMin: number,
  dosingWindowMin: number
): DoseLabelResult {
  const mainLabel =
    diffMin >= 60
      ? `In ${Math.floor(diffMin / 60)} hour${diffMin >= 120 ? "s" : ""}`
      : `In ${diffMin} min`;

  switch (state) {
    case 2:
      return {
        mainLabel: "Take dose now",
        timeLabel: `within ${dosingWindowMin + diffMin} min`,
        detailsLabel: `You are about to miss a scheduled dose for ${medicineName}. Take the dose now.`,
        color: "#FC9E9E33",
      };
    case 3:
      return {
        mainLabel,
        timeLabel: `from now`,
        detailsLabel: `Thank you for logging a successful dose.`,
        color: "#FC9E9E33",
      };
    case 4:
      return {
        mainLabel,
        timeLabel: `from now`,
        detailsLabel: `You missed a dose. Please take your dose on time.`,
        color: "#FC9E9E33",
      };
    default:
      return {
        mainLabel,
        timeLabel: `from now`,
        detailsLabel: `Take medication ${medicineName}.`,
        color: "#F3F3F3",
      };
  }
}

export const getNextDose = (
  devices: any[],
  getProtocol: {
    (deviceId: string): TreatmentProtocol | undefined;
    (arg0: any): any;
  }
) => {
  const now = dayjs();
  const todayStr = dayjs().format("dddd"); // e.g., "Monday"

  const upcomingDoses: {
    device: any;
    timeMin: number;
    expectedTime: string;
    mainLabel: string;
    timeLabel: string;
    detailsLabel: string;
    state: number;
  }[] = [];

  for (const device of devices) {
    const protocol = getProtocol(device.deviceId);
    if (!protocol?.administrationDays?.includes(todayStr)) continue;

    for (const timeMin of protocol.administrationTimesMin || []) {
      const [hour, minute] = [Math.floor(timeMin / 60), timeMin % 60];
      const doseTime = dayjs()
        .startOf("day")
        .add(hour, "hour")
        .add(minute, "minute");
      const diffMin = doseTime.diff(now, "minute");

      if (diffMin < -protocol.dosingWindowMin) continue; // Skip expired doses

      const expectedTime = doseTime.format("HH:mm");
      const state = getDoseState(
        device.deviceId,
        expectedTime,
        protocol.dosingWindowMin
      );

      const { mainLabel, timeLabel, detailsLabel } = getLabelsForNextDose(
        protocol.medicationName,
        state,
        diffMin,
        protocol.dosingWindowMin
      );

      upcomingDoses.push({
        device,
        timeMin: diffMin,
        expectedTime,
        mainLabel,
        timeLabel,
        detailsLabel,
        state,
      });
    }
  }

  if (upcomingDoses.length === 0) return null;

  upcomingDoses.sort((a, b) => a.timeMin - b.timeMin);

  const next = upcomingDoses[0];
  console.log(
    `Next dose is for ${next.device.deviceName} at ${-next.timeMin} min (${next.expectedTime})`
  );
  return next;
};

export function getDoseState(
  deviceId: string,
  expectedTime: string,
  dosingWindowMin: number
): number {
  const { wasDoseTaken, wasDoseMissed } = useDoseHistoryStore.getState();

  if (wasDoseTaken(deviceId, expectedTime)) return 3;
  if (wasDoseMissed(deviceId, expectedTime)) return 4;

  const [hour, minute] = expectedTime.split(":").map(Number);
  const doseTime = dayjs()
    .startOf("day")
    .add(hour, "hour")
    .add(minute, "minute");
  const now = dayjs();

  const diffMinutes = doseTime.diff(now, "minute");

  if (diffMinutes < -dosingWindowMin) return 5; // Missed (window passed, no record)
  if (-dosingWindowMin <= diffMinutes && diffMinutes <= dosingWindowMin)
    return 2; // Within window

  return 0; // Later
}
