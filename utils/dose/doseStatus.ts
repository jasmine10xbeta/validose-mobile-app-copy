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

  const isDue = now.isAfter(doseTime.subtract(15, "minute")) &&
                now.isBefore(doseTime.add(15, "minute"));

  return isDue ? DoseStatus.Due : DoseStatus.NotTaken;
}