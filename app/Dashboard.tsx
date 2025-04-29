import { useRouter } from "expo-router";
import { useState } from "react";
import { FlatList, Pressable, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import Toast from "react-native-toast-message";
import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import { useAuthStore } from "@/store/authStore";
import useDeviceStore from "@/store/useDeviceStore";

export default function DashboardScreen() {
  const router = useRouter();
  const { logout } = useAuthStore();
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
        });
      }
      if (doseInfoState === 2) {
        Toast.show({
          type: "error",
          text1: "Missed Dose",
          text2: "Please take your Dose on time.",
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
    });
  }

  function onHelpLongPress() {
    Toast.show({
      type: "success",
      text1: "Logout",
    });
    logout();
    router.navigate("/");
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.dashboardContainer}>
        <Pressable onPress={onNextDoseInfoPress}>
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
      </View>
      <View style={styles.dashboardContainerHelpSection}>
        <VButton
          label="Help"
          onPress={onHelpPress}
          onLongPress={onHelpLongPress}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    height: "100%",
    justifyContent: "center",
    alignContent: "center",
    backgroundColor: "#FFF",
  },
  dashboardContainer: {
    height: "90%",
    padding: 20,
    flexDirection: "column",
    alignItems: "center",
    gap: 25,
  },
  dashboardContainerHelpSection: {
    height: "10%",
    paddingLeft: 20,
    paddingRight: 20,
    flexDirection: "column",
    alignItems: "center",
  },
  textInput: {
    height: 50,
    width: 250,
  },
  camera: {
    height: 250,
    width: 250,
  },
  container: {
    flex: 1,
    justifyContent: "center",
  },
  message: {
    textAlign: "center",
    paddingBottom: 10,
  },
  buttonContainer: {
    flexDirection: "row",
  },
  button: {
    flex: 1,
    alignSelf: "flex-end",
    alignItems: "center",
  },
  text: {
    fontSize: 24,
    fontWeight: "bold",
    color: "white",
  },
  scrollViewSection: {
    height: "50%",
  },
});
