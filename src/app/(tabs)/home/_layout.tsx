import { Stack } from "expo-router";

export default function HomeStack() {
  return (
    <Stack screenOptions={{ headerShown: false }}>
      <Stack.Screen name="auth/index" />
      <Stack.Screen name="pairing/index" />
      <Stack.Screen name="ble-debug/console/index" />
      <Stack.Screen name="ble-debug/index" />
      <Stack.Screen name="ble-debug/logs/index" />
      <Stack.Screen name="dashboard/index" />
      <Stack.Screen name="led-info" />
    </Stack>
  );
}
