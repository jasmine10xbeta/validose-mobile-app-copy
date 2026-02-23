import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VReplacementProcessingOverlay } from "@/components/common/VReplacementProcessingOverlay";

import {
  doesMatchReplacementStep1Signal,
  REPLACEMENT_FLOW_SIGNAL,
} from "@/constants/replacementFlow";
import {
  subscribeToBleCharacteristic,
  writeReplacementProcessStopped,
} from "@/utils/ble";
import {
  buildReplacementErrorRoute,
  decodeReplacementErrorFromHex,
} from "@/utils/replacementError";

export default function ReplaceMedicationStepScreen() {
  const router = useRouter();
  const { startedAt } = useLocalSearchParams<{ startedAt?: string }>();
  const hasNavigatedRef = useRef(false);

  const startAtMs = useMemo(() => {
    const parsed = Number(startedAt);
    return Number.isFinite(parsed) ? parsed : Date.now();
  }, [startedAt]);
  const [remainingSeconds, setRemainingSeconds] = useState(() => {
    const elapsed = Math.floor((Date.now() - startAtMs) / 1000);
    return Math.max(0, 300 - elapsed);
  });
  const [isTransitionProcessing, setIsTransitionProcessing] = useState(false);

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

    async function watchReplacementSignal() {
      try {
        unsub = await subscribeToBleCharacteristic(
          REPLACEMENT_FLOW_SIGNAL.characteristicUuid,
          REPLACEMENT_FLOW_SIGNAL.serviceUuid,
          async ({ hex }) => {
            if (!active || hasNavigatedRef.current) return;
            const decodedError = decodeReplacementErrorFromHex(hex);
            if (decodedError) {
              hasNavigatedRef.current = true;
              router.replace(
                buildReplacementErrorRoute({
                  startedAtMs: startAtMs,
                  step: 1,
                  progress: 25,
                  title: decodedError.title,
                  message: decodedError.message,
                  unitHex: decodedError.unitHex,
                  reasonHex: decodedError.reasonHex,
                })
              );
              return;
            }
            if (!doesMatchReplacementStep1Signal(hex)) return;

            setIsTransitionProcessing(true);
            await waitForMinimumTransitionTime();
            if (!active || hasNavigatedRef.current) return;

            hasNavigatedRef.current = true;
            router.replace(
              `/home/dashboard/replace-medication-checking?startedAt=${startAtMs}`
            );
          }
        );
      } catch (error) {
        console.warn("[Replacement] Could not subscribe to step signal", error);
      }
    }

    watchReplacementSignal();

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
          progress: 25,
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
          <TouchableOpacity onPress={() => router.replace("/home/dashboard")} style={styles.cancelButton}>
            <Text style={styles.cancelText}>Cancel</Text>
          </TouchableOpacity>
          <View style={[styles.timerPill, isLastMinute && styles.timerPillCritical]}>
            <Text style={[styles.timerText, isLastMinute && styles.timerTextCritical]}>{timerLabel}</Text>
          </View>
        </View>

        <View style={styles.progressHeader}>
          <Text style={styles.progressLeft}>Step 1 of 4</Text>
          <Text style={styles.progressRight}>25%</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={styles.progressFill} />
        </View>

        <Text style={styles.title}>
          Remove the bottle{"\n"}
          from dock
        </Text>

        <View style={styles.imagePanel}>
          <View style={styles.ringBackLarge} />
          <View style={styles.ringBackMedium} />
          <View style={styles.ringBackSmall} />
          <View style={styles.dockBody}>
            <View style={styles.dockTopCap} />
            <View style={styles.dockRingOuter}>
              <View style={styles.dockRingInner} />
              <View style={styles.dockIndicator} />
            </View>
            <View style={styles.dockBottomRing} />
          </View>
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
    backgroundColor: "#EFEFF1",
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
    width: "21%",
    height: "100%",
    backgroundColor: "#2B7481",
    borderRadius: 999,
  },
  title: {
    marginTop: 58,
    marginBottom: 28,
    textAlign: "center",
    color: "#2D3745",
    fontSize: 24,
    lineHeight: 28,
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
  dockIndicator: {
    position: "absolute",
    width: 16,
    height: 8,
    borderRadius: 2,
    backgroundColor: "#F08A00",
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
