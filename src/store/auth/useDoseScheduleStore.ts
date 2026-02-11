import AsyncStorage from "@react-native-async-storage/async-storage";
import dayjs from "dayjs";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";
import {
  getNotificationId,
  scheduleDoseNotification,
} from "@/utils/notifications/notificationScheduler";
import { TreatmentProtocol } from "./useTreatmentProtocolStore";

interface ScheduledDose {
  deviceId: string;
  expectedTime: string; // "HH:mm"
  status: "upcoming" | "missed" | "taken"; // derived from history
}

interface DoseScheduleStore {
  today: Record<string, ScheduledDose[]>; // deviceId → doses
  initializeSchedule(deviceId: string, protocol: TreatmentProtocol): void;
  updateStatus(
    deviceId: string,
    expectedTime: string,
    status: ScheduledDose["status"]
  ): void;
  getDoses(deviceId: string): ScheduledDose[];
}

const useDoseScheduleStore = create<DoseScheduleStore>()(
  persist(
    (set, get) => ({
      today: {},

      initializeSchedule: (deviceId, protocol) => {
        const today = dayjs().format("dddd"); // e.g., "Monday"
        console.log(`Initializing doses for ${deviceId} on ${today}`);

        // Skip initialization if today is not a scheduled day
        if (!protocol.administrationDays.includes(today)) {
          console.log(
            `Skipping dose initialization for ${deviceId}, not scheduled for ${today}`
          );
          set((state) => ({
            today: { ...state.today, [deviceId]: [] },
          }));
          return;
        }

        const doses: ScheduledDose[] = protocol.administrationTimesMin.map(
          (min) => {
            const h = Math.floor(min / 60)
              .toString()
              .padStart(2, "0");
            const m = (min % 60).toString().padStart(2, "0");
            const expectedTime = `${h}:${m}`;

            const doseTime = dayjs().startOf("day").add(min, "minute");
            const startTime = doseTime.subtract(
              protocol.dosingWindowMin,
              "minute"
            );
            const endTime = doseTime.add(protocol.dosingWindowMin, "minute");

            const medicine = protocol.medicine ?? "your medication";

            scheduleDoseNotification(
              getNotificationId(deviceId, expectedTime, "start"),
              "Upcoming Dose",
              `Prepare to take ${medicine}.`,
              startTime.toDate()
            );

            scheduleDoseNotification(
              getNotificationId(deviceId, expectedTime, "dose"),
              "Time to Take Dose",
              `Please take ${medicine} now.`,
              doseTime.toDate()
            );

            scheduleDoseNotification(
              getNotificationId(deviceId, expectedTime, "end"),
              "Dose Window Ending",
              `You may miss your dose of ${medicine}. Please take dose now.`,
              endTime.toDate()
            );

            return {
              deviceId,
              expectedTime,
              status: "upcoming",
            };
          }
        );

        set((state) => ({
          today: { ...state.today, [deviceId]: doses },
        }));
      },

      updateStatus: (deviceId, expectedTime, status) => {
        const current = get().today[deviceId] || [];
        const updated = current.map((d) =>
          d.expectedTime === expectedTime ? { ...d, status } : d
        );
        set((state) => ({ today: { ...state.today, [deviceId]: updated } }));
      },

      getDoses: (deviceId) => get().today[deviceId] || [],
    }),
    {
      name: "dose-schedule-storage",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useDoseScheduleStore;
