import { Feather } from "@expo/vector-icons";
import { useRouter } from "expo-router";
import { ScrollView, StyleSheet, Text, TouchableOpacity, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

type TutorialItem = {
  id: string;
  title: string;
  subtitle: string;
  accent: string;
  tileBg: string;
  showArrow?: boolean;
};

const useNotificationItems: TutorialItem[] = [
  {
    id: "charging",
    title: "Charging",
    subtitle: "Green blink",
    accent: "#6FBE96",
    tileBg: "#CFE9DE",
  },
  {
    id: "full",
    title: "Fully charged",
    subtitle: "Solid green",
    accent: "#6FBE96",
    tileBg: "#DDF2E7",
  },
  {
    id: "low-battery",
    title: "Low battery",
    subtitle: "Orange blink • 2 beeps",
    accent: "#F29A2D",
    tileBg: "#F8E5CC",
  },
  {
    id: "pairing",
    title: "Bluetooth pairing mode",
    subtitle: "Blue blink",
    accent: "#6A9BEF",
    tileBg: "#DDE8FA",
  },
  {
    id: "dose-due",
    title: "Dose due",
    subtitle: "Turquoise blink • 6 beeps",
    accent: "#74C6D2",
    tileBg: "#D9EEF2",
  },
];

const errorNotificationItems: TutorialItem[] = [
  {
    id: "extended-disconnect",
    title: "Extended Ring-Dock\ndisconnection",
    subtitle: "Violet blink • 5 beeps",
    accent: "#B46AE9",
    tileBg: "#EADBFA",
    showArrow: true,
  },
  {
    id: "device-error",
    title: "Device error",
    subtitle: "Red blink • 3 beeps",
    accent: "#EF5A5A",
    tileBg: "#F9DEE0",
    showArrow: true,
  },
];

export default function LedInfoScreen() {
  const router = useRouter();

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <View style={styles.headerRow}>
          <TouchableOpacity onPress={() => router.back()} style={styles.closeButton}>
            <Text style={styles.closeText}>Close</Text>
          </TouchableOpacity>
          <Text style={styles.headerTitle}>Device states tutorial</Text>
          <View style={styles.headerSpacer} />
        </View>

        <ScrollView
          showsVerticalScrollIndicator={false}
          contentContainerStyle={styles.scrollContent}
        >
          <Text style={styles.sectionTitle}>Use Notifications</Text>
          {useNotificationItems.map((item) => (
            <TutorialCard key={item.id} item={item} />
          ))}

          <Text style={[styles.sectionTitle, styles.sectionTitleError]}>
            Error Notifications
          </Text>
          {errorNotificationItems.map((item) => (
            <TutorialCard key={item.id} item={item} />
          ))}
        </ScrollView>
      </View>
    </SafeAreaView>
  );
}

function TutorialCard({ item }: { item: TutorialItem }) {
  const router = useRouter();
  const isErrorCard = item.showArrow === true;

  return (
    <TouchableOpacity
      activeOpacity={isErrorCard ? 0.75 : 1}
      disabled={!isErrorCard}
      onPress={() =>
        router.push(
          `/home/led-info-troubleshooting?errorId=${encodeURIComponent(item.id)}`
        )
      }
      style={styles.card}
    >
      <View style={[styles.thumbArea, { backgroundColor: item.tileBg }]}>
        <View style={styles.thumbBaseCircle} />
        <View style={styles.thumbDockBody} />
        <View style={styles.thumbCap} />
      </View>
      <View style={[styles.accentBar, { backgroundColor: item.accent }]} />
      <View style={styles.cardContent}>
        <Text style={styles.cardTitle}>{item.title}</Text>
        <Text style={styles.cardSubtitle}>{item.subtitle}</Text>
      </View>
      {item.showArrow ? (
        <Feather name="chevron-right" size={24} color="#8693A2" style={styles.cardArrow} />
      ) : null}
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: "#F3F4F6",
  },
  container: {
    flex: 1,
    paddingHorizontal: 14,
    paddingTop: 8,
  },
  headerRow: {
    height: 44,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  closeButton: {
    minWidth: 60,
  },
  closeText: {
    fontSize: 29 / 2,
    color: "#4D5A69",
    fontWeight: "400",
  },
  headerTitle: {
    fontSize: 31 / 2,
    color: "#252F3B",
    fontWeight: "700",
  },
  headerSpacer: {
    width: 60,
  },
  scrollContent: {
    paddingBottom: 24,
  },
  sectionTitle: {
    marginTop: 18,
    marginBottom: 8,
    fontSize: 20 / 1,
    fontWeight: "700",
    color: "#2D3745",
  },
  sectionTitleError: {
    marginTop: 10,
  },
  card: {
    height: 82,
    borderRadius: 10,
    borderWidth: 1,
    borderColor: "#DFE3E8",
    backgroundColor: "#F8F9FB",
    flexDirection: "row",
    alignItems: "center",
    overflow: "hidden",
    marginBottom: 8,
  },
  thumbArea: {
    width: 76,
    height: "100%",
    alignItems: "center",
    justifyContent: "flex-end",
    paddingBottom: 8,
  },
  thumbBaseCircle: {
    position: "absolute",
    width: 64,
    height: 26,
    borderRadius: 20,
    backgroundColor: "rgba(255,255,255,0.35)",
    bottom: 6,
  },
  thumbDockBody: {
    width: 30,
    height: 46,
    borderRadius: 14,
    borderWidth: 1,
    borderColor: "#A6B1BE",
    backgroundColor: "#F3F4F6",
    marginBottom: 2,
  },
  thumbCap: {
    position: "absolute",
    bottom: 40,
    width: 18,
    height: 10,
    borderRadius: 6,
    borderWidth: 1,
    borderColor: "#A6B1BE",
    backgroundColor: "#ECEFF2",
  },
  accentBar: {
    width: 10,
    height: "100%",
  },
  cardContent: {
    flex: 1,
    paddingHorizontal: 12,
    justifyContent: "center",
  },
  cardTitle: {
    color: "#4E5969",
    fontSize: 34 / 2,
    lineHeight: 22,
    fontWeight: "700",
  },
  cardSubtitle: {
    color: "#4E5969",
    fontSize: 34 / 2,
    lineHeight: 22,
    marginTop: 2,
  },
  cardArrow: {
    marginRight: 12,
  },
});
