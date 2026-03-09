import { Modal, Pressable, ScrollView, Text, View } from "react-native";

import { PpiType } from "@/utils/ble/messageProtocolPpi";

import { styles } from "../styles";
import type { FlowStepState, QuickFlowAction, QuickFlowMeta } from "../types";

type FlowProgress = {
  activeIndex: number;
  errorIndex: number | null;
};

type FlowHelpModalProps = {
  visible: boolean;
  selectedFlowMeta: QuickFlowMeta;
  selectedFlowAction: QuickFlowAction;
  selectedFlowStatus: string;
  flowStatusBadge: string;
  selectedFlowPayloadValueText: string;
  flowSteps: string[];
  flowProgress: FlowProgress;
  onClose: () => void;
};

export function FlowHelpModal({
  visible,
  selectedFlowMeta,
  selectedFlowAction,
  selectedFlowStatus,
  flowStatusBadge,
  selectedFlowPayloadValueText,
  flowSteps,
  flowProgress,
  onClose,
}: FlowHelpModalProps) {
  return (
    <Modal visible={visible} animationType="slide" transparent onRequestClose={onClose}>
      <View style={styles.flowModalBackdrop}>
        <Pressable style={styles.flowModalDismissArea} onPress={onClose} />
        <View style={styles.flowModalCard}>
          <View style={styles.flowModalHandle} />
          <View style={styles.flowModalHeader}>
            <View style={styles.flowModalHeaderTextWrap}>
              <Text style={styles.flowModalTitle}>Protocol Steps</Text>
              <Text style={styles.flowModalSubtitle}>{selectedFlowMeta.title} · Master Data Mode</Text>
            </View>
            <Pressable style={styles.flowModalClose} onPress={onClose}>
              <Text style={styles.flowModalCloseText}>Done</Text>
            </Pressable>
          </View>
          <View style={styles.flowModalMetaRow}>
            <View style={styles.flowModalMetaPill}>
              <Text style={styles.flowModalMetaText}>{selectedFlowMeta.ppiName}</Text>
            </View>
            <View style={styles.flowModalMetaPill}>
              <Text style={styles.flowModalMetaText}>{selectedFlowMeta.typeName}</Text>
            </View>
            <View style={[styles.flowModalMetaPill, styles.flowModalMetaPillStatus]}>
              <Text style={styles.flowModalMetaTextStatus}>{flowStatusBadge}</Text>
            </View>
          </View>
          <View style={styles.flowModalCurrentCard}>
            <Text style={styles.flowModalCurrentLabel}>
              {selectedFlowMeta.typeId === PpiType.PUSH ? "Value To Push" : "Payload Value"}
            </Text>
            <Text style={styles.flowModalCurrentText} selectable>
              {selectedFlowPayloadValueText}
            </Text>
          </View>
          <ScrollView
            style={styles.flowModalStepsScroll}
            contentContainerStyle={styles.flowStepsList}
            showsVerticalScrollIndicator={false}
          >
            {flowSteps.map((step, index) => {
              let stepState: FlowStepState = "pending";
              if (flowProgress.errorIndex !== null) {
                if (index < flowProgress.errorIndex) stepState = "done";
                else if (index === flowProgress.errorIndex) stepState = "error";
              } else if (selectedFlowStatus === "SENT_DATA") {
                stepState = "done";
              } else if (index < flowProgress.activeIndex) {
                stepState = "done";
              } else if (index === flowProgress.activeIndex) {
                stepState = "active";
              }

              return (
                <View key={`${selectedFlowAction}-${index}`} style={styles.flowStepRow}>
                  <View style={styles.flowStepRail}>
                    <View
                      style={[
                        styles.flowStepDot,
                        stepState === "active" && styles.flowStepDotActive,
                        stepState === "done" && styles.flowStepDotDone,
                        stepState === "error" && styles.flowStepDotError,
                      ]}
                    />
                    {index < flowSteps.length - 1 ? (
                      <View
                        style={[
                          styles.flowStepConnector,
                          stepState === "done" && styles.flowStepConnectorDone,
                          stepState === "error" && styles.flowStepConnectorError,
                        ]}
                      />
                    ) : null}
                  </View>
                  <View
                    style={[
                      styles.flowStepCard,
                      stepState === "active" && styles.flowStepCardActive,
                      stepState === "done" && styles.flowStepCardDone,
                      stepState === "error" && styles.flowStepCardError,
                    ]}
                  >
                    <Text
                      style={[
                        styles.flowStepText,
                        stepState === "active" && styles.flowStepTextActive,
                        stepState === "done" && styles.flowStepTextDone,
                        stepState === "error" && styles.flowStepTextError,
                      ]}
                    >
                      {index + 1}. {step}
                    </Text>
                  </View>
                </View>
              );
            })}
          </ScrollView>
        </View>
      </View>
    </Modal>
  );
}
