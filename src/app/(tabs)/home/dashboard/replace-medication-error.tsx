import { Feather } from "@expo/vector-icons";
import { useLocalSearchParams, useRouter } from "expo-router";
import { useEffect, useMemo, useState } from "react";
import { Image, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { showToast } from "@/components/common/VToast";
import { createSupportRequest } from "@/services/support";

const BOTTLE_ERROR_IMAGE = require("../../../../assets/images/png/bottle-error.png");

export default function ReplaceMedicationErrorScreen() {
  const router = useRouter();
  const {
    startedAt,
    step,
    progress,
    title,
    message,
    unit,
    reason,
  } = useLocalSearchParams<{
    startedAt?: string;
    step?: string;
    progress?: string;
    title?: string;
    message?: string;
    unit?: string;
    reason?: string;
  }>();

  const startAtMs = useMemo(() => {
    const parsed = Number(startedAt);
    return Number.isFinite(parsed) ? parsed : Date.now();
  }, [startedAt]);
  const [remainingSeconds, setRemainingSeconds] = useState(() => {
    const elapsed = Math.floor((Date.now() - startAtMs) / 1000);
    return Math.max(0, 300 - elapsed);
  });
  const [isHelpLoading, setIsHelpLoading] = useState(false);

  useEffect(() => {
    const interval = setInterval(() => {
      const elapsed = Math.floor((Date.now() - startAtMs) / 1000);
      setRemainingSeconds(Math.max(0, 300 - elapsed));
    }, 1000);

    return () => clearInterval(interval);
  }, [startAtMs]);

  const timerLabel = formatTimer(remainingSeconds);
  const stepLabel = Number.isFinite(Number(step)) ? Number(step) : 4;
  const progressLabel = Number.isFinite(Number(progress)) ? Number(progress) : 95;
  const errorTitle = title?.trim() || "Replacement Error";
  const errorMessage =
    message?.trim() ||
    "Something went wrong during replacement. Please try again or contact support.";

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <View style={styles.topRow}>
          <TouchableOpacity onPress={() => router.back()} style={styles.cancelButton}>
            <Text style={styles.cancelText}>Cancel</Text>
          </TouchableOpacity>
          <View style={styles.timerPill}>
            <Text style={styles.timerText}>{timerLabel}</Text>
            <Feather name="rotate-ccw" size={20} color="#8794A3" />
          </View>
        </View>

        <View style={styles.progressHeader}>
          <Text style={styles.progressLeft}>{`Step ${stepLabel} of 4`}</Text>
          <Text style={styles.progressRight}>{`${progressLabel}%`}</Text>
        </View>
        <View style={styles.progressTrack}>
          <View style={[styles.progressFill, { width: `${Math.min(progressLabel, 100)}%` }]} />
        </View>

        <Text style={styles.title}>{errorTitle}</Text>
        <Text style={styles.message}>{errorMessage}</Text>
        {unit || reason ? (
          <Text style={styles.codeText}>{`Unit ${unit ?? "----"} | Reason ${reason ?? "----"}`}</Text>
        ) : null}

        <View style={styles.imagePanel}>
          <Image source={BOTTLE_ERROR_IMAGE} style={styles.stepImage} resizeMode="cover" />
        </View>

        <VButton
          label="Start again"
          onPress={() => router.replace("/home/dashboard/replace-medication?restart=1")}
          style={styles.primaryButton}
        />
        <VButton
          label="Help"
          onPress={async () => {
            if (isHelpLoading) return;
            setIsHelpLoading(true);
            try {
              const supportResponse = await createSupportRequest();
              if (supportResponse?.id) {
                showToast(
                  "success",
                  "Notification sent",
                  "Someone will be in touch soon."
                );
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
    backgroundColor: "#E1E7ED",
    paddingHorizontal: 12,
    paddingVertical: 8,
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  timerText: {
    color: "#8794A3",
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
    height: "100%",
    backgroundColor: "#EF5A5A",
    borderRadius: 999,
  },
  title: {
    marginTop: 54,
    textAlign: "center",
    color: "#2D3745",
    fontSize: 24,
    fontWeight: "700",
  },
  message: {
    marginTop: 8,
    textAlign: "center",
    color: "#4D5A69",
    fontSize: 32 / 2,
    lineHeight: 44 / 2,
    paddingHorizontal: 8,
  },
  codeText: {
    marginTop: 6,
    textAlign: "center",
    color: "#8B95A3",
    fontSize: 12,
  },
  imagePanel: {
    marginTop: 24,
    borderRadius: 12,
    overflow: "hidden",
    height: 350,
    backgroundColor: "#F4F5F6",
    justifyContent: "flex-end",
    alignItems: "center",
    marginBottom: 16,
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
    backgroundColor: "#F1E3E6",
    bottom: -20,
  },
  ringBackMedium: {
    position: "absolute",
    width: 420,
    height: 170,
    borderRadius: 220,
    backgroundColor: "#EED7DC",
    bottom: 16,
  },
  ringBackSmall: {
    position: "absolute",
    width: 300,
    height: 140,
    borderRadius: 160,
    backgroundColor: "#E8CAD0",
    bottom: -16,
  },
  topBottle: {
    position: "absolute",
    width: 50,
    height: 88,
    borderWidth: 1,
    borderColor: "#9A6A20",
    borderBottomLeftRadius: 20,
    borderBottomRightRadius: 20,
    borderTopLeftRadius: 10,
    borderTopRightRadius: 10,
    backgroundColor: "#F2A022",
    bottom: 182,
  },
  topBottleCap: {
    position: "absolute",
    width: 94,
    height: 56,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 32,
    backgroundColor: "#F1F3F5",
    bottom: 220,
  },
  bottomDock: {
    width: 124,
    height: 160,
    backgroundColor: "#D5D9DE",
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderTopLeftRadius: 40,
    borderTopRightRadius: 40,
    borderBottomLeftRadius: 58,
    borderBottomRightRadius: 58,
    marginBottom: 10,
    alignItems: "center",
    justifyContent: "flex-start",
  },
  bottomDockOpening: {
    marginTop: 14,
    width: 62,
    height: 38,
    borderRadius: 22,
    backgroundColor: "#F3F4F6",
    borderWidth: 1,
    borderColor: "#9AA5B3",
  },
  primaryButton: {
    width: "86%",
    alignSelf: "center",
    backgroundColor: "#286B78",
    borderColor: "#286B78",
    marginBottom: 10,
  },
  secondaryButton: {
    width: "86%",
    alignSelf: "center",
    backgroundColor: "#F4F5F6",
    borderColor: "#CBD5E1",
  },
  secondaryLabel: {
    color: "#2A6574",
  },
});

function formatTimer(totalSeconds: number): string {
  const mins = Math.floor(totalSeconds / 60);
  const secs = totalSeconds % 60;
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
}
