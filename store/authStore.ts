import * as SecureStore from "expo-secure-store";
import { create } from "zustand";
import { StateStorage, persist, createJSONStorage } from "zustand/middleware";

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

interface AuthState {
  token: string | null;
  userId: string | null;
  isLoggedIn: boolean;
  doneLogging: boolean;
  setToken: (token: string | null) => void;
  setUserId: (userId: string | null) => void;
  logout: () => void;
}

export const useAuthStore = create<AuthState>()(
  persist(
    (set) => ({
      token: null,
      userId: null,
      isLoggedIn: false,
      doneLogging: false,
      setToken: (token: string | null) =>
        set({ token, isLoggedIn: !!token, doneLogging: true }),
      setUserId: (userId: string | null) => set({ userId }),
      logout: () => set({ userId: null, isLoggedIn: false, doneLogging: true }),
    }),
    {
      name: "auth-storage",
      storage: createJSONStorage(() => SecureStorage),
    }
  )
);
