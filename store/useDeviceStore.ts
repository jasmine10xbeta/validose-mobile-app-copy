import * as SecureStore from "expo-secure-store";
import { create } from "zustand";
import { StateStorage, persist, createJSONStorage } from "zustand/middleware";

// Standard SecureStorage - no custom serialization needed for arrays
const SecureStorage: StateStorage = {
  getItem: async (name: string): Promise<string | null> => {
    return (await SecureStore.getItemAsync(name)) || null;
  },
  setItem: async (name: string, value: string): Promise<void> => {
    await SecureStore.setItemAsync(name, value);
  },
  removeItem: async (name: string): Promise<void> => {
    await SecureStore.deleteItemAsync(name);
  },
};

export interface Device {
  id: string;
  name: string;
  medicine: string;
  medicineState?: number;
  color?: string;
  status?: "Connected" | "Disconnected";
  regimen_id: string;
  indication_code: string;
  dosage_amount: number;
  administration_days: any[];
  administration_times_min: any[];
  frequency_count: number;
  dosing_window_min: number;
  active: boolean;
  notes?: string;
  previousTreatment?: Partial<Omit<Device, "id" | "name" | "color" | "status">>;
}

interface DeviceState {
  devices: Device[]; // Changed from Set to Array
  addDevice: (device: Device) => boolean; // Returns boolean indicating success
  removeDevice: (deviceId: string) => void;
  getDeviceList: () => Device[];
  removeAll: () => void;
  updateDeviceById: (deviceId: string, updates: Partial<Device>) => void;

  dailyDoseLog: Record<string, string>; // deviceId -> ISO Date
  recordDoseTaken: (deviceId: string) => void;
  wasDoseTakenToday: (deviceId: string) => boolean;
}

const useDeviceStore = create<DeviceState>()(
  persist(
    (set, get) => ({
      devices: [], // Initialize as an empty array
      addDevice: (device: Device) => {
        const currentDevices = get().devices;

        // Check for uniqueness of deviceId and deviceName
        const idExists = currentDevices.some((d) => d.id === device.id);
        const nameExists = currentDevices.some((d) => d.name === device.name);

        if (idExists || nameExists) {
          return false;
        }

        // If unique, add the device
        set((state) => ({ devices: [...state.devices, device] }));
        return true;
      },
      removeDevice: (deviceId: string) => {
        set((state) => ({
          devices: state.devices.filter((device) => device.id !== deviceId),
        }));
      },
      getDeviceList: () => {
        return get().devices;
      },
      removeAll: () => {
        set({ devices: [] });
      },
      updateDeviceById: (deviceId, updates) => {
        set((state) => ({
          devices: state.devices.map((device) =>
            device.id === deviceId
              ? {
                  ...device,
                  previousTreatment: {
                    regimen_id: device.regimen_id,
                    indication_code: device.indication_code,
                    dosage_amount: device.dosage_amount,
                    administration_days: device.administration_days,
                    administration_times_min: device.administration_times_min,
                    frequency_count: device.frequency_count,
                    dosing_window_min: device.dosing_window_min,
                  },
                  ...updates,
                }
              : device
          ),
        }));
      },
      dailyDoseLog: {},
      recordDoseTaken: (deviceId) =>
        set((state) => ({
          dailyDoseLog: {
            ...state.dailyDoseLog,
            [deviceId]: new Date().toISOString().split("T")[0], // Only the date
          },
        })),
      wasDoseTakenToday: (deviceId) => {
        const logDate = get().dailyDoseLog[deviceId];
        const today = new Date().toISOString().split("T")[0];
        return logDate === today;
      },
    }),
    {
      name: "device-storage",
      storage: createJSONStorage(() => SecureStorage), // Use the standard SecureStorage
    }
  )
);

export default useDeviceStore;
