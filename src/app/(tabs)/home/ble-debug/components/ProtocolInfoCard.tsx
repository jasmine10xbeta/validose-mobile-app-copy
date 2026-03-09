import { Text, View } from "react-native";

import { styles } from "../styles";

type ProtocolInfo = {
  protocolState: string;
  txState: string;
  currentSessionId: number | null;
  logCount: number;
  ackTimeoutMs: number;
  maxRetries: number;
  txReadyTimeoutMs: number;
  txCompletionWaitMs: number;
  lateAckWatchMs: number;
  processIntervalMs: number;
  maxPacketLength: number;
};

type ProtocolInfoCardProps = {
  protocolInfo: ProtocolInfo;
  serviceUuid: string;
  txUuid: string;
  rxUuid: string;
};

export function ProtocolInfoCard({
  protocolInfo,
  serviceUuid,
  txUuid,
  rxUuid,
}: ProtocolInfoCardProps) {
  return (
    <View style={styles.protocolInfoCard}>
      <Text style={styles.protocolInfoTitle}>Protocol Info</Text>
      <View style={styles.protocolInfoGrid}>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>State</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.protocolState}</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Current TX</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.txState}</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Current session_id (app)</Text>
          <Text style={styles.protocolInfoValue}>
            {protocolInfo.currentSessionId === null ? "(not started)" : protocolInfo.currentSessionId}
          </Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Debug events</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.logCount}</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>ACK timeout</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.ackTimeoutMs} ms</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Max retries</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.maxRetries}</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>TX ready timeout</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.txReadyTimeoutMs} ms</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>TX completion wait</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.txCompletionWaitMs} ms</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Late ACK watch</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.lateAckWatchMs} ms</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Process interval</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.processIntervalMs} ms</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Max packet length</Text>
          <Text style={styles.protocolInfoValue}>{protocolInfo.maxPacketLength} bytes</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>Service UUID</Text>
          <Text style={styles.protocolInfoValue}>{serviceUuid}</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>TX UUID (App → Device)</Text>
          <Text style={styles.protocolInfoValue}>{txUuid}</Text>
        </View>
        <View style={styles.protocolInfoRow}>
          <Text style={styles.protocolInfoLabel}>RX UUID (Device → App)</Text>
          <Text style={styles.protocolInfoValue}>{rxUuid}</Text>
        </View>
      </View>
    </View>
  );
}
