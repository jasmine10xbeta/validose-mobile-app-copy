import { useEffect, useState } from "react";
import { FlatList, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { showToast } from "@/components/common/Toast";
import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import useDeviceStore from "@/store/useDeviceStore";
import useDoseStore from "@/store/useDoseStore";
import { getDeviceDosageSchedule } from "@/utils/axios/api/__mocks__/dosage";

export default function DashboardScreen() {
  const { getDeviceList, updateDeviceById } = useDeviceStore();
  const { initializeDoses } = useDoseStore();
  const [doseInfoState, setDoseInfoState] = useState<number>(0);

  useEffect(() => {
    const fetchDosageDetails = async () => {
      const devices = getDeviceList();

      for (const device of devices) {
        try {
          const doseInfo = await getDeviceDosageSchedule(device.id);

          if (doseInfo) {
            // Compare new doseInfo with existing device treatment data
            const hasChanged = JSON.stringify(device.administration_days) !== JSON.stringify(doseInfo.administration_days) ||
              JSON.stringify(device.administration_times_min) !== JSON.stringify(doseInfo.administration_times_min);

            if (hasChanged) {
              // Save old treatment as needed (could be extended to history array)
              updateDeviceById(device.id, {
                ...doseInfo,
                previousTreatment: {
                  administration_days: device.administration_days,
                  administration_times_min: device.administration_times_min,
                },
              });
              initializeDoses([
                {
                  ...device,
                  ...doseInfo,
                },
              ]);
            }
          } else {
            // If no response, just initialize with current treatment
            initializeDoses([device]);
          }
        } catch (error) {
          console.error(`Failed to fetch dosage for ${device.id}`, error);
          initializeDoses([device]);
        }
      }
    };

    fetchDosageDetails();
  }, [getDeviceList, initializeDoses, updateDeviceById]);

  function onHelpPress() {
    showToast("success", "Notification sent", "Someone will be in touch soon.");
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <VNextDoseInfo
        infoState={doseInfoState}
        mainLabel="Take dose now"
        timeLabel="within the hour"
      />
      <View style={styles.scrollViewSection}>
        {isDevicesConnected ? (
          <FlatList
            initialNumToRender={4}
            renderItem={({ item }) => (
              <VMedicationItem
                item={item}
                state={item?.status || ""}
                color={item.color || ""}
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
