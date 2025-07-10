import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";

export interface Device {
  color: any;
  deviceId: string;
  deviceName: string;
  connected: boolean;
  linkedProtocolId?: string;
  status?: string;
  medicineState?: number;
}

interface DeviceStore {
  devices: Device[];
  addDevice(device: Device): boolean;
  updateDevice(deviceId: string, data: Partial<Device>): void;
  getDevice(deviceId: string): Device | undefined;
  getDeviceList: () => Device[];
  removeDevice(deviceId: string): void;
  removeAllDevices(): void;
}

const useDeviceStore = create<DeviceStore>()(
  persist(
    (set, get) => ({
      devices: [],
      
      addDevice: (device) => {
        const currentDevices = get().devices;
        const idExists = currentDevices.some(
          (d) => d.deviceId === device.deviceId
        );
        const nameExists = currentDevices.some(
          (d) => d.deviceName === device.deviceName
        );

        if (idExists || nameExists) return false;

        set((state) => ({ devices: [...state.devices, device] }));
        return true;
      },

      updateDevice: (deviceId, data) => {
        set((state) => ({
          devices: state.devices.map((d) =>
            d.deviceId === deviceId ? { ...d, ...data } : d
          ),
        }));
      },

      getDevice: (deviceId) => {
        return get().devices.find((d) => d.deviceId === deviceId);
      },

      getDeviceList: () => get().devices,

      removeDevice: (deviceId) => {
        set((state) => ({
          devices: state.devices.filter((d) => d.deviceId !== deviceId),
        }));
      },

      removeAllDevices: () => {
        set({ devices: [] });
      },
    }),
    {
      name: "device-storage",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useDeviceStore;