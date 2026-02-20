import { Buffer } from "buffer";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  ActivityIndicator,
  Keyboard,
  KeyboardAvoidingView,
  Modal,
  Platform,
  Pressable,
  ScrollView,
  Share,
  StyleSheet,
  Text,
  TextInput,
  TouchableWithoutFeedback,
  View,
} from "react-native";
import { SafeAreaView, useSafeAreaInsets } from "react-native-safe-area-context";

import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "@/constants/ble";
import {
  bondDevice,
  connect,
  discoverServicesAndCharacteristics,
  disconnect,
  readCharacteristic,
  scanLeDevice,
  stopLeScan,
  subscribeToCharacteristic,
  writeCharacteristic,
} from "../../../../modules/tenx-mdk-ble-rn-library/src/index";

type WriteMode = "ascii" | "hex" | "base64";
type DebugPanel = "scan" | "device" | "char" | "write";
type LogEntry = { id: string; message: string };
type PersistedDebugInputs = {
  scanSeconds: string;
  deviceId: string;
  serviceUuid: string;
  characteristicUuid: string;
  writeValue: string;
  consoleSearch: string;
};
type ActionButtonProps = {
  label: string;
  onPress: () => void;
  disabled?: boolean;
  loading?: boolean;
  tone?: "default" | "danger";
};

function decodeBase64Debug(value: string): string {
  try {
    const cleaned = (value || "").trim();
    if (!cleaned) return "base64=empty";
    const hex = Buffer.from(cleaned, "base64").toString("hex");
    const utf8 = Buffer.from(cleaned, "base64").toString("utf8");
    return `base64=${cleaned} | hex=${hex}${utf8 ? ` | utf8=${utf8}` : ""}`;
  } catch {
    return `raw=${value}`;
  }
}

function prettifyForLog(value: unknown): string {
  if (value === null || value === undefined) return String(value);

  if (typeof value === "string") {
    const trimmed = value.trim();
    if (!trimmed) return "";

    try {
      const parsed = JSON.parse(trimmed);
      return JSON.stringify(parsed, null, 2);
    } catch {
      return value;
    }
  }

  if (typeof value === "object") {
    try {
      return JSON.stringify(value, null, 2);
    } catch {
      return String(value);
    }
  }

  return String(value);
}

function ActionButton({
  label,
  onPress,
  disabled,
  loading,
  tone = "default",
}: ActionButtonProps) {
  return (
    <Pressable
      onPress={onPress}
      disabled={disabled || loading}
      style={[
        styles.actionButton,
        tone === "danger" && styles.actionButtonDanger,
        (disabled || loading) && styles.actionButtonDisabled,
      ]}
    >
      <View style={styles.actionButtonInner}>
        {loading ? <ActivityIndicator size="small" color="#173246" /> : null}
        <Text
          style={[
            styles.actionButtonText,
            tone === "danger" && styles.actionButtonTextDanger,
          ]}
        >
          {loading ? "Working..." : label}
        </Text>
      </View>
    </Pressable>
  );
}

const DEBUG_INPUTS_STORAGE_KEY = "validose_ble_debug_inputs_v1";

