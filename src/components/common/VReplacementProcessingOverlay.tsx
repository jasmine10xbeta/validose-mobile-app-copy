import { ActivityIndicator, StyleSheet, Text, TouchableOpacity, View } from "react-native";

interface VReplacementProcessingOverlayProps {
  visible: boolean;
  onReturnToChangingFlow: () => void;
}

export function VReplacementProcessingOverlay({
  visible,
  onReturnToChangingFlow,
}: VReplacementProcessingOverlayProps) {
  if (!visible) return null;

  return (
    <View style={styles.overlay}>
      <ActivityIndicator size="small" color="#FFFFFF" />
      <Text style={styles.message}>
        To cancel the replacement flow{"\n"}
        put ring back on the old bottle{"\n"}
        and place bottle in the dock.
      </Text>
      <TouchableOpacity onPress={onReturnToChangingFlow}>
        <Text style={styles.linkText}>Click to return to changing flow</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  overlay: {
    ...StyleSheet.absoluteFillObject,
    zIndex: 10,
    backgroundColor: "rgba(0,0,0,0.48)",
    justifyContent: "center",
    alignItems: "center",
    paddingHorizontal: 24,
  },
  message: {
    marginTop: 16,
    textAlign: "center",
    color: "#FFFFFF",
    fontSize: 33 / 2,
    lineHeight: 44 / 2,
    fontWeight: "500",
  },
  linkText: {
    marginTop: 10,
    color: "#FFFFFF",
    textDecorationLine: "underline",
    fontSize: 31 / 2,
    fontWeight: "500",
  },
});
