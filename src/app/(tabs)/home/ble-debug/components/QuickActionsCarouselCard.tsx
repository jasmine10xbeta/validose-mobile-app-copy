import {
  type NativeScrollEvent,
  type NativeSyntheticEvent,
  ScrollView,
  Text,
  TextInput,
  View,
} from "react-native";

import { QUICK_FLOW_META } from "../constants";
import { styles } from "../styles";
import { BaseliningGuideCard } from "./BaseliningGuideCard";
import { CalibrationGuideCard } from "./CalibrationGuideCard";
import { CompactFeedbackCard } from "./CompactFeedbackCard";
import { QuickPpiButton } from "./QuickPpiButton";
import type {
  BaseliningFeedbackSnapshot,
  BaseliningGuideStage,
  CalibrationDataSnapshot,
  CalibrationFeedbackSnapshot,
  CalibrationGuideStage,
  CompactFeedbackPreview,
  QuickActionPage,
  QuickFlowAction,
} from "../types";

type QuickActionsCarouselCardProps = {
  quickActionPages: QuickActionPage[];
  quickActionsViewportHeight?: number;
  quickActionsViewportWidth: number;
  quickActionsPageIndex: number;
  loadingAction: string | null;
  isConnected: boolean;
  developmentCmdInput: string;
  developmentCmdInputError: string;
  calibrationWeightInput: string;
  calibrationWeightInputError: string;
  calibrationGuideStage: CalibrationGuideStage;
  calibrationGuideStageLabel: string;
  calibrationGuideInstruction: string;
  calibrationCompletionMessage: string;
  calibrationDataSnapshot: CalibrationDataSnapshot | null;
  calibrationFeedbackSnapshot: CalibrationFeedbackSnapshot | null;
  calibrationFeedbackPreview: CompactFeedbackPreview | null;
  calibrationResponsePreview: CompactFeedbackPreview | null;
  baseliningGuideStage: BaseliningGuideStage;
  baseliningGuideInstruction: string;
  baseliningFeedbackSnapshot: BaseliningFeedbackSnapshot | null;
  baseliningFeedbackPreview: CompactFeedbackPreview | null;
  baseliningResponsePreview: CompactFeedbackPreview | null;
  onViewportWidthChange: (width: number) => void;
  onQuickActionsMomentumEnd: (event: NativeSyntheticEvent<NativeScrollEvent>) => void;
  onQuickActionPageLayout: (pageId: string, height: number) => void;
  onDevelopmentCmdInputChange: (text: string) => void;
  onCalibrationWeightInputChange: (text: string) => void;
  onQuickActionPress: (action: QuickFlowAction) => void;
  onQuickActionInfoPress: (action: QuickFlowAction) => void;
  quickActionSubtitle: (action: QuickFlowAction) => string;
  isBlockedByOtherAction: (action: string) => boolean;
  isCalibrationActionEnabled: (action: QuickFlowAction) => boolean;
  isBaseliningActionEnabled: (action: QuickFlowAction) => boolean;
};

