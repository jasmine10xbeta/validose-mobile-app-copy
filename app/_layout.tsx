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
import "react-native-reanimated";
import { PaperProvider } from "react-native-paper";
import { SafeAreaProvider } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";

import { showToast } from "@/components/common/Toast";
import { useColorScheme } from "@/hooks/useColorScheme";
import useDeviceStore from "@/store/useDeviceStore";
import { AuthenticationProvider } from "@/utils/provider/AuthenticationProvider";
import { useAuth } from "@/utils/provider/AuthenticationProvider";
import { queryClient } from "@/utils/tanstackQuery/tanstackQuery";
import {
  scanLeDevice,
  bondDevice,
} from "../modules/tenx-mdk-ble-rn-library/src/index";

SplashScreen.preventAutoHideAsync();
SplashScreen.setOptions({
  fade: false,
});

function AppInitializer({ onReady }: { onReady: () => void }) {
  const { user, isLoading, isSignedOut } = useAuth();
  const router = useRouter();
  const { updateDeviceById } = useDeviceStore();

  useEffect(() => {
    const initializeApp = async () => {
      if (isLoading) return;

      try {
        await scanLeDevice(2);

        if (isSignedOut) {
          router.replace("/reconnect");
          return;
        }

        if (!user?.token) {
          return;
        }

        const storedDevices = useDeviceStore.getState().devices;

        if (!storedDevices || storedDevices.length === 0) {
          router.replace("/pairing");
          return;
        }

        let allConnected = true;
        await new Promise((res) => setTimeout(res, 1000));

        for (const device of storedDevices) {
          try {
            const result = await bondDevice(device.deviceId);
            const isConnected = !!result;

            updateDeviceById(
              device.deviceId,
              { status: isConnected ? "Connected" : "Disconnected", medicineState: isConnected ? 0 : 6 },
            );
            if (!isConnected) allConnected = false;
          } catch (err) {
            showToast("error", `Failed to connect to device ${device.deviceId}`, `${err}`);
            updateDeviceById(device.deviceId, { status: "Disconnected", medicineState: 6 });
            allConnected = false;
          }

          await new Promise((res) => setTimeout(res, 500));
        }

        router.replace(allConnected ? "/dashboard" : "/pairing");
      } catch (e) {
        showToast("error", "App initialization failed", `${e}`);
        router.replace("/");
      } finally {
        onReady();
      }
    };

    initializeApp();
  }, [user, isLoading, isSignedOut]);

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
      <QueryClientProvider client={queryClient}>
        <PaperProvider>
          <ThemeProvider
            value={colorScheme === "dark" ? DarkTheme : DefaultTheme}
          >
            <AuthenticationProvider>
              <AppInitializer onReady={handleAppReady} />
              <Stack>
                <Stack.Screen name="index" options={{ headerShown: false }} />
                <Stack.Screen
                  name="reconnect"
                  options={{ headerShown: false }}
                />
                <Stack.Screen name="pairing" options={{ headerShown: false }} />
                <Stack.Screen
                  name="dashboard"
                  options={{ headerShown: false }}
                />
                <Stack.Screen
                  name="+not-found"
                  options={{ headerShown: false }}
                />
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
