import dayjs from "dayjs";
import * as SecureStore from "expo-secure-store";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";
import { Device } from "@/store/useDeviceStore";

const SecureStorage = {
  getItem: (key: string) => SecureStore.getItemAsync(key),
  setItem: (key: string, value: string) => SecureStore.setItemAsync(key, value),
  removeItem: (key: string) => SecureStore.deleteItemAsync(key),
};

interface ExpectedDose {
  expectedTime: string;
  takenAt?: string;
  taken?: boolean;
  protocolId: string;
}

interface DailyDeviceDose {
  deviceId: string;
  date: string;
  doses: ExpectedDose[];
}

interface DoseStore {
  doseRecords: DailyDeviceDose[];
  initializeDoses: (devices: Device[]) => void;
  markDoseTaken: (deviceId: string, timestamp: string) => void;
  getDosesForToday: (deviceId: string) => ExpectedDose[];
}

export const useDoseStore = create<DoseStore>()(
  persist(
    (set, get) => ({
      doseRecords: [],
      initializeDoses: (devices) => {
        const today = dayjs();
        const todayDay = today.format("dddd"); // e.g., 'Monday'
        const todayStr = today.format("YYYY-MM-DD");

        const newRecords = devices
          .map((device: Device): DailyDeviceDose | null => {
            if (!device.administrationDays.includes(todayDay)) return null;

            const doses: ExpectedDose[] = device.administrationTimesMin.map(
              (min: number) => {
                const time = dayjs().startOf("day").add(min, "minute");
                return {
                  expectedTime: time.format("HH:mm"),
                  protocolId: device.protocolId,
                };
              }
            );

            return {
              deviceId: device.deviceId,
              date: todayStr,
              doses,
            };
          })
          .filter((record): record is DailyDeviceDose => record !== null); // Type guard

        set({ doseRecords: newRecords });
      },
      markDoseTaken: (deviceId: string, timestamp: string) => {
        const today = dayjs().format("YYYY-MM-DD");
        set((state) => {
          const updatedRecords = state.doseRecords.map((record) => {
            if (record.deviceId === deviceId && record.date === today) {
              const doseIdx = record.doses.findIndex((d) => !d.taken);
              if (doseIdx !== -1) {
                record.doses[doseIdx] = {
                  ...record.doses[doseIdx],
                  taken: true,
                  takenAt: timestamp,
                };
              }
            }
            return record;
          });

          return { doseRecords: updatedRecords };
        });
      },
      getDosesForToday: (deviceId: string) => {
        const today = dayjs().format("YYYY-MM-DD");
        return (
          get().doseRecords.find(
            (record) => record.deviceId === deviceId && record.date === today
          )?.doses || []
        );
      },
    }),
    {
      name: "dose-storage",
      storage: createJSONStorage(() => SecureStorage),
    }
  )
);

export default useDoseStore;
