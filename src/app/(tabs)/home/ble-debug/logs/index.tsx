import { useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";
import { SafeAreaView, useSafeAreaInsets } from "react-native-safe-area-context";

import {
  validoseAqua3,
  validoseDarkBlue,
  validoseGrey,
  validoseWhite,
} from "@/constants/colors";
import {
  type BleDebugLogEntry,
  clearBleDebugLogs,
  getBleDebugLogs,
  subscribeBleDebugLogs,
} from "@/utils/ble/debugLogStore";
import { PpiId, PpiType } from "@/utils/ble/messageProtocolPpi";
import { exportLogsToFile } from "@/utils/log";
import { formatHexTokensInText } from "../helpers";

type DirectionTag = "IN" | "OUT" | "SYS";
type PacketTag = "ACK" | "NAK" | "DATA" | "SYNC" | "INFO";

type DecoratedLogEntry = BleDebugLogEntry & {
  formattedMessage: string;
  direction: DirectionTag;
  packet: PacketTag;
  ppiTag: string | null;
  typeTag: string | null;
  flowKey: string | null;
  flowLabel: string | null;
  connectTop: boolean;
  connectBottom: boolean;
};
const BUILD_INFO_BADGE_CLEARANCE_PX = 72;
const SHOW_ONLY_DOSE_EVENT_PPI = true;
// const SHOW_ONLY_DOSE_EVENT_PPI = false; // Uncomment and disable the line above to render all PPI types.
const DOSE_EVENT_PPI_TAG = `PPI ${PpiId[PpiId.AD_DOSE_EVENT_REPORT]}`;

function stripLeadingLogTags(message: string): string {
  if (!message) return "";

  const [firstLine, ...restLines] = message.split("\n");

  const timestampSeparatorIndex = firstLine.indexOf("  ");
  const hasTimestampPrefix = timestampSeparatorIndex >= 0;
  const prefix = hasTimestampPrefix ? firstLine.slice(0, timestampSeparatorIndex + 2) : "";
  const content = hasTimestampPrefix ? firstLine.slice(timestampSeparatorIndex + 2) : firstLine;
  const strippedContent = content.replace(/^(?:\[[^\]\n]+\]\s*)+/, "").trimStart();
  const cleanedFirstLine = `${prefix}${strippedContent}`;

  if (!restLines.length) {
    return cleanedFirstLine;
  }

  return [cleanedFirstLine, ...restLines].join("\n");
}

function getDirectionTag(rawMessage: string): DirectionTag {
  const lower = rawMessage.toLowerCase();

  if (
    lower.includes("parsed incoming value") ||
    lower.includes("received incoming value") ||
    lower.includes("notify") ||
    lower.includes("incoming")
  ) {
    return "IN";
  }

  if (
    lower.includes("writing packet") ||
    lower.includes("send ") ||
    lower.includes("sent ") ||
    lower.includes("tx ")
  ) {
    return "OUT";
  }

  return "SYS";
}

function getPacketTag(rawMessage: string): PacketTag {
  const pktTypeMatch = rawMessage.match(/"pktTypeLabel"\s*:\s*"([^"]+)"/i);
  const pktType = (pktTypeMatch?.[1] ?? "").toUpperCase();

  if (pktType.includes("ACK")) return "ACK";
  if (pktType.includes("NAK")) return "NAK";
  if (pktType.includes("DATA")) return "DATA";
  if (pktType.includes("SYNC")) return "SYNC";

  const upper = rawMessage.toUpperCase();
  if (upper.includes("NAK")) return "NAK";
  if (upper.includes("ACK")) return "ACK";
  if (upper.includes("DATA")) return "DATA";
  if (upper.includes("SYNC")) return "SYNC";
  return "INFO";
}

function extractFlowInfo(rawMessage: string): { flowKey: string | null; flowLabel: string | null } {
  const counterMatch =
    rawMessage.match(/"pktCounter"\s*:\s*(\d+)/) ??
    rawMessage.match(/"pkt_counter"\s*:\s*(\d+)/);
  const sessionMatch =
    rawMessage.match(/"sessionId"\s*:\s*(\d+)/) ??
    rawMessage.match(/"session_id"\s*:\s*(\d+)/);

  const pktCounter = counterMatch?.[1] ?? null;
  const sessionId = sessionMatch?.[1] ?? null;

  if (!pktCounter && !sessionId) {
    return { flowKey: null, flowLabel: null };
  }

  const flowKey = `${sessionId ?? "?"}:${pktCounter ?? "?"}`;
  if (sessionId && pktCounter) {
    return { flowKey, flowLabel: `s${sessionId} · p${pktCounter}` };
  }
  if (pktCounter) {
    return { flowKey, flowLabel: `p${pktCounter}` };
  }
  return { flowKey, flowLabel: `s${sessionId}` };
}

