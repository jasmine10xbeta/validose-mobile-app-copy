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
};

export function QuickPpiButton({
  title,
  subtitle,
  onPress,
  onInfoPress,
  loading,
  disabled,
}: QuickPpiButtonProps) {
  return (
    <Pressable
      onPress={onPress}
      disabled={disabled || loading}
      style={[styles.quickPpiButton, (disabled || loading) && styles.quickPpiButtonDisabled]}
    >
      <View style={styles.quickPpiButtonTop}>
        <Text style={styles.quickPpiButtonTitle}>{title}</Text>
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
              style={styles.quickPpiInfoButton}
            >
              <Text style={styles.quickPpiInfoButtonText}>i</Text>
            </Pressable>
          ) : null}
        </View>
      </View>
      <Text style={styles.quickPpiButtonSubtitle}>{subtitle}</Text>
    </Pressable>
  );
}
