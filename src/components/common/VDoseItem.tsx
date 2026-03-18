import { StyleSheet, View } from "react-native";
import MissedDose from "@/assets/images/svg/missed-dose-indicator.svg";
import { validoseWhite } from "@/constants/colors";
import { VDoseText } from "./VDoseText";

interface VDoseItemProps {
  doseNumber: number;
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

export function VDoseItem(props: VDoseItemProps) {
  const isUpcoming = props.state === 0;
  const isTaken = props.state === 2;
  const isMissed = props.state === 4;
  const isUnknown = props.state === 5;
  
  const borderColor = isUpcoming ? withOpacity(props.color, 0.4) : props.color;
  const textColor = isTaken ? validoseWhite : borderColor;

  const styles = StyleSheet.create({
    doseItem: {
      borderRadius: 20,
      height: 40,
      width: 40,
      borderWidth: isUpcoming ? 1 : 1.5,
      justifyContent: "center",
      borderStyle: isUpcoming ? "dashed" : "solid",
      borderColor,
      backgroundColor: isTaken ? props.color : undefined,
    },
    missedDoseContainer: {
      justifyContent: "center",
      alignItems: "center",
    },
  });

  return (
    <View style={styles.doseItem}>
      {isMissed || isUnknown ? (
        <View style={styles.missedDoseContainer}>
          <MissedDose width={28} height={28} color={props.color} />
        </View>
      ) : (
        <VDoseText color={textColor}>
          {isTaken ? "✓" : props.doseNumber}
        </VDoseText>
      )}
    </View>
  );
}