function asRecord(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return null;
  }
  return value as Record<string, unknown>;
}

function parseLogPayloadObject(rawMessage: string): Record<string, unknown> | null {
  const lineBreakIndex = rawMessage.indexOf("\n");
  if (lineBreakIndex < 0) {
    return null;
  }

  const payloadText = rawMessage.slice(lineBreakIndex + 1).trim();
  if (!payloadText) {
    return null;
  }

  try {
    return asRecord(JSON.parse(payloadText));
  } catch {
    return null;
  }
}

function normalizePpiLabel(ppi: number | null, ppiName: string | null): string | null {
  if (ppiName && ppiName.trim()) {
    return ppiName.trim().replace(/^PPI_/, "");
  }

  if (ppi === null) {
    return null;
  }

  const enumLabel = PpiId[ppi as PpiId];
  if (typeof enumLabel === "string" && enumLabel) {
    return enumLabel;
  }

  return String(ppi);
}

function normalizeTypeLabel(type: number | null, typeName: string | null): string | null {
  if (typeName && typeName.trim()) {
    return typeName.trim();
  }

  if (type === null) {
    return null;
  }

  const enumLabel = PpiType[type as PpiType];
  if (typeof enumLabel === "string" && enumLabel) {
    return enumLabel;
  }

  return String(type);
}

function extractPpiTypeTags(rawMessage: string): { ppiTag: string | null; typeTag: string | null } {
  const parsed = parseLogPayloadObject(rawMessage);

  const recordsToCheck: Record<string, unknown>[] = [];
  if (parsed) {
    recordsToCheck.push(parsed);
    const payload = asRecord(parsed.payload);
    if (payload) {
      recordsToCheck.push(payload);
    }
    const receivedPacket = asRecord(parsed.receivedPacket);
    if (receivedPacket) {
      recordsToCheck.push(receivedPacket);
      const receivedPayload = asRecord(receivedPacket.payload);
      if (receivedPayload) {
        recordsToCheck.push(receivedPayload);
      }
    }
  }

  let ppiValue: number | null = null;
  let ppiName: string | null = null;
  let typeValue: number | null = null;
  let typeName: string | null = null;

  for (const record of recordsToCheck) {
    if (ppiValue === null && typeof record.ppi === "number") {
      ppiValue = record.ppi;
    }
    if (!ppiName && typeof record.ppiName === "string") {
      ppiName = record.ppiName;
    }
    if (!ppiName && typeof record.ppi_name === "string") {
      ppiName = record.ppi_name;
    }
    if (!ppiName && typeof record.payload_ppi === "number") {
      const payloadPpi = record.payload_ppi;
      const enumLabel = PpiId[payloadPpi as PpiId];
      ppiName = typeof enumLabel === "string" ? enumLabel : String(payloadPpi);
    }

    if (typeValue === null && typeof record.type === "number") {
      typeValue = record.type;
    }
    if (!typeName && typeof record.typeName === "string") {
      typeName = record.typeName;
    }
    if (!typeName && typeof record.type_name === "string") {
      typeName = record.type_name;
    }
    if (!typeName && typeof record.payload_type === "number") {
      const payloadType = record.payload_type;
      const enumLabel = PpiType[payloadType as PpiType];
      typeName = typeof enumLabel === "string" ? enumLabel : String(payloadType);
    }
  }

  const resolvedPpi = normalizePpiLabel(ppiValue, ppiName);
  const resolvedType = normalizeTypeLabel(typeValue, typeName);

  return {
    ppiTag: resolvedPpi ? `PPI ${resolvedPpi}` : null,
    typeTag: resolvedType ? `TYPE ${resolvedType}` : null,
  };
}

function getDirectionTagStyle(direction: DirectionTag) {
  switch (direction) {
    case "IN":
      return styles.tagIn;
    case "OUT":
      return styles.tagOut;
    default:
      return styles.tagSys;
  }
}

