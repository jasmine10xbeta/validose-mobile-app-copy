import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { ActivityIndicator, Animated, Image, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VReplacementProcessingOverlay } from "@/components/common/VReplacementProcessingOverlay";
import { showToast } from "@/components/common/VToast";
import {
  doesMatchReplacementCheckingCompleteSignal,
  doesMatchReplacementStep1Signal,
  doesMatchReplacementStep2ToStep3Signal,
  doesMatchReplacementStep3ToStep4CheckingSignal,
  doesMatchReplacementStep4CheckingToSuccessSignal,
} from "@/constants/replacementFlow";
import { createSupportRequest } from "@/services/support";
import {
  subscribeToReplacementFlowSignal,
  writeReplacementProcessStopped,
  writeReplacementProcessRestarted,
  writeReplacementProcessStarted,
} from "@/utils/ble";
import { decodeReplacementErrorFromHex } from "@/utils/replacementError";

const REPLACEMENT_STEPS = [
  "Remove bottle from dock",
  "Remove ring from old bottle",
  "Attach ring to new bottle",
  "Place new bottle in dock",
];

const ASSEMBLE_RING_IMAGE = require("../../../../assets/images/png/assemble-ring.png");
const ATTACH_NEW_BOTTLE_IMAGE = require("../../../../assets/images/png/attach-new-bottle.png");
const BOTTLE_ERROR_IMAGE = require("../../../../assets/images/png/bottle-error.png");
const CHECK_DOCK_TWO_IMAGE = require("../../../../assets/images/png/check-dock-2.png");
const CHECK_DOCK_IMAGE = require("../../../../assets/images/png/check-dock.png");
const INSTALLED_BOTTLE_IMAGE = require("../../../../assets/images/png/installed-bottle.png");
const REMOVE_BOTTLE_IMAGE = require("../../../../assets/images/png/remove-bottle-2.png");
const REMOVE_RING_IMAGE = require("../../../../assets/images/png/remove-ring.png");

type ReplacementStage =
  | "intro"
  | "step1"
  | "checking1"
  | "step2"
  | "step3"
  | "step4Checking"
  | "success"
  | "error";

type ReplacementErrorState = {
  step: number;
  progress: number;
  title: string;
  message: string;
  unitHex: string;
  reasonHex: string;
};

const ACTIVE_FLOW_STAGES: ReplacementStage[] = [
  "step1",
  "checking1",
  "step2",
  "step3",
  "step4Checking",
];
const REPLACEMENT_STAGE_SIGNAL_PREFIX = "stage:";
const FLOW_STAGE_ORDER: Record<ReplacementStage, number> = {
  intro: 0,
  step1: 1,
  checking1: 2,
  step2: 3,
  step3: 4,
  step4Checking: 5,
  success: 6,
  error: 7,
};

function parseReplacementStageSignal(signal: string): ReplacementStage | null {
  if (!signal.startsWith(REPLACEMENT_STAGE_SIGNAL_PREFIX)) {
    return null;
  }

  const rawStage = signal.slice(REPLACEMENT_STAGE_SIGNAL_PREFIX.length).toLowerCase();
  if (rawStage === "step1") return "step1";
  if (rawStage === "checking1") return "checking1";
  if (rawStage === "step2") return "step2";
  if (rawStage === "step3") return "step3";
  if (rawStage === "step4checking") return "step4Checking";
  if (rawStage === "success") return "success";
  if (rawStage === "error") return "error";
  return null;
}

function shouldMoveStageForward(currentStage: ReplacementStage, nextStage: ReplacementStage): boolean {
  return FLOW_STAGE_ORDER[nextStage] > FLOW_STAGE_ORDER[currentStage];
}

function isActiveFlowStage(stage: ReplacementStage): boolean {
  return ACTIVE_FLOW_STAGES.includes(stage);
}

