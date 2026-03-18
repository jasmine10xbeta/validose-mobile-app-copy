import { StyleSheet, View } from "react-native";

interface VDoseLineProps {
  state: number;
  color: string;
}

// Helper to apply 60% opacity to hex color
function withOpacity(hex: string, opacity: number = 0.4) {
  const alpha = Math.round(opacity * 255)
    .toString(16)
    .padStart(2, "0");
  return hex.length === 7 ? `${hex}${alpha}` : hex;
}

export function VDoseLine(props: VDoseLineProps) {
  const styles = createStyles(props.state, props.color);
  return <View style={styles.doseItem} />;
}

const createStyles = (state: number, color: string) => {
  const adjustedColor = state === 0 ? withOpacity(color, 0.4) : color;

  return StyleSheet.create({
    doseItem: {
      width: 26,
      height: 0,
      alignSelf: "center",
      borderWidth: 1,
      borderStyle: state === 0 ? "dashed" : "solid",
      borderColor: adjustedColor,
    },
  });
};