function getPacketTagStyle(packet: PacketTag) {
  switch (packet) {
    case "ACK":
      return styles.tagAck;
    case "NAK":
      return styles.tagNak;
    case "DATA":
      return styles.tagData;
    case "SYNC":
      return styles.tagSync;
    default:
      return styles.tagInfo;
  }
}

export default function BleDebugLogsScreen() {
  const router = useRouter();
  const insets = useSafeAreaInsets();
  const [logs, setLogs] = useState(getBleDebugLogs());
  const [isPaused, setIsPaused] = useState(false);
  const pausedRef = useRef(false);
  const logListBottomPadding = 20 + Math.max(insets.bottom, 8) + BUILD_INFO_BADGE_CLEARANCE_PX;

  useEffect(() => {
    pausedRef.current = isPaused;
  }, [isPaused]);

  useEffect(() => {
    return subscribeBleDebugLogs(() => {
      if (pausedRef.current) {
        return;
      }
      setLogs(getBleDebugLogs());
    });
  }, []);

  const orderedLogs = useMemo(() => [...logs].reverse(), [logs]);
  const visibleLogs = useMemo(
    () => orderedLogs.filter((entry) => !entry.message.toLowerCase().includes("firmware")),
    [orderedLogs]
  );

  const decoratedLogs = useMemo<DecoratedLogEntry[]>(
    () =>
      visibleLogs.map((entry) => {
        const { flowKey, flowLabel } = extractFlowInfo(entry.message);
        const { ppiTag, typeTag } = extractPpiTypeTags(entry.message);
        return {
          ...entry,
          formattedMessage: formatHexTokensInText(stripLeadingLogTags(entry.message)),
          direction: getDirectionTag(entry.message),
          packet: getPacketTag(entry.message),
          ppiTag,
          typeTag,
          flowKey,
          flowLabel,
          connectTop: false,
          connectBottom: false,
        };
      }),
    [visibleLogs]
  );

  const ppiFilteredLogs = useMemo<DecoratedLogEntry[]>(() => {
    if (!SHOW_ONLY_DOSE_EVENT_PPI) {
      return decoratedLogs;
    }

    return decoratedLogs.filter(
      (entry) => entry.ppiTag === null || entry.ppiTag === DOSE_EVENT_PPI_TAG
    );
  }, [decoratedLogs]);

  const formattedVisibleLogs = useMemo<DecoratedLogEntry[]>(() => {
    const mapped = ppiFilteredLogs;

    return mapped.map((entry, index, all) => {
      const previous = all[index - 1];
      const next = all[index + 1];
      const connectTop = Boolean(entry.flowKey && previous?.flowKey === entry.flowKey);
      const connectBottom = Boolean(entry.flowKey && next?.flowKey === entry.flowKey);

      return {
        ...entry,
        connectTop,
        connectBottom,
      };
    });
  }, [ppiFilteredLogs]);

  async function onShare() {
    await exportLogsToFile(formattedVisibleLogs.map((entry) => entry.formattedMessage));
  }

  function onTogglePause() {
    setIsPaused((previous) => {
      const next = !previous;
      if (!next) {
        setLogs(getBleDebugLogs());
      }
      return next;
    });
  }

  function onClear() {
    clearBleDebugLogs();
    setLogs(getBleDebugLogs());
  }

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.header}>
        <Pressable style={styles.headerButton} onPress={() => router.back()}>
          <Text style={styles.headerButtonText}>Back</Text>
        </Pressable>
        <Text style={styles.title}>BLE Logs</Text>
        <View style={styles.headerActions}>
          <Pressable style={styles.headerButton} onPress={onShare} disabled={!visibleLogs.length}>
            <Text style={styles.headerButtonText}>Share</Text>
          </Pressable>
          <Pressable
            style={[styles.headerButton, isPaused ? styles.headerButtonActive : null]}
            onPress={onTogglePause}
          >
            <Text
              style={[styles.headerButtonText, isPaused ? styles.headerButtonTextActive : null]}
            >
              {isPaused ? "Resume" : "Pause"}
            </Text>
          </Pressable>
          <Pressable
            style={styles.headerButton}
            onPress={onClear}
            disabled={!logs.length}
          >
            <Text style={styles.headerButtonText}>Clear</Text>
          </Pressable>
        </View>
      </View>

      <Text style={styles.subTitle}>
        {formattedVisibleLogs.length} entries{isPaused ? " · Paused" : ""}
      </Text>

      <ScrollView contentContainerStyle={[styles.logList, { paddingBottom: logListBottomPadding }]}>
        {!formattedVisibleLogs.length ? (
          <Text style={styles.emptyText}>No logs yet.</Text>
        ) : (
          formattedVisibleLogs.map((entry) => (
            <View key={entry.id} style={styles.logRow}>
              <View style={styles.logCard}>
                <View style={styles.tagRow}>
                  <Text style={[styles.tag, getDirectionTagStyle(entry.direction)]}>{entry.direction}</Text>
                  <Text style={[styles.tag, getPacketTagStyle(entry.packet)]}>{entry.packet}</Text>
                  <Text style={[styles.tag, styles.tagMeta]}>{entry.ppiTag ?? "PPI -"}</Text>
                  <Text style={[styles.tag, styles.tagMeta]}>{entry.typeTag ?? "TYPE -"}</Text>
                </View>
                <Text style={styles.logText} selectable>
                  {entry.formattedMessage}
                </Text>
              </View>
            </View>
          ))
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: validoseWhite,
    paddingHorizontal: 16,
    paddingBottom: 16,
  },
  header: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    marginBottom: 10,
  },
  title: {
    fontSize: 18,
    fontWeight: "700",
    color: validoseDarkBlue,
  },
  subTitle: {
    fontSize: 12,
    color: validoseGrey,
    marginBottom: 8,
  },
  headerActions: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  headerButton: {
    borderWidth: 1,
    borderColor: validoseAqua3,
    borderRadius: 12,
    paddingHorizontal: 10,
    paddingVertical: 6,
    minWidth: 56,
    alignItems: "center",
    backgroundColor: validoseWhite,
  },
  headerButtonText: {
    fontSize: 12,
    fontWeight: "700",
    color: validoseAqua3,
  },
  headerButtonActive: {
    borderColor: validoseDarkBlue,
    backgroundColor: "#EEF1F4",
  },
  headerButtonTextActive: {
    color: validoseDarkBlue,
  },
  logList: {
    gap: 8,
    paddingBottom: 20,
  },
  logRow: {
    flexDirection: "row",
    alignItems: "stretch",
  },
  logCard: {
    flex: 1,
    borderWidth: 1,
    borderColor: "#E4E7EC",
    borderRadius: 10,
    backgroundColor: "#FAFBFC",
    padding: 10,
  },
  tagRow: {
    flexDirection: "row",
    alignItems: "center",
    gap: 6,
    marginBottom: 6,
  },
  tag: {
    fontSize: 10,
    fontWeight: "700",
    letterSpacing: 0.2,
    borderWidth: 1,
    borderRadius: 8,
    paddingHorizontal: 6,
    paddingVertical: 2,
    overflow: "hidden",
  },
  tagIn: {
    color: "#2E6573",
    borderColor: "#BBDCE3",
    backgroundColor: "#F4FBFC",
  },
  tagOut: {
    color: "#4E549F",
    borderColor: "#CCD0F0",
    backgroundColor: "#F7F8FE",
  },
  tagSys: {
    color: "#5E6670",
    borderColor: "#D8DEE6",
    backgroundColor: "#F7F9FB",
  },
  tagAck: {
    color: "#2E7756",
    borderColor: "#C6E5D4",
    backgroundColor: "#F5FBF7",
  },
  tagNak: {
    color: "#8A4A00",
    borderColor: "#F0D9B3",
    backgroundColor: "#FFF9EF",
  },
  tagData: {
    color: "#2E6573",
    borderColor: "#BBDCE3",
    backgroundColor: "#F4FBFC",
  },
  tagSync: {
    color: "#2D5C96",
    borderColor: "#C6DBF5",
    backgroundColor: "#F4F8FF",
  },
  tagInfo: {
    color: "#5E6670",
    borderColor: "#D8DEE6",
    backgroundColor: "#F7F9FB",
  },
  tagMeta: {
    color: "#4C5561",
    borderColor: "#D3DAE3",
    backgroundColor: "#F6F8FA",
  },
  flowLabel: {
    fontSize: 10,
    fontWeight: "700",
    color: "#667085",
  },
  logText: {
    fontSize: 11,
    lineHeight: 16,
    color: validoseDarkBlue,
    fontFamily: "Courier",
  },
  emptyText: {
    fontSize: 12,
    color: validoseGrey,
  },
});
