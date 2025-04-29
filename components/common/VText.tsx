import { StyleSheet } from "react-native";
import { TextStyle } from "react-native/Libraries/StyleSheet/StyleSheetTypes";
import { Text, TextProps } from "react-native-paper";
import {
  validoseAqua,
  validoseAqua3,
  validoseDarkBlue,
  validoseGrey,
  validoseWhite,
} from "@/constants/Colors";

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

export function VText(props: VTextProps & TextProps<string> & TextStyle) {
  return (
    <Text style={styles[`text${props.textVariant}`]} {...props}>
      {props.children}
    </Text>
  );
}

const styles = StyleSheet.create({
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
    width: "100%",
    color: validoseWhite,
    textAlign: "center",
    fontSize: 16,
  },
  textLabelMedicine2: {
    width: "100%",
    color: validoseWhite,
    textAlign: "center",
    fontSize: 25,
    fontWeight: "bold",
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
    fontWeight: "700",
    fontSize: 16,
  },
  textDeviceItemState: {
    color: validoseAqua,
    fontWeight: "500",
    fontSize: 16,
  },
});
