import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";
import { sendDoseRecordsToBackend } from "@/utils/axios/api/__mocks__/dosage";
import { cancelDoseNotifications } from "@/utils/notifications/notificationScheduler";

export interface DoseRecord {
  deviceId: string;
  expectedTime: string;
  taken: boolean;
  timestamp: string;
  synced: boolean;
}

interface DoseHistoryStore {
  records: DoseRecord[];
  lastSyncedAt: string | null;
  markDoseTaken(deviceId: string, expectedTime: string): void;
  syncHistoryToBackend(): Promise<void>;
  cleanOldSyncedRecords: () => void;
  wasDoseTaken(deviceId: string, expectedTime: string): boolean;
  wasDoseMissed(deviceId: string, expectedTime: string): boolean;
  flagAsSynced(deviceId: string, expectedTime: string): void;
  getHistoryForDevice(deviceId: string): DoseRecord[];
}

const useDoseHistoryStore = create<DoseHistoryStore>()(
  persist(
    (set, get) => ({
      records: [],
      lastSyncedAt: null,

      markDoseTaken: (deviceId, expectedTime) => {
        const newRecord: DoseRecord = {
          deviceId,
          expectedTime,
          taken: true,
          timestamp: new Date().toISOString(),
          synced: false,
        };

        // Cancel associated notifications
        cancelDoseNotifications(deviceId, expectedTime);
        set((state) => ({ records: [...state.records, newRecord] }));
      },

      syncHistoryToBackend: async () => {
        const state = get();
        const unsynced = state.records.filter((r) => {
          const age = Date.now() - new Date(r.timestamp).getTime();
          return !r.synced && age <= 2 * 24 * 60 * 60 * 1000; // 2 days
        });

        if (unsynced.length === 0) return;

        try {
          // TODO: Replace with backend call
          await sendDoseRecordsToBackend(unsynced);

          const updatedRecords = state.records.map((r) =>
            unsynced.find(
              (u) =>
                u.deviceId === r.deviceId && u.expectedTime === r.expectedTime
            )
              ? { ...r, synced: true }
              : r
          );

          set({
            records: updatedRecords,
            lastSyncedAt: new Date().toISOString(),
          });
        } catch (e) {
          console.warn("Sync failed, will retry later", e);
        }
      },

      cleanOldSyncedRecords: () => {
        const now = new Date();
        const todayStr = now.toISOString().split("T")[0];

        set((state) => {
          const filtered = state.records.filter((record) => {
            const recordDate = record.timestamp.split("T")[0];

            // Keep if:
            // 1. It’s today
            // 2. It’s not synced
            return recordDate === todayStr || !record.synced;
          });

          return { records: filtered };
        });
      },

      wasDoseTaken: (
        deviceId,
        expectedTime // TODO: Also check if same day
      ) =>
        get().records.some(
          (r) =>
            r.deviceId === deviceId &&
            r.expectedTime === expectedTime &&
            r.taken
        ),

      wasDoseMissed: (deviceId, expectedTime) => {
        const record = get().records.find(
          (r) => r.deviceId === deviceId && r.expectedTime === expectedTime
        );
        return record ? !record.taken : false;
      },

      flagAsSynced: (deviceId, expectedTime) => {
        set((state) => ({
          records: state.records.map((r) =>
            r.deviceId === deviceId && r.expectedTime === expectedTime
              ? { ...r, synced: true }
              : r
          ),
        }));
      },

      getHistoryForDevice: (deviceId) =>
        get().records.filter((r) => r.deviceId === deviceId),
    }),
    {
      name: "dose-history-storage",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useDoseHistoryStore;
