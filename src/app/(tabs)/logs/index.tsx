import { useFocusEffect } from "@react-navigation/native";
import { useNavigation } from "expo-router";
import { useEffect, useRef, useCallback } from "react";
import { ScrollView, Text, View, Button, Pressable } from "react-native";

import { useSafeAreaInsets } from "react-native-safe-area-context";
import { useLogs } from "@/providers/log";
import { exportLogsToFile } from "@/utils/log";

export default function LogsTab() {
  const logs = useLogs();
  const scrollRef = useRef<ScrollView>(null);
  const navigation = useNavigation();

  // Only show non-empty logs
  const filteredLogs = logs.filter((entry) => {
    if (entry === null || entry === undefined) return false;

    const message =
      typeof entry === "string"
        ? entry
        : typeof entry === "object" && "message" in entry
        ? String((entry as { message?: unknown }).message ?? "")
        : String(entry);

    return message.replace(/\s+/g, "") !== "";
  });

  // Scroll to bottom when logs change
  useEffect(() => {
    if (scrollRef.current && filteredLogs.length > 0) {
      setTimeout(() => {
        scrollRef.current?.scrollToEnd({ animated: true });
      }, 30);
    }
  }, [filteredLogs.length]);

  // Also scroll to bottom when tab is focused
  useFocusEffect(
    useCallback(() => {
      setTimeout(() => {
        scrollRef.current?.scrollToEnd({ animated: true });
      }, 30);
    }, [])
  );

  // Listen for tab press
  useEffect(() => {
    const unsubscribe = navigation.addListener("focus", () => {
      setTimeout(() => {
        scrollRef.current?.scrollToEnd({ animated: true });
      }, 30);
    });
    return unsubscribe;
  }, [navigation]);

  const handleExport = async () => {
    // await exportLogsToFile(filteredLogs);
    await exportLogsToFile(filteredLogs.map((log) => log.message));
  };

  const insets = useSafeAreaInsets();

  return (
    <View style={{ flex: 1, backgroundColor: "white" }}>
      <View style={{ position: "absolute", top: 50, right: 12, zIndex: 10 }}>
        {/* <Button title="Export Logs" onPress={handleExport} /> */}
        <Pressable
          onPress={handleExport}
          style={{
            backgroundColor: "white",
            paddingVertical: 8,
            paddingHorizontal: 16,
            borderRadius: 6,
            borderWidth: 1,
            borderColor: "#ccc",
            shadowColor: "#000",
            shadowOpacity: 0.1,
            shadowOffset: { width: 0, height: 2 },
            shadowRadius: 2,
            elevation: 2,
          }}
        >
          <Text style={{ color: "#252F3B", fontWeight: "600" }}>Export Logs</Text>
        </Pressable>
      </View>
      <ScrollView
        ref={scrollRef}
        style={{
          flex: 1,
          backgroundColor: "white",
          padding: 12,
          paddingTop: 100,
        }}
        // contentContainerStyle={{ paddingBottom: 32 }}
        contentContainerStyle={{
          paddingBottom: insets.bottom + 80, // Enough to stay above tabs
        }}
        keyboardShouldPersistTaps="handled"
      >
        {filteredLogs.length === 0 && (
          <Text style={{ color: "#999", textAlign: "center", marginTop: 24 }}>
            No logs yet.
          </Text>
        )}
        {filteredLogs.map((log, i) => (
          <View key={i} style={{ marginBottom: 4 }}>
            <View
              style={{
                backgroundColor: "#fff",
                borderRadius: 8,
                paddingVertical: 14,
                paddingHorizontal: 10,
                borderLeftWidth: 3,
                borderLeftColor: "#252F3B",
                shadowColor: "#000",
                shadowOpacity: 0.04,
                shadowOffset: { width: 0, height: 2 },
                shadowRadius: 2,
                elevation: 2,
              }}
            >
              {/* Timestamp */}
              <Text
                style={{
                  fontFamily: "Menlo",
                  fontSize: 12,
                  color: "#64748b",
                  marginBottom: 6,
                }}
              >
                [{log.timestamp}]
              </Text>

              {/* Message */}
              <Text
                selectable
                style={{
                  color: "#252F3B",
                  fontSize: 13,
                  fontFamily: "Menlo",
                }}
              >
                {log.message}
              </Text>
            </View>
          </View>
        ))}
      </ScrollView>
    </View>
  );
}
