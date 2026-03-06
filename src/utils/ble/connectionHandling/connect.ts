import { Buffer } from "buffer";

import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import {
  bondDevice,
  connect,
  discoverServicesAndCharacteristics,
  scanLeDevice,
} from "../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { BleMessageProtocol, MsgProtError } from "../messageProtocol";
import { USE_MESSAGE_PROTOCOL_PPI, MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS } from "./constants";
import { resolveMessageProtocolUuidsFromDiscovery } from "./discovery";
import {
  relayMessageProtocolConsoleLog,
  setupMessageProtocolHandlers,
  waitForTxSendable,
} from "./protocol";
import {
  getMessageProtocolInstance,
  setMessageProtocol,
  stopAndClearMessageProtocol,
} from "./state";
import {
  subscribeToBatteryLevel,
  subscribeToDoseEvent,
  subscribeToError,
} from "./subscriptions";
import { writeDoseSchedule, writeSystemTime } from "./writes";

const { addDevice, updateDevice } = useDeviceStore.getState();

export async function connectAndSetupDevice(deviceName: string) {
  if (useDevStore.getState().isMockBleModeEnabled()) {
    const mockDeviceId = deviceName || `MOCK-${Date.now()}`;
    const mockDeviceName = deviceName || "MOCK-VALIDOSE";

    const added = addDevice({
      connected: true,
      color: "",
      batteryLevel: 100,
      error: "",
      deviceId: mockDeviceId,
      deviceName: mockDeviceName,
    });

    if (!added) {
      updateDevice(mockDeviceId, {
        connected: true,
        color: "",
        batteryLevel: 100,
        error: "",
      });
    }

    console.log("\n");
    console.log(`[MOCK BLE] Bypassing BLE setup for ${mockDeviceName}`);
    return { deviceId: mockDeviceId, deviceName: mockDeviceName, status: "success" };
  }

  const scanResponse = await scanLeDevice(1);

  console.log("\n");
  console.log("Scan result:", scanResponse);

  let deviceId = "";
  let resolvedDeviceName = "";

  try {
    const bondResponse = await bondDevice(deviceName);

    console.log("\n");
    console.log(`Bonding with device ${deviceName}`);
    console.log("Response:", bondResponse);

    if (!bondResponse || bondResponse.length === 0) {
      const connectResponse = await connect(deviceName);

      if (connectResponse) {
        deviceId = connectResponse.deviceId;
        resolvedDeviceName = connectResponse.deviceName;
      } else {
        return { error: "Failed to bond with device", status: "error" };
      }
    } else {
      deviceId = bondResponse.deviceId;
      resolvedDeviceName = bondResponse.deviceName;
    }

    const added = addDevice({
      connected: true,
      color: "",
      batteryLevel: -1,
      error: "",
      deviceId,
      deviceName: resolvedDeviceName,
    });

    console.log("Device successfully added?", added);

    if (!added) {
      updateDevice(deviceId, {
        connected: true,
        color: "",
        batteryLevel: -1,
        error: "",
      });
    }

    const discoveryResponse = await discoverServicesAndCharacteristics();
    const resolvedMpUuids = resolveMessageProtocolUuidsFromDiscovery(discoveryResponse);

    console.log("\n");
    console.log("[MP] Resolved UUIDs from discovery:", resolvedMpUuids);

    stopAndClearMessageProtocol();
    setMessageProtocol(
      new BleMessageProtocol({
        txCharacteristicUUID: resolvedMpUuids.txUuid,
        rxCharacteristicUUID: resolvedMpUuids.rxUuid,
        processIntervalMs: MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS,
        // Native BLE layer negotiates MTU up front (247 target on Android); ATT payload is MTU - 3.
        // Use 244-byte packet budget here until MTU is exposed to JS directly.
        maxPacketLength: 244,
        isMaster: true,
        enableSyncControl: true,
        sendAckNak: true,
        autoConsumeRx: true,
        onRxDataAcked: (packet) => {
          const payloadHex = Buffer.from(packet.payload.payload).toString("hex");
          if (!payloadHex) {
            return;
          }

          // void ingestRawHardwareData({
          //   payloadHex,
          //   timestamp: new Date(),
          //   deviceId,
          // }).catch((error) => {
          //   console.warn("[MP][INGEST] Failed to ingest ACKed payload.", {
          //     error: error instanceof Error ? error.message : String(error),
          //     pktCounter: packet.pktCounter,
          //     sessionId: packet.sessionId,
          //     ppi: packet.payload.ppi,
          //     type: packet.payload.type,
          //   });
          // });
        },
        logger: {
          debug: () => undefined,
          info: (...args: unknown[]) => relayMessageProtocolConsoleLog("INFO", args),
          warn: (...args: unknown[]) => relayMessageProtocolConsoleLog("WARN", args),
          error: (...args: unknown[]) => relayMessageProtocolConsoleLog("ERR", args),
        },
      })
    );

    const messageProtocol = getMessageProtocolInstance();
    if (!messageProtocol) {
      return { error: "Failed to initialize message protocol", status: "error" };
    }

    await messageProtocol.start();
    const syncStartResult = await messageProtocol.startSync();
    const syncReady =
      syncStartResult === MsgProtError.NONE
        ? await waitForTxSendable(messageProtocol)
        : false;

    console.info("[MP][PROTOCOL] Message protocol started.", {
      mode: "MASTER",
      enableSyncControl: true,
      sendAckNak: true,
      txCharacteristicUuid: resolvedMpUuids.txUuid,
      rxCharacteristicUuid: resolvedMpUuids.rxUuid,
      sessionId: messageProtocol.getCurrentSessionId(),
      syncStartResult,
      syncReady,
    });
    if (!syncReady) {
      console.warn(
        "[MP][PROTOCOL] Sync did not complete in readiness window; waiting for SYNC_ACK."
      );
    }

    if (USE_MESSAGE_PROTOCOL_PPI) {
      setupMessageProtocolHandlers(deviceId);
    } else {
      await subscribeToDoseEvent(deviceId);
    }
    await subscribeToBatteryLevel(deviceId);
    await subscribeToError(deviceId);

    await new Promise((res) => setTimeout(res, 300));

    await writeSystemTime();
    await writeDoseSchedule({
      dosage_amount: 2,
      events_per_day: 4,
      max_temperature_threshold: 60,
      temperature_avg_time_window_min: 30,
      window: [
        { start_min: 630, end_min: 30 },
        { start_min: 840, end_min: 30 },
        { start_min: 1050, end_min: 30 },
        { start_min: 1260, end_min: 30 },
      ],
    });

    return { deviceId, deviceName: resolvedDeviceName, status: "success" };
  } catch (error) {
    console.log("error", error);

    stopAndClearMessageProtocol();

    updateDevice(deviceName, {
      connected: false,
      color: "",
      batteryLevel: -1,
      error: "",
    });

    return { error, status: "error" };
  }
}
