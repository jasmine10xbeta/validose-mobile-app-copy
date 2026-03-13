import { Text, View } from "react-native";

import {
  CALIBRATION_STATE_ENTRIES,
  describeCalibrationMotion,
  describeCalibrationState,
  getCalibrationInstructionForState,
} from "../screenUtils";
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
  const currentState = feedbackSnapshot?.currentState ?? null;
  const stateText = describeCalibrationState(currentState);
  const motionText = describeCalibrationMotion(feedbackSnapshot?.motionState ?? null);
  const feedbackUpdatedAt = feedbackSnapshot?.updatedAt ?? "--";
  const dataUpdatedAt = dataSnapshot?.updatedAt ?? "--";
  const stateInstruction = getCalibrationInstructionForState(currentState);
  const instructionText = stateInstruction ?? instruction;
  const stagePillStyle =
    stage === "IDLE"
      ? styles.calibrationGuideStagePillIdle
      : stage === "COMPLETED"
        ? styles.calibrationGuideStagePillDone
        : styles.calibrationGuideStagePillActive;
  const stageTextStyle =
    stage === "IDLE"
      ? styles.calibrationGuideStageTextIdle
      : stage === "COMPLETED"
        ? styles.calibrationGuideStageTextDone
        : styles.calibrationGuideStageTextActive;

  return (
    <View style={styles.calibrationGuideCard}>
      <View style={styles.calibrationGuideHeader}>
        <Text style={styles.calibrationGuideTitle}>Calibration State</Text>
        <Text style={styles.calibrationGuideDataUpdatedAt}>{feedbackUpdatedAt}</Text>
      </View>
      <View style={styles.calibrationGuideStageRow}>
        <View style={[styles.calibrationGuideStagePill, stagePillStyle]}>
          <Text style={[styles.calibrationGuideStageText, stageTextStyle]}>{stageLabel}</Text>
        </View>
      </View>
      {instructionText ? (
        <Text style={styles.calibrationGuideInstructionText}>{instructionText}</Text>
      ) : null}
      <Text style={styles.calibrationGuideCurrentState}>{stateText}</Text>
      {completionMessage ? (
        <Text style={styles.calibrationGuideCompletionText}>{completionMessage}</Text>
      ) : null}
      <View style={styles.calibrationGuideSignalRow}>
        <View style={styles.calibrationGuideSignalChip}>
          <Text style={styles.calibrationGuideSignalLabel}>Motion</Text>
          <Text style={styles.calibrationGuideSignalValue}>{motionText}</Text>
        </View>
        <View style={styles.calibrationGuideSignalChip}>
          <Text style={styles.calibrationGuideSignalLabel}>Feedback</Text>
          <Text style={styles.calibrationGuideSignalValue}>{feedbackUpdatedAt}</Text>
        </View>
        <View style={styles.calibrationGuideSignalChip}>
          <Text style={styles.calibrationGuideSignalLabel}>Data</Text>
          <Text style={styles.calibrationGuideSignalValue}>{dataUpdatedAt}</Text>
        </View>
      </View>
      <View style={styles.calibrationGuideStateList}>
        {CALIBRATION_STATE_ENTRIES.map((entry) => {
          const isActive = currentState === entry.code;
          const isErrorState = entry.code === 6;
          const isDone =
            !isErrorState && currentState !== null && currentState > entry.code && currentState !== 6;
          const stepValue = entry.stepNumber === null ? "!" : String(entry.stepNumber);

          return (
            <View
              key={`calibration-state-${entry.code}`}
              style={[
                styles.calibrationGuideStateItem,
                isDone ? styles.calibrationGuideStateItemDone : null,
                isActive ? styles.calibrationGuideStateItemActive : null,
                isErrorState ? styles.calibrationGuideStateItemError : null,
              ]}
            >
              <Text
                style={[
                  styles.calibrationGuideStateCode,
                  isDone ? styles.calibrationGuideStateCodeDone : null,
                  isActive ? styles.calibrationGuideStateCodeActive : null,
                ]}
              >
                {stepValue}
              </Text>
              <View style={styles.calibrationGuideStateTextWrap}>
                <Text
                  style={[
                    styles.calibrationGuideStateLabel,
                    isDone ? styles.calibrationGuideStateLabelDone : null,
                    isActive ? styles.calibrationGuideStateLabelActive : null,
                  ]}
                >
                  {entry.label}
                </Text>
                <Text style={styles.calibrationGuideStateInstruction}>{entry.instruction}</Text>
                {entry.hint ? (
                  <Text style={styles.calibrationGuideStateHint}>{entry.hint}</Text>
                ) : null}
              </View>
            </View>
          );
        })}
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
