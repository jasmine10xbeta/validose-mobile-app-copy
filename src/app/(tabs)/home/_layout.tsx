import { Stack } from "expo-router";

const popupModalOptions = {
  presentation: "transparentModal" as const,
  animation: "slide_from_bottom" as const,
  contentStyle: { backgroundColor: "transparent" },
};

const replacementModalOptions = {
  presentation: "transparentModal" as const,
  animation: "slide_from_bottom" as const,
  contentStyle: { backgroundColor: "rgba(55, 65, 81, 0.24)" },
};

export default function HomeStack() {
  return (
    <Stack screenOptions={{ headerShown: false }}>
      <Stack.Screen name="auth/index" />
      <Stack.Screen name="pairing/index" />
      <Stack.Screen
        name="pairing/manual-pairing"
        options={popupModalOptions}
      />
      <Stack.Screen name="ble-debug/console/index" />
      <Stack.Screen name="ble-debug/index" />
      <Stack.Screen name="ble-debug/logs/index" />
      <Stack.Screen name="dashboard/index" />
      <Stack.Screen
        name="dashboard/replace-medication"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-checking"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step-2"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step-3"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-step-4-checking"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-error"
        options={replacementModalOptions}
      />
      <Stack.Screen
        name="dashboard/replace-medication-success"
        options={replacementModalOptions}
      />
      <Stack.Screen name="led-info" options={popupModalOptions} />
      <Stack.Screen name="led-info-troubleshooting" options={popupModalOptions} />
    </Stack>
  );
}
