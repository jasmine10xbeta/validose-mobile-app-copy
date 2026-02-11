import { StyleSheet } from "react-native";
import Toast, {
  BaseToast,
  ErrorToast,
  ToastConfig,
} from "react-native-toast-message";

export type ToastType = "success" | "error" | "info";

const styles = StyleSheet.create({
  content: {
    flex: 1,
  },
  text1: {
    fontSize: 15,
    color: "#0F172A",
    flexWrap: "wrap",
  },
  text2: {
    fontSize: 13,
    color: "#1F2937",
    flexWrap: "wrap",
    paddingTop: 4,
  },
});

type Accent = "success" | "info" | "error";

const accentColors: Record<Accent, string> = {
  success: "#2EC4B6",
  info: "#3F83F8",
  error: "#EF4444",
};

const renderBaseToast = (accent: Accent) =>
  function render(props: any) {
    return (
      <BaseToast
        {...props}
        style={{
          height: "auto",
          paddingTop: 16,
          paddingBottom: 16,
          borderLeftColor: accentColors[accent],
        }}
        contentContainerStyle={styles.content}
        text1NumberOfLines={3}
        text2NumberOfLines={4}
        text1Style={styles.text1}
        text2Style={styles.text2}
      />
    );
  };

const renderErrorToast = (props: any) => (
  <ErrorToast
    {...props}
    style={{
      height: "auto",
      paddingTop: 16,
      paddingBottom: 16,
      borderLeftColor: accentColors.error,
    }}
    contentContainerStyle={styles.content}
    text1NumberOfLines={3}
    text2NumberOfLines={4}
    text1Style={styles.text1}
    text2Style={styles.text2}
  />
);

export const toastConfig: ToastConfig = {
  success: renderBaseToast("success"),
  info: renderBaseToast("info"),
  error: renderErrorToast,
};

export function showToast(type: ToastType, title: string, message?: string) {
  Toast.show({
    type,
    text1: title,
    text2: message,
    topOffset: 50,
    visibilityTime: 6000,
    autoHide: true,
  });
}
