import { useState } from "react";
import { FlatList, Pressable, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import useDeviceStore from "@/store/useDeviceStore";
import { showToast } from "@/utils/toastUtils";

export default function DashboardScreen() {
  const { getDeviceList } = useDeviceStore();
  const [doseInfoState, setDoseInfoState] = useState<number>(0);

  function onNextDoseInfoPress() {
    if (doseInfoState > 2) {
      setDoseInfoState(0);
    } else {
      if (doseInfoState === 1) {
        showToast(
          "success",
          "Successful dose",
          "Congratulations you make a dose."
        );
      }
      if (doseInfoState === 2) {
        showToast("error", "Missed dose", "Please take your dose on time.");
      }
      setDoseInfoState((prev) => prev + 1);
    }
  }

  function onHelpPress() {
    showToast("success", "Notification sent", "Someone will be in touch soon.");
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <Pressable onPress={onNextDoseInfoPress} style={{ width: "100%" }}>
        <VNextDoseInfo
          infoState={doseInfoState}
          mainLabel="Take dose now"
          timeLabel="within the hour"
        />
      </Pressable>
      <View style={styles.scrollViewSection}>
        {isDevicesConnected ? (
          <FlatList
            initialNumToRender={4}
            renderItem={({ item }) => (
              <VMedicationItem
                item={item}
                state={"Conected"}
                color={item.color}
              />
            )}
            keyExtractor={(item) => item.id}
            data={getDeviceList()}
          />
        ) : null}
      </View>
      <VButton
        label="Help"
        onPress={onHelpPress}
        style={styles.helpButton}
        labelStyle={styles.helpButtonText}
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    height: "100%",
    backgroundColor: "#FFF",
    alignItems: "center",
    paddingHorizontal: 20,
  },
  scrollViewSection: {
    height: "50%",
  },

  // Help Button Styles
  helpButton: {
    marginTop: 10,
    borderColor: "#C9E3E4",
    borderWidth: 2,
    width: "100%",
    padding: 5,
    borderRadius: 25,
    position: "absolute",
    bottom: 40,
  },
  helpButtonText: {
    color: "#2E7787",
    fontSize: 16,
    fontWeight: "600",
    // fontFamily: "Inter",
  },
});
