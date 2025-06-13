// store/useNetworkStore.ts
import NetInfo from "@react-native-community/netinfo";
import { create } from "zustand";
import useDoseHistoryStore from "./useDoseHistoryStore";

interface NetworkStore {
  isConnected: boolean;
  lastDisconnectedAt: number | null;
  initialize: () => void;
  isStaleSync: () => boolean;
}

const useNetworkStore = create<NetworkStore>((set) => ({
  isConnected: true,
  lastDisconnectedAt: null,

  initialize: () => {
    NetInfo.addEventListener((state) => {
      const connected = !!state.isConnected;
      set((prev) => ({
        isConnected: connected,
        lastDisconnectedAt: connected ? null : prev.lastDisconnectedAt ?? Date.now(),
      }));
    });
  },

  isStaleSync: () => {
    const lastSyncedAt = useDoseHistoryStore.getState().lastSyncedAt;
    if (!lastSyncedAt) return true;

    const diff = Date.now() - new Date(lastSyncedAt).getTime();
    return diff > 24 * 60 * 60 * 1000; // > 24h
  },
}));

export default useNetworkStore;