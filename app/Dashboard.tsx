import { useState } from "react";
import { FlatList, Pressable, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";
import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import useDeviceStore from "@/store/useDeviceStore";

export default function DashboardScreen() {
  const { getDeviceList } = useDeviceStore();
  const [doseInfoState, setDoseInfoState] = useState<number>(0);

  function onNextDoseInfoPress() {
    if (doseInfoState > 2) {
      setDoseInfoState(0);
    } else {
      if (doseInfoState === 1) {
        Toast.show({
          type: "success",
          text1: "Successful Dose",
          text2: "Congratulations you make a Dose.",
          topOffset: 50,
        });
      }
      if (doseInfoState === 2) {
        Toast.show({
          type: "error",
          text1: "Missed Dose",
          text2: "Please take your Dose on time.",
          topOffset: 50,
        });
      }
      setDoseInfoState((prev) => prev + 1);
    }
  }

  function onHelpPress() {
    Toast.show({
      type: "success",
      text1: "Notification Sent",
      text2: "Someone will be in touch soon 📞",
      topOffset: 50,
    });
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
