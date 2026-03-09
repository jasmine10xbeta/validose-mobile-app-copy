import { Pressable, Text, View } from "react-native";

import { PpiType } from "@/utils/ble/messageProtocolPpi";

import { formatDecodedValue, formatHexBytes } from "../helpers";
import { getMpPacketTypeLabel } from "../screenUtils";
import { styles } from "../styles";
import { ActionButton } from "./ActionButton";
import type {
  IncomingDetailsTab,
  PendingResponseMatcher,
  PpiRxPreview,
  PpiTxPreview,
  PushAckPreview,
} from "../types";

type PayloadDetailsCardProps = {
  lastPpiTxPreview: PpiTxPreview | null;
  lastPpiRxPreview: PpiRxPreview | null;
  lastPpiRxHumanReadable: string;
  incomingDetailsTab: IncomingDetailsTab;
  resolvedResponsePreview: PpiRxPreview | null;
  resolvedResponseHumanReadable: string;
  resolvedPushAckPreview: PushAckPreview | null;
  pendingResponseMatcher: PendingResponseMatcher | null;
  onIncomingDetailsTabChange: (tab: IncomingDetailsTab) => void;
  onRepeatLastPacket: () => void;
  repeatDisabled: boolean;
  repeatLoading: boolean;
};

function renderIncomingPreviewDetails(preview: PpiRxPreview, humanReadable: string) {
  return (
    <>
      <Text style={styles.ppiPreviewMeta}>
        {preview.receivedAt} · {preview.source}
      </Text>
      {typeof preview.ppi === "number" ? (
        <Text style={styles.ppiPreviewLine}>
          PPI: {preview.ppiName} ({preview.ppi}) · Type: {preview.typeName} ({preview.type}) · Len:{" "}
          {preview.pktPayloadLen ?? 0}
        </Text>
      ) : null}
      <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {preview.payloadHex ? formatHexBytes(preview.payloadHex) : "(empty)"}
      </Text>
      <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {preview.payloadBase64 || "(empty)"}
      </Text>
      <Text style={styles.ppiPreviewSectionLabel}>Incoming MP Header</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {preview.mpFrame
          ? JSON.stringify(
              {
                pkt_crc: preview.mpFrame.crc,
                pkt_counter: preview.mpFrame.pktCounter,
                session_id: preview.mpFrame.sessionId,
                pkt_type: preview.mpFrame.pktType,
                pkt_type_label: getMpPacketTypeLabel(preview.mpFrame.pktType),
                status: preview.mpFrame.status,
                payload_type: preview.mpFrame.payloadType,
                payload_ppi: preview.mpFrame.payloadPpi,
                pkt_payload_len: preview.mpFrame.pktPayloadLen,
                frame_len: preview.mpFrame.frameLength,
              },
              null,
              2
            )
          : "(not captured yet)"}
      </Text>
      <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Hex</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {preview.fullFrameHex ? formatHexBytes(preview.fullFrameHex) : "(not captured yet)"}
      </Text>
      <Text style={styles.ppiPreviewSectionLabel}>Incoming Full Frame Base64</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {preview.fullFrameBase64 || "(not captured yet)"}
      </Text>
      <Text style={styles.ppiPreviewSectionLabel}>Decoded Value</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {formatDecodedValue(preview.decoded)}
      </Text>
      {/*<Text style={styles.ppiPreviewSectionLabel}>Parsed Payload</Text>
      <Text style={styles.ppiPreviewCode} selectable>
        {humanReadable || "(not available)"}
      </Text>*/}
    </>
  );
}