export function QuickActionsCarouselCard({
  quickActionPages,
  quickActionsViewportHeight,
  quickActionsViewportWidth,
  quickActionsPageIndex,
  loadingAction,
  isConnected,
  developmentCmdInput,
  developmentCmdInputError,
  calibrationWeightInput,
  calibrationWeightInputError,
  calibrationGuideStage,
  calibrationGuideStageLabel,
  calibrationGuideInstruction,
  calibrationCompletionMessage,
  calibrationDataSnapshot,
  calibrationFeedbackSnapshot,
  calibrationFeedbackPreview,
  calibrationResponsePreview,
  baseliningGuideStage,
  baseliningGuideInstruction,
  baseliningFeedbackSnapshot,
  baseliningFeedbackPreview,
  baseliningResponsePreview,
  onViewportWidthChange,
  onQuickActionsMomentumEnd,
  onQuickActionPageLayout,
  onDevelopmentCmdInputChange,
  onCalibrationWeightInputChange,
  onQuickActionPress,
  onQuickActionInfoPress,
  quickActionSubtitle,
  isBlockedByOtherAction,
  isCalibrationActionEnabled,
  isBaseliningActionEnabled,
}: QuickActionsCarouselCardProps) {
  return (
    <View style={styles.section}>
      <Text style={styles.sectionTitle}>Quick Actions</Text>
      <View
        style={[
          styles.quickPpiCarouselViewport,
          quickActionsViewportHeight ? { height: quickActionsViewportHeight } : null,
        ]}
        onLayout={(event) => {
          const nextWidth = event.nativeEvent.layout.width;
          if (nextWidth > 0 && Math.abs(nextWidth - quickActionsViewportWidth) > 1) {
            onViewportWidthChange(nextWidth);
          }
        }}
      >
        <ScrollView
          horizontal
          pagingEnabled
          decelerationRate="fast"
          showsHorizontalScrollIndicator={false}
          onMomentumScrollEnd={onQuickActionsMomentumEnd}
          contentContainerStyle={styles.quickPpiCarouselContent}
        >
          {quickActionPages.map((page) => (
            <View
              key={page.id}
              style={[
                styles.quickPpiPage,
                quickActionsViewportWidth ? { width: quickActionsViewportWidth } : null,
              ]}
              onLayout={(event) => {
                onQuickActionPageLayout(page.id, event.nativeEvent.layout.height);
              }}
            >
              <Text style={styles.quickPpiPageTitle}>{page.title}</Text>
              {page.id === "developer" ? (
                (() => {
                  const developerAction = page.actions[0];
                  if (!developerAction) {
                    return null;
                  }

                  const meta = QUICK_FLOW_META[developerAction.key];
                  const busyKey = meta.busyKey;
                  const isDisabled = !isConnected || isBlockedByOtherAction(busyKey);

                  return (
                    <View style={styles.quickDevCommandCard}>
                      <View style={styles.quickDevCommandHeader}>
                        <Text style={styles.quickDevCommandTitle}>Development Command</Text>
                        <Text style={styles.quickDevCommandMeta}>AD_DEVELOPMENT_CMD · RQ</Text>
                      </View>
                      <View
                        style={[
                          styles.quickDevInlineInputWrap,
                          developmentCmdInputError ? styles.quickDevInlineInputWrapError : null,
                        ]}
                      >
                        <Text style={styles.quickDevInlineInputPrefix}>uint8</Text>
                        <TextInput
                          value={developmentCmdInput}
                          onChangeText={onDevelopmentCmdInputChange}
                          placeholder="e.g. 12 or 0x0C"
                          placeholderTextColor="#98A2B3"
                          keyboardType="default"
                          autoCapitalize="none"
                          autoCorrect={false}
                          style={styles.quickDevInlineInput}
                        />
                      </View>
                      {developmentCmdInputError ? (
                        <Text style={styles.quickDevInputErrorText}>{developmentCmdInputError}</Text>
                      ) : (
                        <Text style={styles.quickDevInputHint}>
                          Decimal 0-255 or hex 0x00-0xFF.
                        </Text>
                      )}
                      <QuickPpiButton
                        title={meta.title}
                        subtitle={quickActionSubtitle(developerAction.key)}
                        onPress={() => {
                          onQuickActionPress(developerAction.key);
                        }}
                        onInfoPress={() => onQuickActionInfoPress(developerAction.key)}
                        disabled={isDisabled}
                        loading={loadingAction === busyKey}
                        fullWidth
                      />
                    </View>
                  );
                })()
              ) : (
                <>
                  {page.id === "calibration" ? (
                    <View style={styles.quickDevCommandCard}>
                      <View style={styles.quickDevCommandHeader}>
                        <Text style={styles.quickDevCommandTitle}>Calibration Weight</Text>
                        <Text style={styles.quickDevCommandMeta}>mg · uint32</Text>
                      </View>
                      <View
                        style={[
                          styles.quickDevInlineInputWrap,
                          calibrationWeightInputError ? styles.quickDevInlineInputWrapError : null,
                        ]}
                      >
                        <Text style={styles.quickDevInlineInputPrefix}>mg</Text>
                        <TextInput
                          value={calibrationWeightInput}
                          onChangeText={onCalibrationWeightInputChange}
                          placeholder="e.g. 5000"
                          placeholderTextColor="#98A2B3"
                          keyboardType="number-pad"
                          autoCapitalize="none"
                          autoCorrect={false}
                          style={styles.quickDevInlineInput}
                        />
                      </View>
                      {calibrationWeightInputError ? (
                        <Text style={styles.quickDevInputErrorText}>
                          {calibrationWeightInputError}
                        </Text>
                      ) : (
                        <Text style={styles.quickDevInputHint}>
                          Used by Start/Stop Calibration payload.
                        </Text>
                      )}
                    </View>
                  ) : null}
                  <View style={styles.quickPpiGrid}>
                    {page.actions.map((item) => {
                      const meta = QUICK_FLOW_META[item.key];
                      const busyKey = meta.busyKey;
                      const isGuidedDisabled =
                        (page.id === "calibration" && !isCalibrationActionEnabled(item.key)) ||
                        (page.id === "baselining" && !isBaseliningActionEnabled(item.key));
                      return (
                        <QuickPpiButton
                          key={item.key}
                          title={meta.title}
                          subtitle={quickActionSubtitle(item.key)}
                          onPress={() => {
                            onQuickActionPress(item.key);
                          }}
                          onInfoPress={() => onQuickActionInfoPress(item.key)}
                          disabled={!isConnected || isBlockedByOtherAction(busyKey) || isGuidedDisabled}
                          loading={loadingAction === busyKey}
                        />
                      );
                    })}
                  </View>
                </>
              )}
              {page.id === "calibration" ? (
                <View style={styles.quickPageFeedbackWrap}>
                  <CalibrationGuideCard
                    stage={calibrationGuideStage}
                    stageLabel={calibrationGuideStageLabel}
                    instruction={calibrationGuideInstruction}
                    completionMessage={calibrationCompletionMessage}
                    feedbackSnapshot={calibrationFeedbackSnapshot}
                    dataSnapshot={calibrationDataSnapshot}
                  />
                  <CompactFeedbackCard
                    title="Calibration"
                    feedbackPreview={calibrationFeedbackPreview}
                    responsePreview={calibrationResponsePreview}
                  />
                </View>
              ) : null}
              {page.id === "baselining" ? (
                <View style={styles.quickPageFeedbackWrap}>
                  <BaseliningGuideCard
                    stage={baseliningGuideStage}
                    instruction={baseliningGuideInstruction}
                    feedbackSnapshot={baseliningFeedbackSnapshot}
                  />
                  <CompactFeedbackCard
                    title="Baselining"
                    feedbackPreview={baseliningFeedbackPreview}
                    responsePreview={baseliningResponsePreview}
                  />
                </View>
              ) : null}
            </View>
          ))}
        </ScrollView>
      </View>
      {quickActionPages.length > 1 ? (
        <View style={styles.quickPpiPager}>
          {quickActionPages.map((page, index) => (
            <View
              key={`quick-actions-page-${page.id}`}
              style={[
                styles.quickPpiPagerDot,
                quickActionsPageIndex === index && styles.quickPpiPagerDotActive,
              ]}
            />
          ))}
        </View>
      ) : null}
    </View>
  );
}
