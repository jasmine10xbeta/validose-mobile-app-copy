import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";

import { TreatmentProtocol } from "@/types/treatmentProtocol";
import { scheduleMedNotifications } from "@/utils/notifications/notificationScheduler";
import { getBackendProtocol } from "@/utils/protocol/mockProtocol";

const PROTOCOL_ID = "PROTOCOL_ID";

interface ProtocolStore {
  protocol: TreatmentProtocol | null;

  loadProtocol: () => Promise<TreatmentProtocol | null>;

  syncProtocol: () => Promise<{
    updated: boolean;
    protocol: TreatmentProtocol;
  }>;

  setProtocol: (protocol: TreatmentProtocol) => Promise<void>;
}

export const useProtocolStore = create<ProtocolStore>((set, get) => ({
  protocol: null,

  // Load from AsyncStorage
  loadProtocol: async () => {
    const data = await AsyncStorage.getItem(PROTOCOL_ID);
    if (data) {
      const protocol: TreatmentProtocol = JSON.parse(data);
      set({ protocol });
      return protocol;
    }
    return null;
  },

  // Fetch from backend, compare, store if newer
  syncProtocol: async () => {
    const local = get().protocol || await get().loadProtocol();
    const backend: TreatmentProtocol = await getBackendProtocol();

    if (!local || backend.version > local.version) {
      await AsyncStorage.setItem(PROTOCOL_ID, JSON.stringify(backend));
      set({ protocol: backend });

      // Optional side effect: trigger reschedule
      await scheduleMedNotifications(backend);

      return { updated: true, protocol: backend };
    }

    return { updated: false, protocol: local };
  },

  // Manually set and persist
  setProtocol: async (protocol: TreatmentProtocol) => {
    await AsyncStorage.setItem(PROTOCOL_ID, JSON.stringify(protocol));
    set({ protocol });
  },
}));