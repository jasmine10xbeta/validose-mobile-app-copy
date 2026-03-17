import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { Image, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VReplacementProcessingOverlay } from "@/components/common/VReplacementProcessingOverlay";

import { doesMatchReplacementStep2ToStep3Signal } from "@/constants/replacementFlow";
import {
  subscribeToReplacementFlowSignal,
  writeReplacementProcessStopped,
} from "@/utils/ble";
import {
  buildReplacementErrorRoute,
  decodeReplacementErrorFromHex,
} from "@/utils/replacementError";

const REMOVE_RING_IMAGE = require("../../../../assets/images/png/remove-ring.png");

export default function ReplaceMedicationStep2Screen() {
  const router = useRouter();
  const { startedAt } = useLocalSearchParams<{ startedAt?: string }>();
  const hasNavigatedRef = useRef(false);
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

    async function watchStep2Signal() {
      try {
        unsub = await subscribeToReplacementFlowSignal(async (hex) => {
          if (!active || hasNavigatedRef.current) return;
          const decodedError = decodeReplacementErrorFromHex(hex);
          if (decodedError) {
            hasNavigatedRef.current = true;
            router.replace(
              buildReplacementErrorRoute({
                startedAtMs: startAtMs,
                step: 2,
                progress: 50,
                title: decodedError.title,
                message: decodedError.message,
                unitHex: decodedError.unitHex,
                reasonHex: decodedError.reasonHex,
              })
            );
            return;
          }
          if (!doesMatchReplacementStep2ToStep3Signal(hex)) return;

          setIsTransitionProcessing(true);
          await waitForMinimumTransitionTime();
          if (!active || hasNavigatedRef.current) return;

          hasNavigatedRef.current = true;
          router.replace(
            `/home/dashboard/replace-medication-step-3?startedAt=${startAtMs}`
          );
        });
      } catch (error) {
        console.warn("[Replacement] Could not subscribe to step 2 signal", error);
      }
    }

    watchStep2Signal();

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
          step: 2,
          progress: 50,
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
          <Text style={styles.progressLeft}>Step 2 of 4</Text>
          <Text style={styles.progressRight}>50%</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={styles.progressFill} />
        </View>

        <Text style={styles.title}>
          Remove the ring from the{"\n"}
          eye-drop bottle
        </Text>

        <View style={styles.imagePanel}>
          <Image source={REMOVE_RING_IMAGE} style={styles.stepImage} resizeMode="cover" />
        </View>

        <Text style={styles.helpText}>
          Hold the bottle securely with one hand and <Text style={styles.helpTextBold}>pull
          {"\n"}the Ring upward</Text> toward the top of the bottle{"\n"}
          with the other hand.
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
    width: "50%",
    height: "100%",
    backgroundColor: "#2B7481",
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
  stepImage: {
    width: "100%",
    height: "100%",
  },
  ringBackLarge: {
    position: "absolute",
    width: 560,
    height: 220,
    borderRadius: 280,
    backgroundColor: "#F0ECE5",
    bottom: -20,
  },
  ringBackMedium: {
    position: "absolute",
    width: 420,
    height: 170,
    borderRadius: 220,
    backgroundColor: "#EFE2D1",
    bottom: 16,
  },
  ringBackSmall: {
    position: "absolute",
    width: 300,
    height: 140,
    borderRadius: 160,
    backgroundColor: "#ECDCC5",
    bottom: -16,
  },
  frameLayer: {
    ...StyleSheet.absoluteFillObject,
    justifyContent: "flex-end",
    alignItems: "center",
  },
  bottleBody: {
    width: 74,
    height: 104,
    borderWidth: 1,
    borderColor: "#9A6A20",
    borderBottomLeftRadius: 28,
    borderBottomRightRadius: 28,
    borderTopLeftRadius: 14,
    borderTopRightRadius: 14,
    backgroundColor: "#F2A022",
    marginBottom: 16,
  },
  bottleBodyFrameB: {
    width: 74,
    height: 104,
    borderWidth: 1,
    borderColor: "#9A6A20",
    borderBottomLeftRadius: 28,
    borderBottomRightRadius: 28,
    borderTopLeftRadius: 14,
    borderTopRightRadius: 14,
    backgroundColor: "#F2A022",
    marginBottom: 16,
  },
  neckFrameB: {
    position: "absolute",
    width: 44,
    height: 70,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 18,
    backgroundColor: "#F1F3F5",
    bottom: 96,
  },
  ringPiece: {
    position: "absolute",
    width: 150,
    height: 76,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 50,
    backgroundColor: "#F1F3F5",
    bottom: 74,
  },
  ringPieceFrameB: {
    position: "absolute",
    width: 150,
    height: 50,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 30,
    backgroundColor: "#F1F3F5",
    bottom: 228,
  },
  capTop: {
    position: "absolute",
    width: 74,
    height: 82,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 38,
    backgroundColor: "#F1F3F5",
    bottom: 124,
  },
  capTopFrameB: {
    position: "absolute",
    width: 74,
    height: 64,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 32,
    backgroundColor: "#F1F3F5",
    bottom: 108,
  },
  helpText: {
    marginTop: 18,
    textAlign: "center",
    color: "#4D5A69",
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
