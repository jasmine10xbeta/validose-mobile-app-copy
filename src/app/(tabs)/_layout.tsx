import { Tabs } from "expo-router";

export default function TabsLayout() {
  return (
    <Tabs screenOptions={{ headerShown: false }}>
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
