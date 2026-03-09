import { Text, View } from "react-native";

import { styles } from "../styles";
import type { CompactFeedbackPreview } from "../types";

type CompactFeedbackCardProps = {
  title: string;
  feedbackPreview: CompactFeedbackPreview | null;
  responsePreview: CompactFeedbackPreview | null;
};

function CompactFeedbackSlot({
  mode,
  preview,
}: {
  mode: "RESPONSE" | "FEEDBACK";
  preview: CompactFeedbackPreview | null;
}) {
  const isResponse = mode === "RESPONSE";
  const emptyText = "";
  const summaryText = preview?.summary?.trim() || emptyText;

  return (
    <View
      style={[
        styles.compactFeedbackSlot,
        isResponse ? styles.compactFeedbackSlotResponse : styles.compactFeedbackSlotFeedback,
      ]}
    >
      <View style={styles.compactFeedbackSlotTop}>
        <Text
          style={[
            styles.compactFeedbackBadge,
            isResponse ? styles.compactFeedbackBadgeResponse : styles.compactFeedbackBadgeFeedback,
          ]}
        >
          {isResponse ? "Latest Response" : "Latest Feedback"}
        </Text>
        <Text style={styles.compactFeedbackTime}>{preview?.updatedAt ?? "--"}</Text>
      </View>
      <Text style={!preview ? styles.compactFeedbackEmpty : styles.compactFeedbackText} numberOfLines={2}>
        {summaryText}
      </Text>
    </View>
  );
}

export function CompactFeedbackCard({
  title,
  feedbackPreview,
  responsePreview,
}: CompactFeedbackCardProps) {
  const lastUpdate = feedbackPreview?.updatedAt || responsePreview?.updatedAt || "--";

  return (
      <View style={styles.compactFeedbackLaneGrid}>
        <CompactFeedbackSlot mode="RESPONSE" preview={responsePreview} />
        <CompactFeedbackSlot mode="FEEDBACK" preview={feedbackPreview} />
      </View>
  );
}
