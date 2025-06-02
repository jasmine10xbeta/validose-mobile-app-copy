import AsyncStorage from "@react-native-async-storage/async-storage";
import create from "zustand";

interface MedicationDeviceMapping {
  protocol_id: string;
  device_id: string;
  medication_name: string;
}

interface MappingStore {
  mappings: MedicationDeviceMapping[];
  loadMappings: () => Promise<void>;
  getMedicationForDevice: (device_id: string) => string | null;
  linkDeviceToMedication: (device_id: string, medication_name: string, protocol_id: string) => Promise<void>;
}

const STORAGE_KEY = "medication_device_mappings";

export const useMedicationDeviceMappingStore = create<MappingStore>((set, get) => ({
  mappings: [],

  loadMappings: async () => {
    const data = await AsyncStorage.getItem(STORAGE_KEY);
    if (data) set({ mappings: JSON.parse(data) });
  },

  getMedicationForDevice: (device_id: string) => {
    const match = get().mappings.find((m) => m.device_id === device_id);
    return match?.medication_name ?? null;
  },

  linkDeviceToMedication: async (device_id, medication_name, protocol_id) => {
    const existing = get().mappings.filter((m) => m.device_id !== device_id);
    const updated = [...existing, { device_id, medication_name, protocol_id }];
    await AsyncStorage.setItem(STORAGE_KEY, JSON.stringify(updated));
    set({ mappings: updated });
  },
}));