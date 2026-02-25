import { ActivityIndicator, Pressable, StyleProp, Text, View, ViewStyle } from "react-native";

import { validoseWhite } from "@/constants/colors";

import { styles } from "../styles";

type ActionButtonProps = {
  label: string;
  onPress: () => void;
  disabled?: boolean;
  loading?: boolean;
  tone?: "default" | "danger";
  style?: StyleProp<ViewStyle>;
};

export function ActionButton({
  label,
  onPress,
  disabled,
  loading,
  tone = "default",
  style,
}: ActionButtonProps) {
  return (
    <Pressable
      onPress={onPress}
      disabled={disabled || loading}
      style={[
        styles.actionButton,
        style,
        tone === "danger" && styles.actionButtonDanger,
        (disabled || loading) && styles.actionButtonDisabled,
      ]}
    >
      <View style={styles.actionButtonInner}>
        {loading ? <ActivityIndicator size="small" color={validoseWhite} /> : null}
        <Text
          style={[
            styles.actionButtonText,
            tone === "danger" && styles.actionButtonTextDanger,
          ]}
        >
          {loading ? "Working..." : label}
        </Text>
      </View>
    </Pressable>
  );
}