export function PayloadDetailsCard({
  lastPpiTxPreview,
  lastPpiRxPreview,
  lastPpiRxHumanReadable,
  incomingDetailsTab,
  resolvedResponsePreview,
  resolvedResponseHumanReadable,
  resolvedPushAckPreview,
  pendingResponseMatcher,
  onIncomingDetailsTabChange,
  onRepeatLastPacket,
  repeatDisabled,
  repeatLoading,
}: PayloadDetailsCardProps) {
  return (
    <View style={styles.ppiPreviewCard}>
      <Text style={styles.ppiPreviewTitle}>Payload Details</Text>
      <Text style={styles.ppiPreviewPrimaryLabel}>Last Sent</Text>
      {/* <View style={styles.lastPacketActionRow}>
        <ActionButton
          label="Repeat Last Packet"
          onPress={onRepeatLastPacket}
          disabled={repeatDisabled}
          loading={repeatLoading}
          style={styles.lastPacketActionButton}
        />
      </View> */}
      {!lastPpiTxPreview ? (
        <Text style={styles.ppiPreviewEmpty}>No payload sent yet.</Text>
      ) : (
        <>
          <Text style={styles.ppiPreviewMeta}>
            {lastPpiTxPreview.sentAt} · {lastPpiTxPreview.action} · {lastPpiTxPreview.status}
          </Text>
          <Text style={styles.ppiPreviewLine}>
            PPI: {lastPpiTxPreview.ppiName} ({lastPpiTxPreview.ppi}) · Type: {lastPpiTxPreview.typeName} (
            {lastPpiTxPreview.type}) · Len: {lastPpiTxPreview.pktPayloadLen}
          </Text>
          <Text style={styles.ppiPreviewSectionLabel}>Structure (PPI payload envelope)</Text>
          <Text style={styles.ppiPreviewCode} selectable>
            {JSON.stringify(
              {
                type: lastPpiTxPreview.type,
                ppi: lastPpiTxPreview.ppi,
                pktPayloadLen: lastPpiTxPreview.pktPayloadLen,
              },
              null,
              2
            )}
          </Text>
          <Text style={styles.ppiPreviewSectionLabel}>Structure (Message Protocol header)</Text>
          <Text style={styles.ppiPreviewCode} selectable>
            {lastPpiTxPreview.mpFrame
              ? JSON.stringify(
                  {
                    pkt_crc: lastPpiTxPreview.mpFrame.crc,
                    pkt_counter: lastPpiTxPreview.mpFrame.pktCounter,
                    session_id: lastPpiTxPreview.mpFrame.sessionId,
                    pkt_type: lastPpiTxPreview.mpFrame.pktType,
                    pkt_type_label: getMpPacketTypeLabel(lastPpiTxPreview.mpFrame.pktType),
                    status: lastPpiTxPreview.mpFrame.status,
                    payload_type: lastPpiTxPreview.mpFrame.payloadType,
                    payload_ppi: lastPpiTxPreview.mpFrame.payloadPpi,
                    pkt_payload_len: lastPpiTxPreview.mpFrame.pktPayloadLen,
                    frame_len: lastPpiTxPreview.mpFrame.frameLength,
                  },
                  null,
                  2
                )
              : "(not captured yet)"}
          </Text>
          <Text style={styles.ppiPreviewSectionLabel}>Payload Hex</Text>
          <Text style={styles.ppiPreviewCode} selectable>
            {lastPpiTxPreview.payloadHex ? formatHexBytes(lastPpiTxPreview.payloadHex) : "(empty)"}
          </Text>
          <Text style={styles.ppiPreviewSectionLabel}>Payload Base64</Text>
          <Text style={styles.ppiPreviewCode} selectable>
            {lastPpiTxPreview.payloadBase64 || "(empty)"}
          </Text>
          <Text style={styles.ppiPreviewSectionLabel}>
            Full Frame Hex (CRC + header + PPI + payload)
          </Text>
          <Text style={styles.ppiPreviewCode} selectable>
            {lastPpiTxPreview.fullFrameHex
              ? formatHexBytes(lastPpiTxPreview.fullFrameHex)
              : "(not captured yet)"}
          </Text>
          <Text style={styles.ppiPreviewSectionLabel}>Full Frame Base64</Text>
          <Text style={styles.ppiPreviewCode} selectable>
            {lastPpiTxPreview.fullFrameBase64 || "(not captured yet)"}
          </Text>
        </>
      )}

      <Text style={styles.ppiPreviewPrimaryLabel}>Last Incoming Update</Text>
      <View style={styles.panelTabs}>
        <Pressable
          style={[styles.tabButton, incomingDetailsTab === "RESOLVED" ? styles.tabButtonActive : null]}
          onPress={() => onIncomingDetailsTabChange("RESOLVED")}
        >
          <Text style={[styles.tabText, incomingDetailsTab === "RESOLVED" ? styles.tabTextActive : null]}>
            Response To Last Sent
          </Text>
        </Pressable>
        <Pressable
          style={[styles.tabButton, incomingDetailsTab === "LATEST" ? styles.tabButtonActive : null]}
          onPress={() => onIncomingDetailsTabChange("LATEST")}
        >
          <Text style={[styles.tabText, incomingDetailsTab === "LATEST" ? styles.tabTextActive : null]}>
            Latest Incoming
          </Text>
        </Pressable>
      </View>
      {incomingDetailsTab === "LATEST" ? (
        !lastPpiRxPreview ? (
          <Text style={styles.ppiPreviewEmpty}>No incoming value yet.</Text>
        ) : (
          renderIncomingPreviewDetails(lastPpiRxPreview, lastPpiRxHumanReadable)
        )
      ) : !lastPpiTxPreview || lastPpiTxPreview.status !== "SENT_DATA" ? (
        <Text style={styles.ppiPreviewEmpty}>No sent request available yet.</Text>
      ) : resolvedResponsePreview ? (
        <>
          <Text style={styles.ppiPreviewMeta}>
            Resolved for {lastPpiTxPreview.action} sent at {lastPpiTxPreview.sentAt}
          </Text>
          {renderIncomingPreviewDetails(resolvedResponsePreview, resolvedResponseHumanReadable)}
        </>
      ) : resolvedPushAckPreview &&
        lastPpiTxPreview.type === PpiType.PUSH &&
        resolvedPushAckPreview.actionName === lastPpiTxPreview.action ? (
        <>
          <Text style={styles.ppiPreviewMeta}>
            ACK received for {resolvedPushAckPreview.actionName} at {resolvedPushAckPreview.ackedAt}
          </Text>
          <Text style={styles.ppiPreviewLine}>
            PPI: {resolvedPushAckPreview.ppi} · Type: {resolvedPushAckPreview.type} · session_id:{" "}
            {resolvedPushAckPreview.sessionId ?? "(unknown)"} · pkt_counter:{" "}
            {resolvedPushAckPreview.pktCounter ?? "(unknown)"}
          </Text>
        </>
      ) : pendingResponseMatcher ? (
        <Text style={styles.ppiPreviewEmpty}>
          Waiting for {pendingResponseMatcher.requestType === PpiType.PUSH ? "ACK/response" : "response"} to{" "}
          {pendingResponseMatcher.actionName} (PPI {pendingResponseMatcher.ppi}) sent at{" "}
          {pendingResponseMatcher.sentAt}.
        </Text>
      ) : (
        <Text style={styles.ppiPreviewEmpty}>No matching response resolved yet for last sent request.</Text>
      )}
    </View>
  );
}
