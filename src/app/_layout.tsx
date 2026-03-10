import {
  DarkTheme,
  DefaultTheme,
  ThemeProvider,
} from "@react-navigation/native";
import { useFonts } from "expo-font";
import * as Notifications from "expo-notifications";

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

import { showToast, toastConfig } from "@/components/common/VToast";
import { useAuth, AuthenticationProvider } from "@/providers/auth";
import { LogProvider } from "@/providers/log";
import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import { AllowedPaths } from "@/types/navigation";
import { connectAndSetupDevice } from "@/utils/ble";
import { refreshExpiringSchedules } from "@/utils/schedule";

SplashScreen.preventAutoHideAsync();
SplashScreen.setOptions({ fade: false });

Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowAlert: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});

function AppInitializer({ onReady }: { onReady: () => void }) {
  const { user, isLoading } = useAuth();                  // COMMENT WHILE DEBUGGING AUTH
  const router = useRouter();

  // const { user, isLoading, signOut } = useAuth();      // UNCOMMENT FOR DEBUGGING AUTH
  // const { removeAllDevices } = useDeviceStore();       // UNCOMMENT FOR DEBUGGING AUTH

  useEffect(() => {
    if (isLoading) return;

    const initializeApp = async () => {
      try {
        const isMockMode = useDevStore.getState().isMockBleModeEnabled();

        // removeAllDevices();                              // UNCOMMENT FOR DEBUGGING AUTH
        // signOut();                                       // UNCOMMENT FOR DEBUGGING AUTH

        // Return to auth screen if user is not signed in
        if (!user?.access_token && !isMockMode) {
          return redirectTo("/home/auth");
        }

        // Return to pairing screen if no devices are stored
        const storedDevices = useDeviceStore.getState().devices;
        if (!hasStoredDevices(storedDevices)) {
          return redirectTo("/home/pairing");
        }

        await connectToAllDevices(storedDevices);
        if (!isMockMode) {
          await refreshExpiringSchedules();
        }
        redirectTo(isMockMode ? "/home/dashboard" : "/home/pairing");
      } catch (e) {
        console.error("App initialization failed:", e);
        showToast("error", "App initialization failed", `${e}`);
        
        redirectTo("/home/auth");
      } finally {
        onReady();
      }
    };

    initializeApp();
  }, [isLoading]);

  const redirectTo = (path: AllowedPaths) => {
    console.log("Redirecting to", path);

    router.replace(path);
  };

  const hasStoredDevices = (devices: any[]) => {
    console.log(`Stored devices below..`, devices);

    if (!devices || devices.length === 0) {
      console.log("No stored devices..");
      return false;
    }

    return true;
  };

  // Connect to all devices and setup with subscriptions
  const connectToAllDevices = async (devices: any[]) => {
    console.log("Attempting to connect to all stored devices..");
    let allConnected = true;

    for (const device of devices) {
      const deviceId = device.deviceName;
      const connected = await connectAndSetupDevice(deviceId);

      if (connected.status === "error") allConnected = false;
    }

    return allConnected;
  };

  return null;
}

export default function RootLayout() {
  const colorScheme = useColorScheme();
  const [fontsLoaded] = useFonts({Inter: require("../assets/fonts/Inter_28pt-Regular.ttf")});
  
  const [appReady, setAppReady] = useState(false);
  const handleAppReady = () => setAppReady(true);

  useEffect(() => {
    if (fontsLoaded && appReady) SplashScreen.hideAsync();
  }, [fontsLoaded, appReady]);

  return (
    <SafeAreaProvider>
      <PaperProvider>
        <ThemeProvider value={colorScheme === "dark" ? DarkTheme : DefaultTheme}>
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
              <Toast config={toastConfig} />
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
