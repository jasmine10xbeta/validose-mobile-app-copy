import { Feather } from "@expo/vector-icons";
import { useLocalSearchParams, useRouter } from "expo-router";
import { useState } from "react";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { showToast } from "@/components/common/VToast";
import { createSupportRequest } from "@/services/support";

type TroubleshootingContent = {
  title: string;
  subtitle: string;
  accent: string;
  tileBg: string;
  panelRingA: string;
  panelRingB: string;
  panelRingC: string;
  bodyText: string;
  emphasizedText: string;
};

const TROUBLESHOOTING_COPY: Record<string, TroubleshootingContent> = {
  "extended-disconnect": {
    title: "Extended Ring-Dock\ndisconnection",
    subtitle: "Violet blink • 5 beeps",
    accent: "#B46AE9",
    tileBg: "#EADBFA",
    panelRingA: "#E4DAEF",
    panelRingB: "#DCCDED",
    panelRingC: "#D4C0EA",
    bodyText:
      "If the ring has been removed from the dock for an extended period, an alert will activate. To resolve the alert, ",
    emphasizedText: "place the ring securely back onto the dock.",
  },
  "device-error": {
    title: "Device error",
    subtitle: "Red blink • 3 beeps",
    accent: "#EF5A5A",
    tileBg: "#F9DEE0",
    panelRingA: "#F2E0E2",
    panelRingB: "#EDD1D5",
    panelRingC: "#E9C2C9",
    bodyText: "A device issue was detected. To resolve the alert, ",
    emphasizedText: "re-seat the ring and bottle on the dock and retry.",
  },
};