export default function BleDebugScreen() {
  const insets = useSafeAreaInsets();
  const router = useRouter();
  const [scanSeconds, setScanSeconds] = useState("5");
  const [deviceId, setDeviceId] = useState("");
  const [serviceUuid, setServiceUuid] = useState(SERVICE_UUIDS.CUSTOM_SERVICE);
  const [characteristicUuid, setCharacteristicUuid] = useState(
    CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL
  );
  const [writeValue, setWriteValue] = useState("");
  const [writeMode, setWriteMode] = useState<WriteMode>("ascii");
  const [activePanel, setActivePanel] = useState<DebugPanel>("scan");
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [loadingAction, setLoadingAction] = useState<string | null>(null);
  const [lastEvent, setLastEvent] = useState("Ready");
  const [consoleFullscreen, setConsoleFullscreen] = useState(false);
  const [consoleSearch, setConsoleSearch] = useState("");
  const [copyModalVisible, setCopyModalVisible] = useState(false);
  const [copyPayload, setCopyPayload] = useState("");
  const [copyTitle, setCopyTitle] = useState("Copy Response");
  const [inputsHydrated, setInputsHydrated] = useState(false);

  const logScrollRef = useRef<ScrollView | null>(null);
  const fullscreenLogScrollRef = useRef<ScrollView | null>(null);
  const subscriptionsRef = useRef<(() => void)[]>([]);

  const canRun = useMemo(() => Boolean(characteristicUuid.trim()), [characteristicUuid]);
  const filteredLogs = useMemo(() => {
    const q = consoleSearch.trim().toLowerCase();
    if (!q) return logs;
    return logs.filter((entry) => entry.message.toLowerCase().includes(q));
  }, [consoleSearch, logs]);

  function addLog(message: string, payload?: unknown) {
    const body = payload === undefined ? "" : `\n${prettifyForLog(payload)}`;
    setLastEvent(message);

    setLogs((prev) => [
      ...prev,
      {
        id: `${Date.now()}-${Math.random().toString(16).slice(2, 6)}`,
        message: `${new Date().toLocaleTimeString()}  ${message}${body}`,
      },
    ].slice(-250));
  }

  function getWritePayloadBase64() {
    const trimmed = writeValue.trim();
    if (writeMode === "base64") return trimmed;
    if (writeMode === "ascii") return Buffer.from(trimmed, "utf8").toString("base64");

    const hex = trimmed.replace(/\s+/g, "");
    if (!hex || hex.length % 2 !== 0 || !/^[\da-fA-F]+$/.test(hex)) {
      throw new Error("Hex must contain an even number of 0-9/A-F chars.");
    }

    return Buffer.from(hex, "hex").toString("base64");
  }

  async function withBusy(name: string, run: () => Promise<void>) {
    setLoadingAction(name);
    try {
      await run();
    } finally {
      setLoadingAction(null);
    }
  }

  function latestLogText() {
    return filteredLogs[filteredLogs.length - 1]?.message ?? "";
  }

  function allLogsText() {
    return filteredLogs.map((entry) => entry.message).join("\n\n");
  }

  function openCopyModal(title: string, text: string) {
    setCopyTitle(title);
    setCopyPayload(text);
    setCopyModalVisible(true);
  }

  async function shareText(title: string, text: string) {
    if (!text.trim()) {
      addLog("[SHARE] Nothing to share.");
      return;
    }

    try {
      await Share.share({ title, message: text });
      addLog(`[SHARE] Opened share sheet for ${title}.`);
    } catch (error) {
      addLog(`[SHARE][ERR] ${String(error)}`);
    }
  }

  async function onScan() {
    await withBusy("scan", async () => {
      try {
        const secs = Number(scanSeconds) || 5;
        const results = await scanLeDevice(secs);
        addLog(`[SCAN] ${secs}s`, results);
      } catch (error) {
        addLog(`[SCAN][ERR] ${String(error)}`);
      }
    });
  }

  async function onStopScan() {
    await withBusy("stop-scan", async () => {
      try {
        const results = await stopLeScan();
        addLog("[SCAN-STOP]", results);
      } catch (error) {
        addLog(`[SCAN-STOP][ERR] ${String(error)}`);
      }
    });
  }

  async function onBond() {
    await withBusy("bond", async () => {
      try {
        const secs = Number(scanSeconds) || 5;
        const scanResults = await scanLeDevice(secs);
        addLog(`[PRE-SCAN][BOND] ${secs}s`, scanResults);
        const res = await bondDevice(deviceId.trim());
        addLog("[BOND]", res);
      } catch (error) {
        addLog(`[BOND][ERR] ${String(error)}`);
      }
    });
  }

  async function onConnect() {
    await withBusy("connect", async () => {
      try {
        const secs = Number(scanSeconds) || 5;
        const scanResults = await scanLeDevice(secs);
        addLog(`[PRE-SCAN][CONNECT] ${secs}s`, scanResults);
        const res = await connect(deviceId.trim());
        addLog("[CONNECT]", res);
      } catch (error) {
        addLog(`[CONNECT][ERR] ${String(error)}`);
      }
    });
  }

  async function onDisconnect() {
    await withBusy("disconnect", async () => {
      try {
        const res = await disconnect();
        addLog("[DISCONNECT]", res);
      } catch (error) {
        addLog(`[DISCONNECT][ERR] ${String(error)}`);
      }
    });
  }

  async function onDiscover() {
    await withBusy("discover", async () => {
      try {
        const res = await discoverServicesAndCharacteristics();
        addLog("[DISCOVER]", res);
      } catch (error) {
        addLog(`[DISCOVER][ERR] ${String(error)}`);
      }
    });
  }

  async function onRead() {
    await withBusy("read", async () => {
      try {
        const raw = await readCharacteristic(characteristicUuid.trim());
        addLog(`[READ] ${characteristicUuid}`, decodeBase64Debug(String(raw || "")));
      } catch (error) {
        addLog(`[READ][ERR] ${String(error)}`);
      }
    });
  }

  async function onWrite() {
    await withBusy("write", async () => {
      try {
        const payload = getWritePayloadBase64();
        const res = await writeCharacteristic(characteristicUuid.trim(), payload);
        addLog(`[WRITE] ${characteristicUuid}`, {
          mode: writeMode,
          raw: writeValue,
          base64: payload,
          result: res,
        });
      } catch (error) {
        addLog(`[WRITE][ERR] ${String(error)}`);
      }
    });
  }

  async function onSubscribe() {
    await withBusy("subscribe", async () => {
      try {
        const unsub = await subscribeToCharacteristic(
          characteristicUuid.trim(),
          serviceUuid.trim(),
          ({ uuid, fullUuid, hex, deviceId: eventDeviceId }) => {
            addLog("[NOTIFY]", {
              uuid,
              fullUuid,
              deviceId: eventDeviceId,
              hex,
            });
          }
        );

        subscriptionsRef.current.push(unsub);
        addLog("[SUBSCRIBE]", { serviceUuid, characteristicUuid });
      } catch (error) {
        addLog(`[SUBSCRIBE][ERR] ${String(error)}`);
      }
    });
  }

  function onClearSubscriptions() {
    subscriptionsRef.current.forEach((fn) => fn());
    subscriptionsRef.current = [];
    addLog("[SUBSCRIBE] Cleared all listeners");
  }

  useEffect(() => {
    requestAnimationFrame(() => {
      logScrollRef.current?.scrollToEnd({ animated: true });
      fullscreenLogScrollRef.current?.scrollToEnd({ animated: true });
    });
  }, [logs, consoleFullscreen]);

  useEffect(() => {
    let mounted = true;

    async function hydrateInputs() {
      try {
        const raw = await AsyncStorage.getItem(DEBUG_INPUTS_STORAGE_KEY);
        if (!raw) return;
        const parsed = JSON.parse(raw) as Partial<PersistedDebugInputs>;
        if (!mounted) return;

        if (typeof parsed.scanSeconds === "string") setScanSeconds(parsed.scanSeconds);
        if (typeof parsed.deviceId === "string") setDeviceId(parsed.deviceId);
        if (typeof parsed.serviceUuid === "string") setServiceUuid(parsed.serviceUuid);
        if (typeof parsed.characteristicUuid === "string") {
          setCharacteristicUuid(parsed.characteristicUuid);
        }
        if (typeof parsed.writeValue === "string") setWriteValue(parsed.writeValue);
        if (typeof parsed.consoleSearch === "string") setConsoleSearch(parsed.consoleSearch);
      } catch (error) {
        addLog("[PERSIST][ERR] Failed to load debug inputs", String(error));
      } finally {
        if (mounted) setInputsHydrated(true);
      }
    }

    hydrateInputs();

    return () => {
      mounted = false;
    };
  }, []);

  useEffect(() => {
    if (!inputsHydrated) return;

    const payload: PersistedDebugInputs = {
      scanSeconds,
      deviceId,
      serviceUuid,
      characteristicUuid,
      writeValue,
      consoleSearch,
    };

    AsyncStorage.setItem(DEBUG_INPUTS_STORAGE_KEY, JSON.stringify(payload)).catch((error) => {
      addLog("[PERSIST][ERR] Failed to save debug inputs", String(error));
    });
  }, [
    inputsHydrated,
    scanSeconds,
    deviceId,
    serviceUuid,
    characteristicUuid,
    writeValue,
    consoleSearch,
  ]);

  useEffect(() => {
    return () => {
      subscriptionsRef.current.forEach((fn) => fn());
      subscriptionsRef.current = [];
    };
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <KeyboardAvoidingView
        style={styles.keyboardContainer}
        behavior={Platform.OS === "ios" ? "padding" : "height"}
      >
        <TouchableWithoutFeedback onPress={Keyboard.dismiss} accessible={false}>
          <ScrollView
            contentContainerStyle={styles.content}
            keyboardShouldPersistTaps="handled"
          >
            <View style={styles.panel}>
            <View style={styles.header}>
              <Text style={styles.title}>BLE Debug</Text>
              <Text style={styles.subtitle}>Focused controls with realtime feedback</Text>
            </View>

            <View style={styles.headerActions}>
              <ActionButton label="Back" onPress={() => router.back()} />
              <ActionButton label="Clear Log" onPress={() => setLogs([])} tone="danger" />
            </View>

            <View style={styles.statusCard}>
              <View style={styles.statusTop}>
                <Text style={styles.statusLabel}>Current Action</Text>
                {loadingAction ? (
                  <View style={styles.statusBusy}>
                    <ActivityIndicator size="small" color="#0E7490" />
                    <Text style={styles.statusBusyText}>{loadingAction}</Text>
                  </View>
                ) : (
                  <Text style={styles.statusIdle}>idle</Text>
                )}
              </View>
              <Text style={styles.statusEvent} numberOfLines={2}>
                {lastEvent}
              </Text>
            </View>

            <View style={styles.logBox}>
              <View style={styles.logHeader}>
                <Text style={styles.logTitle}>Live Console</Text>
              <View style={styles.logHeaderRight}>
                  <Text style={styles.logMeta}>
                    {filteredLogs.length}/{logs.length} events
                  </Text>
                  <Pressable onPress={() => setConsoleFullscreen(true)} style={styles.logHeaderButton}>
                    <Text style={styles.logHeaderButtonText}>Full Screen</Text>
                  </Pressable>
                </View>
            </View>

            <TextInput
              value={consoleSearch}
              onChangeText={setConsoleSearch}
              placeholder="Search logs..."
              placeholderTextColor="#7E97AA"
              style={styles.logSearchInput}
            />

            <View style={styles.logActions}>
                <Pressable
                  style={styles.logActionButton}
                  onPress={() => openCopyModal("Copy Latest Response", latestLogText())}
                >
                  <Text style={styles.logActionButtonText}>Copy Latest</Text>
                </Pressable>
                <Pressable
                  style={styles.logActionButton}
                  onPress={() => openCopyModal("Copy All Responses", allLogsText())}
                >
                  <Text style={styles.logActionButtonText}>Copy All</Text>
                </Pressable>
                <Pressable
                  style={styles.logActionButton}
                  onPress={() => shareText("Latest BLE Response", latestLogText())}
                >
                  <Text style={styles.logActionButtonText}>Share Latest</Text>
                </Pressable>
                <Pressable
                  style={styles.logActionButton}
                  onPress={() => shareText("All BLE Responses", allLogsText())}
                >
                  <Text style={styles.logActionButtonText}>Share All</Text>
                </Pressable>
              </View>

              <ScrollView
                ref={logScrollRef}
                style={styles.logScroll}
                nestedScrollEnabled
                keyboardShouldPersistTaps="handled"
              >
                {filteredLogs.length === 0 ? (
                  <Text style={styles.logEmpty}>No events yet.</Text>
                ) : (
                  filteredLogs.map((entry) => (
                    <Text key={entry.id} style={styles.logLine} selectable>
                      {entry.message}
                    </Text>
                  ))
                )}
              </ScrollView>
            </View>

            <View style={styles.panelTabs}>
              {(
                [
                  ["scan", "Scanner"],
                  ["device", "Device"],
                  ["char", "Read/Sub"],
                  ["write", "Write"],
                ] as [DebugPanel, string][]
              ).map(([key, label]) => (
                <Pressable
                  key={key}
                  style={[styles.tabButton, activePanel === key && styles.tabButtonActive]}
                  onPress={() => setActivePanel(key)}
                >
                  <Text style={[styles.tabText, activePanel === key && styles.tabTextActive]}>
                    {label}
                  </Text>
                </Pressable>
              ))}
            </View>

            {activePanel === "scan" ? (
              <View style={styles.section}>
                <Text style={styles.sectionTitle}>Scanner</Text>
                <Text style={styles.inputLabel}>Scan seconds</Text>
                <TextInput
                  value={scanSeconds}
                  onChangeText={setScanSeconds}
                  style={styles.input}
                  placeholder="5"
                  keyboardType="number-pad"
                  placeholderTextColor="#8A99A7"
                />
                <View style={styles.row}>
                  <ActionButton label="Scan" onPress={onScan} loading={loadingAction === "scan"} />
                  <ActionButton
                    label="Stop"
                    onPress={onStopScan}
                    loading={loadingAction === "stop-scan"}
                  />
                </View>
              </View>
            ) : null}

            {activePanel === "device" ? (
              <View style={styles.section}>
                <Text style={styles.sectionTitle}>Device Connection</Text>
                <Text style={styles.inputLabel}>Device id / name</Text>
                <TextInput
                  value={deviceId}
                  onChangeText={setDeviceId}
                  style={styles.input}
                  placeholder="VAL-OP ..."
                  placeholderTextColor="#8A99A7"
                />
                <View style={styles.row}>
                  <ActionButton label="Bond" onPress={onBond} loading={loadingAction === "bond"} />
                  <ActionButton
                    label="Connect"
                    onPress={onConnect}
                    loading={loadingAction === "connect"}
                  />
                  <ActionButton
                    label="Disconnect"
                    onPress={onDisconnect}
                    loading={loadingAction === "disconnect"}
                  />
                </View>
                <View style={styles.row}>
                  <ActionButton
                    label="Discover Services/Chars"
                    onPress={onDiscover}
                    loading={loadingAction === "discover"}
                  />
                </View>
              </View>
            ) : null}

            {activePanel === "char" ? (
              <View style={styles.section}>
                <Text style={styles.sectionTitle}>Characteristic Operations</Text>
                <Text style={styles.inputLabel}>Service UUID (for subscribe)</Text>
                <TextInput
                  value={serviceUuid}
                  onChangeText={setServiceUuid}
                  style={styles.input}
                  placeholderTextColor="#8A99A7"
                />
                <Text style={styles.inputLabel}>Characteristic UUID</Text>
                <TextInput
                  value={characteristicUuid}
                  onChangeText={setCharacteristicUuid}
                  style={styles.input}
                  placeholderTextColor="#8A99A7"
                />
                <View style={styles.row}>
                  <ActionButton
                    label="Read"
                    onPress={onRead}
                    disabled={!canRun}
                    loading={loadingAction === "read"}
                  />
                  <ActionButton
                    label="Subscribe"
                    onPress={onSubscribe}
                    disabled={!canRun}
                    loading={loadingAction === "subscribe"}
                  />
                  <ActionButton label="Clear Subs" onPress={onClearSubscriptions} />
                </View>
              </View>
            ) : null}

            {activePanel === "write" ? (
              <View style={styles.section}>
                <Text style={styles.sectionTitle}>Write Payload</Text>
                <TextInput
                  value={writeValue}
                  onChangeText={setWriteValue}
                  style={[styles.input, styles.writeInput]}
                  placeholder="text / hex / base64"
                  placeholderTextColor="#8A99A7"
                  multiline
                />
                <View style={styles.modeRow}>
                  {(["ascii", "hex", "base64"] as WriteMode[]).map((mode) => (
                    <Pressable
                      key={mode}
                      style={[styles.modeButton, writeMode === mode && styles.modeButtonActive]}
                      onPress={() => setWriteMode(mode)}
                    >
                      <Text style={[styles.modeText, writeMode === mode && styles.modeTextActive]}>
                        {mode.toUpperCase()}
                      </Text>
                    </Pressable>
                  ))}
                </View>
                <View style={styles.row}>
                  <ActionButton
                    label="Write"
                    onPress={onWrite}
                    disabled={!canRun}
                    loading={loadingAction === "write"}
                  />
                </View>
              </View>
            ) : null}
            </View>
          </ScrollView>
        </TouchableWithoutFeedback>
      </KeyboardAvoidingView>

      <Modal
        transparent
        animationType="slide"
        visible={copyModalVisible}
        onRequestClose={() => setCopyModalVisible(false)}
      >
        <View style={styles.copyModalBackdrop}>
          <View style={styles.copyModalCard}>
            <Text style={styles.copyModalTitle}>{copyTitle}</Text>
            <Text style={styles.copyModalHint}>
              Tap inside the text box, then use native Copy from the selection menu.
            </Text>
            <TextInput
              value={copyPayload}
              multiline
              editable={false}
              selectTextOnFocus
              autoFocus
              style={styles.copyModalInput}
            />
            <View style={styles.copyModalActions}>
              <ActionButton label="Close" onPress={() => setCopyModalVisible(false)} />
              <ActionButton label="Share" onPress={() => shareText(copyTitle, copyPayload)} />
            </View>
          </View>
        </View>
      </Modal>

      <Modal
        visible={consoleFullscreen}
        animationType="slide"
        onRequestClose={() => setConsoleFullscreen(false)}
      >
        <TouchableWithoutFeedback onPress={Keyboard.dismiss} accessible={false}>
          <SafeAreaView
            edges={["top", "left", "right", "bottom"]}
            style={[
              styles.fullscreenConsoleRoot,
              {
                paddingTop: Math.max(insets.top, 14),
                paddingBottom: Math.max(insets.bottom, 14),
              },
            ]}
          >
            <View style={styles.fullscreenConsoleHeader}>
            <Text style={styles.fullscreenConsoleTitle}>Live Console</Text>
            <Pressable
              style={styles.fullscreenConsoleCloseButton}
              onPress={() => setConsoleFullscreen(false)}
            >
              <Text style={styles.fullscreenConsoleCloseText}>Close</Text>
            </Pressable>
          </View>

          <View style={styles.fullscreenConsoleMetaRow}>
            <Text style={styles.logMeta}>{filteredLogs.length}/{logs.length} events</Text>
          </View>

          <TextInput
            value={consoleSearch}
            onChangeText={setConsoleSearch}
            placeholder="Search logs..."
            placeholderTextColor="#7E97AA"
            style={styles.logSearchInput}
          />

          <View style={styles.logActions}>
            <Pressable
              style={styles.logActionButton}
              onPress={() => openCopyModal("Copy Latest Response", latestLogText())}
            >
              <Text style={styles.logActionButtonText}>Copy Latest</Text>
            </Pressable>
            <Pressable
              style={styles.logActionButton}
              onPress={() => openCopyModal("Copy All Responses", allLogsText())}
            >
              <Text style={styles.logActionButtonText}>Copy All</Text>
            </Pressable>
            <Pressable
              style={styles.logActionButton}
              onPress={() => shareText("Latest BLE Response", latestLogText())}
            >
              <Text style={styles.logActionButtonText}>Share Latest</Text>
            </Pressable>
            <Pressable
              style={styles.logActionButton}
              onPress={() => shareText("All BLE Responses", allLogsText())}
            >
              <Text style={styles.logActionButtonText}>Share All</Text>
            </Pressable>
          </View>

            <ScrollView
              ref={fullscreenLogScrollRef}
              style={styles.fullscreenConsoleScroll}
              keyboardShouldPersistTaps="handled"
            >
              {filteredLogs.length === 0 ? (
                <Text style={styles.logEmpty}>No events yet.</Text>
              ) : (
                filteredLogs.map((entry) => (
                  <Text key={entry.id} style={styles.logLine} selectable>
                    {entry.message}
                  </Text>
                ))
              )}
            </ScrollView>
          </SafeAreaView>
        </TouchableWithoutFeedback>
      </Modal>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: "#EEF2F5" },
  keyboardContainer: { flex: 1 },
  content: { padding: 16, paddingBottom: 48, alignItems: "center" },
  panel: {
    width: "100%",
    maxWidth: 560,
    backgroundColor: "#FFFFFF",
    borderRadius: 18,
    padding: 16,
    borderWidth: 1,
    borderColor: "#E3E8EE",
    gap: 12,
    shadowColor: "#101828",
    shadowOpacity: 0.05,
    shadowRadius: 12,
    shadowOffset: { width: 0, height: 6 },
    elevation: 2,
  },
  header: { alignItems: "center", gap: 4 },
  headerActions: { flexDirection: "row", justifyContent: "center", gap: 8 },
  title: { fontSize: 24, fontWeight: "700", color: "#132534", textAlign: "center" },
  subtitle: { fontSize: 13, color: "#5B6B79", textAlign: "center" },
  statusCard: {
    borderWidth: 1,
    borderColor: "#D6E2EB",
    borderRadius: 12,
    padding: 12,
    backgroundColor: "#F3FAFF",
    gap: 8,
  },
  statusTop: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  statusLabel: { fontSize: 12, fontWeight: "700", color: "#1E4058" },
  statusBusy: { flexDirection: "row", gap: 6, alignItems: "center" },
  statusBusyText: { fontSize: 12, fontWeight: "700", color: "#0E7490" },
  statusIdle: { fontSize: 12, color: "#557189", fontWeight: "700" },
  statusEvent: { fontSize: 12, color: "#2A4358" },
  panelTabs: { flexDirection: "row", justifyContent: "space-between", gap: 6 },
  tabButton: {
    flex: 1,
    borderWidth: 1,
    borderColor: "#D3DEE8",
    borderRadius: 10,
    paddingVertical: 8,
    alignItems: "center",
    backgroundColor: "#F7FAFC",
  },
  tabButtonActive: {
    borderColor: "#0E7490",
    backgroundColor: "#E7F8FC",
  },
  tabText: { fontSize: 12, color: "#3A566C", fontWeight: "700" },
  tabTextActive: { color: "#0E7490" },
  section: {
    borderWidth: 1,
    borderColor: "#E7ECF1",
    borderRadius: 12,
    padding: 12,
    gap: 8,
    backgroundColor: "#FAFCFE",
  },
  sectionTitle: { fontSize: 14, fontWeight: "700", color: "#1B3347" },
  inputLabel: { fontSize: 12, color: "#5C6F7E", fontWeight: "600" },
  input: {
    borderWidth: 1,
    borderColor: "#D7DEE6",
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 10,
    minHeight: 44,
    backgroundColor: "#FFFFFF",
    color: "#122535",
    fontSize: 14,
  },
  writeInput: { minHeight: 76, textAlignVertical: "top" },
  row: { flexDirection: "row", gap: 8, flexWrap: "wrap", alignItems: "center" },
  actionButton: {
    borderWidth: 1,
    borderColor: "#CAD5DF",
    borderRadius: 999,
    paddingHorizontal: 14,
    paddingVertical: 9,
    backgroundColor: "#FFFFFF",
  },
  actionButtonDanger: {
    borderColor: "#F3C9C5",
    backgroundColor: "#FFF5F5",
  },
  actionButtonDisabled: { opacity: 0.5 },
  actionButtonInner: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    gap: 6,
  },
  actionButtonText: { color: "#173246", fontSize: 12, fontWeight: "700" },
  actionButtonTextDanger: { color: "#A7362D" },
  modeRow: { flexDirection: "row", gap: 8, justifyContent: "center" },
  modeButton: {
    borderWidth: 1,
    borderColor: "#CAD6E2",
    borderRadius: 999,
    paddingHorizontal: 14,
    paddingVertical: 8,
    backgroundColor: "#FFFFFF",
  },
  modeButtonActive: {
    backgroundColor: "#0E7490",
    borderColor: "#0E7490",
  },
  modeText: { color: "#365167", fontWeight: "700", fontSize: 11 },
  modeTextActive: { color: "#FFFFFF" },
  logBox: {
    borderWidth: 1,
    borderColor: "#D9E1E8",
    borderRadius: 12,
    padding: 12,
    backgroundColor: "#0F1B2A",
    minHeight: 220,
    maxHeight: 280,
    overflow: "hidden",
  },
  logHeader: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 8,
  },
  logHeaderRight: { flexDirection: "row", alignItems: "center", gap: 8 },
  logTitle: {
    fontSize: 13,
    fontWeight: "700",
    color: "#BFD6E8",
  },
  logHeaderButton: {
    borderWidth: 1,
    borderColor: "#2B4A63",
    borderRadius: 999,
    paddingHorizontal: 10,
    paddingVertical: 4,
    backgroundColor: "#10273A",
  },
  logHeaderButtonText: { color: "#D6E7F3", fontSize: 11, fontWeight: "700" },
  logMeta: { fontSize: 11, color: "#8FA9BC", fontWeight: "600" },
  logSearchInput: {
    borderWidth: 1,
    borderColor: "#24455F",
    borderRadius: 10,
    backgroundColor: "#112536",
    color: "#D9E9F5",
    paddingHorizontal: 10,
    paddingVertical: 8,
    fontSize: 12,
    marginBottom: 10,
  },
  logActions: { flexDirection: "row", flexWrap: "wrap", gap: 6, marginBottom: 10 },
  logActionButton: {
    borderWidth: 1,
    borderColor: "#2C4960",
    borderRadius: 999,
    backgroundColor: "#0F2537",
    paddingHorizontal: 10,
    paddingVertical: 5,
  },
  logActionButtonText: { color: "#CFE4F2", fontSize: 11, fontWeight: "700" },
  logScroll: { maxHeight: 180 },
  logEmpty: { fontSize: 12, color: "#90A4B7" },
  logLine: {
    fontSize: 11,
    lineHeight: 16,
    color: "#D6E4EF",
    marginBottom: 7,
    fontFamily: "Courier",
  },
  copyModalBackdrop: {
    flex: 1,
    backgroundColor: "rgba(9,16,27,0.55)",
    justifyContent: "flex-end",
  },
  copyModalCard: {
    backgroundColor: "#FFFFFF",
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    padding: 16,
    gap: 10,
    maxHeight: "80%",
  },
  copyModalTitle: { fontSize: 16, fontWeight: "700", color: "#17293A" },
  copyModalHint: { fontSize: 12, color: "#607286" },
  copyModalInput: {
    minHeight: 180,
    borderWidth: 1,
    borderColor: "#D3DDE7",
    borderRadius: 10,
    backgroundColor: "#F9FBFD",
    padding: 10,
    color: "#122535",
    textAlignVertical: "top",
  },
  copyModalActions: { flexDirection: "row", gap: 8, justifyContent: "flex-end" },
  fullscreenConsoleRoot: {
    flex: 1,
    backgroundColor: "#0F1B2A",
    padding: 14,
    gap: 10,
  },
  fullscreenConsoleHeader: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  fullscreenConsoleTitle: {
    fontSize: 18,
    fontWeight: "700",
    color: "#E4F0F8",
  },
  fullscreenConsoleCloseButton: {
    borderWidth: 1,
    borderColor: "#2B4A63",
    borderRadius: 999,
    backgroundColor: "#10273A",
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  fullscreenConsoleCloseText: {
    color: "#D6E7F3",
    fontSize: 12,
    fontWeight: "700",
  },
  fullscreenConsoleMetaRow: {
    flexDirection: "row",
    justifyContent: "flex-end",
  },
  fullscreenConsoleScroll: {
    flex: 1,
    borderWidth: 1,
    borderColor: "#24455F",
    borderRadius: 10,
    backgroundColor: "#112536",
    padding: 10,
  },
});
