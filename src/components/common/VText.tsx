import { StyleSheet } from "react-native";
import { TextStyle } from "react-native/Libraries/StyleSheet/StyleSheetTypes";
import { Text, TextProps } from "react-native-paper";

import {
  validoseAqua,
  validoseAqua3,
  validoseDarkBlue,
  validoseGrey,
  validoseWhite,
} from "@/constants/colors";

interface VTextProps {
  textVariant:
    | "Body"
    | "Button"
    | "Header"
    | "Disabled"
    | "Label"
    | "LabelUnderline"
    | "LabelDose"
    | "LabelMedicine"
    | "LabelMedicineBold"
    | "LabelMedicine1"
    | "LabelMedicine2"
    | "LabelMedicine1Dark"
    | "LabelMedicine2Dark"
    | "DeviceItem"
    | "DeviceItemState";
}

export function VText({
  textVariant,
  style,
  children,
  ...rest
}: VTextProps & TextProps<string> & TextStyle) {
  return (
    <Text style={[styles.textBase, styles[`text${textVariant}`], style]} {...rest}>
      {children}
    </Text>
  );
}

const styles = StyleSheet.create({
  textBase: {
    fontFamily: "Inter",
  },
  textBody: {
    width: "100%",
    color: validoseDarkBlue,
    textAlign: "center",
    fontSize: 35,
    fontWeight: "700",
  },
  textButton: {
    width: "100%",
    color: validoseAqua3,
    textAlign: "center",
    fontSize: 16,
  },
  textHeader: {
    width: "100%",
    color: validoseAqua3,
    textAlign: "center",
    fontSize: 20,
  },
  textDisabled: {
    width: "100%",
    color: validoseAqua,
    textAlign: "center",
    fontSize: 16,
  },
  textLabel: {
    width: "100%",
    color: validoseDarkBlue,
    textAlign: "center",
    fontSize: 20,
  },
  textLabelUnderline: {
    width: "100%",
    color: validoseDarkBlue,
    textAlign: "center",
    fontSize: 16,
    textDecorationLine: "underline",
    fontWeight: "500",
  },
  textLabelDose: {
    width: "100%",
    color: validoseGrey,
    textAlign: "center",
    fontSize: 16,
  },
  textLabelMedicine: {
    width: "85%",
    color: validoseDarkBlue,
    textAlign: "left",
    fontSize: 16,
  },
  textLabelMedicineBold: {
    width: "85%",
    color: validoseDarkBlue,
    textAlign: "left",
    fontSize: 16,
    fontWeight: "bold",
  },
  textLabelMedicine1: {
    color: validoseWhite,
    fontSize: 12,
    fontWeight: "500",
    textAlign: "center",
  },
  textLabelMedicine2: {
    width: "100%",
    color: validoseWhite,
    textAlign: "center",
    fontSize: 32,
    fontWeight: "600",
  },
  textLabelMedicine1Dark: {
    width: "100%",
    color: validoseDarkBlue,
    textAlign: "center",
    fontSize: 16,
  },
  textLabelMedicine2Dark: {
    width: "100%",
    color: validoseDarkBlue,
    textAlign: "center",
    fontSize: 25,
    fontWeight: "bold",
  },
  textDeviceItem: {
    color: validoseDarkBlue,
    fontWeight: "500",
    fontSize: 16,
  },
  textDeviceItemState: {
    color: validoseAqua3,
    fontWeight: "500",
    fontSize: 16,
  },
});
