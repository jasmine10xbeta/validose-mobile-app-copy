import { useLocalSearchParams, useRouter } from "expo-router";
import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import { ActivityIndicator, Animated, Easing, Image, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VReplacementProcessingOverlay } from "@/components/common/VReplacementProcessingOverlay";
import { showToast } from "@/components/common/VToast";
import {
  doesMatchReplacementCheckingCompleteSignal,
  doesMatchReplacementStep1Signal,
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

const BASELINE_STEP1_PART1_IMAGE = require("../../../../assets/images/png/baseline-step-1/part1.png");
const BASELINE_STEP1_PART2_IMAGE = require("../../../../assets/images/png/baseline-step-1/part2.png");
const BASELINE_STEP2_PART1_IMAGE = require("../../../../assets/images/png/baseline-step-2/part1.png");
const BASELINE_STEP2_PART2_IMAGE = require("../../../../assets/images/png/baseline-step-2/part2.png");
const BASELINE_STEP2_PART3_IMAGE = require("../../../../assets/images/png/baseline-step-2/part3.png");
const BASELINE_STEP2_PART4_IMAGE = require("../../../../assets/images/png/baseline-step-2/part4.png");
const BASELINE_STEP3_PART1_IMAGE = require("../../../../assets/images/png/baseline-step-3/part1.png");
const BASELINE_STEP3_PART2_IMAGE = require("../../../../assets/images/png/baseline-step-3/part2.png");
const BASELINE_STEP4_PART1_IMAGE = require("../../../../assets/images/png/baseline-step-4/part1.png");
const BASELINE_STEP4_PART2_IMAGE = require("../../../../assets/images/png/baseline-step-4/part2.png");
const BASELINE_STEP5_PART1_IMAGE = require("../../../../assets/images/png/baseline-step-5/part1.png");
const BASELINE_STEP5_PART2_IMAGE = require("../../../../assets/images/png/baseline-step-5/part2.png");
const BASELINE_STEP6_PART1_IMAGE = require("../../../../assets/images/png/baseline-step-6/part1.png");
const BASELINE_STEP6_PART2_IMAGE = require("../../../../assets/images/png/baseline-step-6/part2.png");
const BOTTLE_ERROR_IMAGE = require("../../../../assets/images/png/bottle-error.png");
const CHECK_DOCK_TWO_IMAGE = require("../../../../assets/images/png/check-dock-2.png");
const INSTALLED_BOTTLE_IMAGE = require("../../../../assets/images/png/installed-bottle.png");

type ReplacementStage =
  | "intro"
  | "step1"
  | "checking1"
  | "step2"
  | "step3"
  | "step3Docking"
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
  "step3Docking",
  "step4Checking",
];
const REPLACEMENT_STAGE_SIGNAL_PREFIX = "stage:";
const FLOW_STAGE_ORDER: Record<ReplacementStage, number> = {
  intro: 0,
  step1: 1,
  checking1: 2,
  step2: 3,
  step3: 4,
  step3Docking: 5,
  step4Checking: 6,
  success: 7,
  error: 8,
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
  if (rawStage === "step3docking") return "step3Docking";
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
    case "step3Docking":
      return { step: 3, progress: 82 };
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
  const stepOnePartOneOpacity = useRef(new Animated.Value(1)).current;
  const stepOnePartTwoOpacity = useRef(new Animated.Value(0)).current;
  const stepTwoPartOneOpacity = useRef(new Animated.Value(1)).current;
  const stepTwoPartTwoOpacity = useRef(new Animated.Value(0)).current;
  const stepTwoPartThreeOpacity = useRef(new Animated.Value(0)).current;
  const stepTwoPartFourOpacity = useRef(new Animated.Value(0)).current;
  const stepThreePartOneOpacity = useRef(new Animated.Value(1)).current;
  const stepThreePartTwoOpacity = useRef(new Animated.Value(0)).current;
  const stepFourPartOneOpacity = useRef(new Animated.Value(1)).current;
  const stepFourPartTwoOpacity = useRef(new Animated.Value(0)).current;
  const stepFivePartOneOpacity = useRef(new Animated.Value(1)).current;
  const stepFivePartTwoOpacity = useRef(new Animated.Value(0)).current;
  const stepSixPartOneOpacity = useRef(new Animated.Value(1)).current;
  const stepSixPartTwoOpacity = useRef(new Animated.Value(0)).current;
  const flowStageRef = useRef<ReplacementStage>("intro");
  const transitionInProgressRef = useRef(false);

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
    flowStageRef.current = flowStage;
  }, [flowStage]);

  useEffect(() => {
    if (flowStage !== "step1") {
      stepOnePartOneOpacity.setValue(1);
      stepOnePartTwoOpacity.setValue(0);
      return;
    }

    const fadeDurationMs = 1800;
    const fadeEasing = Easing.inOut(Easing.quad);
    const animation = Animated.loop(
      Animated.sequence([
        Animated.parallel([
          Animated.timing(stepOnePartOneOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepOnePartTwoOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
        Animated.parallel([
          Animated.timing(stepOnePartOneOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepOnePartTwoOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
      stepOnePartOneOpacity.setValue(1);
      stepOnePartTwoOpacity.setValue(0);
    };
  }, [flowStage, stepOnePartOneOpacity, stepOnePartTwoOpacity]);

  useEffect(() => {
    if (flowStage !== "checking1") {
      stepTwoPartOneOpacity.setValue(1);
      stepTwoPartTwoOpacity.setValue(0);
      stepTwoPartThreeOpacity.setValue(0);
      stepTwoPartFourOpacity.setValue(0);
      return;
    }

    const fadeDurationMs = 1100;
    const fadeEasing = Easing.inOut(Easing.quad);
    const crossFade = (from: Animated.Value, to: Animated.Value) =>
      Animated.parallel([
        Animated.timing(from, {
          toValue: 0,
          duration: fadeDurationMs,
          easing: fadeEasing,
          useNativeDriver: true,
        }),
        Animated.timing(to, {
          toValue: 1,
          duration: fadeDurationMs,
          easing: fadeEasing,
          useNativeDriver: true,
        }),
      ]);

    const animation = Animated.loop(
      Animated.sequence([
        crossFade(stepTwoPartOneOpacity, stepTwoPartTwoOpacity),
        crossFade(stepTwoPartTwoOpacity, stepTwoPartThreeOpacity),
        crossFade(stepTwoPartThreeOpacity, stepTwoPartFourOpacity),
        crossFade(stepTwoPartFourOpacity, stepTwoPartOneOpacity),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
      stepTwoPartOneOpacity.setValue(1);
      stepTwoPartTwoOpacity.setValue(0);
      stepTwoPartThreeOpacity.setValue(0);
      stepTwoPartFourOpacity.setValue(0);
    };
  }, [
    flowStage,
    stepTwoPartOneOpacity,
    stepTwoPartTwoOpacity,
    stepTwoPartThreeOpacity,
    stepTwoPartFourOpacity,
  ]);

  useEffect(() => {
    if (flowStage !== "step2") {
      stepThreePartOneOpacity.setValue(1);
      stepThreePartTwoOpacity.setValue(0);
      return;
    }

    const fadeDurationMs = 1500;
    const fadeEasing = Easing.inOut(Easing.quad);
    const animation = Animated.loop(
      Animated.sequence([
        Animated.parallel([
          Animated.timing(stepThreePartOneOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepThreePartTwoOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
        Animated.parallel([
          Animated.timing(stepThreePartOneOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepThreePartTwoOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
      stepThreePartOneOpacity.setValue(1);
      stepThreePartTwoOpacity.setValue(0);
    };
  }, [flowStage, stepThreePartOneOpacity, stepThreePartTwoOpacity]);

  useEffect(() => {
    if (flowStage !== "step3") {
      stepFourPartOneOpacity.setValue(1);
      stepFourPartTwoOpacity.setValue(0);
      return;
    }

    const fadeDurationMs = 1500;
    const fadeEasing = Easing.inOut(Easing.quad);
    const animation = Animated.loop(
      Animated.sequence([
        Animated.parallel([
          Animated.timing(stepFourPartOneOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepFourPartTwoOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
        Animated.parallel([
          Animated.timing(stepFourPartOneOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepFourPartTwoOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
      stepFourPartOneOpacity.setValue(1);
      stepFourPartTwoOpacity.setValue(0);
    };
  }, [flowStage, stepFourPartOneOpacity, stepFourPartTwoOpacity]);

  useEffect(() => {
    if (flowStage !== "step3Docking") {
      stepFivePartOneOpacity.setValue(1);
      stepFivePartTwoOpacity.setValue(0);
      return;
    }

    const fadeDurationMs = 1500;
    const fadeEasing = Easing.inOut(Easing.quad);
    const animation = Animated.loop(
      Animated.sequence([
        Animated.parallel([
          Animated.timing(stepFivePartOneOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepFivePartTwoOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
        Animated.parallel([
          Animated.timing(stepFivePartOneOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepFivePartTwoOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
      stepFivePartOneOpacity.setValue(1);
      stepFivePartTwoOpacity.setValue(0);
    };
  }, [flowStage, stepFivePartOneOpacity, stepFivePartTwoOpacity]);

  useEffect(() => {
    if (flowStage !== "step4Checking") {
      stepSixPartOneOpacity.setValue(1);
      stepSixPartTwoOpacity.setValue(0);
      return;
    }

    const fadeDurationMs = 1500;
    const fadeEasing = Easing.inOut(Easing.quad);
    const animation = Animated.loop(
      Animated.sequence([
        Animated.parallel([
          Animated.timing(stepSixPartOneOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepSixPartTwoOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
        Animated.parallel([
          Animated.timing(stepSixPartOneOpacity, {
            toValue: 1,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
          Animated.timing(stepSixPartTwoOpacity, {
            toValue: 0,
            duration: fadeDurationMs,
            easing: fadeEasing,
            useNativeDriver: true,
          }),
        ]),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
      stepSixPartOneOpacity.setValue(1);
      stepSixPartTwoOpacity.setValue(0);
    };
  }, [flowStage, stepSixPartOneOpacity, stepSixPartTwoOpacity]);

  useEffect(() => {
    if (flowStage !== "step2") {
      return;
    }

    const timeoutId = setTimeout(() => {
      setFlowStage((currentStage) => {
        const nextStage = currentStage === "step2" ? "step3" : currentStage;
        flowStageRef.current = nextStage;
        return nextStage;
      });
    }, 15000);

    return () => clearTimeout(timeoutId);
  }, [flowStage]);

  useEffect(() => {
    if (flowStage !== "step3") {
      return;
    }

    const timeoutId = setTimeout(() => {
      setFlowStage((currentStage) => {
        const nextStage = currentStage === "step3" ? "step3Docking" : currentStage;
        flowStageRef.current = nextStage;
        return nextStage;
      });
    }, 15000);

    return () => clearTimeout(timeoutId);
  }, [flowStage]);

  useEffect(() => {
    let active = true;
    let unsub: (() => void) | null = null;

    async function moveTo(nextStage: ReplacementStage) {
      if (transitionInProgressRef.current || !active) return;
      transitionInProgressRef.current = true;
      setIsTransitionProcessing(true);
      await waitForMinimumTransitionTime();
      if (!active) {
        transitionInProgressRef.current = false;
        setIsTransitionProcessing(false);
        return;
      }
      flowStageRef.current = nextStage;
      setFlowStage(nextStage);
      setIsTransitionProcessing(false);
      transitionInProgressRef.current = false;
    }

    async function watchFlowSignals() {
      try {
        unsub = await subscribeToReplacementFlowSignal(async (hex) => {
          if (!active) return;
          const currentStage = flowStageRef.current;
          if (!isActiveFlowStage(currentStage)) {
            return;
          }
          const currentStageMeta = getStageMeta(currentStage);

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
              flowStageRef.current = "error";
              setFlowStage("error");
              setIsTransitionProcessing(false);
              transitionInProgressRef.current = false;
              return;
            }

            if (shouldMoveStageForward(currentStage, nextStage)) {
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
            flowStageRef.current = "error";
            setFlowStage("error");
            setIsTransitionProcessing(false);
            transitionInProgressRef.current = false;
            return;
          }

          if (currentStage === "step1" && doesMatchReplacementStep1Signal(hex)) {
            await moveTo("checking1");
            return;
          }

          if (currentStage === "checking1" && doesMatchReplacementCheckingCompleteSignal(hex)) {
            await moveTo("step2");
            return;
          }

          if (
            (currentStage === "step2" || currentStage === "step3" || currentStage === "step3Docking") &&
            doesMatchReplacementStep3ToStep4CheckingSignal(hex)
          ) {
            await moveTo("step4Checking");
            return;
          }

          if (currentStage === "step4Checking" && doesMatchReplacementStep4CheckingToSuccessSignal(hex)) {
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
      transitionInProgressRef.current = false;
      if (unsub) unsub();
    };
  }, []);

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
      flowStageRef.current = "error";
      setFlowStage("error");
      setIsTransitionProcessing(false);
      transitionInProgressRef.current = false;
    })();

    return () => {
      cancelled = true;
    };
  }, [flowStage, remainingSeconds]);

  const timerLabel = formatTimer(remainingSeconds);
  const isLastMinute = remainingSeconds <= 60;

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
      case "step3Docking":
        return "Place the assembled Ring\nand bottle onto the Dock";
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
            Hold the bottle securely with one hand and{" "}
            <Text style={styles.helpTextBold}>pull the Ring upward</Text> toward the top of the
            bottle with the other hand.
            {"\n"}
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
      case "step3Docking":
        return (
          <Text style={styles.helpText}>
            If the Ring sits flush and makes full contact with{"\n"}the Dock, the connection is secure
          </Text>
        );
      default:
        return null;
    }
  }, [flowStage]);

  function renderActiveFlowImage() {
    if (flowStage === "step1") {
      return (
        <>
          <Animated.Image
            source={BASELINE_STEP1_PART1_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepOnePartOneOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP1_PART2_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepOnePartTwoOpacity }]}
            resizeMode="cover"
          />
        </>
      );
    }
    if (flowStage === "checking1") {
      return (
        <>
          <Animated.Image
            source={BASELINE_STEP2_PART1_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepTwoPartOneOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP2_PART2_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepTwoPartTwoOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP2_PART3_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepTwoPartThreeOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP2_PART4_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepTwoPartFourOpacity }]}
            resizeMode="cover"
          />
        </>
      );
    }
    if (flowStage === "step2") {
      return (
        <>
          <Animated.Image
            source={BASELINE_STEP3_PART1_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepThreePartOneOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP3_PART2_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepThreePartTwoOpacity }]}
            resizeMode="cover"
          />
        </>
      );
    }
    if (flowStage === "step3") {
      return (
        <>
          <Animated.Image
            source={BASELINE_STEP4_PART1_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepFourPartOneOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP4_PART2_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepFourPartTwoOpacity }]}
            resizeMode="cover"
          />
        </>
      );
    }
    if (flowStage === "step3Docking") {
      return (
        <>
          <Animated.Image
            source={BASELINE_STEP5_PART1_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepFivePartOneOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP5_PART2_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepFivePartTwoOpacity }]}
            resizeMode="cover"
          />
        </>
      );
    }
    if (flowStage === "step4Checking") {
      return (
        <>
          <Animated.Image
            source={BASELINE_STEP6_PART1_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepSixPartOneOpacity }]}
            resizeMode="cover"
          />
          <Animated.Image
            source={BASELINE_STEP6_PART2_IMAGE}
            style={[styles.stepImageFrame, { opacity: stepSixPartTwoOpacity }]}
            resizeMode="cover"
          />
        </>
      );
    }
    return <Image source={CHECK_DOCK_TWO_IMAGE} style={styles.stepImage} resizeMode="cover" />;
  }

  function resetToIntro(restartFlow: boolean) {
    flowStageRef.current = "intro";
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
          "Ensure the Validose device is nearby and try again."
        );
        return;
      }

      setStartedAtMs(Date.now());
      setRemainingSeconds(300);
      setErrorState(null);
      flowStageRef.current = "step1";
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

  function renderModalShell(content: ReactNode) {
    return (
      <View style={styles.modalRoot}>
        <View pointerEvents="none" style={styles.backdrop} />
        <SafeAreaView edges={["bottom"]} style={styles.safeArea}>
          {content}
        </SafeAreaView>
      </View>
    );
  }

  if (flowStage === "intro") {
    return renderModalShell(
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
    );
  }

  if (flowStage === "success") {
    return renderModalShell(
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

          <Text style={[styles.title, styles.successTitle]}>
            Your new bottle has been{"\n"}installed and verified
          </Text>

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
            style={[styles.confirmButton, styles.successReturnButton]}
          />
        </View>
    );
  }

  if (flowStage === "error") {
    const step = errorState?.step ?? 4;
    const progress = errorState?.progress ?? 95;
    const title = errorState?.title ?? "Replacement Error";
    const message =
      errorState?.message ??
      "Something went wrong during replacement. Please try again or contact support.";

    return renderModalShell(
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

          <Text style={[styles.title, styles.errorTitle]}>{title}</Text>
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
            style={[styles.confirmButton, styles.errorActionButton]}
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
            style={[styles.secondaryButton, styles.errorActionButton]}
            labelStyle={styles.secondaryLabel}
          />
        </View>
    );
  }

  const stageMeta = getStageMeta(flowStage);
  const isCheckingDockStage = flowStage === "checking1";
  const totalSteps = isCheckingDockStage ? 2 : 4;

  return renderModalShell(
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
          <Text style={styles.progressLeft}>{`Step ${stageMeta.step} of ${totalSteps}`}</Text>
          <Text style={styles.progressRight}>{`${stageMeta.progress}%`}</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={[styles.progressFill, { width: `${stageMeta.progress}%` }]} />
        </View>

        <Text
          style={[
            styles.title,
            flowStage === "step1" ? styles.step1Title : null,
            isCheckingDockStage ? styles.checkingTitle : null,
            flowStage === "step2" ? styles.step2Title : null,
            flowStage === "step3" || flowStage === "step3Docking" ? styles.step3Title : null,
            flowStage === "step4Checking" ? styles.step4CheckingTitle : null,
          ]}
        >
          {activeStageTitle}
        </Text>

        {(flowStage === "checking1" || flowStage === "step4Checking") ? (
          <View
            style={[
              styles.connectingRow,
              isCheckingDockStage ? styles.checkingConnectingRowBelowTitle : null,
              flowStage === "step4Checking" ? styles.step4CheckingConnectingRow : null,
            ]}
          >
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
  );
}

const styles = StyleSheet.create({
  modalRoot: {
    flex: 1,
    justifyContent: "flex-end",
  },
  backdrop: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: "rgba(255, 255, 255, 0.95)",
  },
  safeArea: {
    height: "90%",
    backgroundColor: "#FFF",
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    shadowColor: "#000000",
    shadowOpacity: 0.18,
    shadowRadius: 20,
    shadowOffset: { width: 0, height: -4 },
    elevation: 24,
  },
  container: {
    flex: 1,
    paddingHorizontal: 18,
    paddingTop: 8,
    paddingBottom: 20,
  },
  cancelButton: {
    paddingVertical: 8,
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
    // marginBottom: 24,
    // textAlign: "center",
    color: "#2D3745",
    fontSize: 26,
    lineHeight: 30,
    fontWeight: "700",
    marginVertical: 16,
  },
  step1Title: {
    textAlign: "center",
    marginVertical: 26,
  },
  checkingTitle: {
    textAlign: "center",
    fontSize: 26,
    marginVertical: 26,
  },
  step2Title: {
    textAlign: "center",
    marginVertical: 26,
  },
  step3Title: {
    textAlign: "center",
    marginVertical: 26,
  },
  step4CheckingTitle: {
    textAlign: "center",
    fontSize: 26,
    marginTop: 16,
    marginBottom: 10,
  },
  successTitle: {
    textAlign: "center",
    fontSize: 26,
    marginVertical: 26,
  },
  errorTitle: {
    textAlign: "center",
    fontSize: 26,
    marginVertical: 26,
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
  checkingConnectingRowBelowTitle: {
    marginBottom: 26,
    marginTop: -12,
    marginRight: 8
  },
  step4CheckingConnectingRow: {
    marginTop: 0,
    marginBottom: 16,
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
  successReturnButton: {
    marginTop: 24,
    alignSelf: "center",
  },
  errorActionButton: {
    alignSelf: "center",
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
