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
  const isDisabled = Boolean(disabled);
  const isInactive = isDisabled || Boolean(loading);

  return (
    <Pressable
      onPress={onPress}
      disabled={isInactive}
      style={[
        styles.actionButton,
        style,
        tone === "danger" && styles.actionButtonDanger,
        isDisabled && styles.actionButtonDisabled,
      ]}
    >
      <View style={styles.actionButtonInner}>
        {loading ? <ActivityIndicator size="small" color={validoseWhite} /> : null}
        <Text
          style={[
            styles.actionButtonText,
            tone === "danger" && styles.actionButtonTextDanger,
            isDisabled && styles.actionButtonTextDisabled,
          ]}
        >
          {loading ? "Working..." : label}
        </Text>
      </View>
    </Pressable>
  );
}
