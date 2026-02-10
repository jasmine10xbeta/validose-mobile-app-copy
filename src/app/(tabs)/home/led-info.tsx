import { useRouter } from "expo-router";
import { StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VText } from "@/components/common/VText";

const LED_INFO = [
  {
    title: "Red blink, 3 beeps",
    description: "Take missed dose",
  },
  {
    title: "Turquoise long blink, 6 beeps",
    description: "Take your dose",
  },
  {
    title: "Slow blue blink",
    description: "Device is in pairing mode",
  },
  {
    title: "Slow orange blinks, 3 beeps",
    description: "Device is in pairing mode",
  },
  {
    title: "Green slow blink",
    description: "Device is charging",
  },
    {
    title: "Solid green",
    description: "Device is fully charged",
  },
];

export default function LedInfoScreen() {
  const router = useRouter();

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.container}>
        <VText textVariant="Body">Device states tutorial</VText>

        <View style={styles.ledList}>
          {LED_INFO.map((item) => (
            <View key={item.title} style={styles.ledRow}>
              <VText textVariant="LabelMedicineBold" style={styles.ledTitle}>
                {item.title}
              </VText>
              <VText textVariant="LabelMedicineBold" style={styles.ledTitle}>{item.description}</VText>
            </View>
          ))}
        </View>

        <VButton onPress={() => router.back()} label="Close tutorial" />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
  },
  container: {
    flex: 1,
    alignContent: "center",
    justifyContent: "center",
    paddingHorizontal: 24,
    paddingVertical: 32,
    gap: 6,
  },
  subtitle: {
    textAlign: "left",
  },
  ledList: {
    flex: 1,
    gap: 6,
    marginTop: 12,
  },
  ledRow: {
    borderWidth: 1,
    borderColor: "#E2E8F0",
    borderRadius: 12,
    padding: 16,
    gap: 6,
  },
  ledTitle: {
    textAlign: "left",
  },
  backButton: {
    marginTop: 16,
  },
});
