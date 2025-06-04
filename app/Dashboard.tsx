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
          const doseInfo = await getDeviceDosageSchedule(device.deviceId);

          if (doseInfo) {
            // Compare new doseInfo with existing device treatment data
            const hasChanged =
              JSON.stringify(device.administrationDays) !==
                JSON.stringify(doseInfo.administrationDays) ||
              JSON.stringify(device.administrationTimesMin) !==
                JSON.stringify(doseInfo.administrationTimesMin);

            if (hasChanged) {
              const protocolChangeId = `protocol-${Date.now()}`;  //TODO: Replace with new protocol ID from backend

              // TODO: Add notification here
              showToast(
                "success",
                "Treatment change detected",
                "Updating treatment... Please review the details on the dashboard below."
              );

              // Save old treatment as needed (could be extended to history array)
              updateDeviceById(device.deviceId, {
                ...doseInfo,
                previousTreatment: device,
                protocolId: protocolChangeId,
              });
              initializeDoses([
                {
                  ...device,
                  ...doseInfo,
                  protocolId: protocolChangeId,
                },
              ]);
            }
          } else {
            // If no response, just initialize with current treatment
            initializeDoses([device]);
          }
        } catch (error) {
          const errorMessage = error instanceof Error ? error.message : String(error);
          showToast("error", `Failed to fetch dosage for ${device.deviceId}`, errorMessage);
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
            renderItem={({ item }) => <VMedicationItem item={item} />}
            keyExtractor={(item) => item.deviceId}
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
