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
    deviceName: string,
    event_at: string,
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

      acknowledgeDoseEvent: (deviceName: string, event_id: string, data: any) => {
        const existingSchedules = get().schedules[deviceName] || [];

        const updated = existingSchedules.map((sch) => {
          if (sch.id === event_id) {
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
            [deviceName]: updated,
          },
        }));

        return updated.find((sch) => sch.id === event_id) ?? null;
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
        console.log("\n");
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
          {} as Record<string, []>
        );

        set({ s: newSchedules });
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
