import { Text, View } from "react-native";

import {
  BASELINING_STATE_ENTRIES,
  describeBaseliningMotion,
  describeBaseliningState,
} from "../screenUtils";
import { styles } from "../styles";
import type { BaseliningFeedbackSnapshot, BaseliningGuideStage } from "../types";

type BaseliningGuideCardProps = {
  stage: BaseliningGuideStage;
  instruction: string;
  feedbackSnapshot: BaseliningFeedbackSnapshot | null;
};

const BASELINING_STAGE_LABELS: Record<BaseliningGuideStage, string> = {
  IDLE: "Ready",
  START_SENT: "Starting",
  RUNNING: "In Progress",
  AWAITING_VALIDATION: "Awaiting Validation",
  VALIDATION_SENT: "Validation Sent",
  COMPLETED: "Completed",
  ERROR: "Error",
};

export function BaseliningGuideCard({ stage, instruction, feedbackSnapshot }: BaseliningGuideCardProps) {
  const currentState = feedbackSnapshot?.currentState ?? null;
  const currentStateText = describeBaseliningState(currentState);
  const motionText = describeBaseliningMotion(feedbackSnapshot?.motionState ?? null);
  const updatedAt = feedbackSnapshot?.updatedAt ?? "--";
  const avgWeightText =
    typeof feedbackSnapshot?.avgWeightMg === "number" ? `${feedbackSnapshot.avgWeightMg} mg` : "--";
  const stdDevText =
    typeof feedbackSnapshot?.stdDev === "number" ? String(feedbackSnapshot.stdDev) : "--";
  const isRingPresent = feedbackSnapshot?.isRingPresent;
  const ringText =
    isRingPresent === null || isRingPresent === undefined
      ? "--"
      : isRingPresent
        ? "Present"
        : "Not Present";
  const nfcId = feedbackSnapshot?.medicationNfcIdHex ?? "--";

  return (
    <View style={styles.baseliningGuideCard}>
      <View style={styles.baseliningGuideHeader}>
        <Text style={styles.baseliningGuideTitle}>Baselining State</Text>
        <Text style={styles.baseliningGuideUpdatedAt}>{updatedAt}</Text>
      </View>
      <View style={styles.baseliningGuideStageRow}>
        <Text style={styles.baseliningGuideStagePill}>{BASELINING_STAGE_LABELS[stage]}</Text>
      </View>
      {instruction ? <Text style={styles.baseliningGuideInstruction}>{instruction}</Text> : null}
      <Text style={styles.baseliningGuideCurrentState}>{currentStateText}</Text>
      <View style={styles.baseliningGuideMetricsRow}>
        <View style={styles.baseliningGuideMetricChip}>
          <Text style={styles.baseliningGuideMetricLabel}>Motion</Text>
          <Text style={styles.baseliningGuideMetricValue}>{motionText}</Text>
        </View>
        <View style={styles.baseliningGuideMetricChip}>
          <Text style={styles.baseliningGuideMetricLabel}>Avg Weight</Text>
          <Text style={styles.baseliningGuideMetricValue}>{avgWeightText}</Text>
        </View>
        <View style={styles.baseliningGuideMetricChip}>
          <Text style={styles.baseliningGuideMetricLabel}>Std Dev</Text>
          <Text style={styles.baseliningGuideMetricValue}>{stdDevText}</Text>
        </View>
        <View style={styles.baseliningGuideMetricChip}>
          <Text style={styles.baseliningGuideMetricLabel}>Ring</Text>
          <Text style={styles.baseliningGuideMetricValue}>{ringText}</Text>
        </View>
      </View>
      <View style={styles.baseliningGuideNfcRow}>
        <Text style={styles.baseliningGuideNfcLabel}>Medication NFC ID</Text>
        <Text style={styles.baseliningGuideNfcValue}>{nfcId}</Text>
      </View>
      <View style={styles.baseliningGuideStateList}>
        {BASELINING_STATE_ENTRIES.map((entry) => {
          const isActive = currentState === entry.code;
          const isErrorState = entry.code === 7;
          return (
            <View
              key={`baselining-state-${entry.code}`}
              style={[
                styles.baseliningGuideStateItem,
                isActive ? styles.baseliningGuideStateItemActive : null,
                isErrorState ? styles.baseliningGuideStateItemError : null,
              ]}
            >
              <Text
                style={[
                  styles.baseliningGuideStateCode,
                  isActive ? styles.baseliningGuideStateCodeActive : null,
                ]}
              >
                {entry.code}
              </Text>
              <View style={styles.baseliningGuideStateTextWrap}>
                <Text
                  style={[
                    styles.baseliningGuideStateLabel,
                    isActive ? styles.baseliningGuideStateLabelActive : null,
                  ]}
                >
                  {entry.label}
                </Text>
                {entry.hint ? (
                  <Text style={styles.baseliningGuideStateHint}>{entry.hint}</Text>
                ) : null}
              </View>
            </View>
          );
        })}
      </View>
    </View>
  );
}