export default function LedInfoTroubleshootingScreen() {
  const router = useRouter();
  const { errorId } = useLocalSearchParams<{ errorId?: string }>();
  const [isHelpLoading, setIsHelpLoading] = useState(false);

  const content = TROUBLESHOOTING_COPY[errorId ?? ""] ?? TROUBLESHOOTING_COPY["extended-disconnect"];

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <View style={styles.headerRow}>
          <TouchableOpacity onPress={() => router.back()} style={styles.backButton}>
            <Feather name="chevron-left" size={24} color="#8A95A3" />
            <Text style={styles.backText}>Back</Text>
          </TouchableOpacity>
          <Text style={styles.headerTitle}>Troubleshooting</Text>
          <View style={styles.headerSpacer} />
        </View>

        <View style={styles.errorCard}>
          <View style={[styles.thumbArea, { backgroundColor: content.tileBg }]}>
            <View style={styles.thumbBaseCircle} />
            <View style={styles.thumbDockBody} />
            <View style={styles.thumbCap} />
          </View>
          <View style={[styles.accentBar, { backgroundColor: content.accent }]} />
          <View style={styles.errorCardContent}>
            <Text style={styles.errorCardTitle}>{content.title}</Text>
            <Text style={styles.errorCardSubtitle}>{content.subtitle}</Text>
          </View>
        </View>

        <View style={styles.imagePanel}>
          <View style={[styles.ringBackLarge, { backgroundColor: content.panelRingA }]} />
          <View style={[styles.ringBackMedium, { backgroundColor: content.panelRingB }]} />
          <View style={[styles.ringBackSmall, { backgroundColor: content.panelRingC }]} />

          <View style={styles.topBottle} />
          <View style={styles.topBottleCap} />
          <View style={[styles.bottomDock, { borderColor: content.accent }]}>
            <View style={styles.bottomDockOpening} />
          </View>
        </View>

        <Text style={styles.descriptionText}>
          {content.bodyText}
          <Text style={styles.descriptionTextBold}>{content.emphasizedText}</Text>
        </Text>

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
          style={styles.helpButton}
          labelStyle={styles.helpButtonText}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: "#F3F4F6",
  },
  container: {
    flex: 1,
    paddingHorizontal: 14,
    paddingTop: 8,
    paddingBottom: 18,
  },
  headerRow: {
    height: 44,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    marginBottom: 10,
  },
  backButton: {
    flexDirection: "row",
    alignItems: "center",
    minWidth: 72,
  },
  backText: {
    fontSize: 16,
    color: "#4D5A69",
    fontWeight: "500",
  },
  headerTitle: {
    fontSize: 34 / 2,
    color: "#252F3B",
    fontWeight: "700",
  },
  headerSpacer: {
    width: 72,
  },
  errorCard: {
    height: 82,
    borderRadius: 10,
    borderWidth: 1,
    borderColor: "#DFE3E8",
    backgroundColor: "#F8F9FB",
    flexDirection: "row",
    alignItems: "center",
    overflow: "hidden",
    marginBottom: 18,
  },
  thumbArea: {
    width: 76,
    height: "100%",
    alignItems: "center",
    justifyContent: "flex-end",
    paddingBottom: 8,
  },
  thumbBaseCircle: {
    position: "absolute",
    width: 64,
    height: 26,
    borderRadius: 20,
    backgroundColor: "rgba(255,255,255,0.35)",
    bottom: 6,
  },
  thumbDockBody: {
    width: 30,
    height: 46,
    borderRadius: 14,
    borderWidth: 1,
    borderColor: "#A6B1BE",
    backgroundColor: "#F3F4F6",
    marginBottom: 2,
  },
  thumbCap: {
    position: "absolute",
    bottom: 40,
    width: 18,
    height: 10,
    borderRadius: 6,
    borderWidth: 1,
    borderColor: "#A6B1BE",
    backgroundColor: "#ECEFF2",
  },
  accentBar: {
    width: 10,
    height: "100%",
  },
  errorCardContent: {
    flex: 1,
    paddingHorizontal: 12,
    justifyContent: "center",
  },
  errorCardTitle: {
    color: "#4E5969",
    fontSize: 34 / 2,
    lineHeight: 22,
    fontWeight: "700",
  },
  errorCardSubtitle: {
    color: "#4E5969",
    fontSize: 34 / 2,
    lineHeight: 22,
    marginTop: 2,
  },
  imagePanel: {
    borderRadius: 12,
    overflow: "hidden",
    height: 390,
    backgroundColor: "#F4F5F6",
    justifyContent: "flex-end",
    alignItems: "center",
  },
  ringBackLarge: {
    position: "absolute",
    width: 560,
    height: 220,
    borderRadius: 280,
    bottom: -20,
  },
  ringBackMedium: {
    position: "absolute",
    width: 420,
    height: 170,
    borderRadius: 220,
    bottom: 16,
  },
  ringBackSmall: {
    position: "absolute",
    width: 300,
    height: 140,
    borderRadius: 160,
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
    bottom: 206,
  },
  topBottleCap: {
    position: "absolute",
    width: 94,
    height: 56,
    borderWidth: 1,
    borderColor: "#9AA5B3",
    borderRadius: 32,
    backgroundColor: "#F1F3F5",
    bottom: 244,
  },
  bottomDock: {
    width: 124,
    height: 160,
    backgroundColor: "#D5D9DE",
    borderWidth: 2,
    borderTopLeftRadius: 40,
    borderTopRightRadius: 40,
    borderBottomLeftRadius: 58,
    borderBottomRightRadius: 58,
    marginBottom: 32,
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
  descriptionText: {
    marginTop: 16,
    textAlign: "center",
    color: "#4D5A69",
    fontSize: 34 / 2,
    lineHeight: 24,
    paddingHorizontal: 8,
  },
  descriptionTextBold: {
    color: "#2D3745",
    fontWeight: "700",
  },
  helpButton: {
    marginTop: "auto",
    alignSelf: "center",
    width: "82%",
    backgroundColor: "#F3F4F6",
    borderColor: "#CBD5E1",
  },
  helpButtonText: {
    color: "#2E7787",
    fontWeight: "600",
  },
});
