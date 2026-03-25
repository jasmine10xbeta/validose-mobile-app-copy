import {
  DarkTheme,
  DefaultTheme,
  ThemeProvider,
  type Theme,
} from "@react-navigation/native";
import { useFonts } from "expo-font";
import * as Notifications from "expo-notifications";
import { Slot, useRootNavigationState, useRouter } from "expo-router";
import * as SplashScreen from "expo-splash-screen";
import { StatusBar } from "expo-status-bar";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import "react-native-reanimated";
import { useColorScheme, View } from "react-native";
import { PaperProvider } from "react-native-paper";
import { SafeAreaProvider } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";

// import { Stack, useRouter, Slot } from "expo-router";    // COMMENT WHILE DEBUGGING ONLY

import { VBuildInfoBadge } from "@/components/common/VBuildInfoBadge";
import { showToast, toastConfig } from "@/components/common/VToast";
import { validoseWhite } from "@/constants/colors";
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

function AppInitializer({
  onReady,
  navigationReady,
}: {
  onReady: () => void;
  navigationReady: boolean;
}) {
  const { user, isLoading } = useAuth();                  // COMMENT WHILE DEBUGGING AUTH
  const router = useRouter();
  const hasInitializedRef = useRef(false);

  // const { user, isLoading, signOut } = useAuth();      // UNCOMMENT FOR DEBUGGING AUTH
  // const { removeAllDevices } = useDeviceStore();       // UNCOMMENT FOR DEBUGGING AUTH

  const redirectTo = useCallback((path: AllowedPaths) => {
    console.log("Redirecting to", path);
    router.replace(path);
  }, [router]);

  useEffect(() => {
    if (!navigationReady || isLoading || hasInitializedRef.current) return;
    hasInitializedRef.current = true;

    let isCancelled = false;

    const runBackgroundBootstrap = async (
      devices: any[],
      isMockMode: boolean
    ) => {
      try {
        await connectToAllDevices(devices);
        if (!isMockMode) {
          await refreshExpiringSchedules();
        }
      } catch (error) {
        if (!isCancelled) {
          console.error("Background app bootstrap failed:", error);
        }
      }
    };

    try {
      const isMockMode = useDevStore.getState().isMockBleModeEnabled();

      // removeAllDevices();                              // UNCOMMENT FOR DEBUGGING AUTH
      // signOut();                                       // UNCOMMENT FOR DEBUGGING AUTH

      // Return to auth screen if user is not signed in
      if (!user?.access_token && !isMockMode) {
        redirectTo("/home/auth");
        return;
      }

      // Return to pairing screen if no devices are stored
      const storedDevices = useDeviceStore.getState().devices;
      if (!hasStoredDevices(storedDevices)) {
        redirectTo("/home/pairing");
        return;
      }

      redirectTo(isMockMode ? "/home/dashboard" : "/home/pairing");
      void runBackgroundBootstrap(storedDevices, isMockMode);
    } catch (e) {
      console.error("App initialization failed:", e);
      showToast("error", "App initialization failed", `${e}`);
      redirectTo("/home/auth");
    } finally {
      onReady();
    }

    return () => {
      isCancelled = true;
    };
  }, [isLoading, navigationReady, onReady, redirectTo, user?.access_token]);

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
      const deviceIdentifier = device.deviceId || device.deviceName;
      const connected = await connectAndSetupDevice(deviceIdentifier);

      if (connected.status === "error") allConnected = false;
    }

    return allConnected;
  };

  return null;
}

export default function RootLayout() {
  const colorScheme = useColorScheme();
  const [fontsLoaded] = useFonts({Inter: require("../assets/fonts/Inter_28pt-Regular.ttf")});
  const rootNavigationState = useRootNavigationState();
  const navigationReady = Boolean(rootNavigationState?.key);
  
  const [appReady, setAppReady] = useState(false);
  const [splashHidden, setSplashHidden] = useState(false);
  const handleAppReady = () => setAppReady(true);
  const navigationTheme = useMemo<Theme>(() => {
    const baseTheme = colorScheme === "dark" ? DarkTheme : DefaultTheme;
    return {
      ...baseTheme,
      colors: {
        ...baseTheme.colors,
        background: validoseWhite,
        card: validoseWhite,
      },
    };
  }, [colorScheme]);

  useEffect(() => {
    if (!fontsLoaded || !appReady || !navigationReady || splashHidden) return;

    let cancelled = false;

    const hideSplash = async () => {
      try {
        // Let the initial replaced route commit before hiding native splash.
        await new Promise((resolve) => requestAnimationFrame(() => resolve(null)));
        if (cancelled) return;
        await SplashScreen.hideAsync();
        if (cancelled) return;

        // Keep an app-owned white cover for an extra frame window to prevent
        // any transient black frame while the first route finalizes layout.
        await new Promise((resolve) => requestAnimationFrame(() => resolve(null)));
        await new Promise((resolve) => requestAnimationFrame(() => resolve(null)));
        await new Promise((resolve) => setTimeout(resolve, 80));
        if (cancelled) return;
        setSplashHidden(true);
      } catch (error) {
        console.warn("Failed to hide splash screen:", error);
      }
    };

    void hideSplash();

    return () => {
      cancelled = true;
    };
  }, [fontsLoaded, appReady, navigationReady, splashHidden]);

  return (
    <SafeAreaProvider>
      <PaperProvider>
        <ThemeProvider value={navigationTheme}>
          <AuthenticationProvider>
            <LogProvider>
              <View style={{ flex: 1, backgroundColor: validoseWhite }}>
                <AppInitializer onReady={handleAppReady} navigationReady={navigationReady} />
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
                <VBuildInfoBadge />
                <StatusBar style="auto" />
                <Toast config={toastConfig} />
                {!splashHidden ? (
                  <View
                    pointerEvents="none"
                    style={{
                      position: "absolute",
                      top: 0,
                      right: 0,
                      bottom: 0,
                      left: 0,
                      backgroundColor: validoseWhite,
                    }}
                  />
                ) : null}
              </View>
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
