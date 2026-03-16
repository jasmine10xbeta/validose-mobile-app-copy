import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";
import { Schedule } from "@/types/schedule";

interface ScheduleStore {
  schedules: Record<string, Schedule[]>;      // device_id: Schedule[]
  lastUpdated: number;                        // device_id: timestamp
  storeSchedules: (deviceId: string, scheduleList: Schedule[]) => void;
  getTodaySchedules: () => Record<string, Schedule[]>;
  acknowledgeDoseEvent: (
    deviceId: string,
    doseId: string,
    data: any
  ) => Schedule | null;
  acknowledgeDoseEventByTimestamp: (
    deviceId: string,
    eventAt: string | number | Date,
    data: any
  ) => Schedule | null;
  markBackendSynced: (deviceId: string, doseId: string) => void;
  clearOldSchedules: () => void;
  clearSchedules: () => void;
  setLastUpdated: (lastUpdated: number) => void;
}

const useScheduleStore = create<ScheduleStore>()(
  persist(
    (set, get) => ({
      schedules: {},
      lastUpdated: 0,

      storeSchedules: (deviceId, scheduleList) => {
        if (!Array.isArray(scheduleList)) {
          console.warn(
            `[storeSchedules] Invalid scheduleList for device ${deviceId}:`,
            scheduleList
          );
          return;
        }

        const newSchedules = scheduleList.map((s) => ({
          ...s,
          firmware_acknowledged: false,
          backend_synced: false,
        }));

        set((state) => ({
          schedules: {
            ...state.schedules,
            [deviceId]: newSchedules,
          },
        }));
      },

      getTodaySchedules: () => {
        const now = new Date();
        const utcMidnight = new Date(Date.UTC(now.getFullYear(), now.getMonth(), now.getDate()));
        const utcTomorrow = new Date(utcMidnight);
        utcTomorrow.setUTCDate(utcMidnight.getUTCDate() + 1);

        const todaySchedules: Record<string, Schedule[]> = {};

        for (const [deviceId, list] of Object.entries(get().schedules)) {
          todaySchedules[deviceId] = list.filter((s) => {
            const eventTime = new Date(s.event_at_local);
            return eventTime >= utcMidnight && eventTime < utcTomorrow;
          });
        }

        return todaySchedules;
      },

      acknowledgeDoseEvent: (deviceId: string, doseId: string, data: any) => {
        const existingSchedules = get().schedules[deviceId] || [];

        const updated = existingSchedules.map((sch) => {
          if (sch.id === doseId) {
            return {
              ...sch,
              firmware_acknowledged: true,
              firmware_info: data,
            };
          }

          return sch;
        });

        set((state) => ({
          schedules: {
            ...state.schedules,
            [deviceId]: updated,
          },
        }));

        return updated.find((sch) => sch.id === doseId) ?? null;
      },

      acknowledgeDoseEventByTimestamp: (deviceId, eventAt, data) => {
        const existingSchedules = get().schedules[deviceId] || [];
        if (!existingSchedules.length) return null;

        const eventAtDate =
          eventAt instanceof Date
            ? eventAt
            : new Date(typeof eventAt === "number" ? eventAt : String(eventAt));
        const eventAtMs = eventAtDate.getTime();
        if (Number.isNaN(eventAtMs)) return null;

        let matched = existingSchedules.find((schedule) => {
          const startMs = new Date(schedule.window_starts_at_local).getTime();
          const endMs = new Date(schedule.window_ends_at_local).getTime();
          if (Number.isNaN(startMs) || Number.isNaN(endMs)) return false;
          return eventAtMs >= startMs && eventAtMs <= endMs;
        });

        if (!matched) {
          const nearest = existingSchedules
            .map((schedule) => {
              const eventMs = new Date(schedule.event_at_local).getTime();
              const distance = Number.isNaN(eventMs) ? Number.POSITIVE_INFINITY : Math.abs(eventAtMs - eventMs);
              const toleranceMs = Math.max(
                60 * 60 * 1000,
                Math.max(1, Number(schedule.dosing_window_min) || 0) * 60 * 1000,
              );
              return { schedule, distance, toleranceMs };
            })
            .filter((candidate) => candidate.distance <= candidate.toleranceMs)
            .sort((a, b) => a.distance - b.distance);

          matched = nearest[0]?.schedule;
        }

        if (!matched) return null;

        const updated = existingSchedules.map((schedule) => {
          if (schedule.id !== matched?.id) return schedule;

          return {
            ...schedule,
            firmware_acknowledged: true,
            firmware_info: data,
          };
        });

        set((state) => ({
          schedules: {
            ...state.schedules,
            [deviceId]: updated,
          },
        }));

        return updated.find((schedule) => schedule.id === matched?.id) ?? null;
      },

      markBackendSynced: (deviceId, doseId) => {
        const existingSchedules = get().schedules[deviceId] || [];
        const updatedSchedules = existingSchedules.map((sch) => {
          if (sch.id === doseId) {
            return {
              ...sch,
              backend_synced: true,
            };
          }
          return sch;
        });

        set((state) => ({
          schedules: {
            ...state.schedules,
            [deviceId]: updatedSchedules,
          },
        }));
      },

      clearOldSchedules: () => {
        console.log("[Scheduler] Cleaning old schedules..");
        
        const now = Date.now();
        const newSchedules = Object.entries(get().schedules).reduce(
          (acc, [deviceId, list]) => {
            acc[deviceId] = list.filter((s) => {
              const eventTime = new Date(s.event_at_local).getTime();
              return s.backend_synced || eventTime > now;
            });
            return acc;
          },
          {} as Record<string, Schedule[]>
        );

        set({ schedules: newSchedules });
      },

      clearSchedules: () => set({ schedules: {} }),

      setLastUpdated: (lastUpdated) => {
        set({ lastUpdated: new Date(lastUpdated).getTime() });
      },
    }),
    {
      name: "dose-schedules",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useScheduleStore;
