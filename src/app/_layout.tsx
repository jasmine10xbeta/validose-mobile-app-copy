import {
  DarkTheme,
  DefaultTheme,
  ThemeProvider,
} from "@react-navigation/native";
import { useFonts } from "expo-font";
// import { Stack, useRouter, Slot } from "expo-router";    // COMMENT WHILE DEBUGGING
import { useRouter, Slot } from "expo-router";              // UNCOMMENT FOR DEBUGGING ONLY
import {} from "expo-router";
import * as SplashScreen from "expo-splash-screen";
import { StatusBar } from "expo-status-bar";
import { useEffect, useState } from "react";
import { useColorScheme } from "react-native";
import "react-native-reanimated";
import { PaperProvider } from "react-native-paper";
import { SafeAreaProvider } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";

import { showToast } from "@/components/common/VToast";
import { useAuth, AuthenticationProvider } from "@/providers/auth";
import { LogProvider } from "@/providers/log";
import useDeviceStore from "@/store/device";
import useTreatmentStore from "@/store/treatment";
import { AllowedPaths } from "@/types/navigation";
import { connectAndSetupDevice } from "@/utils/ble";
import { refreshExpiringSchedules } from "@/utils/schedule";

SplashScreen.preventAutoHideAsync();
SplashScreen.setOptions({ fade: false });

function AppInitializer({ onReady }: { onReady: () => void }) {
  const { user, isLoading, signOut } = useAuth();
  const { removeAllDevices } = useDeviceStore();
  const { treatments } = useTreatmentStore();

  const [hasInitialized, setHasInitialized] = useState(false);

  const router = useRouter();

  useEffect(() => {
    if (isLoading || hasInitialized) return;

    const initializeApp = async () => {
      try {
        // removeAllDevices();                              // UNCOMMENT FOR DEBUGGING
        // signOut();                                       // UNCOMMENT FOR DEBUGGING

        setHasInitialized(true);

        if (!user?.access_token) return redirectTo("/home/auth");

        const storedDevices = useDeviceStore.getState().devices;
        if (!hasStoredDevices(storedDevices)) return redirectTo("/home/pairing");

        const allConnected = await connectToAllDevices(storedDevices);
        await refreshExpiringSchedules();

        redirectTo(allConnected ? "/home/pairing" : "/home/pairing");
      } catch (e) {
        console.error("App initialization failed:", e);
        showToast("error", "App initialization failed", `${e}`);
        redirectTo("/home/auth");
      } finally {
        onReady();
      }
    };

    initializeApp();
  }, [isLoading, hasInitialized]);

  const redirectTo = (path: AllowedPaths) => {
    console.log("\n");
    console.log("Redirecting to", path);

    router.replace(path);
  };

  const hasStoredDevices = (devices: any[]) => {
    console.log("\n");
    console.log(`Stored devices below..`, devices);

    if (!devices || devices.length === 0) {
      console.log("No stored devices..");
      return false;
    }

    return true;
  };

  // Connect to all devices and setup with subscriptions
  const connectToAllDevices = async (devices: any[]) => {
    console.log("\n");
    console.log("Attempting to connect to all stored devices..");
    let allConnected = true;

    for (const device of devices) {
      const deviceId = device.deviceName;
      const connected = await connectAndSetupDevice(deviceId, treatments);

      if (connected.status === "error") allConnected = false;
      // await delay(500);
    }

    return allConnected;
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

  return (
    <SafeAreaProvider>
      <PaperProvider>
        <ThemeProvider
          value={colorScheme === "dark" ? DarkTheme : DefaultTheme}
        >
          <AuthenticationProvider>
            <LogProvider>
              <AppInitializer onReady={handleAppReady} />
              {/* COMMENT FOR DEBUG MODE
                  <Stack screenOptions={{ headerShown: false, gestureEnabled: false }}>
                    <Stack.Screen name="index" />
                    <Stack.Screen name="reconnect" />
                    <Stack.Screen name="pairing" />
                    <Stack.Screen name="manual-pairing" />
                    <Stack.Screen name="dashboard" />
                    <Stack.Screen name="+not-found" />
                  </Stack> */}
              <Slot />
              <StatusBar style="auto" />
              <Toast />
            </LogProvider>
          </AuthenticationProvider>

          {/* COMMENT FOR DEBUG MODE 
              <StatusBar style="auto" />
              <Toast /> */}
        </ThemeProvider>
      </PaperProvider>
    </SafeAreaProvider>
  );
}
