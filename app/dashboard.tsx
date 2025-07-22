import { useEffect, useState } from "react";
import { FlatList, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import VNetworkInfo from "@/components/common/VNetworkInfo";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import { showToast } from "@/components/common/VToast";
import useDeviceStore from "@/store/useDeviceStore";
import useDoseHistoryStore from "@/store/useDoseHistoryStore";
import useDoseScheduleStore from "@/store/useDoseScheduleStore";
import useTreatmentProtocolStore from "@/store/useTreatmentProtocolStore";
import { getLatestTreatmentProtocol } from "@/utils/axios/api/__mocks__/dosage";
import { clearToken } from "@/utils/axios/api/token";
import { getNextDose } from "@/utils/dose/doseHelper";

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
  const [isDoseInitialized, setIsDoseInitialized] = useState(false);

  const { scheduleDosesForToday } = useDoseScheduleStore();
  const { getStoredTreatmentProtocol, storeTreatmentProtocol } = useTreatmentProtocolStore();
  const { getDeviceList, updateDevice, removeAllDevices } = useDeviceStore();
  const { records, markDoseTaken } = useDoseHistoryStore();

  const [doseInfoState, setDoseInfoState] = useState<number>(0);
  const [doseInfo, setDoseInfo] = useState<{
    mainLabel: string;
    timeLabel: string;
    detailsLabel?: string;
  } | null>(null);

  const fetchDosageDetails = async () => {
    setIsDoseInitialized(false);
    const devices = getDeviceList();

    for (const device of devices) {
      try {
        const latestTreatmentProtocol = await getLatestTreatmentProtocol(device.deviceId);
        const storedTreatmentProtocol = getStoredTreatmentProtocol(device.deviceId);

        if (latestTreatmentProtocol) {
          // Compare latest treatment protocol with existing device treatment data
          const hasChanged =
            JSON.stringify(storedTreatmentProtocol?.administrationDays) !== JSON.stringify(latestTreatmentProtocol.administrationDays) ||
            JSON.stringify(storedTreatmentProtocol?.administrationTimesMin) !== JSON.stringify(latestTreatmentProtocol.administrationTimesMin);

          if (hasChanged) {
            const newTreatmentProtocolId = `protocol-${Date.now()}`; //TODO: Replace with new protocol ID from backend
            console.log(`Treatment protocol for device ${device.deviceId} has changed`);

            storeTreatmentProtocol(device.deviceId, latestTreatmentProtocol);
            updateDevice(device.deviceId, { linkedTreatmentProtocolId: newTreatmentProtocolId });
            scheduleDosesForToday(device.deviceId, latestTreatmentProtocol);

            // TODO: Add notification here
            showToast(
              "success",
              "Treatment change detected",
              "Updating treatment... Please review the details on the dashboard below."
            );
          } else {
            if (storedTreatmentProtocol) {
              scheduleDosesForToday(device.deviceId, storedTreatmentProtocol);
            }
          }
        } else {
          // If no response, just initialize with stored treatment protocol
          console.log(`Could not fetch treatment protocol for device ${device.deviceId}.`);

          if (storedTreatmentProtocol) {
            console.log("Initializing doses based on stored treatment protocol...");
            scheduleDosesForToday(device.deviceId, storedTreatmentProtocol);
          } else {
            console.log("Could not find stored treatment protocol as well.");
          }
        }
      } catch (error) {
        const errorMessage = error instanceof Error ? error.message : String(error);
        showToast(
          "error",
          `Failed to fetch dosage for ${device.deviceId}`,
          errorMessage
        );

        const storedTreatmentProtocol = getStoredTreatmentProtocol(device.deviceId);
        if (storedTreatmentProtocol) {
          scheduleDosesForToday(device.deviceId, storedTreatmentProtocol);
        }
      } finally {
        setIsDoseInitialized(true);
      }
    }
  };

  const updateDoseState = () => {
    console.log("Reloading dose states...");
    const devices = getDeviceList();
    const nextDose = getNextDose(devices, getStoredTreatmentProtocol);

    if (!nextDose) {
      setDoseInfo(null);
      setDoseInfoState(0);
      return;
    }

    const {
      device,
      mainLabel,
      timeLabel,
      detailsLabel,
      state
    } = nextDose;

    const protocol = getStoredTreatmentProtocol(device.deviceId);
    if (!protocol) {
      setDoseInfo(null);
      setDoseInfoState(0);
      return;
    }

    setDoseInfoState(state);
    setDoseInfo({ mainLabel, timeLabel, detailsLabel });
  };

  useEffect(() => {
    fetchDosageDetails();
  }, []);

  // // Dose info refresher
  useEffect(() => {
    updateDoseState(); // run once

    const interval = setInterval(updateDoseState, 60 * 1000); // every minute
    return () => clearInterval(interval);
  }, [records]);

  function onHelpPress() {
    showToast("success", "Notification sent", "Someone will be in touch soon.");
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <VNetworkInfo />
      {isDoseInitialized && (
        <>
          <VNextDoseInfo {...{ ...doseInfo, state: doseInfoState }} />
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
        </>
      )}
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

  // Help Button Styles
  doseButton: {
    borderColor: "#C9E3E4",
    borderWidth: 2,
    width: "49%",
    padding: 5,
    borderRadius: 25,
    position: "absolute",
    bottom: -40,
  },
  doseButtonText: {
    color: "#2E7787",
    fontSize: 16,
    fontWeight: "600",
    // fontFamily: "Inter",
  },
});
