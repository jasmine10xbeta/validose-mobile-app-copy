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
  regimenId: string;
  protocolId: string;
  deviceId: string;
  deviceName: string;
  medicine: string;
  medicineState?: number;
  color?: string;
  status?: "Connected" | "Disconnected";
  indicationCode: string;
  dosageAmount: number;
  administrationDays: any[];
  administrationTimesMin: any[];
  frequencyCount: number;
  dosingWindowMin: number;
  active: boolean;
  notes?: string;
  previousTreatment?: Partial<Omit<Device, "deviceId" | "deviceName" | "color" | "status">>;
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
        const idExists = currentDevices.some((d) => d.id === device.deviceId);
        const nameExists = currentDevices.some((d) => d.deviceName === device.deviceName);

        if (idExists || nameExists) {
          return false;
        }

        // If unique, add the device
        set((state) => ({ devices: [...state.devices, device] }));
        return true;
      },
      removeDevice: (deviceId: string) => {
        set((state) => ({
          devices: state.devices.filter((device) => device.deviceId !== deviceId),
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
            device.deviceId === deviceId
              ? {
                  ...device,
                  previousTreatment: {
                    regimen_id: device.regimenId,
                    indication_code: device.indicationCode,
                    dosage_amount: device.dosageAmount,
                    administrationDays: device.administrationDays,
                    administration_times_min: device.administrationTimesMin,
                    frequency_count: device.frequencyCount,
                    dosing_window_min: device.dosingWindowMin,
                    protocolId: device.protocolId,
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