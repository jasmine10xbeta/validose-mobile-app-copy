import AsyncStorage from "@react-native-async-storage/async-storage";
import {
  DarkTheme,
  DefaultTheme,
  ThemeProvider,
} from "@react-navigation/native";
import { QueryClientProvider } from "@tanstack/react-query";
import { useFonts } from "expo-font";
import { Stack, useRouter } from "expo-router";
import * as SplashScreen from "expo-splash-screen";
import { StatusBar } from "expo-status-bar";
import { useEffect } from "react";
import "react-native-reanimated";
import { PaperProvider } from "react-native-paper";
import { SafeAreaProvider } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";

import { useColorScheme } from "@/hooks/useColorScheme";
import useDeviceStore from "@/store/useDeviceStore";
import { AuthenticationProvider } from "@/utils/provider/AuthenticationProvider";
import { queryClient } from "@/utils/tanstackQuery/tanstackQuery";
import {
  scanLeDevice,
  bondDevice,
} from "../modules/tenx-mdk-ble-rn-library/src/index";

SplashScreen.preventAutoHideAsync();

SplashScreen.setOptions({
  duration: 5000,
  fade: true,
});

export default function RootLayout() {
  const router = useRouter();
  const colorScheme = useColorScheme();
  const { updateDeviceStatus } = useDeviceStore();
  const [loaded] = useFonts({
    Inter: require("../assets/fonts/Inter_28pt-Regular.ttf"),
  });

  useEffect(() => {
    const initializeApp = async () => {
      if (!loaded) return;

      await scanLeDevice(2);
      const userId = await AsyncStorage.getItem("userId");

      if (!userId) {
        router.replace("/");
        await SplashScreen.hideAsync();
        return;
      }

      // TODO: Check if token exists and is valid

      const storedDevices = useDeviceStore.getState().devices;
      if (!storedDevices || storedDevices.length === 0) {
        router.replace("/pairing");
        await SplashScreen.hideAsync();
        return;
      }

      let allConnected = true;
      await new Promise((res) => setTimeout(res, 1000));

      for (const device of storedDevices) {
        try {
          const result = await bondDevice(device.id);
          const isConnected = !!result;

          updateDeviceStatus(
            device.id,
            isConnected ? "Connected" : "Disconnected"
          );
          if (!isConnected) allConnected = false;
        } catch (err) {
          console.log("Failed to connect to device:", device.id, err);
          updateDeviceStatus(device.id, "Disconnected");
          allConnected = false;
        }

        await new Promise((res) => setTimeout(res, 500));
      }

      if (allConnected) {
        router.replace("/dashboard");
      } else {
        router.replace("/pairing");
      }
      await SplashScreen.hideAsync();
    };

    initializeApp();
  }, [loaded, router, updateDeviceStatus]);

  if (!loaded) {
    return null;
  }

  return (
    <SafeAreaProvider>
      <QueryClientProvider client={queryClient}>
        <PaperProvider>
          <ThemeProvider
            value={colorScheme === "dark" ? DarkTheme : DefaultTheme}
          >
            <AuthenticationProvider>
              <Stack>
                <Stack.Screen name="index" options={{ headerShown: false }} />
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
