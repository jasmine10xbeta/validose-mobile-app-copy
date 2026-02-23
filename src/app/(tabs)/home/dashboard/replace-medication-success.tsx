import { Feather } from "@expo/vector-icons";
import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useState } from "react";
import { StyleSheet, Text, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";

export default function ReplaceMedicationSuccessScreen() {
  const router = useRouter();
  const { startedAt } = useLocalSearchParams<{ startedAt?: string }>();

  const startAtMs = useMemo(() => {
    const parsed = Number(startedAt);
    return Number.isFinite(parsed) ? parsed : Date.now();
  }, [startedAt]);
  const [remainingSeconds, setRemainingSeconds] = useState(() => {
    const elapsed = Math.floor((Date.now() - startAtMs) / 1000);
    return Math.max(0, 300 - elapsed);
  });

  useEffect(() => {
    const interval = setInterval(() => {
      const elapsed = Math.floor((Date.now() - startAtMs) / 1000);
      setRemainingSeconds(Math.max(0, 300 - elapsed));
    }, 1000);

    return () => clearInterval(interval);
  }, [startAtMs]);

  const timerLabel = formatTimer(remainingSeconds);

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <View style={styles.topRow}>
          <View style={styles.leftSpacer} />
          <View style={styles.timerPill}>
            <Text style={styles.timerText}>{timerLabel}</Text>
            <Feather name="rotate-ccw" size={20} color="#2A6574" />
          </View>
        </View>

        <View style={styles.progressHeader}>
          <Text style={styles.progressLeft}>Step 4 of 4</Text>
          <Text style={styles.progressRight}>100%</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={styles.progressFill} />
        </View>

        <Text style={styles.title}>
          Your new bottle has been{"\n"}
          installed and verified
        </Text>

        <View style={styles.imagePanel}>
          <View style={styles.ringBackLarge} />
          <View style={styles.ringBackMedium} />
          <View style={styles.ringBackSmall} />
          <View style={styles.dockBody}>
            <View style={styles.dockTopCap} />
            <View style={styles.dockRingOuter}>
              <View style={styles.dockRingInner} />
            </View>
            <View style={styles.dockBottomRing} />
          </View>
        </View>

        <Text style={styles.helpText}>
          Place the eye-drop bottle with the Ring{"\n"}
          attached on the Dock when not in use.
        </Text>

        <VButton
          label="Return to Dashboard"
          onPress={() => router.replace("/home/dashboard")}
          style={styles.primaryButton}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: "#EFEFF1",
  },
  container: {
    flex: 1,
    paddingHorizontal: 16,
    paddingTop: 8,
    paddingBottom: 24,
  },
  topRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  leftSpacer: {
    width: 54,
    height: 1,
  },
  timerPill: {
    borderRadius: 10,
    backgroundColor: "#CEE2E6",
    paddingHorizontal: 12,
    paddingVertical: 8,
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  timerText: {
    color: "#2A6574",
    fontSize: 15,
    fontWeight: "500",
  },
  progressHeader: {
    marginTop: 14,
    marginBottom: 6,
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  progressLeft: {
    color: "#4D5A69",
    fontSize: 17,
    fontWeight: "500",
  },
  progressRight: {
    color: "#4D5A69",
    fontSize: 17,
    fontWeight: "500",
  },
  progressTrack: {
    height: 9,
    backgroundColor: "#DCE2E8",
    borderRadius: 999,
    overflow: "hidden",
  },
  progressFill: {
    width: "100%",
    height: "100%",
    backgroundColor: "#65B77E",
    borderRadius: 999,
  },
  title: {
    marginTop: 56,
    marginBottom: 28,
    textAlign: "center",
    color: "#2D3745",
    fontSize: 24,
    lineHeight: 31,
    fontWeight: "700",
  },
  imagePanel: {
    borderRadius: 12,
    overflow: "hidden",
    height: 355,
    backgroundColor: "#F4F5F6",
    justifyContent: "flex-end",
    alignItems: "center",
  },
  ringBackLarge: {
    position: "absolute",
    width: 560,
    height: 220,
    borderRadius: 280,
    backgroundColor: "#E1ECEF",
    bottom: -20,
  },
  ringBackMedium: {
    position: "absolute",
    width: 420,
    height: 170,
    borderRadius: 220,
    backgroundColor: "#D3E6EB",
    bottom: 16,
  },
  ringBackSmall: {
    position: "absolute",
    width: 300,
    height: 140,
    borderRadius: 160,
    backgroundColor: "#C5DFE5",
    bottom: -16,
  },
  dockBody: {
    width: 124,
    height: 184,
    backgroundColor: "#D5D9DE",
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderTopLeftRadius: 44,
    borderTopRightRadius: 44,
    borderBottomLeftRadius: 58,
    borderBottomRightRadius: 58,
    marginBottom: 28,
    alignItems: "center",
    justifyContent: "flex-start",
    overflow: "visible",
  },
  dockTopCap: {
    position: "absolute",
    top: -16,
    width: 74,
    height: 36,
    borderRadius: 20,
    backgroundColor: "#ECEFF2",
    borderWidth: 1,
    borderColor: "#9AA5B3",
  },
  dockRingOuter: {
    marginTop: 28,
    width: 92,
    height: 22,
    borderRadius: 12,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    backgroundColor: "#F1F2F4",
    alignItems: "center",
    justifyContent: "center",
  },
  dockRingInner: {
    width: 80,
    height: 10,
    borderRadius: 6,
    backgroundColor: "#FFFFFF",
    borderWidth: 0.5,
    borderColor: "#BFC8D0",
  },
  dockBottomRing: {
    position: "absolute",
    bottom: 22,
    width: 108,
    height: 24,
    borderRadius: 14,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    backgroundColor: "#D7DBDF",
  },
  helpText: {
    marginTop: 18,
    marginBottom: 20,
    textAlign: "center",
    color: "#4D5A69",
    fontSize: 16.5,
    lineHeight: 22,
  },
  primaryButton: {
    width: "86%",
    alignSelf: "center",
    backgroundColor: "#286B78",
    borderColor: "#286B78",
  },
});

function formatTimer(totalSeconds: number): string {
  const mins = Math.floor(totalSeconds / 60);
  const secs = totalSeconds % 60;
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
}
