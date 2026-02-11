// File: store/treatment.ts
import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";
import { Treatment } from "@/types/treatment";

interface TreatmentStore {
  treatments: Record<string, Treatment> | null; // treatment_id => Treatment
  storeTreatment: (treatment: Treatment) => void;
  storeTreatments: (treatments: Treatment[]) => void;
  getTreatmentById: (treatmentId: string) => Treatment | undefined;
  getDeviceTreatmentId: (deviceId: string) => string | undefined;
  getDeviceTreatment: (deviceId: string) => Treatment | undefined;
  getScheduleUpdatedAt: (treatmentId: string) => string | undefined;
  clearTreatments: () => void;
}

const useTreatmentStore = create<TreatmentStore>()(
  persist(
    (set, get) => ({
      treatments: {},

      storeTreatments: (treatmentList) => {
        if (!Array.isArray(treatmentList)) {
          console.warn("treatmentList is not an array", treatmentList);
          return;
        }

        const treatmentMap = treatmentList.reduce(
          (acc, treatment) => {
            if (!treatment?.id) {
              console.warn("Treatment missing id", treatment);
              return acc;
            }
            acc[treatment.id] = treatment;
            return acc;
          },
          {} as Record<string, Treatment>
        );

        set({
          treatments: {
            ...get().treatments,
            ...treatmentMap,
          },
        });
      },

      storeTreatment: (treatment: Treatment) => {
        set((state) => ({
          treatments: {
            ...state.treatments,
            [treatment.id]: treatment,
          },
        }));
      },

      getTreatmentById: (treatmentId) => {
        return get().treatments?.[treatmentId];
      },

      getDeviceTreatmentId: (deviceId) => {
        const entry =
          get().treatments?.[deviceId] ??
          Object.values(get().treatments ?? {}).find(
            (t) => t.device_id === deviceId
          );

        return entry?.id;
      },

      getDeviceTreatment: (deviceId) => {
        const entry =
          get().treatments?.[deviceId] ??
          Object.values(get().treatments ?? {}).find(
            (t) => t.device_id === deviceId
          );

        return entry;
      },

      getScheduleUpdatedAt: (treatmentId) => {
        return get().treatments?.[treatmentId]?.schedule_updated_at;
      },

      clearTreatments: () => set({ treatments: {} }),
    }),
    {
      name: "treatment-store",
      storage: createJSONStorage(() => AsyncStorage),
    }
  )
);

export default useTreatmentStore;
