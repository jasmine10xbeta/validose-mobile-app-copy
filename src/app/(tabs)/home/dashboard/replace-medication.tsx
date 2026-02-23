import { useLocalSearchParams, useRouter } from "expo-router";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { showToast } from "@/components/common/VToast";
import {
  writeReplacementProcessRestarted,
  writeReplacementProcessStarted,
} from "@/utils/ble";

const REPLACEMENT_STEPS = [
  "Remove bottle from dock",
  "Remove ring from old bottle",
  "Attach ring to new bottle",
  "Place new bottle in dock",
];

export default function ReplaceMedicationScreen() {
  const router = useRouter();
  const { restart } = useLocalSearchParams<{ restart?: string }>();
  const isRestart = restart === "1" || restart === "true";

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <TouchableOpacity onPress={() => router.replace("/home/dashboard")} style={styles.cancelButton}>
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
          <Text style={styles.noticeText}>The device will verify each step automatically.</Text>
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
            onPress={async () => {
              const started = isRestart
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

              router.push(
                `/home/dashboard/replace-medication-step?startedAt=${Date.now()}`
              );
            }}
            style={styles.confirmButton}
          />
        </View>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: "#EFEFEF",
  },
  container: {
    flex: 1,
    paddingHorizontal: 16,
    paddingTop: 8,
    paddingBottom: 20,
  },
  cancelButton: {
    alignSelf: "flex-start",
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
  title: {
    color: "#2D3745",
    fontSize: 24,
    fontWeight: "700",
  },
  subtitle: {
    color: "#4D5A69",
    fontSize: 17,
    lineHeight: 23,
  },
  subtitleBold: {
    fontWeight: "700",
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
    color: "#8D4E0B",
    fontSize: 18,
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
    backgroundColor: "#F6F7F8",
    flexDirection: "row",
    alignItems: "center",
    paddingHorizontal: 10,
  },
  stepNumberCircle: {
    height: 32,
    width: 32,
    borderRadius: 16,
    borderWidth: 1.5,
    borderColor: "#1F6C83",
    alignItems: "center",
    justifyContent: "center",
    marginRight: 12,
  },
  stepNumberText: {
    color: "#1F6C83",
    fontSize: 14,
    fontWeight: "500",
  },
  stepText: {
    color: "#1B1F24",
    fontSize: 17,
    fontWeight: "600",
  },
  footer: {
    marginTop: "auto",
    alignItems: "center",
    paddingBottom: 6,
  },
  footerTitle: {
    color: "#4D5A69",
    fontSize: 16,
    fontWeight: "500",
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
    width: "86%",
    backgroundColor: "#286B78",
    borderColor: "#286B78",
  },
});
