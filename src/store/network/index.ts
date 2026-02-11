import NetInfo from "@react-native-community/netinfo";
import { create } from "zustand";

import useScheduleStore from "../schedule";

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
    const lastUpdated = useScheduleStore.getState().lastUpdated;  // TODO: Update this logic for backend dose event syncing instead of when the schedule store is updated
    if (!lastUpdated) return true;

    const diff = Date.now() - new Date(lastUpdated).getTime();
    return diff > 24 * 60 * 60 * 1000; // > 24h
  },
}));

export default useNetworkStore;
