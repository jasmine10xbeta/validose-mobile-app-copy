import AsyncStorage from "@react-native-async-storage/async-storage";
import { create } from "zustand";
import { createJSONStorage, persist } from "zustand/middleware";

function parseBoolean(value: string | undefined): boolean {
  if (!value) return false;
  const normalized = value.trim().toLowerCase();
  return normalized === "1" || normalized === "true" || normalized === "yes";
}

const envEnabled = parseBoolean(
  process.env.ENABLE_BLE_BYPASS ?? process.env.EXPO_PUBLIC_ENABLE_BLE_BYPASS
);
const envDefaultMode = parseBoolean(
  process.env.BLE_BYPASS_MODE ?? process.env.EXPO_PUBLIC_BLE_BYPASS_MODE
);
const envBypassKey = (process.env.BLE_BYPASS_KEY ?? process.env.EXPO_PUBLIC_BLE_BYPASS_KEY ?? "")
  .trim()
  .toUpperCase();

interface DevStore {
  allowBleBypass: boolean;
  mockBleMode: boolean;
  bypassKey: string;
  setMockBleMode: (enabled: boolean) => void;
  enableMockBleMode: () => void;
  disableMockBleMode: () => void;
  matchesBypassKey: (input: string) => boolean;
  isMockBleModeEnabled: () => boolean;
}

function extractBypassCandidates(input: string): string[] {
  const raw = (input || "").trim();
  if (!raw) return [];

  const values = new Set<string>();
  values.add(raw);

  try {
    const parsed = JSON.parse(raw);
    if (typeof parsed === "string") values.add(parsed);
    if (parsed && typeof parsed === "object") {
      const obj = parsed as Record<string, unknown>;
      const jsonKeys = ["code", "key", "bypass_key", "onboarding_code", "value"];
      jsonKeys.forEach((k) => {
        const v = obj[k];
        if (typeof v === "string" && v.trim()) values.add(v.trim());
      });
    }
  } catch {
    // Non-JSON payload.
  }

  try {
    if (raw.includes("://") || raw.startsWith("http://") || raw.startsWith("https://")) {
      const url = new URL(raw);
      const queryKeys = ["code", "key", "bypass_key", "onboarding_code", "value"];
      queryKeys.forEach((k) => {
        const v = url.searchParams.get(k);
        if (v && v.trim()) values.add(v.trim());
      });
    }
  } catch {
    // Non-URL payload.
  }

  return Array.from(values);
}

const useDevStore = create<DevStore>()(
  persist(
    (set, get) => ({
      allowBleBypass: __DEV__ || envEnabled,
      mockBleMode: envDefaultMode,
      bypassKey: envBypassKey,
      setMockBleMode: (enabled) => set({ mockBleMode: enabled }),
      enableMockBleMode: () => set({ mockBleMode: true }),
      disableMockBleMode: () => set({ mockBleMode: false }),
      matchesBypassKey: (input) => {
        const candidates = extractBypassCandidates(input).map((v) =>
          v.trim().toUpperCase()
        );
        return (
          get().allowBleBypass &&
          get().bypassKey.length > 0 &&
          candidates.length > 0 &&
          candidates.includes(get().bypassKey)
        );
      },
      isMockBleModeEnabled: () => get().allowBleBypass && get().mockBleMode,
    }),
    {
      name: "dev-settings",
      storage: createJSONStorage(() => AsyncStorage),
      partialize: (state) => ({ mockBleMode: state.mockBleMode }),
    }
  )
);

export default useDevStore;
