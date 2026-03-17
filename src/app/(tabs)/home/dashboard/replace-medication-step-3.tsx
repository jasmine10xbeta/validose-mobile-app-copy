import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { Animated, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VReplacementProcessingOverlay } from "@/components/common/VReplacementProcessingOverlay";
import { doesMatchReplacementStep3ToStep4CheckingSignal } from "@/constants/replacementFlow";
import {
  subscribeToReplacementFlowSignal,
  writeReplacementProcessStopped,
} from "@/utils/ble";
import {
  buildReplacementErrorRoute,
  decodeReplacementErrorFromHex,
} from "@/utils/replacementError";

const ASSEMBLE_RING_IMAGE = require("../../../../assets/images/png/assemble-ring.png");
const ATTACH_NEW_BOTTLE_IMAGE = require("../../../../assets/images/png/attach-new-bottle.png");

export default function ReplaceMedicationStep3Screen() {
  const router = useRouter();
  const { startedAt } = useLocalSearchParams<{ startedAt?: string }>();
  const animationPhase = useRef(new Animated.Value(0)).current;
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
    const animation = Animated.loop(
      Animated.sequence([
        Animated.timing(animationPhase, {
          toValue: 1,
          duration: 1000,
          useNativeDriver: true,
        }),
        Animated.delay(220),
        Animated.timing(animationPhase, {
          toValue: 0,
          duration: 1000,
          useNativeDriver: true,
        }),
        Animated.delay(220),
      ])
    );

    animation.start();

    return () => animation.stop();
  }, [animationPhase]);

  useEffect(() => {
    let unsub: (() => void) | null = null;
    let active = true;

    async function watchStep3Signal() {
      try {
        unsub = await subscribeToReplacementFlowSignal(async (hex) => {
          if (!active || hasNavigatedRef.current) return;
          const decodedError = decodeReplacementErrorFromHex(hex);
          if (decodedError) {
            hasNavigatedRef.current = true;
            router.replace(
              buildReplacementErrorRoute({
                startedAtMs: startAtMs,
                step: 3,
                progress: 70,
                title: decodedError.title,
                message: decodedError.message,
                unitHex: decodedError.unitHex,
                reasonHex: decodedError.reasonHex,
              })
            );
            return;
          }
          if (!doesMatchReplacementStep3ToStep4CheckingSignal(hex)) return;

          setIsTransitionProcessing(true);
          await waitForMinimumTransitionTime();
          if (!active || hasNavigatedRef.current) return;

          hasNavigatedRef.current = true;
          router.replace(
            `/home/dashboard/replace-medication-step-4-checking?startedAt=${startAtMs}`
          );
        });
      } catch (error) {
        console.warn("[Replacement] Could not subscribe to step 3 signal", error);
      }
    }

    watchStep3Signal();

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
          step: 3,
          progress: 70,
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
  const attachOpacity = animationPhase.interpolate({
    inputRange: [0, 1],
    outputRange: [1, 0.38],
  });
  const assembleOpacity = animationPhase.interpolate({
    inputRange: [0, 1],
    outputRange: [0.18, 0.92],
  });

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
          <Text style={styles.progressLeft}>Step 3 of 4</Text>
          <Text style={styles.progressRight}>70%</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={styles.progressFill} />
        </View>

        <Text style={styles.title}>
          Attaching the Ring to{"\n"}
          new Bottle
        </Text>

        <View style={styles.imagePanel}>
          <Animated.Image
            source={ATTACH_NEW_BOTTLE_IMAGE}
            style={[styles.stepImageFrame, { opacity: attachOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={ASSEMBLE_RING_IMAGE}
            style={[styles.stepImageFrame, { opacity: assembleOpacity }]}
            resizeMode="cover"
          />
        </View>

        <Text style={styles.helpText}>
          Position the Ring over the top of the bottle.{"\n"}
          <Text style={styles.helpTextBold}>Press down firmly on the Ring until it clips</Text>
          {"\n"}
          securely onto the neck portion of the bottle.
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
    width: "72%",
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
  stepImageFrame: {
    position: "absolute",
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
  neck: {
    position: "absolute",
    width: 44,
    height: 70,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 18,
    backgroundColor: "#F1F3F5",
    bottom: 96,
  },
  capTop: {
    position: "absolute",
    width: 74,
    height: 64,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 32,
    backgroundColor: "#F1F3F5",
    bottom: 108,
  },
  ringPieceTop: {
    position: "absolute",
    width: 150,
    height: 50,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 30,
    backgroundColor: "#F1F3F5",
    bottom: 228,
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
