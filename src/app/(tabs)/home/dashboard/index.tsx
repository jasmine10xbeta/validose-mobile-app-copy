import dayjs from "dayjs";
import { useRouter } from "expo-router";
import { useEffect, useState } from "react";
import {
  ActivityIndicator,
  FlatList,
  Image,
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
const HELP_NOTIF_ICON = require("../../../../assets/images/png/help-notif.png");

function formatSupportRequestTime(createdAt?: string): string {
  if (!createdAt) return "—";
  const created = dayjs(createdAt);
  if (!created.isValid()) return "—";

  const now = dayjs();
  if (created.isSame(now, "day")) {
    const minutesAgo = now.diff(created, "minute");
    if (minutesAgo < 60) {
      const clampedMinutes = Math.max(1, minutesAgo);
      return `${clampedMinutes} min${clampedMinutes === 1 ? "" : "s"} ago`;
    }

    return `${created.format("h:mm a")}, today`;
  }

  if (created.isSame(now.subtract(1, "day"), "day")) {
    return `${created.format("h:mm a")}, yesterday`;
  }

  return `${created.format("h:mm a")}, ${created.format("MMM D YYYY")}`;
}

function getSupportRequestStatusPresentation(status?: string): {
  label: string;
  backgroundColor: string;
} {
  const normalized = (status || "").trim().toLowerCase();

  if (
    normalized.includes("in_progress") ||
    normalized.includes("in progress") ||
    normalized.includes("pending")
  ) {
    return {
      label: "In progress",
      backgroundColor: "#4CB78233",
    };
  }

  if (normalized.includes("resolved")) {
    return {
      label: "resolved",
      backgroundColor: "#DFEFF0",
    };
  }

  return {
    label: "Sent",
    backgroundColor: "#FAF2E8",
  };
}

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
      syncTreatmentsAndSchedules({
        reason: "dashboard-mount",
      }).catch((error) => {
        console.error("[Dashboard] Failed to sync treatments/schedules:", error);
      }); // COMMENT FOR DEBUGGING
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
    const createdDate = formatSupportRequestTime(item?.created_at);
    const { label: statusLabel, backgroundColor: statusAccentColor } =
      getSupportRequestStatusPresentation(item?.status);

    return (
      <View style={styles.requestRow}>
        <View style={styles.requestDetails}>
          <Text style={styles.requestPreview}>Beginning of a message</Text>
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
        rightAccessory={
          <TouchableOpacity
            style={styles.topActionNotificationButton}
            onPress={handleOpenInbox}
            accessibilityRole="button"
            accessibilityLabel="Open inbox notifications"
            hitSlop={{ top: 8, bottom: 8, left: 8, right: 8 }}
          >
            <Image source={HELP_NOTIF_ICON} style={styles.topActionNotificationIcon} resizeMode="contain" />
          </TouchableOpacity>
        }
      />
      <VNextDoseInfo todaySchedulesByDevice={todaySchedulesByDevice} />
      <View style={styles.scrollViewSection}>
        {isDevicesConnected ? (
          <FlatList
            initialNumToRender={4}
            renderItem={({ item, index }) => {
              const scheduleKey = item.deviceId || item.deviceName;
              return (
                <VMedicationItem
                  item={item}
                  schedule={todaySchedulesByDevice[scheduleKey] || []}
                  index={index}
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
                style={styles.sheetHeaderAction}
              >
                <Text style={styles.sheetHeaderActionText}>Close</Text>
              </TouchableOpacity>
              <Text style={styles.sheetTitle}>History</Text>
              <View style={styles.sheetHeaderSpacer} />
            </View>
            <Text style={styles.sheetSubtitle}>
              Below is the list of help requests you placed.{"\n"}
              We will reply to each individually and contact you for troubleshooting.
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
                      No help requests yet.
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
  topActionNotificationButton: {
    padding: 8,
    borderRadius: 20,
  },
  topActionNotificationIcon: {
    width: 22,
    height: 22,
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
    marginBottom: 14,
  },
  sheetHeaderAction: {
    width: 54,
  },
  sheetHeaderActionText: {
    fontSize: 16,
    fontWeight: "400",
    color: "#505A66",
  },
  sheetHeaderSpacer: {
    width: 54,
  },
  sheetTitle: {
    flex: 1,
    textAlign: "center",
    fontSize: 17,
    lineHeight: 22,
    letterSpacing: -0.43,
    fontWeight: "500",
    fontFamily: "SF Pro",
    color: "#252F3B",
  },
  sheetSubtitle: {
    marginVertical: 28,
    fontSize: 16,
    lineHeight: 21,
    color: "#505A66",
    justifyContent: "center",
    textAlign: "center",
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
    // minHeight: 88,
    paddingHorizontal: 12,
    paddingVertical: 13,
    borderWidth: 1,
    borderColor: "#E6EAF0",
    backgroundColor: "#FFFFFF",
    borderRadius: 10,
    marginBottom: 10,
    shadowColor: "#000000",
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.06,
    shadowRadius: 1,
    elevation: 1,
  },
  requestDetails: {
    flex: 1,
    paddingRight: 12,
  },
  requestPreview: {
    fontSize: 15,
    color: "#181C21",
    fontWeight: "500",
    marginBottom: 4,
  },
  requestDate: {
    fontSize: 15,
    color: "#505A66",
    fontWeight: "400",
  },
  requestStatusPill: {
    minHeight: 34,
    borderRadius: 6,
    paddingVertical: 8,
    paddingHorizontal: 12,
    justifyContent: "center",
    alignItems: "center",
  },
  requestStatusText: {
    fontSize: 15,
    fontWeight: "500",
    // fontFamily: "Inter",
    letterSpacing: -0.12,
    color: "#7A4400",
  },
  listEmptyContent: {
    flexGrow: 1,
    justifyContent: "center",
  },
  // Help Button Styles
  helpButton: {
    backgroundColor: "#FFF",
    borderColor: "#E1E5EB",
    borderWidth: 1,
    width: "75%",
    padding: 5,
    borderRadius: 25,
    position: "absolute",
    bottom: 50,
    shadowColor: "#252F3B",
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.12,
    shadowRadius: 1,
    elevation: 1,
  },
  helpButtonText: {
    color: "#255F6C",
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
