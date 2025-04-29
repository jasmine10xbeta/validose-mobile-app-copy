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
  modicineState: number;
  color: string;
}

interface DeviceState {
  devices: Device[]; // Changed from Set to Array
  addDevice: (device: Device) => boolean; // Returns boolean indicating success
  removeDevice: (deviceId: string) => void;
  getDeviceList: () => Device[];
  removeAll: () => void;
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
          return false; // Indicate failure
        }

        // If unique, add the device
        set((state) => ({ devices: [...state.devices, device] }));
        return true; // Indicate success
      },
      removeDevice: (deviceId: string) => {
        set((state) => ({
          devices: state.devices.filter((device) => device.id !== deviceId),
        }));
      },
      getDeviceList: () => {
        return get().devices; // Directly return the array
      },
      removeAll: () => {
        set({ devices: [] }); // Clear the array
      },
    }),
    {
      name: "device-storage",
      storage: createJSONStorage(() => SecureStorage), // Use the standard SecureStorage
    }
  )
);

export default useDeviceStore;
