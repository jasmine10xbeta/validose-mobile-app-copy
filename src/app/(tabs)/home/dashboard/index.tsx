import { Feather } from "@expo/vector-icons";
import dayjs from "dayjs";
import { useRouter } from "expo-router";
import { useEffect, useState } from "react";
import {
  ActivityIndicator,
  FlatList,
  Modal,
  StyleSheet,
  Text,
  Dimensions,
  TouchableOpacity,
  TouchableWithoutFeedback,
  View,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

import { VButton } from "@/components/common/VButton";
import { VMedicationItem } from "@/components/common/VMedicationItem";
import VNetworkInfo from "@/components/common/VNetworkInfo";
import { VNextDoseInfo } from "@/components/common/VNextDoseInfo";
import { showToast } from "@/components/common/VToast";
import { VTopActions } from "@/components/common/VTopActions";
import { useAuth } from "@/providers/auth";
import {
  createSupportRequest,
  getSupportRequests,
} from "@/services/support";
import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { SupportRequest } from "@/types/support";
import { connectAndSetupDevice } from "@/utils/ble";
import { syncTreatmentsAndSchedules } from "@/utils/schedule";

const INBOX_SHEET_HEIGHT = Dimensions.get("window").height * 0.9;

export default function DashboardScreen() {
  const router = useRouter();
  const { user } = useAuth();
  const isMockMode = useDevStore((state) => state.isMockBleModeEnabled());
  const { treatments, clearTreatments } = useTreatmentStore();
  const { schedules, clearSchedules, getTodaySchedules } = useScheduleStore();
  const todaySchedulesByDevice = getTodaySchedules();

  const { getDeviceList, removeDevice } = useDeviceStore();
  const [connectedDeviceId, setConnectedDeviceId] = useState<string>("");
  const [isInboxOpen, setIsInboxOpen] = useState(false);
  const [inboxLoading, setInboxLoading] = useState(false);
  const [inboxError, setInboxError] = useState<string | null>(null);
  const [supportRequests, setSupportRequests] = useState<SupportRequest[]>([]);
  const hasAccessToken = Boolean(user?.access_token);
  const handleHelpPress = () => router.push("/home/led-info");

  useEffect(() => {
    // clearTreatments();                   // UNCOMMENT FOR DEBUGGING
    // clearSchedules();                    // UNCOMMENT FOR DEBUGGING
    if (!isMockMode) {
      syncTreatmentsAndSchedules();           // COMMENT FOR DEBUGGING
    }
  }, [isMockMode]);

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
        .map((scheduleItem) =>
          new Date(scheduleItem.event_at_local).toLocaleTimeString([], {
            hour: "2-digit",
            minute: "2-digit",
          })
        )
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

  const handleOpenInbox = async () => {
    setIsInboxOpen(true);
    setInboxError(null);
    setInboxLoading(true);

    try {
      const requests = await getSupportRequests();
      setSupportRequests(requests);
    } catch (error) {
      const message = error instanceof Error ? error.message : "Unknown error";
      console.error("[Dashboard] Failed to load inbox:", message);
      setSupportRequests([]);
      setInboxError("Unable to load inbox right now.");
    } finally {
      setInboxLoading(false);
    }
  };

  const handleCloseInbox = () => {
    setIsInboxOpen(false);
  };

  const renderSupportRequest = ({ item }: { item: SupportRequest }) => {
    const createdDate = item?.created_at
      ? dayjs(item.created_at).format("DD.MM.YYYY [at] HH:mm")
      : "—";
    const statusLabel = item?.status
      ? item.status.replace(/_/g, " ").toUpperCase()
      : "UNKNOWN";
    const isResolvedStatus = item?.status?.toUpperCase() === "RESOLVED";
    const statusAccentColor = isResolvedStatus ? "#EDEFF1" : "#C9E3E4";

    return (
      <View style={styles.requestRow}>
        <View
          style={[styles.requestIcon, { backgroundColor: statusAccentColor }]}
        >
          <Feather
            name="mail"
            size={20}
            color="#000000"
            style={styles.mailIcon}
          />
        </View>
        <View style={styles.requestDetails}>
          <Text style={styles.requestDate}>{createdDate}</Text>
        </View>
        <View
          style={[
            styles.requestStatusPill,
            { backgroundColor: statusAccentColor },
          ]}
        >
          <Text style={styles.requestStatusText}>{statusLabel}</Text>
        </View>
      </View>
    );
  };

  return (
    <SafeAreaView style={styles.alignContent}>
      {/* <VNetworkInfo /> */}
      <VTopActions
        style={styles.topActions}
        onPressHelp={handleHelpPress}
        personDisabled={!hasAccessToken}
      />
      <View style={styles.headerRow}>
        <TouchableOpacity
          style={styles.notificationButton}
          onPress={handleOpenInbox}
          accessibilityRole="button"
          accessibilityLabel="Open inbox notifications"
          hitSlop={{ top: 8, bottom: 8, left: 8, right: 8 }}
        >
          <Feather name="bell" size={21} color="#2E7787" />
        </TouchableOpacity>
      </View>
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
            showToast(
              "success",
              "Notification sent",
              "Someone will be in touch soon."
            );
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
      <Modal
        animationType="slide"
        transparent
        visible={isInboxOpen}
        onRequestClose={handleCloseInbox}
      >
        <View style={styles.modalRoot}>
          <TouchableWithoutFeedback onPress={handleCloseInbox}>
            <View style={styles.backdrop} />
          </TouchableWithoutFeedback>
          <View style={styles.sheetContainer}>
            <View style={styles.sheetHeader}>
              <TouchableOpacity
                onPress={handleCloseInbox}
                accessibilityRole="button"
                accessibilityLabel="Close inbox"
                hitSlop={{ top: 8, bottom: 8, left: 8, right: 8 }}
              >
                <Feather name="chevron-left" size={24} color="#000000" />
              </TouchableOpacity>
              <Text style={styles.sheetTitle}>Inbox</Text>
              <View style={styles.sheetHeaderSpacer} />
            </View>
            {inboxLoading ? (
              <View style={styles.loaderContainer}>
                <ActivityIndicator size="small" color="#2E7787" />
              </View>
            ) : inboxError ? (
              <View style={styles.messageContainer}>
                <Text style={styles.messageText}>{inboxError}</Text>
              </View>
            ) : (
              <FlatList
                data={supportRequests}
                keyExtractor={(item) => item.id}
                renderItem={renderSupportRequest}
                ListEmptyComponent={() => (
                  <View style={styles.messageContainer}>
                    <Text style={styles.messageText}>
                      No open support requests.
                    </Text>
                  </View>
                )}
                contentContainerStyle={
                  supportRequests.length === 0
                    ? styles.listEmptyContent
                    : undefined
                }
                showsVerticalScrollIndicator={false}
              />
            )}
          </View>
        </View>
      </Modal>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    flex: 1,
    backgroundColor: "#FFF",
    alignItems: "center",
    paddingHorizontal: 20,
  },
  topActions: {
    alignSelf: "stretch",
    marginTop: 8,
    marginBottom: 12,
  },
  headerRow: {
    alignSelf: "stretch",
    alignItems: "flex-end",
    paddingTop: 12,
  },
  notificationButton: {
    padding: 8,
    paddingBottom: 0,
    borderRadius: 20,
  },
  scrollViewSection: {
    height: "50%",
    alignSelf: "stretch",
  },
  modalRoot: {
    flex: 1,
    justifyContent: "flex-end",
  },
  backdrop: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: "rgba(255, 255, 255, 0.95)",
  },
  sheetContainer: {
    backgroundColor: "#FFF",
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    paddingHorizontal: 20,
    paddingTop: 16,
    paddingBottom: 32,
    height: INBOX_SHEET_HEIGHT,
    shadowColor: "#000000",
    shadowOpacity: 0.18,
    shadowRadius: 30,
    shadowOffset: { width: 0, height: 15 },
    elevation: 24,
  },
  sheetHeader: {
    flexDirection: "row",
    alignItems: "center",
    marginBottom: 38,
  },
  sheetHeaderSpacer: {
    width: 24,
    height: 24,
  },
  sheetTitle: {
    flex: 1,
    textAlign: "center",
    fontSize: 17,
    lineHeight: 22,
    letterSpacing: -0.43,
    fontWeight: "600",
    fontFamily: "SF Pro",
    color: "#333333",
  },
  loaderContainer: {
    paddingVertical: 32,
    alignItems: "center",
    justifyContent: "center",
  },
  messageContainer: {
    paddingVertical: 32,
    alignItems: "center",
    justifyContent: "center",
  },
  messageText: {
    fontSize: 16,
    color: "#4F5D75",
    textAlign: "center",
  },
  requestRow: {
    flexDirection: "row",
    alignItems: "center",
    height: 91,
    paddingHorizontal: 16,
    borderWidth: 1,
    borderColor: "#E0E0E0",
    backgroundColor: "#FFFFFF",
  },
  requestIcon: {
    width: 46,
    height: 46,
    borderRadius: 23,
    justifyContent: "center",
    alignItems: "center",
  },
  mailIcon: {
    borderColor: "#000000",
  },
  requestDetails: {
    marginLeft: 16,
    flex: 1,
  },
  requestDate: {
    fontSize: 16,
    color: "#252F3B",
    fontWeight: "500",
  },
  requestStatusPill: {
    height: 28,
    borderRadius: 16,
    paddingVertical: 7,
    paddingHorizontal: 16,
    justifyContent: "center",
    alignItems: "center",
  },
  requestStatusText: {
    fontSize: 12,
    fontWeight: "600",
    // fontFamily: "Inter",
    lineHeight: 12,
    letterSpacing: -0.12,
    textTransform: "uppercase",
    color: "#252F3B",
  },
  listEmptyContent: {
    flexGrow: 1,
    justifyContent: "center",
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
