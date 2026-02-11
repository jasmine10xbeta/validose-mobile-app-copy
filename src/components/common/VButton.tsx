import { StyleSheet } from "react-native";
import { Button, ButtonProps } from "react-native-paper";
import { validoseAqua, validoseAqua3 } from "@/constants/colors";
import { VText } from "./VText";

interface VButtonProps {
  label: string;
  labelStyle?: object;
}

export function VButton(props: VButtonProps & Omit<ButtonProps, "children">) {
  return (
    <Button
      rippleColor="transparent"
      mode="outlined"
      style={props.disabled ? styles.buttonDisabled : styles.button}
      {...props}
    >
      <VText style={[styles.buttonText, props.labelStyle]} textVariant={props.disabled ? "Disabled" : "Button"}>
        {props.label}
      </VText>
    </Button>
  );
}

const styles = StyleSheet.create({
  button: {
    borderColor: validoseAqua,
    borderWidth: 2,
    borderRadius: 25,
    padding: 5,
    width: "100%",
    textDecorationColor: validoseAqua3,
  },
  buttonDisabled: {
    backgroundColor: validoseAqua3,
    borderColor: validoseAqua3,
    borderWidth: 2,
    borderRadius: 25,
    padding: 5,
    width: "100%",
    textDecorationColor: validoseAqua3,
  },
  buttonText: {
    fontSize: 16,
    fontWeight: "500",
    // fontFamily: "Inter",
    color: validoseAqua3
  }
});
