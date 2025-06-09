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
import { getNextDoseInfoForDevices } from "@/utils/dose/doseHelper";

/**
 * The main dashboard screen which displays the next upcoming dose and a list of
 * all connected devices with their current treatment status.
 *
 * The dashboard screen is responsible for:
 * 1. Fetching the dosage schedule for all connected devices.
 * 2. Initializing the doses for each device.
 * 3. Polling for updates to the dosage schedule every minute.
 * 4. Updating the treatment protocol for a device if it has changed.
 * 5. Displaying the next upcoming dose based on the current time.
 * 6. Displaying a list of connected devices with their current treatment status.
 */
export default function DashboardScreen() {
  const { getDeviceList, updateDeviceById } = useDeviceStore();
  const { initializeDoses } = useDoseStore();
  const [doseInfoState, setDoseInfoState] = useState<number>(0);
  const [doseInfo, setDoseInfo] = useState<{
    mainLabel: string;
    timeLabel: string;
    detailsLabel?: string;
  } | null>(null);

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
              console.log(`Treatment protocol for device ${device.deviceId} has changed`)
              const protocolChangeId = `protocol-${Date.now()}`;  //TODO: Replace with new protocol ID from backend

              // TODO: Add notification here
              showToast(
                "success",
                "Treatment change detected",
                "Updating treatment... Please review the details on the dashboard below."
              );

              // Save old treatment as needed (could be extended to history array)
              console.log("Updating treatment protocol & reinitializing dose");
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
            console.log(`No treatment protocol for device ${device.deviceId}`);
            console.log("Initializing doses");

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

  // Dose info refresher
  useEffect(() => {
    const updateDoseState = () => {
      const devices = getDeviceList();
      const nextDose = getNextDoseInfoForDevices(devices);

      if (!nextDose) {
        setDoseInfo(null);
        setDoseInfoState(-1);
        return;
      }

      const { device, timeMin, mainLabel, timeLabel, detailsLabel } = nextDose;

      if (timeMin <= 0 && timeMin >= -device.dosingWindowMin) {
        setDoseInfoState(2);
      } else if (timeMin <= 0 && timeMin >= -device.dosingWindowMin * 2) {
        setDoseInfoState(1);
      } else {
        setDoseInfoState(0);
      }

      setDoseInfo({ mainLabel, timeLabel, detailsLabel });
    };

    updateDoseState(); // run once

    const interval = setInterval(updateDoseState, 60 * 1000); // every minute
    return () => clearInterval(interval);
  }, [getDeviceList]);

  function onHelpPress() {
    showToast("success", "Notification sent", "Someone will be in touch soon.");
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <VNextDoseInfo {...doseInfo} />
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
