import { useEffect, useState } from "react";
import { FlatList, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import VNetworkInfo from "@/components/common/VNetworkInfo";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import { showToast } from "@/components/common/VToast";
import { createSupportRequest } from "@/services/support";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { connectAndSetupDevice } from "@/utils/ble";
import { syncTreatmentsAndSchedules } from "@/utils/schedule";

export default function DashboardScreen() {
  const { treatments, clearTreatments } = useTreatmentStore();
  const { schedules, clearSchedules, getTodaySchedules } = useScheduleStore();
  const todaySchedulesByDevice = getTodaySchedules();

  const { getDeviceList, removeDevice } = useDeviceStore();
  const [connectedDeviceId, setConnectedDeviceId] = useState<string>("");

  useEffect(() => {
    // clearTreatments();                   // UNCOMMENT FOR DEBUGGING
    // clearSchedules();                    // UNCOMMENT FOR DEBUGGING
    syncTreatmentsAndSchedules();           // COMMENT FOR DEBUGGING
  }, []);

  useEffect(() => {
    const entries = Object.entries(todaySchedulesByDevice);

    if (entries.length === 0) {
      console.log("[Dashboard] No doses scheduled for today.");
      return;
    }

    console.log("[Dashboard] Today's dose schedule:");
    entries.forEach(([deviceName, deviceSchedules]) => {
      if (!deviceSchedules || deviceSchedules.length === 0) {
        console.log(`  • ${deviceName}: no remaining doses today.`);
        return;
      }

      const times = deviceSchedules
        .map((scheduleItem) => new Date(scheduleItem.event_at_local).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }))
        .join(", ");

      console.log(`  • ${deviceName}: ${times}`);
    });
  }, [todaySchedulesByDevice]);

  // UNCOMMENT FOR DEBUG MODE
  async function resetDevice(
    deviceAddress: string,
    removeDevice: (deviceId: string) => void
  ) {
    try {
      // Step 1: Clear all linked devices locally
      removeDevice(deviceAddress);

      const connected = await connectAndSetupDevice(deviceAddress);
      if (connected?.error) {
        showToast("error", connected?.error.toString());
      }
      // TODO: Handle toast
    } catch (error) {
      console.error("Error resetting device:", error);
    }
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <VNetworkInfo />
      <>
        <VNextDoseInfo todaySchedulesByDevice={todaySchedulesByDevice} />
        <View style={styles.scrollViewSection}>
          {isDevicesConnected ? (
            <FlatList
              initialNumToRender={4}
              renderItem={({ item }) => {
                return (
                  <VMedicationItem
                    item={item}
                    schedule={todaySchedulesByDevice[item.deviceName] || []}
                  />
                );
              }}
              keyExtractor={(item) => item.deviceId}
              data={getDeviceList()}
            />
          ) : null}
        </View>
        <VButton
          // === COMMENT FROM HERE FOR DEBUG MODE ===
          label="Help"
          onPress={async () => {
            const supportResponse = await createSupportRequest();
            if (supportResponse?.id) {
              showToast("success", "Notification sent", "Someone will be in touch soon.");
            } else {
              showToast("error", "Error", "Could not create support request.");
              return;
            }
          }}
          // === COMMENT TILL HERE FOR DEBUG MODE ===

          // === UNCOMMENT FROM HERE FOR DEBUG ===
          // label="Reset Device"
          // onPress={() => {
          //   Alert.alert(
          //     "Confirm Reset",
          //     "Confirming device removed from Bluetooth settings?",
          //     [
          //       {
          //         text: "Cancel",
          //         style: "cancel",
          //       },
          //       {
          //         text: "Yes",
          //         onPress: () => resetDevice(connectedDeviceId, removeDevice),
          //       },
          //     ],
          //     { cancelable: true }
          //   );
          // }}
          // === UNCOMMENT TILL HERE FOR DEBUG ONLY ===
          style={styles.helpButton}
          labelStyle={styles.helpButtonText}
        />
      </>
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
    backgroundColor: "#FFF",
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
