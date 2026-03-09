import { ActivityIndicator, Pressable, Text, View } from "react-native";

import { validoseButtonColor } from "@/constants/colors";

import { styles } from "../styles";

type QuickPpiButtonProps = {
  title: string;
  subtitle: string;
  onPress: () => void;
  onInfoPress?: () => void;
  loading?: boolean;
  disabled?: boolean;
  fullWidth?: boolean;
};

export function QuickPpiButton({
  title,
  subtitle,
  onPress,
  onInfoPress,
  loading,
  disabled,
  fullWidth,
}: QuickPpiButtonProps) {
  const isDisabled = Boolean(disabled);
  const isInactive = isDisabled || Boolean(loading);

  return (
    <Pressable
      onPress={onPress}
      disabled={isInactive}
      style={[
        styles.quickPpiButton,
        fullWidth ? styles.quickPpiButtonFullWidth : null,
        isDisabled && styles.quickPpiButtonDisabled,
      ]}
    >
      <View style={styles.quickPpiButtonTop}>
        <Text style={[styles.quickPpiButtonTitle, isDisabled && styles.quickPpiButtonTitleDisabled]}>
          {title}
        </Text>
        <View style={styles.quickPpiButtonActions}>
          <View style={styles.quickPpiLoaderSlot}>
            {loading ? <ActivityIndicator size="small" color={validoseButtonColor} /> : null}
          </View>
          {onInfoPress ? (
            <Pressable
              onPress={(event) => {
                event.stopPropagation();
                onInfoPress();
              }}
              hitSlop={6}
              style={[styles.quickPpiInfoButton, isDisabled && styles.quickPpiInfoButtonDisabled]}
            >
              <Text
                style={[
                  styles.quickPpiInfoButtonText,
                  isDisabled && styles.quickPpiInfoButtonTextDisabled,
                ]}
              >
                i
              </Text>
            </Pressable>
          ) : null}
        </View>
      </View>
      <Text style={[styles.quickPpiButtonSubtitle, isDisabled && styles.quickPpiButtonSubtitleDisabled]}>
        {subtitle}
      </Text>
    </Pressable>
  );
}
