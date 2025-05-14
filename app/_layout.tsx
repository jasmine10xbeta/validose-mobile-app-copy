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
import { VHeader } from "@/components/common/VHeader";
import { useColorScheme } from "@/hooks/useColorScheme";
import { useAuthStore } from "@/store/authStore";
import { AuthenticationProvider } from "@/utils/provider/AuthenticationProvider";
import { queryClient } from "@/utils/tanstackQuery/tanstackQuery";

SplashScreen.preventAutoHideAsync();

SplashScreen.setOptions({
  duration: 5000,
  fade: true,
});

export default function RootLayout() {
  const router = useRouter();
  const { isLoggedIn } = useAuthStore();
  const colorScheme = useColorScheme();
  const [loaded] = useFonts({
    Inter: require("../assets/fonts/Inter_28pt-Regular.ttf"),
  });

  useEffect(() => {
    if (loaded) {
      SplashScreen.hideAsync();
      if (isLoggedIn) {
        router.push("/dashboard");
      }
    }
  }, [loaded, router, isLoggedIn]);

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
