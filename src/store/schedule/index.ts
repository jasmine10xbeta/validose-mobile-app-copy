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
    timestampUnix: number,
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

      acknowledgeDoseEvent: (deviceId, timestampUnix, data) => {
        const matchTime = new Date(timestampUnix * 1000);
        const existingSchedules = get().schedules[deviceId] || [];
        let dose_data: Schedule | undefined;

        const updatedSchedules = existingSchedules.map((sch) => {
          const windowStart = new Date(sch.window_starts_at_local);
          const windowEnd = new Date(sch.window_ends_at_local);

          // Match if the timestamp falls within the scheduled window
          const isWithinWindow = matchTime >= windowStart && matchTime <= windowEnd;

          console.log(`\n[Acknowledgment] Is within window (${sch.window_starts_at_local} - ${windowEnd})? ${isWithinWindow}`);

          const event_at = "2025-09-09T11:00:00.000-04:00";

          if (isWithinWindow || event_at === sch.event_at_local) {
            dose_data = {
              ...sch,
              firmware_acknowledged: true,
              firmware_info: data,
            };
            
            console.log(`💊 [BLE] Acknowledged dose event: \n\n${dose_data}`);

            return dose_data;
          }

          return sch;
        });

        if (dose_data) {
          set((state) => ({
            schedules: {
              ...state.schedules,
              [deviceId]: updatedSchedules,
            },
          }));
        }

        return dose_data || null;
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
