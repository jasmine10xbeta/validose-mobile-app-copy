import { Stack } from "expo-router";

export default function HomeStack() {
  return (
    <Stack screenOptions={{ headerShown: false }}>
      <Stack.Screen name="auth/index" />
      <Stack.Screen name="pairing/index" />
      <Stack.Screen name="dashboard/index" />
      <Stack.Screen name="led-info" />
    </Stack>
  );
}
