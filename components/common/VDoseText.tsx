import { StyleSheet } from "react-native";
import { TextStyle } from "react-native/Libraries/StyleSheet/StyleSheetTypes";
import { Text, TextProps } from "react-native-paper";

interface VDoseTextProps {
  color?: string;
}

export function VDoseText(
  props: VDoseTextProps & TextProps<string> & TextStyle
) {
  const styles = StyleSheet.create({
    text: {
      width: "100%",
      color: props.color,
      textAlign: "center",
      fontSize: 25,
      fontWeight: "700",
    },
  });

  return (
    <Text style={styles.text} {...props}>
      {props.children}
    </Text>
  );
}
