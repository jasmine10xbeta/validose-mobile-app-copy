import { useRouter } from "expo-router";
import { useEffect, useMemo, useState } from "react";
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import {
  validoseAqua3,
  validoseDarkBlue,
  validoseGrey,
  validoseWhite,
} from "@/constants/colors";
import {
  clearBleDebugLogs,
  getBleDebugLogs,
  subscribeBleDebugLogs,
} from "@/utils/ble/debugLogStore";

export default function BleDebugLogsScreen() {
  const router = useRouter();
  const [logs, setLogs] = useState(getBleDebugLogs());

  useEffect(() => {
    return subscribeBleDebugLogs(() => {
      setLogs(getBleDebugLogs());
    });
  }, []);

  const orderedLogs = useMemo(() => [...logs].reverse(), [logs]);

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.header}>
        <Pressable style={styles.headerButton} onPress={() => router.back()}>
          <Text style={styles.headerButtonText}>Back</Text>
        </Pressable>
        <Text style={styles.title}>BLE Logs</Text>
        <Pressable
          style={styles.headerButton}
          onPress={() => clearBleDebugLogs()}
          disabled={!logs.length}
        >
          <Text style={styles.headerButtonText}>Clear</Text>
        </Pressable>
      </View>

      <Text style={styles.subTitle}>{logs.length} entries</Text>

      <ScrollView contentContainerStyle={styles.logList}>
        {!orderedLogs.length ? (
          <Text style={styles.emptyText}>No logs yet.</Text>
        ) : (
          orderedLogs.map((entry) => (
            <View key={entry.id} style={styles.logCard}>
              <Text style={styles.logText} selectable>
                {entry.message}
              </Text>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: validoseWhite,
    paddingHorizontal: 16,
    paddingBottom: 16,
  },
  header: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    marginBottom: 10,
  },
  title: {
    fontSize: 18,
    fontWeight: "700",
    color: validoseDarkBlue,
  },
  subTitle: {
    fontSize: 12,
    color: validoseGrey,
    marginBottom: 10,
  },
  headerButton: {
    borderWidth: 1,
    borderColor: validoseAqua3,
    borderRadius: 12,
    paddingHorizontal: 10,
    paddingVertical: 6,
    minWidth: 56,
    alignItems: "center",
    backgroundColor: validoseWhite,
  },
  headerButtonText: {
    fontSize: 12,
    fontWeight: "700",
    color: validoseAqua3,
  },
  logList: {
    gap: 8,
    paddingBottom: 20,
  },
  logCard: {
    borderWidth: 1,
    borderColor: "#E4E7EC",
    borderRadius: 10,
    backgroundColor: "#FAFBFC",
    padding: 10,
  },
  logText: {
    fontSize: 11,
    lineHeight: 16,
    color: validoseDarkBlue,
    fontFamily: "Courier",
  },
  emptyText: {
    fontSize: 12,
    color: validoseGrey,
  },
});

