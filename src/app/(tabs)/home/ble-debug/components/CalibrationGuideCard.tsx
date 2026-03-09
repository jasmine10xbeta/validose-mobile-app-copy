import { Text, View } from "react-native";

import { describeCalibrationMotion, describeCalibrationState } from "../screenUtils";
import { styles } from "../styles";
import type {
  CalibrationDataSnapshot,
  CalibrationFeedbackSnapshot,
  CalibrationGuideStage,
} from "../types";

type CalibrationGuideCardProps = {
  stage: CalibrationGuideStage;
  stageLabel: string;
  instruction: string;
  completionMessage: string;
  feedbackSnapshot: CalibrationFeedbackSnapshot | null;
  dataSnapshot: CalibrationDataSnapshot | null;
};

export function CalibrationGuideCard({
  stage,
  stageLabel,
  instruction,
  completionMessage,
  feedbackSnapshot,
  dataSnapshot,
}: CalibrationGuideCardProps) {
  const stateText = describeCalibrationState(feedbackSnapshot?.currentState ?? null);
  const motionText = describeCalibrationMotion(feedbackSnapshot?.motionState ?? null);
  const feedbackUpdatedAt = feedbackSnapshot?.updatedAt ?? "--";
  const dataUpdatedAt = dataSnapshot?.updatedAt ?? "--";

  return (
    <View style={styles.calibrationGuideCard}>
      <Text style={styles.calibrationGuideInstructionText}>{instruction}</Text>
      {completionMessage ? (
        <Text style={styles.calibrationGuideCompletionText}>{completionMessage}</Text>
      ) : null}
      <View style={styles.calibrationGuideSignalRow}>
        <View style={styles.calibrationGuideSignalChip}>
          <Text style={styles.calibrationGuideSignalLabel}>State</Text>
          <Text style={styles.calibrationGuideSignalValue}>{stateText}</Text>
        </View>
        <View style={styles.calibrationGuideSignalChip}>
          <Text style={styles.calibrationGuideSignalLabel}>Motion</Text>
          <Text style={styles.calibrationGuideSignalValue}>{motionText}</Text>
        </View>
        <View style={styles.calibrationGuideSignalChip}>
          <Text style={styles.calibrationGuideSignalLabel}>Feedback</Text>
          <Text style={styles.calibrationGuideSignalValue}>{feedbackUpdatedAt}</Text>
        </View>
      </View>
      <View style={styles.calibrationGuideDataCard}>
        <View style={styles.calibrationGuideDataHeader}>
          <Text style={styles.calibrationGuideDataTitle}>Calibration Data</Text>
          <Text style={styles.calibrationGuideDataUpdatedAt}>{dataUpdatedAt}</Text>
        </View>
        {!dataSnapshot ? (
          <Text style={styles.calibrationGuideDataEmpty}></Text>
        ) : (
          <View style={styles.calibrationGuideDataGrid}>
            <View style={styles.calibrationGuideDataCell}>
              <Text style={styles.calibrationGuideDataLabel}>Zero Offset</Text>
              <Text style={styles.calibrationGuideDataValue}>{dataSnapshot.zeroOffset ?? "--"}</Text>
            </View>
            <View style={styles.calibrationGuideDataCell}>
              <Text style={styles.calibrationGuideDataLabel}>Calibration Factor</Text>
              <Text style={styles.calibrationGuideDataValue}>
                {dataSnapshot.calibrationFactor ?? "--"}
              </Text>
            </View>
            <View style={styles.calibrationGuideDataCell}>
              <Text style={styles.calibrationGuideDataLabel}>Full Assembly (mg)</Text>
              <Text style={styles.calibrationGuideDataValue}>
                {dataSnapshot.fullAssemblyWeightMg ?? "--"}
              </Text>
            </View>
          </View>
        )}
      </View>
    </View>
  );
}
