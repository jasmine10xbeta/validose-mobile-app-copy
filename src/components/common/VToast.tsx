import { Feather } from "@expo/vector-icons";
import { StyleSheet, Text, View } from "react-native";
import Toast, { ToastConfig } from "react-native-toast-message";

export type ToastType = "success" | "error" | "info";

type ToastVisual = {
  circleBackground: string;
  circleIconColor: string;
  iconName: React.ComponentProps<typeof Feather>["name"];
};

const visuals: Record<ToastType, ToastVisual> = {
  success: {
    circleBackground: "#72C992",
    circleIconColor: "#0E6B3A",
    iconName: "bell",
  },
  info: {
    circleBackground: "#CFE1FF",
    circleIconColor: "#345EAA",
    iconName: "info",
  },
  error: {
    circleBackground: "#FFD7D7",
    circleIconColor: "#B42323",
    iconName: "alert-circle",
  },
};

type ToastCardProps = {
  type: ToastType;
  text1?: string;
  text2?: string;
};

function ToastCard({ type, text1, text2 }: ToastCardProps) {
  const visual = visuals[type];

  return (
    <View style={styles.outerShell}>
      <View style={styles.toastCard}>
        <View style={styles.textColumn}>
          {text1 ? <Text style={styles.title}>{text1}</Text> : null}
          {text2 ? <Text style={styles.subtitle}>{text2}</Text> : null}
        </View>
        <View style={[styles.iconCircle, { backgroundColor: visual.circleBackground }]}>
          <Feather name={visual.iconName} size={22} color={visual.circleIconColor} />
        </View>
      </View>
    </View>
  );
}

function SuccessToast(props: { text1?: string; text2?: string }) {
  return <ToastCard type="success" text1={props.text1} text2={props.text2} />;
}

function InfoToast(props: { text1?: string; text2?: string }) {
  return <ToastCard type="info" text1={props.text1} text2={props.text2} />;
}

function ErrorToastCard(props: { text1?: string; text2?: string }) {
  return <ToastCard type="error" text1={props.text1} text2={props.text2} />;
}

export const toastConfig: ToastConfig = {
  success: SuccessToast,
  info: InfoToast,
  error: ErrorToastCard,
};

export function showToast(type: ToastType, title: string, message?: string) {
  Toast.show({
    type,
    text1: title,
    text2: message,
    topOffset: 68,
    visibilityTime: 5000,
    autoHide: true,
  });
}

const styles = StyleSheet.create({
  outerShell: {
    width: "100%",
    alignItems: "center",
  },
  toastCard: {
    width: "90%",
    borderRadius: 14,
    borderWidth: 1.5,
    marginTop: 12,
    borderColor: "#252F3B0A",
    backgroundColor: "#FFFFFF",
    paddingLeft: 14,
    paddingRight: 12,
    paddingVertical: 10,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  textColumn: {
    flex: 1,
    paddingRight: 10,
  },
  title: {
    color: "#1D252E",
    fontSize: 16,
    fontWeight: "500",
    marginBottom: 3,
  },
  subtitle: {
    color: "#505A66",
    fontSize: 16,
    fontWeight: "400",
  },
  iconCircle: {
    width: 50,
    height: 50,
    borderRadius: 25,
    alignItems: "center",
    justifyContent: "center",
  },
});
