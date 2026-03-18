import { StyleSheet } from "react-native";
import { TextStyle } from "react-native/Libraries/StyleSheet/StyleSheetTypes";
import { Text, TextProps } from "react-native-paper";

interface VDoseTextProps {
  color?: string;
}

export function VDoseText(
  props: VDoseTextProps & TextProps<string> & TextStyle
) {
  const styles = createStyles(props.color);

  return (
    <Text style={styles.text} {...props}>
      {props.children}
    </Text>
  );
}

const createStyles = (color?: string) =>
  StyleSheet.create({
    text: {
      color: color,
      textAlign: "center",
      fontSize: 22,
      fontWeight: "500",
    },
  });
