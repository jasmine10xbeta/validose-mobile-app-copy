import { useRouter } from "expo-router";
import { Pressable, StyleSheet, View } from "react-native";

import { VManualLinkingSheet } from "@/components/common/VManualLinkingSheet";

export default function ManualPairingScreen() {
  const router = useRouter();

  const closeModal = () => router.back();

  return (
    <View style={styles.overlayRoot}>
      <Pressable style={StyleSheet.absoluteFill} onPress={closeModal} />
      <View style={styles.sheetArea}>
        <VManualLinkingSheet
          onBack={closeModal}
          onClose={closeModal}
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  overlayRoot: {
    flex: 1,
    justifyContent: "flex-end",
    backgroundColor: "rgba(8, 15, 26, 0.10)",
  },
  sheetArea: {
    width: "100%",
  },
});
