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
import { exportLogsToFile } from "@/utils/log";
import { formatHexTokensInText } from "./ble-debug/helpers";

export default function BleDebugLogsScreen() {
  const router = useRouter();
  const [logs, setLogs] = useState(getBleDebugLogs());

  useEffect(() => {
    return subscribeBleDebugLogs(() => {
      setLogs(getBleDebugLogs());
    });
  }, []);

  const orderedLogs = useMemo(() => [...logs].reverse(), [logs]);
  const visibleLogs = useMemo(
    () => orderedLogs.filter((entry) => !entry.message.toLowerCase().includes("firmware")),
    [orderedLogs]
  );
  const formattedVisibleLogs = useMemo(
    () =>
      visibleLogs.map((entry) => ({
        ...entry,
        formattedMessage: formatHexTokensInText(entry.message),
      })),
    [visibleLogs]
  );

  async function onShare() {
    await exportLogsToFile(formattedVisibleLogs.map((entry) => entry.formattedMessage));
  }

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.header}>
        <Pressable style={styles.headerButton} onPress={() => router.back()}>
          <Text style={styles.headerButtonText}>Back</Text>
        </Pressable>
        <Text style={styles.title}>BLE Logs</Text>
        <View style={styles.headerActions}>
          <Pressable style={styles.headerButton} onPress={onShare} disabled={!visibleLogs.length}>
            <Text style={styles.headerButtonText}>Share</Text>
          </Pressable>
          <Pressable
            style={styles.headerButton}
            onPress={() => clearBleDebugLogs()}
            disabled={!logs.length}
          >
            <Text style={styles.headerButtonText}>Clear</Text>
          </Pressable>
        </View>
      </View>

      <Text style={styles.subTitle}>
        {formattedVisibleLogs.length} entries
      </Text>

      <ScrollView contentContainerStyle={styles.logList}>
        {!formattedVisibleLogs.length ? (
          <Text style={styles.emptyText}>No logs yet.</Text>
        ) : (
          formattedVisibleLogs.map((entry) => (
            <View key={entry.id} style={styles.logCard}>
              <Text style={styles.logText} selectable>
                {entry.formattedMessage}
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
    marginBottom: 8,
  },
  headerActions: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
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
