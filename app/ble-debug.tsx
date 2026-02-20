import { Buffer } from "buffer";
import { useRouter } from "expo-router";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  ScrollView,
  StyleSheet,
  TextInput,
  View,
  Pressable,
  Text,
} from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

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
} from "../modules/tenx-mdk-ble-rn-library/src/index";
import { VButton } from "../src/components/common/VButton";
import { VText } from "../src/components/common/VText";
import {
  CHARACTERISTIC_UUIDS,
  SERVICE_UUIDS,
} from "../src/constants/ble";

type WriteMode = "ascii" | "hex" | "base64";
type LogEntry = { id: string; message: string };

function envEnabled() {
  const raw = String(
    process.env.ENABLE_BLE_BYPASS ?? process.env.EXPO_PUBLIC_ENABLE_BLE_BYPASS ?? ""
  )
    .trim()
    .toLowerCase();
  return raw === "1" || raw === "true" || raw === "yes";
}

function decodeBase64Debug(value: string): string {
  try {
    const cleaned = (value || "").trim();
    if (!cleaned) return "base64=empty";
    const hex = Buffer.from(cleaned, "base64").toString("hex");
    let ascii = "";
    try {
      ascii = Buffer.from(cleaned, "base64").toString("utf8");
    } catch {
      ascii = "";
    }
    return `base64=${cleaned} | hex=${hex}${ascii ? ` | utf8=${ascii}` : ""}`;
  } catch {
    return `raw=${value}`;
  }
}

function formatNow() {
  return new Date().toLocaleTimeString();
}