function getStageMeta(stage: ReplacementStage): { step: number; progress: number } {
  switch (stage) {
    case "step1":
      return { step: 1, progress: 25 };
    case "checking1":
      return { step: 1, progress: 30 };
    case "step2":
      return { step: 2, progress: 50 };
    case "step3":
      return { step: 3, progress: 70 };
    case "step4Checking":
      return { step: 4, progress: 95 };
    case "success":
      return { step: 4, progress: 100 };
    case "error":
      return { step: 4, progress: 95 };
    default:
      return { step: 1, progress: 0 };
  }
}

export default function ReplaceMedicationScreen() {
  const router = useRouter();
  const { restart } = useLocalSearchParams<{ restart?: string }>();
  const initialRestart = restart === "1" || restart === "true";

  const [flowStage, setFlowStage] = useState<ReplacementStage>("intro");
  const [shouldRestart, setShouldRestart] = useState(initialRestart);
  const [startedAtMs, setStartedAtMs] = useState<number | null>(null);
  const [remainingSeconds, setRemainingSeconds] = useState(300);
  const [isStarting, setIsStarting] = useState(false);
  const [isTransitionProcessing, setIsTransitionProcessing] = useState(false);
  const [errorState, setErrorState] = useState<ReplacementErrorState | null>(null);
  const [isHelpLoading, setIsHelpLoading] = useState(false);
  const animationPhase = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (startedAtMs === null) {
      return;
    }

    const updateRemaining = () => {
      const elapsed = Math.floor((Date.now() - startedAtMs) / 1000);
      setRemainingSeconds(Math.max(0, 300 - elapsed));
    };

    updateRemaining();
    const interval = setInterval(updateRemaining, 1000);
    return () => clearInterval(interval);
  }, [startedAtMs]);

  useEffect(() => {
    if (flowStage !== "step3") {
      animationPhase.setValue(0);
      return;
    }

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
  }, [animationPhase, flowStage]);

  useEffect(() => {
    if (!isActiveFlowStage(flowStage)) {
      return;
    }

    let active = true;
    let transitionInProgress = false;
    let unsub: (() => void) | null = null;

    const currentStageMeta = getStageMeta(flowStage);

    async function moveTo(nextStage: ReplacementStage) {
      if (transitionInProgress || !active) return;
      transitionInProgress = true;
      setIsTransitionProcessing(true);
      await waitForMinimumTransitionTime();
      if (!active) return;
      setFlowStage(nextStage);
      setIsTransitionProcessing(false);
      transitionInProgress = false;
    }

    async function watchFlowSignals() {
      try {
        unsub = await subscribeToReplacementFlowSignal(async (hex) => {
          if (!active) return;

          const nextStage = parseReplacementStageSignal(hex);
          if (nextStage) {
            if (nextStage === "error") {
              setErrorState({
                step: currentStageMeta.step,
                progress: currentStageMeta.progress,
                title: "Replacement Error",
                message: "Dock reported an error during medication replacement. Please start again.",
                unitHex: "FEED",
                reasonHex: "0001",
              });
              setFlowStage("error");
              setIsTransitionProcessing(false);
              transitionInProgress = false;
              return;
            }

            if (shouldMoveStageForward(flowStage, nextStage)) {
              await moveTo(nextStage);
            }
            return;
          }

          const decodedError = decodeReplacementErrorFromHex(hex);
          if (decodedError) {
            setErrorState({
              step: currentStageMeta.step,
              progress: currentStageMeta.progress,
              title: decodedError.title,
              message: decodedError.message,
              unitHex: decodedError.unitHex,
              reasonHex: decodedError.reasonHex,
            });
            setFlowStage("error");
            setIsTransitionProcessing(false);
            return;
          }

          if (flowStage === "step1" && doesMatchReplacementStep1Signal(hex)) {
            await moveTo("checking1");
            return;
          }

          if (flowStage === "checking1" && doesMatchReplacementCheckingCompleteSignal(hex)) {
            await moveTo("step2");
            return;
          }

          if (flowStage === "step2" && doesMatchReplacementStep2ToStep3Signal(hex)) {
            await moveTo("step3");
            return;
          }

          if (flowStage === "step3" && doesMatchReplacementStep3ToStep4CheckingSignal(hex)) {
            await moveTo("step4Checking");
            return;
          }

          if (flowStage === "step4Checking" && doesMatchReplacementStep4CheckingToSuccessSignal(hex)) {
            await moveTo("success");
          }
        });
      } catch (error) {
        console.warn("[Replacement] Could not subscribe to replacement flow signal", error);
      }
    }

    watchFlowSignals();

    return () => {
      active = false;
      transitionInProgress = false;
      if (unsub) unsub();
    };
  }, [flowStage]);

  useEffect(() => {
    if (!isActiveFlowStage(flowStage) || remainingSeconds > 0) {
      return;
    }

    let cancelled = false;
    const currentStageMeta = getStageMeta(flowStage);

    (async () => {
      await writeReplacementProcessStopped();
      if (cancelled) return;

      setErrorState({
        step: currentStageMeta.step,
        progress: currentStageMeta.progress,
        title: "Replacement Timed Out",
        message: "Replacement window expired. Please start again.",
        unitHex: "TIME",
        reasonHex: "0000",
      });
      setFlowStage("error");
      setIsTransitionProcessing(false);
    })();

    return () => {
      cancelled = true;
    };
  }, [flowStage, remainingSeconds]);

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

  const activeStageTitle = useMemo(() => {
    switch (flowStage) {
      case "step1":
        return "Remove the bottle\nfrom dock";
      case "checking1":
      case "step4Checking":
        return "Checking dock";
      case "step2":
        return "Remove the ring from the\neye-drop bottle";
      case "step3":
        return "Attaching the Ring to\nnew Bottle";
      default:
        return "";
    }
  }, [flowStage]);

  const activeStageHelp = useMemo(() => {
    switch (flowStage) {
      case "step1":
      case "checking1":
      case "step4Checking":
        return (
          <Text style={styles.helpText}>
            Keep the dock <Text style={styles.helpTextBold}>stable and on a flat surface</Text>
            {"\n"}
            during the entire process.
          </Text>
        );
      case "step2":
        return (
          <Text style={styles.helpText}>
            Hold the bottle securely with one hand and <Text style={styles.helpTextBold}>pull{"\n"}the Ring upward</Text>
            {"\n"}
            toward the top of the bottle with the other hand.
          </Text>
        );
      case "step3":
        return (
          <Text style={styles.helpText}>
            Position the Ring over the top of the bottle.{"\n"}
            <Text style={styles.helpTextBold}>Press down firmly on the Ring until it clips</Text>
            {"\n"}
            securely onto the neck portion of the bottle.
          </Text>
        );
      default:
        return null;
    }
  }, [flowStage]);

  function renderActiveFlowImage() {
    if (flowStage === "step1") {
      return <Image source={REMOVE_BOTTLE_IMAGE} style={styles.stepImage} resizeMode="cover" />;
    }
    if (flowStage === "checking1") {
      return <Image source={CHECK_DOCK_IMAGE} style={styles.stepImage} resizeMode="cover" />;
    }
    if (flowStage === "step2") {
      return <Image source={REMOVE_RING_IMAGE} style={styles.stepImage} resizeMode="cover" />;
    }
    if (flowStage === "step3") {
      return (
        <>
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
        </>
      );
    }
    return <Image source={CHECK_DOCK_TWO_IMAGE} style={styles.stepImage} resizeMode="cover" />;
  }

  function resetToIntro(restartFlow: boolean) {
    setFlowStage("intro");
    setShouldRestart(restartFlow);
    setStartedAtMs(null);
    setRemainingSeconds(300);
    setIsTransitionProcessing(false);
    setErrorState(null);
  }

  async function onConfirmStart() {
    if (isStarting) return;

    setIsStarting(true);
    try {
      const started = shouldRestart
        ? await writeReplacementProcessRestarted()
        : await writeReplacementProcessStarted();
      if (!started) {
        showToast(
          "error",
          "Could not start replacement",
          "Unable to send start signal to dock."
        );
        return;
      }

      setStartedAtMs(Date.now());
      setRemainingSeconds(300);
      setErrorState(null);
      setFlowStage("step1");
      setShouldRestart(false);
    } finally {
      setIsStarting(false);
    }
  }

  async function onCancelFlow() {
    if (isActiveFlowStage(flowStage)) {
      await writeReplacementProcessStopped();
    }
    router.back();
  }

  if (flowStage === "intro") {
    return (
      <SafeAreaView style={styles.safeArea}>
        <View style={styles.container}>
          <TouchableOpacity onPress={onCancelFlow} style={styles.cancelButton}>
            <Text style={styles.cancelText}>Cancel</Text>
          </TouchableOpacity>

          <View style={styles.headerBlock}>
            <Text style={styles.title}>Replace medication</Text>
            <Text style={styles.subtitle}>
              Keep the dock <Text style={styles.subtitleBold}>stable and on a flat surface</Text>
              {"\n"}
              during the entire process.
            </Text>
          </View>

          <View style={styles.noticeCard}>
            <Text style={styles.noticeText}>{"The device will verify each step \nautomatically."}</Text>
          </View>

          <View style={styles.stepsList}>
            {REPLACEMENT_STEPS.map((step, index) => (
              <View key={step} style={styles.stepRow}>
                <View style={styles.stepNumberCircle}>
                  <Text style={styles.stepNumberText}>{index + 1}</Text>
                </View>
                <Text style={styles.stepText}>{step}</Text>
              </View>
            ))}
          </View>

          <View style={styles.footer}>
            <Text style={styles.footerTitle}>Dock will be in replacement mode for</Text>
            <View style={styles.timePill}>
              <Text style={styles.timeText}>5 mins</Text>
            </View>
            <VButton
              label="Confirm and start"
              onPress={onConfirmStart}
              loading={isStarting}
              disabled={isStarting}
              style={styles.confirmButton}
            />
          </View>
        </View>
      </SafeAreaView>
    );
  }

  if (flowStage === "success") {
    return (
      <SafeAreaView style={styles.safeArea}>
        <View style={styles.container}>
          <View style={styles.topRow}>
            <View style={styles.leftSpacer} />
            <View style={styles.timerPill}>
              <Text style={styles.timerText}>{timerLabel}</Text>
            </View>
          </View>

          <View style={styles.progressHeader}>
            <Text style={styles.progressLeft}>Step 4 of 4</Text>
            <Text style={styles.progressRight}>100%</Text>
          </View>
          <View style={styles.progressTrack}>
            <View style={[styles.progressFill, { width: "100%", backgroundColor: "#65B77E" }]} />
          </View>

          <Text style={styles.title}>Your new bottle has been{"\n"}installed and verified</Text>

          <View style={styles.imagePanel}>
            <Image source={INSTALLED_BOTTLE_IMAGE} style={styles.stepImage} resizeMode="cover" />
          </View>

          <Text style={styles.helpText}>
            Place the eye-drop bottle with the Ring{"\n"}
            attached on the Dock when not in use.
          </Text>

          <VButton
            label="Return to Dashboard"
            onPress={() => router.back()}
            style={styles.confirmButton}
          />
        </View>
      </SafeAreaView>
    );
  }

  if (flowStage === "error") {
    const step = errorState?.step ?? 4;
    const progress = errorState?.progress ?? 95;
    const title = errorState?.title ?? "Replacement Error";
    const message =
      errorState?.message ??
      "Something went wrong during replacement. Please try again or contact support.";

    return (
      <SafeAreaView style={styles.safeArea}>
        <View style={styles.container}>
          <View style={styles.topRow}>
            <TouchableOpacity onPress={onCancelFlow} style={styles.cancelButton}>
              <Text style={styles.cancelText}>Cancel</Text>
            </TouchableOpacity>
            <View style={styles.timerPill}>
              <Text style={styles.timerText}>{timerLabel}</Text>
            </View>
          </View>

          <View style={styles.progressHeader}>
            <Text style={styles.progressLeft}>{`Step ${step} of 4`}</Text>
            <Text style={styles.progressRight}>{`${progress}%`}</Text>
          </View>
          <View style={styles.progressTrack}>
            <View style={[styles.progressFill, { width: `${Math.min(progress, 100)}%`, backgroundColor: "#EF5A5A" }]} />
          </View>

          <Text style={styles.title}>{title}</Text>
          <Text style={styles.errorMessage}>{message}</Text>
          {(errorState?.unitHex || errorState?.reasonHex) ? (
            <Text style={styles.codeText}>{`Unit ${errorState?.unitHex ?? "----"} | Reason ${errorState?.reasonHex ?? "----"}`}</Text>
          ) : null}

          <View style={styles.imagePanel}>
            <Image source={BOTTLE_ERROR_IMAGE} style={styles.stepImage} resizeMode="cover" />
          </View>

          <VButton
            label="Start again"
            onPress={() => resetToIntro(true)}
            style={styles.confirmButton}
          />
          <VButton
            label="Help"
            onPress={async () => {
              if (isHelpLoading) return;
              setIsHelpLoading(true);
              try {
                const supportResponse = await createSupportRequest();
                if (supportResponse?.id) {
                  showToast("success", "Notification sent", "Someone will be in touch soon.");
                } else {
                  showToast("error", "Error", "Could not create support request.");
                }
              } finally {
                setIsHelpLoading(false);
              }
            }}
            loading={isHelpLoading}
            disabled={isHelpLoading}
            style={styles.secondaryButton}
            labelStyle={styles.secondaryLabel}
          />
        </View>
      </SafeAreaView>
    );
  }

  const stageMeta = getStageMeta(flowStage);

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <View style={styles.topRow}>
          <TouchableOpacity onPress={onCancelFlow} style={styles.cancelButton}>
            <Text style={styles.cancelText}>Cancel</Text>
          </TouchableOpacity>
          <View style={[styles.timerPill, isLastMinute && styles.timerPillCritical]}>
            <Text style={[styles.timerText, isLastMinute && styles.timerTextCritical]}>{timerLabel}</Text>
          </View>
        </View>

        <View style={styles.progressHeader}>
          <Text style={styles.progressLeft}>{`Step ${stageMeta.step} of 4`}</Text>
          <Text style={styles.progressRight}>{`${stageMeta.progress}%`}</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={[styles.progressFill, { width: `${stageMeta.progress}%` }]} />
        </View>

        <Text style={styles.title}>{activeStageTitle}</Text>

        {(flowStage === "checking1" || flowStage === "step4Checking") ? (
          <View style={styles.connectingRow}>
            <ActivityIndicator size="small" color="#8A98A7" />
            <Text style={styles.connectingText}>Connecting to dock</Text>
          </View>
        ) : null}

        <View style={styles.imagePanel}>
          {renderActiveFlowImage()}
        </View>

        {activeStageHelp}

        <VReplacementProcessingOverlay
          visible={isTransitionProcessing}
          onReturnToChangingFlow={() => resetToIntro(true)}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    marginTop: "20%",
    backgroundColor: "#FFFFFF",
    borderTopLeftRadius: 28,
    borderTopRightRadius: 28,
    overflow: "hidden",
    shadowColor: "#000000",
    shadowOpacity: 0.14,
    shadowRadius: 20,
    shadowOffset: { width: 0, height: -4 },
    elevation: 18,
  },
  container: {
    flex: 1,
    paddingHorizontal: 16,
    paddingTop: 8,
    paddingBottom: 20,
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
  headerBlock: {
    marginTop: 68,
    marginBottom: 64,
    gap: 10,
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
  timerPillCritical: {
    backgroundColor: "#FDE3E3",
  },
  timerText: {
    color: "#2A6574",
    fontSize: 15,
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
    height: "100%",
    backgroundColor: "#2B7481",
    borderRadius: 999,
  },
  title: {
    // marginTop: 56,
    marginBottom: 24,
    // textAlign: "center",
    color: "#2D3745",
    fontSize: 24,
    lineHeight: 30,
    fontWeight: "700",
  },
  subtitle: {
    color: "#4D5A69",
    fontSize: 17,
    lineHeight: 23,
  },
  subtitleBold: {
    fontWeight: "600",
    color: "#2D3745",
  },
  noticeCard: {
    borderRadius: 8,
    backgroundColor: "#F7EFE5",
    paddingHorizontal: 10,
    paddingVertical: 10,
    marginBottom: 8,
  },
  noticeText: {
    color: "#7A4400",
    fontSize: 16,
    fontWeight: "500",
  },
  stepsList: {
    gap: 8,
  },
  stepRow: {
    height: 48,
    borderRadius: 10,
    borderWidth: 1,
    borderColor: "#DFE3E8",
    backgroundColor: "#FFFFFF",
    flexDirection: "row",
    alignItems: "center",
    paddingHorizontal: 10,
    shadowColor: "#000000",
    shadowOpacity: 0.06,
    shadowRadius: 6,
    shadowOffset: { width: 0, height: 2 },
    elevation: 2,
  },
  stepNumberCircle: {
    height: 32,
    width: 32,
    borderRadius: 16,
    borderWidth: 1,
    borderColor: "#1F6C83",
    alignItems: "center",
    justifyContent: "center",
    marginRight: 12,
  },
  stepNumberText: {
    color: "#1F6C83",
    fontSize: 16,
    fontWeight: "600",
  },
  stepText: {
    color: "#181C21",
    fontSize: 15,
    fontWeight: "600",
  },
  connectingRow: {
    alignSelf: "center",
    marginTop: -6,
    marginBottom: 14,
    flexDirection: "row",
    alignItems: "center",
    gap: 10,
  },
  connectingText: {
    color: "#718096",
    fontSize: 16,
    fontWeight: "500",
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
  stepImageFrame: {
    position: "absolute",
    width: "100%",
    height: "100%",
  },
  helpText: {
    marginTop: 18,
    marginBottom: 20,
    textAlign: "center",
    color: "#505A66",
    fontSize: 16.5,
    lineHeight: 22,
  },
  helpTextBold: {
    color: "#2D3745",
    fontWeight: "700",
  },
  footer: {
    marginTop: "auto",
    alignItems: "center",
    paddingBottom: 6,
  },
  footerTitle: {
    color: "#505A66",
    fontSize: 16,
    fontWeight: "600",
    marginBottom: 8,
  },
  timePill: {
    backgroundColor: "#D3E4E8",
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 8,
    marginBottom: 16,
  },
  timeText: {
    color: "#2A6473",
    fontSize: 16,
    fontWeight: "500",
  },
  confirmButton: {
    paddingVertical: 7,
    width: "86%",
    backgroundColor: "#286B78",
    borderColor: "#286B78",
    borderRadius: 28,
  },
  errorMessage: {
    marginTop: 8,
    textAlign: "center",
    color: "#4D5A69",
    fontSize: 16,
    lineHeight: 22,
    paddingHorizontal: 8,
  },
  codeText: {
    marginTop: 6,
    textAlign: "center",
    color: "#8B95A3",
    fontSize: 12,
  },
  secondaryButton: {
    marginTop: 10,
    paddingVertical: 7,
    width: "86%",
    borderRadius: 28,
    backgroundColor: "#FFFFFF",
    borderWidth: 1,
    borderColor: "#CBD5DF",
  },
  secondaryLabel: {
    color: "#3E4C59",
    fontWeight: "600",
  },
});

function formatTimer(totalSeconds: number): string {
  const mins = Math.floor(totalSeconds / 60);
  const secs = totalSeconds % 60;
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
}

async function waitForMinimumTransitionTime(minMs = 1200): Promise<void> {
  await new Promise((resolve) => setTimeout(resolve, minMs));
}
