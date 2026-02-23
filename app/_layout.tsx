import {
  DarkTheme,
  DefaultTheme,
  ThemeProvider,
} from "@react-navigation/native";
import { QueryClientProvider } from "@tanstack/react-query";
import { useFonts } from "expo-font";
import { Stack, useRouter } from "expo-router";
import {} from "expo-router";
import * as SplashScreen from "expo-splash-screen";
import { StatusBar } from "expo-status-bar";
import { useEffect, useState } from "react";
import { Text, TextInput } from "react-native";
import "react-native-reanimated";
import { PaperProvider } from "react-native-paper";
import { SafeAreaProvider } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";

import { showToast } from "@/components/common/VToast";
import { useColorScheme } from "@/hooks/useColorScheme";
import useDeviceStore from "@/store/useDeviceStore";
import { customLog } from "@/utils/log/logManager";
import { AuthenticationProvider } from "@/utils/provider/AuthenticationProvider";
import { useAuth } from "@/utils/provider/AuthenticationProvider";
import { queryClient } from "@/utils/tanstackQuery/tanstackQuery";
import {
  scanLeDevice,
  connect
} from "../modules/tenx-mdk-ble-rn-library/src/index";
import "@/utils/log/logManager";

SplashScreen.preventAutoHideAsync();
SplashScreen.setOptions({
  fade: false,
});

type AllowedPaths =
  | "/"
  | "/reconnect"
  | "/pairing"
  | "/ble-debug"
  | "/dashboard"
  | `/dashboard?${string}`
  | `/dashboard#${string}`
  | `/?${string}`
  | `/#${string}`;


const waitForHydration = () =>
  new Promise<void>((resolve) => {
    const state = useDeviceStore.getState();
    if (state.hasHydrated) return resolve(); // already hydrated

    const unsub = useDeviceStore.subscribe((state) => {
      if (state.hasHydrated) {
        unsub();
        resolve();
      }
    });
  });

function AppInitializer({ onReady }: { onReady: () => void }) {
  const { user, isLoading, isSignedOut } = useAuth();
  const router = useRouter();
  const { updateDeviceById, removeAll } = useDeviceStore();

  useEffect(() => {
    if (isLoading) return;

    const initializeApp = async () => {
      try {
        await prepareBluetooth();
        await waitForHydration();

        if (isSignedOut) return redirectTo("/reconnect");

        if (!user?.token) return;

        const storedDevices = useDeviceStore.getState().devices;
        if (!hasStoredDevices(storedDevices)) return redirectTo("/pairing");

        const allConnected = await connectToAllDevices(storedDevices);
        redirectTo(allConnected ? "/dashboard" : "/pairing");
      } catch (e) {
        showToast("error", "App initialization failed", `${e}`);
        redirectTo("/");
      } finally {
        onReady();
      }
    };

    initializeApp();
  }, [isLoading, user]);

  // --- Helper Functions ---

  const prepareBluetooth = async () => {
    // customLog("\n");
    customLog("Scanning for devices..");
    await scanLeDevice(2);

    customLog("Scan complete");

    await delay(1000);
  };

  const redirectTo = (path: AllowedPaths) => {
    // customLog("\n");
    customLog("Redirecting to", path);

    router.replace(path as never);
  };

  const hasStoredDevices = (devices: any[]) => {
    // customLog("\n");
    customLog("Stored devices:", devices);

    if (!devices || devices.length === 0) {
      customLog("No stored devices");
      return false;
    }

    return true;
  };

  const connectToAllDevices = async (devices: any[]) => {
    // customLog("\n");
    customLog("Connecting to all devices..");
    let allConnected = true;

    for (const device of devices) {
      const connected = await connectToDevice(device.deviceId);

      if (!connected) allConnected = false;
      await delay(500);
    }

    return allConnected;
  };

  const connectToDevice = async (deviceId: string) => {
    try {
      const result = await connect(deviceId);
      const isConnected = !!result;

      // customLog("\n");
      customLog(`Connection status for ${deviceId}:`, isConnected);
      customLog("Updating device connection status in store now..");

      updateDeviceById(deviceId, {
        status: isConnected ? "Connected" : "Disconnected",
        medicineState: isConnected ? 0 : 6,
      });

      return isConnected;
    } catch (err) {
      showToast("error", `Failed to connect to device ${deviceId}`, `${err}`);
      updateDeviceById(deviceId, { status: "Disconnected", medicineState: 6 });
      return false;
    }
  };

  const delay = (ms: number) => new Promise((res) => setTimeout(res, ms));

  return null;
}

export default function RootLayout() {
  const colorScheme = useColorScheme();
  const [fontsLoaded] = useFonts({
    Inter: require("../assets/fonts/Inter_28pt-Regular.ttf"),
  });
  const [appReady, setAppReady] = useState(false);

  const handleAppReady = () => {
    setAppReady(true);
  };

  useEffect(() => {
    if (fontsLoaded && appReady) {
      SplashScreen.hideAsync();
    }
  }, [fontsLoaded, appReady]);

  useEffect(() => {
    const textDefaults = Text.defaultProps ?? {};
    const textStyleDefaults = textDefaults.style;
    Text.defaultProps = {
      ...textDefaults,
      style: [textStyleDefaults, { fontFamily: "Inter" }],
    };

    const inputDefaults = TextInput.defaultProps ?? {};
    const inputStyleDefaults = inputDefaults.style;
    TextInput.defaultProps = {
      ...inputDefaults,
      style: [inputStyleDefaults, { fontFamily: "Inter" }],
    };
  }, []);

  return (
    <SafeAreaProvider>
      <QueryClientProvider client={queryClient}>
        <PaperProvider>
          <ThemeProvider value={colorScheme === "dark" ? DarkTheme : DefaultTheme} >
            <AuthenticationProvider>
              <AppInitializer onReady={handleAppReady} />
              <Stack>
                <Stack.Screen name="index" options={{ headerShown: false }} />
                <Stack.Screen name="reconnect" options={{ headerShown: false }} />
                <Stack.Screen name="pairing" options={{ headerShown: false }} />
                <Stack.Screen name="ble-debug" options={{ headerShown: false }} />
                <Stack.Screen name="dashboard" options={{ headerShown: false }} />
                <Stack.Screen name="+not-found" options={{ headerShown: false }} />
              </Stack>
            </AuthenticationProvider>

            <StatusBar style="auto" />
            <Toast />
          </ThemeProvider>
        </PaperProvider>
      </QueryClientProvider>
    </SafeAreaProvider>
  );
}