export default function BleDebugScreen() {
  const router = useRouter();
  const enabled = envEnabled();
  const [scanSeconds, setScanSeconds] = useState("5");
  const [deviceId, setDeviceId] = useState("");
  const [serviceUuid, setServiceUuid] = useState(SERVICE_UUIDS.CUSTOM_SERVICE);
  const [characteristicUuid, setCharacteristicUuid] = useState(
    CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL
  );
  const [writeValue, setWriteValue] = useState("");
  const [writeMode, setWriteMode] = useState<WriteMode>("ascii");
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const subscriptionsRef = useRef<(() => void)[]>([]);

  const canRun = useMemo(
    () => Boolean(characteristicUuid.trim()),
    [characteristicUuid]
  );

  function addLog(message: string) {
    const entry: LogEntry = {
      id: `${Date.now()}-${Math.random().toString(16).slice(2, 6)}`,
      message: `${formatNow()}  ${message}`,
    };
    setLogs((prev) => [entry, ...prev].slice(0, 250));
  }

  function getWritePayloadBase64() {
    const trimmed = writeValue.trim();
    if (writeMode === "base64") return trimmed;
    if (writeMode === "ascii") {
      return Buffer.from(trimmed, "utf8").toString("base64");
    }

    const hex = trimmed.replace(/\s+/g, "");
    if (!hex || hex.length % 2 !== 0 || !/^[\da-fA-F]+$/.test(hex)) {
      throw new Error("Hex must contain an even number of 0-9/A-F chars.");
    }
    return Buffer.from(hex, "hex").toString("base64");
  }

  async function onScan() {
    try {
      const secs = Number(scanSeconds) || 5;
      const results = await scanLeDevice(secs);
      addLog(`[SCAN] ${secs}s -> ${JSON.stringify(results)}`);
    } catch (error) {
      addLog(`[SCAN][ERR] ${String(error)}`);
    }
  }

  async function onStopScan() {
    try {
      const results = await stopLeScan();
      addLog(`[SCAN-STOP] ${JSON.stringify(results)}`);
    } catch (error) {
      addLog(`[SCAN-STOP][ERR] ${String(error)}`);
    }
  }

  async function onBond() {
    try {
      const res = await bondDevice(deviceId.trim());
      addLog(`[BOND] ${JSON.stringify(res)}`);
    } catch (error) {
      addLog(`[BOND][ERR] ${String(error)}`);
    }
  }

  async function onConnect() {
    try {
      const res = await connect(deviceId.trim());
      addLog(`[CONNECT] ${JSON.stringify(res)}`);
    } catch (error) {
      addLog(`[CONNECT][ERR] ${String(error)}`);
    }
  }

  async function onDisconnect() {
    try {
      const res = await disconnect();
      addLog(`[DISCONNECT] ${JSON.stringify(res)}`);
    } catch (error) {
      addLog(`[DISCONNECT][ERR] ${String(error)}`);
    }
  }

  async function onDiscover() {
    try {
      const res = await discoverServicesAndCharacteristics();
      addLog(`[DISCOVER] ${JSON.stringify(res)}`);
    } catch (error) {
      addLog(`[DISCOVER][ERR] ${String(error)}`);
    }
  }

  async function onRead() {
    try {
      const raw = await readCharacteristic(characteristicUuid.trim());
      addLog(`[READ] ${characteristicUuid} -> ${decodeBase64Debug(String(raw || ""))}`);
    } catch (error) {
      addLog(`[READ][ERR] ${String(error)}`);
    }
  }

  async function onWrite() {
    try {
      const payload = getWritePayloadBase64();
      const res = await writeCharacteristic(characteristicUuid.trim(), payload);
      addLog(
        `[WRITE] ${characteristicUuid} <= mode=${writeMode} raw=${writeValue} base64=${payload} result=${String(
          res
        )}`
      );
    } catch (error) {
      addLog(`[WRITE][ERR] ${String(error)}`);
    }
  }

  async function onSubscribe() {
    try {
      const unsub = await subscribeToCharacteristic(
        characteristicUuid.trim(),
        serviceUuid.trim(),
        ({ uuid, fullUuid, hex, deviceId: eventDeviceId }) => {
          addLog(
            `[NOTIFY] uuid=${uuid} full=${fullUuid} device=${eventDeviceId} hex=${hex}`
          );
        }
      );
      subscriptionsRef.current.push(unsub);
      addLog(`[SUBSCRIBE] service=${serviceUuid} char=${characteristicUuid}`);
    } catch (error) {
      addLog(`[SUBSCRIBE][ERR] ${String(error)}`);
    }
  }

  function onClearSubscriptions() {
    subscriptionsRef.current.forEach((fn) => fn());
    subscriptionsRef.current = [];
    addLog("[SUBSCRIBE] Cleared all listeners");
  }

  useEffect(() => {
    return () => {
      subscriptionsRef.current.forEach((fn) => fn());
      subscriptionsRef.current = [];
    };
  }, []);

  if (!enabled) {
    return (
      <SafeAreaView style={styles.container}>
        <View style={styles.centerBox}>
          <VText textVariant="Label">BLE debug disabled</VText>
          <VButton onPress={() => router.back()} label="Back" />
        </View>
      </SafeAreaView>
    );
  }

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.content}>
        <VText textVariant="Label" style={styles.title}>
          BLE Debug Console
        </VText>

        <VText textVariant="Body">Scan seconds</VText>
        <TextInput
          value={scanSeconds}
          onChangeText={setScanSeconds}
          style={styles.input}
          placeholder="5"
          keyboardType="number-pad"
        />
        <View style={styles.row}>
          <VButton onPress={onScan} label="Scan" />
          <VButton onPress={onStopScan} label="Stop scan" />
        </View>

        <VText textVariant="Body">Device id / name</VText>
        <TextInput
          value={deviceId}
          onChangeText={setDeviceId}
          style={styles.input}
          placeholder="VAL-OP ..."
        />
        <View style={styles.row}>
          <VButton onPress={onBond} label="Bond" />
          <VButton onPress={onConnect} label="Connect" />
          <VButton onPress={onDisconnect} label="Disconnect" />
        </View>

        <VButton onPress={onDiscover} label="Discover services/chars" />

        <VText textVariant="Body">Service UUID (subscribe)</VText>
        <TextInput
          value={serviceUuid}
          onChangeText={setServiceUuid}
          style={styles.input}
          autoCapitalize="none"
        />
        <VText textVariant="Body">Characteristic UUID</VText>
        <TextInput
          value={characteristicUuid}
          onChangeText={setCharacteristicUuid}
          style={styles.input}
          autoCapitalize="none"
        />
        <View style={styles.row}>
          <VButton onPress={onRead} label="Read" disabled={!canRun} />
          <VButton onPress={onSubscribe} label="Subscribe" disabled={!canRun} />
          <VButton onPress={onClearSubscriptions} label="Clear subs" />
        </View>

        <VText textVariant="Body">Write value</VText>
        <TextInput
          value={writeValue}
          onChangeText={setWriteValue}
          style={styles.input}
          placeholder="text / hex / base64"
          multiline
        />

        <View style={styles.modeRow}>
          {(["ascii", "hex", "base64"] as WriteMode[]).map((mode) => (
            <Pressable
              key={mode}
              style={[styles.modeButton, writeMode === mode && styles.modeButtonActive]}
              onPress={() => setWriteMode(mode)}
            >
              <Text style={styles.modeText}>{mode.toUpperCase()}</Text>
            </Pressable>
          ))}
        </View>

        <VButton onPress={onWrite} label="Write" disabled={!canRun} />
        <VButton onPress={() => setLogs([])} label="Clear log" />
        <VButton onPress={() => router.back()} label="Back" />

        <View style={styles.logBox}>
          {logs.map((entry) => (
            <Text key={entry.id} style={styles.logLine}>
              {entry.message}
            </Text>
          ))}
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: "#FFF" },
  content: { padding: 16, gap: 10, paddingBottom: 60 },
  centerBox: { flex: 1, justifyContent: "center", alignItems: "center", gap: 16 },
  title: { fontSize: 24, fontWeight: "700", textAlign: "center" },
  input: {
    borderWidth: 1,
    borderColor: "#D2D8DF",
    borderRadius: 8,
    paddingHorizontal: 10,
    paddingVertical: 10,
    minHeight: 44,
    backgroundColor: "#FBFCFE",
  },
  row: { flexDirection: "row", gap: 8, flexWrap: "wrap" },
  modeRow: { flexDirection: "row", gap: 8 },
  modeButton: {
    borderWidth: 1,
    borderColor: "#C8CED8",
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 8,
  },
  modeButtonActive: {
    backgroundColor: "#E9F6FA",
    borderColor: "#2E7787",
  },
  modeText: { color: "#2E3742", fontWeight: "600" },
  logBox: {
    borderWidth: 1,
    borderColor: "#D2D8DF",
    borderRadius: 8,
    padding: 10,
    backgroundColor: "#FAFBFC",
    minHeight: 220,
  },
  logLine: { fontSize: 12, color: "#1F2933", marginBottom: 8 },
});
