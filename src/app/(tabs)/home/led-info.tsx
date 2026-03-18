import { Feather } from "@expo/vector-icons";
import { useRouter } from "expo-router";
import {
  FlatList,
  Image,
  type ImageSourcePropType,
  StyleSheet,
  Text,
  TouchableOpacity,
  TouchableWithoutFeedback,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VText } from "@/components/common/VText";

type LedInfoItem = {
  title: string;
  description: string;
  pipeColor: string;
  pipeBreaks: number;
  imageSource: ImageSourcePropType;
};

const LED_INFO: LedInfoItem[] = [
  {
    title: "Charging",
    description: "Green blink",
    pipeColor: "#65BB8D",
    pipeBreaks: 2,
    imageSource: require("../../../assets/images/png/device-charging.png"),
  },
  {
    title: "Fully charged",
    description: "Solid green",
    pipeColor: "#65BB8D",
    pipeBreaks: 0,
    imageSource: require("../../../assets/images/png/device-charged.png"),
  },
  {
    title: "Low battery",
    description: "Orange blink • 2 beeps",
    pipeColor: "#F09525",
    pipeBreaks: 1,
    imageSource: require("../../../assets/images/png/device-low-battery.png"),
  },
  {
    title: "Bluetooth pairing mode",
    description: "Blue blink",
    pipeColor: "#5D9BFF",
    pipeBreaks: 0,
    imageSource: require("../../../assets/images/png/device-pairing-mode.png"),
  },
  {
    title: "Dose due",
    description: "Turquoise blink • 6 beeps",
    pipeColor: "#73D0D7",
    pipeBreaks: 6,
    imageSource: require("../../../assets/images/png/device-dose-due.png"),
  }
];

export default function LedInfoScreen() {
  const router = useRouter();

  return (
    <View style={styles.modalRoot}>
      <TouchableWithoutFeedback onPress={() => router.back()}>
        <View style={styles.backdrop} />
      </TouchableWithoutFeedback>

      <SafeAreaView edges={["bottom"]} style={styles.sheetContainer}>
        <View style={styles.headerRow}>
          <TouchableOpacity
            onPress={() => router.back()}
            accessibilityRole="button"
            accessibilityLabel="Close tutorial"
            hitSlop={{ top: 8, bottom: 8, left: 8, right: 8 }}
            style={styles.closeButton}
          >
            <Text style={styles.closeLabel}>Close</Text>
          </TouchableOpacity>
          <Text style={styles.headerTitle}>Device states tutorial</Text>
          <View style={styles.headerSpacer} />
        </View>

        <FlatList
          data={LED_INFO}
          keyExtractor={(item) => item.title}
          contentContainerStyle={styles.listContent}
          showsVerticalScrollIndicator={false}
          renderItem={({ item }) => (
            <View style={styles.ledCard}>
              <View style={styles.cardRow}>
                <View style={styles.iconPane}>
                  <Image
                    source={item.imageSource}
                    style={styles.iconImage}
                    resizeMode="contain"
                  />
                </View>
                <View style={styles.cardAccentTrack}>
                  {Array.from({ length: Math.max(1, item.pipeBreaks + 1) }).map((_, index, all) => {
                    const isFirstSegment = index === 0;
                    const isLastSegment = index === all.length - 1;

                    return (
                      <View
                        key={`${item.title}-pipe-${index}`}
                        style={[
                          styles.cardAccentSegment,
                          { backgroundColor: item.pipeColor },
                          isFirstSegment ? styles.cardAccentSegmentFirst : null,
                          isLastSegment ? styles.cardAccentSegmentLast : null,
                          index < item.pipeBreaks ? styles.cardAccentSegmentGap : null,
                        ]}
                      />
                    );
                  })}
                </View>
                <View style={styles.copyWrap}>
                  <VText textVariant="LabelMedicineBold" style={styles.ledTitle}>
                    {item.title}
                  </VText>
                  <VText textVariant="Body" style={styles.ledDescription}>
                    {item.description}
                  </VText>
                </View>
              </View>
            </View>
          )}
        />
      </SafeAreaView>
    </View>
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
  modalRoot: {
    flex: 1,
    justifyContent: "flex-end",
  },
  backdrop: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: "rgba(255, 255, 255, 0.95)",
  },
  sheetContainer: {
    backgroundColor: "#FFF",
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    paddingHorizontal: 20,
    paddingTop: 14,
    paddingBottom: 18,
    height: "91%",
    shadowColor: "#000000",
    shadowOpacity: 0.18,
    shadowRadius: 30,
    shadowOffset: { width: 0, height: 15 },
    elevation: 24,
  },
  headerRow: {
    flexDirection: "row",
    alignItems: "center",
    marginBottom: 8,
  },
  closeButton: {
    minWidth: 40,
    height: 40,
    alignItems: "center",
    justifyContent: "center",
  },
  closeLabel: {
    color: "#505A66",
    fontSize: 16,
  },
  headerTitle: {
    flex: 1,
    textAlign: "center",
    color: "#252F3B",
    fontSize: 18,
    fontWeight: "500",
  },
  headerSpacer: {
    width: 40,
    height: 40,
  },
  listContent: {
    paddingTop: 6,
    paddingBottom: 18,
  },
  ledCard: {
    width: "100%",
    marginTop: 8,
  },
  cardRow: {
    flexDirection: "row",
    alignItems: "center",
    paddingHorizontal: 4,
    paddingVertical: 4,
    borderColor: "#FAFBFC",
    borderWidth: 2,
    width: "100%",
    minHeight: 84,
    borderRadius: 14,
    backgroundColor: "#FFFFFF",
    shadowColor: "#000000",
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.1,
    shadowRadius: 1,
    elevation: 1,
  },
  iconPane: {
    backgroundColor: "#F4F6F9",
    width: 70,
    minHeight: 70,
    alignSelf: "stretch",
    borderTopLeftRadius: 8,
    borderBottomLeftRadius: 8,
    justifyContent: "center",
    alignItems: "center",
    marginRight: 3,
  },
  iconImage: {
    width: 70,
    height: 70,
  },
  cardAccentTrack: {
    width: 8,
    alignSelf: "stretch",
    justifyContent: "space-between",
    paddingVertical: 2,
    marginRight: 10,
  },
  cardAccentSegment: {
    flex: 1,
  },
  cardAccentSegmentFirst: {
    borderTopRightRadius: 21,
  },
  cardAccentSegmentLast: {
    borderBottomRightRadius: 21,
  },
  cardAccentSegmentGap: {
    marginBottom: 2,
  },
  copyWrap: {
    flex: 1,
    paddingVertical: 8,
    paddingLeft: 8,
  },
  ledTitle: {
    color: "#505A66",
    fontSize: 17,
    fontWeight: "500",
  },
  ledDescription: {
    marginTop: 6,
    color: "#505A66",
    fontSize: 16,
    fontWeight: "400",
    textAlign: "left",
  },
});
