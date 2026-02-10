import { Tabs } from "expo-router";

// Toggle this flag to quickly show/hide the bottom tab bar.
const SHOW_BOTTOM_TABS = false;

export default function TabsLayout() {
  return (
    <Tabs
      screenOptions={{
        headerShown: false,
        tabBarStyle: SHOW_BOTTOM_TABS ? undefined : { display: "none" },
      }}
    >
      <Tabs.Screen
        name="home"
        options={{
          title: "Home",
          tabBarIcon: () => null,
          tabBarLabelStyle: { fontSize: 14 },
        }}
      />
      <Tabs.Screen
        name="logs/index"
        options={{
          title: "Logs",
          tabBarIcon: () => null,
          tabBarLabelStyle: { fontSize: 14 },
        }}
      />
    </Tabs>
  );
}
