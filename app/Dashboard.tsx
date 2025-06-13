import { useRouter } from "expo-router";
import { useEffect, useState } from "react";
import { FlatList, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import { showToast } from "@/components/common/VToast";
import useDeviceStore from "@/store/useDeviceStore";
import useDoseHistoryStore from "@/store/useDoseHistoryStore";
import useDoseScheduleStore from "@/store/useDoseScheduleStore";
import useTreatmentProtocolStore from "@/store/useTreatmentProtocolStore";
import { getDeviceDosageSchedule } from "@/utils/axios/api/__mocks__/dosage";
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
  const [isInitialized, setInitialized] = useState(false);

  const { initializeSchedule } = useDoseScheduleStore();
  const { getProtocol, setProtocol } = useTreatmentProtocolStore();
  const { getDeviceList, updateDevice, removeAllDevices } = useDeviceStore();
  const { records, markDoseTaken } = useDoseHistoryStore();
  // const doseHistory = useDoseHistoryStore((state) => state.records);

  const router = useRouter();

  const [doseInfoState, setDoseInfoState] = useState<number>(0);
  const [doseInfo, setDoseInfo] = useState<{
    mainLabel: string;
    timeLabel: string;
    detailsLabel?: string;
  } | null>(null);

  useEffect(() => {
    const fetchDosageDetails = async () => {
      setInitialized(false);
      const devices = getDeviceList();

      for (const device of devices) {
        try {
          const doseInfo = await getDeviceDosageSchedule(device.deviceId);
          const currentProtocol = getProtocol(device.deviceId);

          if (doseInfo) {
            // Compare new doseInfo with existing device treatment data
            const hasChanged =
              JSON.stringify(currentProtocol?.administrationDays) !==
                JSON.stringify(doseInfo.administrationDays) ||
              JSON.stringify(currentProtocol?.administrationTimesMin) !==
                JSON.stringify(doseInfo.administrationTimesMin);

            if (hasChanged) {
              const newProtocolId = `protocol-${Date.now()}`; //TODO: Replace with new protocol ID from backend
              console.log(
                `Treatment protocol for device ${device.deviceId} has changed`
              );

              setProtocol(device.deviceId, doseInfo);
              updateDevice(device.deviceId, {
                linkedProtocolId: newProtocolId,
              });
              initializeSchedule(device.deviceId, doseInfo);

              // TODO: Add notification here
              showToast(
                "success",
                "Treatment change detected",
                "Updating treatment... Please review the details on the dashboard below."
              );
            } else {
              if (currentProtocol) {
                initializeSchedule(device.deviceId, currentProtocol);
              }
            }
          } else {
            // If no response, just initialize with current treatment
            console.log(`No treatment protocol for device ${device.deviceId}`);
            console.log("Initializing doses");

            if (currentProtocol) {
              initializeSchedule(device.deviceId, currentProtocol);
            }
          }
        } catch (error) {
          const errorMessage =
            error instanceof Error ? error.message : String(error);
          showToast(
            "error",
            `Failed to fetch dosage for ${device.deviceId}`,
            errorMessage
          );

          const currentProtocol = getProtocol(device.deviceId);
          if (currentProtocol) {
            initializeSchedule(device.deviceId, currentProtocol);
          }
        }
      }
      setInitialized(true);
    };

    fetchDosageDetails();
  }, []);

  // // Dose info refresher
  useEffect(() => {
    const updateDoseState = () => {
      console.log("Reloading dose states...");
      const devices = getDeviceList();
      const nextDose = getNextDose(devices, getProtocol);

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

      const protocol = getProtocol(device.deviceId);
      if (!protocol) {
        setDoseInfo(null);
        setDoseInfoState(0);
        return;
      }

      setDoseInfoState(state);
      setDoseInfo({ mainLabel, timeLabel, detailsLabel });
    };

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
      {isInitialized && (
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
