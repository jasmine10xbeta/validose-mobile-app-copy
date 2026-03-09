import { Pressable, Text, View } from "react-native";

import { styles } from "../styles";
import { ActionButton } from "./ActionButton";

type DebugHeaderCardProps = {
  isConnected: boolean;
  connectedDeviceLabel: string;
  onBack: () => void;
  onOpenLogs: () => void;
  onDisconnect: () => void;
  disconnectDisabled: boolean;
  disconnectLoading: boolean;
};

export function DebugHeaderCard({
  isConnected,
  connectedDeviceLabel,
  onBack,
  onOpenLogs,
  onDisconnect,
  disconnectDisabled,
  disconnectLoading,
}: DebugHeaderCardProps) {
  return (
    <>
      <View style={styles.header}>
        <Pressable style={styles.backButton} onPress={onBack}>
          <Text style={styles.backButtonText}>{"<"}</Text>
        </Pressable>
        <Text style={styles.title}>BLE Debug</Text>
        <Pressable style={styles.logsButton} onPress={onOpenLogs}>
          <Text style={styles.logsButtonText}>Logs</Text>
        </Pressable>
      </View>

      <View
        style={[
          styles.connectionBanner,
          isConnected ? styles.connectionBannerConnected : styles.connectionBannerDisconnected,
        ]}
      >
        <View style={styles.connectionBannerRow}>
          <View>
            <View style={styles.connectionBannerTop}>
              <View
                style={[
                  styles.connectionDot,
                  isConnected ? styles.connectionDotConnected : styles.connectionDotDisconnected,
                ]}
              />
              <Text
                style={[
                  styles.connectionStatusText,
                  isConnected
                    ? styles.connectionStatusTextConnected
                    : styles.connectionStatusTextDisconnected,
                ]}
              >
                {isConnected ? "Connected" : "Disconnected"}
              </Text>
            </View>
            <Text style={styles.connectionDeviceText}>
              {isConnected ? connectedDeviceLabel || "Unknown Device" : "No active device"}
            </Text>
          </View>
          <ActionButton
            label="Disconnect"
            onPress={onDisconnect}
            disabled={disconnectDisabled}
            loading={disconnectLoading}
            tone="danger"
            style={styles.deviceActionButton}
          />
        </View>
      </View>
    </>
  );
}
