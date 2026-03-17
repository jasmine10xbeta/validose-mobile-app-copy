import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { ActivityIndicator, Image, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VReplacementProcessingOverlay } from "@/components/common/VReplacementProcessingOverlay";

import { doesMatchReplacementCheckingCompleteSignal } from "@/constants/replacementFlow";
import {
  subscribeToReplacementFlowSignal,
  writeReplacementProcessStopped,
} from "@/utils/ble";
import {
  buildReplacementErrorRoute,
  decodeReplacementErrorFromHex,
} from "@/utils/replacementError";

const CHECK_DOCK_IMAGE = require("../../../../assets/images/png/check-dock.png");

export default function ReplaceMedicationCheckingScreen() {
  const router = useRouter();
  const { startedAt } = useLocalSearchParams<{ startedAt?: string }>();
  const hasNavigatedRef = useRef(false);
  const [isCheckingComplete, setIsCheckingComplete] = useState(false);
  const [isTransitionProcessing, setIsTransitionProcessing] = useState(false);

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

  useEffect(() => {
    let unsub: (() => void) | null = null;
    let active = true;
    async function watchCheckingSignal() {
      try {
        unsub = await subscribeToReplacementFlowSignal(async (hex) => {
          if (!active || hasNavigatedRef.current) return;
          const decodedError = decodeReplacementErrorFromHex(hex);
          if (decodedError) {
            hasNavigatedRef.current = true;
            router.replace(
              buildReplacementErrorRoute({
                startedAtMs: startAtMs,
                step: 1,
                progress: 30,
                title: decodedError.title,
                message: decodedError.message,
                unitHex: decodedError.unitHex,
                reasonHex: decodedError.reasonHex,
              })
            );
            return;
          }
          if (!doesMatchReplacementCheckingCompleteSignal(hex)) return;

          setIsCheckingComplete(true);
          setIsTransitionProcessing(true);
          await waitForMinimumTransitionTime();
          if (!active || hasNavigatedRef.current) return;

          hasNavigatedRef.current = true;
          router.replace(
            `/home/dashboard/replace-medication-step-2?startedAt=${startAtMs}`
          );
        });
      } catch (error) {
        console.warn("[Replacement] Could not subscribe to checking signal", error);
      }
    }

    watchCheckingSignal();

    return () => {
      active = false;
      if (unsub) unsub();
    };
  }, [router, startAtMs]);

  useEffect(() => {
    if (remainingSeconds > 0 || hasNavigatedRef.current) return;

    hasNavigatedRef.current = true;
    (async () => {
      await writeReplacementProcessStopped();
      router.replace(
        buildReplacementErrorRoute({
          startedAtMs: startAtMs,
          step: 1,
          progress: 30,
          title: "Replacement Timed Out",
          message: "Replacement window expired. Please start again.",
          unitHex: "TIME",
          reasonHex: "0000",
        })
      );
    })();
  }, [remainingSeconds, router, startAtMs]);

  const timerLabel = formatTimer(remainingSeconds);
  const isLastMinute = remainingSeconds <= 60;

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <View style={styles.topRow}>
          <TouchableOpacity onPress={() => router.back()} style={styles.cancelButton}>
            <Text style={styles.cancelText}>Cancel</Text>
          </TouchableOpacity>
          <View style={[styles.timerPill, isLastMinute && styles.timerPillCritical]}>
            <Text style={[styles.timerText, isLastMinute && styles.timerTextCritical]}>{timerLabel}</Text>
          </View>
        </View>

        <View style={styles.progressHeader}>
          <Text style={styles.progressLeft}>Step 1 of 4</Text>
          <Text style={styles.progressRight}>30%</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={styles.progressFill} />
        </View>

        <Text style={styles.title}>Checking dock</Text>
        <View style={styles.connectingRow}>
          {isCheckingComplete ? (
            <>
              <View style={styles.completeBadge}>
                <Text style={styles.completeCheck}>✓</Text>
              </View>
              <Text style={styles.completeText}>Checking complete</Text>
            </>
          ) : (
            <>
              <ActivityIndicator size="small" color="#8A98A7" />
              <Text style={styles.connectingText}>Connecting to dock</Text>
            </>
          )}
        </View>

        <View style={styles.imagePanel}>
          <Image source={CHECK_DOCK_IMAGE} style={styles.stepImage} resizeMode="cover" />
        </View>

        <Text style={styles.helpText}>
          Keep the dock <Text style={styles.helpTextBold}>stable and on a flat surface</Text>
          {"\n"}
          during the entire process.
        </Text>

        <VReplacementProcessingOverlay
          visible={isTransitionProcessing}
          onReturnToChangingFlow={() => router.replace("/home/dashboard/replace-medication?restart=1")}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    marginTop: "9%",
    backgroundColor: "#FFFFFF",
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    overflow: "hidden",
    shadowColor: "#000000",
    shadowOpacity: 0.14,
    shadowRadius: 20,
    shadowOffset: { width: 0, height: -4 },
    elevation: 18,
  },
  container: {
    flex: 1,
    position: "relative",
    paddingHorizontal: 16,
    paddingTop: 8,
    paddingBottom: 24,
  },
  topRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  cancelButton: {
    paddingVertical: 8,
    paddingHorizontal: 8,
  },
  cancelText: {
    color: "#4D5A69",
    fontSize: 15,
    fontWeight: "400",
  },
  timerPill: {
    borderRadius: 10,
    backgroundColor: "#CEE2E6",
    paddingHorizontal: 14,
    paddingVertical: 8,
  },
  timerPillCritical: {
    backgroundColor: "#FDE3E3",
  },
  timerText: {
    color: "#2A6574",
    fontSize: 16,
    fontWeight: "500",
  },
  timerTextCritical: {
    color: "#C91F1F",
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
    width: "33%",
    height: "100%",
    backgroundColor: "#2B7481",
    borderRadius: 999,
  },
  title: {
    marginTop: 56,
    textAlign: "center",
    color: "#2D3745",
    fontSize: 40 / 2,
    lineHeight: 48 / 2,
    fontWeight: "700",
  },
  connectingRow: {
    marginTop: 8,
    marginBottom: 28,
    flexDirection: "row",
    justifyContent: "center",
    alignItems: "center",
    gap: 8,
  },
  connectingText: {
    color: "#556373",
    fontSize: 32 / 2,
    fontWeight: "500",
  },
  completeBadge: {
    width: 16,
    height: 16,
    borderRadius: 8,
    backgroundColor: "#2EA64A",
    alignItems: "center",
    justifyContent: "center",
  },
  completeCheck: {
    color: "#FFFFFF",
    fontSize: 11,
    fontWeight: "700",
    lineHeight: 12,
  },
  completeText: {
    color: "#2EA64A",
    fontSize: 16,
    fontWeight: "600",
  },
  imagePanel: {
    borderRadius: 12,
    overflow: "hidden",
    height: 355,
    backgroundColor: "#F4F5F6",
    justifyContent: "flex-end",
    alignItems: "center",
  },
  stepImage: {
    width: "100%",
    height: "100%",
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
    height: 160,
    backgroundColor: "#D5D9DE",
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderTopLeftRadius: 40,
    borderTopRightRadius: 40,
    borderBottomLeftRadius: 58,
    borderBottomRightRadius: 58,
    marginBottom: 28,
    alignItems: "center",
    justifyContent: "flex-start",
  },
  dockOpeningOuter: {
    marginTop: 14,
    width: 62,
    height: 38,
    borderRadius: 22,
    backgroundColor: "#F3F4F6",
    borderWidth: 1,
    borderColor: "#9AA5B3",
    alignItems: "center",
    justifyContent: "center",
  },
  dockOpeningInner: {
    width: 44,
    height: 26,
    borderRadius: 14,
    backgroundColor: "#8B97A7",
  },
  dockBottomRing: {
    position: "absolute",
    bottom: 20,
    width: 108,
    height: 24,
    borderRadius: 14,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    backgroundColor: "#D7DBDF",
  },
  helpText: {
    marginTop: 18,
    textAlign: "center",
    color: "#505A66",
    fontSize: 16.5,
    lineHeight: 22,
  },
  helpTextBold: {
    color: "#2D3745",
    fontWeight: "700",
  },
});

function formatTimer(totalSeconds: number): string {
  const mins = Math.floor(totalSeconds / 60);
  const secs = totalSeconds % 60;
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
}

async function waitForMinimumTransitionTime(minMs = 2000): Promise<void> {
  await new Promise((resolve) => setTimeout(resolve, minMs));
}
