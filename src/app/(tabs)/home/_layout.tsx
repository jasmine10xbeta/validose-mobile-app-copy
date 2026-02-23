import { Stack } from "expo-router";

export default function HomeStack() {
  return (
    <Stack screenOptions={{ headerShown: false }}>
      <Stack.Screen name="auth/index" />
      <Stack.Screen name="pairing/index" />
      <Stack.Screen name="ble-debug" />
      <Stack.Screen name="dashboard/index" />
      <Stack.Screen
        name="dashboard/replace-medication"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-checking"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step-2"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step-3"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step-4-checking"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-error"
        options={{ animation: "fade" }}
      />
      <Stack.Screen
        name="dashboard/replace-medication-success"
        options={{ animation: "fade" }}
      />
      <Stack.Screen name="led-info" />
      <Stack.Screen name="led-info-troubleshooting" />
    </Stack>
  );
}
