import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";
import { AuthorizedDevice, ValidoseDevice } from "@/types/device";

interface DeviceStore {
  // Authorized Validose devices for this user
  authorizedDevices: AuthorizedDevice[];
  setAuthorizedDevices: (list: AuthorizedDevice[]) => void;
  clearAuthorizedDevices(): void;

  // Connected/known validose devices for this phone
  devices: ValidoseDevice[];
  addDevice(device: ValidoseDevice): boolean;
  updateDevice(deviceId: string, data: Partial<ValidoseDevice>): void;
  getDevice(deviceId: string): ValidoseDevice | undefined;
  getDeviceList: () => ValidoseDevice[];
  removeDevice(deviceId: string): void;
  removeAllDevices(): void;  
}

const useDeviceStore = create<DeviceStore>()(
  persist(
    (set, get) => ({
      authorizedDevices: [],
      setAuthorizedDevices: (list) => set({ authorizedDevices: list }),
      clearAuthorizedDevices: () => set({ authorizedDevices: [] }),
      
      devices: [],
      addDevice: (device) => {
        console.log("\n");
        console.log(`Attempting to add the device below to the store..`, device);

        const currentDevices = get().devices;
        const idExists = currentDevices.some((d) => d.deviceId === device.deviceId);
        const nameExists = currentDevices.some((d) => d.deviceName === device.deviceName);

        if (idExists || nameExists) return false;

        set((state) => ({ devices: [...state.devices, device] }));
        return true;
      },
      updateDevice: (deviceId, data) => {
        console.log("\n");
        console.log(`Attempting to update the device id ${deviceId} in the store..`);
        console.log(data);

        set((state) => ({
          devices: state.devices.map((d) =>
            d.deviceId === deviceId ? { ...d, ...data } : d
          ),
        }));
      },
      getDevice: (deviceId) => get().devices.find((d) => d.deviceId === deviceId),
      getDeviceList: () => get().devices,
      removeDevice: (deviceId) => {
        set((state) => ({
          devices: state.devices.filter((d) => d.deviceId !== deviceId),
        }));
      },
      removeAllDevices: () => set({ devices: [] })
    }),
    {
      name: "device-storage",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useDeviceStore;
