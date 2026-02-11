import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";

export interface TreatmentProtocol {
  protocolId: string;
  regimenId: string;
  deviceId: string;
  medicine: string;
  medicationName: string;
  indicationCode: string;
  dosageAmount: number;
  administrationDays: string[];
  administrationTimesMin: number[];
  frequencyCount: number;
  dosingWindowMin: number;
  notes?: string;
  active: boolean;
}

interface ProtocolStore {
  current: Record<string, TreatmentProtocol>; // deviceId → protocol
  history: TreatmentProtocol[];
  setProtocol(deviceId: string, protocol: TreatmentProtocol): void;
  getProtocol(deviceId: string): TreatmentProtocol | undefined;
}

const useTreatmentProtocolStore = create<ProtocolStore>()(
  persist(
    (set, get) => ({
      current: {},
      history: [],

      setProtocol: (deviceId, protocol) => {
        const prev = get().current[deviceId];
        if (prev) {
          set((state) => ({ history: [...state.history, prev] }));
        }
        set((state) => ({
          current: { ...state.current, [deviceId]: protocol },
        }));
      },

      getProtocol: (deviceId) => get().current[deviceId],
    }),
    {
      name: "treatment-protocol-storage",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useTreatmentProtocolStore;