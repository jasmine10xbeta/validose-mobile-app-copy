import { Feather } from "@expo/vector-icons";
import { useRouter } from "expo-router";
import dayjs from "dayjs";
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
import { VTopActions } from "@/components/common/VTopActions";
import { showToast } from "@/components/common/VToast";
import {
  createSupportRequest,
  getSupportRequests,
} from "@/services/support";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { SupportRequest } from "@/types/support";
import { connectAndSetupDevice } from "@/utils/ble";
import { syncTreatmentsAndSchedules } from "@/utils/schedule";
import { useAuth } from "@/providers/auth";

const INBOX_SHEET_HEIGHT = Dimensions.get("window").height * 0.9;

export default function DashboardScreen() {
  const router = useRouter();
  const { user } = useAuth();
  const { treatments, clearTreatments } = useTreatmentStore();
  const { schedules, clearSchedules, getTodaySchedules } = useScheduleStore();
  const todaySchedulesByDevice = getTodaySchedules();

  const { getDeviceList, removeDevice } = useDeviceStore();
  const [connectedDeviceId, setConnectedDeviceId] = useState<string>("");
  const [isInboxOpen, setIsInboxOpen] = useState(false);
  const [inboxLoading, setInboxLoading] = useState(false);
  const [inboxError, setInboxError] = useState<string | null>(null);
  const [supportRequests, setSupportRequests] = useState<SupportRequest[]>([]);
  const [isHelpLoading, setIsHelpLoading] = useState(false);
  const hasAccessToken = Boolean(user?.access_token);
  const handleHelpPress = () => router.push("/home/led-info");

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

    console.log("\n");
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
    const createdDate = formatHistoryTime(item?.created_at);
    const status = mapHistoryStatus(item?.status);

    return (
      <View style={styles.requestRow}>
        <View style={styles.requestDetails}>
          <Text style={styles.requestTitle}>Beginning of a message</Text>
          <Text style={styles.requestDate}>{createdDate}</Text>
        </View>
        <View
          style={[
            styles.requestStatusPill,
            { backgroundColor: status.backgroundColor },
          ]}
        >
          <Text
            style={[
              styles.requestStatusText,
              { color: status.textColor, textTransform: "none" },
            ]}
          >
            {status.label}
          </Text>
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
          if (isHelpLoading) return;
          setIsHelpLoading(true);
          try {
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
          } finally {
            setIsHelpLoading(false);
          }
        }}
        loading={isHelpLoading}
        disabled={isHelpLoading}
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
                style={styles.closeTextButton}
              >
                <Text style={styles.closeText}>Close</Text>
              </TouchableOpacity>
              <Text style={styles.sheetTitle}>History</Text>
              <View style={styles.sheetHeaderSpacer} />
            </View>
            <Text style={styles.sheetDescription}>
              Below is the list of help requests you placed.{"\n"}
              <Text style={styles.sheetDescriptionBold}>
                We will reply to each individually and contact{"\n"}you for troubleshooting.
              </Text>
            </Text>
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
                      No history available yet.
                    </Text>
                  </View>
                )}
                contentContainerStyle={
                  supportRequests.length === 0
                    ? styles.listEmptyContent
                    : styles.listContent
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
    justifyContent: "center",
    alignItems: "center",
  },
  backdrop: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: "rgba(0, 0, 0, 0.45)",
  },
  sheetContainer: {
    backgroundColor: "#F3F4F6",
    borderRadius: 18,
    paddingHorizontal: 14,
    paddingTop: 12,
    paddingBottom: 20,
    width: "90%",
    height: INBOX_SHEET_HEIGHT * 0.9,
    shadowColor: "#000000",
    shadowOpacity: 0.18,
    shadowRadius: 22,
    shadowOffset: { width: 0, height: 10 },
    elevation: 24,
  },
  sheetHeader: {
    flexDirection: "row",
    alignItems: "center",
    marginBottom: 26,
  },
  closeTextButton: {
    width: 52,
  },
  closeText: {
    fontSize: 15,
    color: "#4D5A69",
    fontWeight: "500",
  },
  sheetHeaderSpacer: {
    width: 52,
    height: 24,
  },
  sheetTitle: {
    flex: 1,
    textAlign: "center",
    fontSize: 32 / 2,
    lineHeight: 22,
    fontWeight: "700",
    color: "#2D3745",
  },
  sheetDescription: {
    textAlign: "center",
    fontSize: 34 / 2,
    lineHeight: 22,
    color: "#4D5A69",
    marginBottom: 16,
  },
  sheetDescriptionBold: {
    color: "#2D3745",
    fontWeight: "700",
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
    height: 92,
    paddingHorizontal: 12,
    borderWidth: 1,
    borderColor: "#E1E5EA",
    borderRadius: 11,
    backgroundColor: "#F9FAFB",
    marginBottom: 8,
  },
  requestDetails: {
    flex: 1,
  },
  requestTitle: {
    fontSize: 34 / 2,
    color: "#2D3745",
    fontWeight: "600",
    marginBottom: 2,
  },
  requestDate: {
    fontSize: 34 / 2,
    color: "#6A7788",
    fontWeight: "400",
  },
  requestStatusPill: {
    height: 40,
    borderRadius: 10,
    paddingVertical: 8,
    paddingHorizontal: 14,
    justifyContent: "center",
    alignItems: "center",
  },
  requestStatusText: {
    fontSize: 17,
    fontWeight: "500",
  },
  listEmptyContent: {
    flexGrow: 1,
    justifyContent: "center",
  },
  listContent: {
    paddingTop: 2,
    paddingBottom: 8,
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

function formatHistoryTime(createdAt?: string | null): string {
  if (!createdAt) return "—";
  const date = dayjs(createdAt);
  if (!date.isValid()) return "—";

  const now = dayjs();
  const diffMinutes = now.diff(date, "minute");

  if (diffMinutes < 1) return "Just now";
  if (diffMinutes < 60) return `${diffMinutes} min ago`;
  if (now.isSame(date, "day")) return date.format("hh:mm a");
  if (now.subtract(1, "day").isSame(date, "day")) return "yesterday";
  if (now.diff(date, "day") < 7) return date.format("dddd");
  if (now.isSame(date, "year")) return date.format("MMM D");
  return date.format("MMM D, YYYY");
}

function mapHistoryStatus(status?: string | null): {
  label: string;
  backgroundColor: string;
  textColor: string;
} {
  const normalized = status?.toUpperCase();

  if (normalized === "RESOLVED") {
    return {
      label: "Resolved",
      backgroundColor: "#D5E7EC",
      textColor: "#2A6574",
    };
  }

  if (normalized === "IN_PROGRESS") {
    return {
      label: "In progress",
      backgroundColor: "#D1E9DE",
      textColor: "#16734A",
    };
  }

  return {
    label: "Send",
    backgroundColor: "#F3EADF",
    textColor: "#8A5A19",
  };
}
