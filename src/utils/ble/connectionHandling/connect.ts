import { Buffer } from "buffer";

import { ingestRawHardwareData } from "@/services/hardware";
import useDevStore from "@/store/dev";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import {
  getCachedDeviceScheduleSyncData,
  type DeviceScheduleSyncData,
} from "@/utils/schedule";
import {
  bondDevice,
  connect,
  discoverServicesAndCharacteristics,
  getConnectedDevice,
  scanLeDevice,
} from "../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { BleMessageProtocol, MsgProtError } from "../messageProtocol";
import { decodePpiPayload, PpiId, PpiType } from "../messageProtocolPpi";
import { USE_MESSAGE_PROTOCOL_PPI, MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS } from "./constants";
import { resolveMessageProtocolUuidsFromDiscovery } from "./discovery";
import {
  requestRuntimePpiState,
  relayMessageProtocolConsoleLog,
  setupMessageProtocolHandlers,
  waitForTxSendable,
} from "./protocol";
import {
  emitMessageProtocolRxDataAcked,
  emitMessageProtocolRxPacket,
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
const MP_MIN_FRAME_LEN_BYTES = 14;
const MP_PACKET_TYPE_DATA = 0;
const POST_CONNECT_TX_READY_TIMEOUT_MS = 1200;
const POST_CONNECT_TX_READY_POLL_MS = 40;

function toErrorMessage(error: unknown): string {
  if (error instanceof Error) return error.message;
  return String(error ?? "");
}

function isAlreadyConnectedError(error: unknown): boolean {
  const normalized = toErrorMessage(error).toLowerCase();
  return normalized.includes("already connected") || normalized.includes("already_connected");
}

function buildCandidateIdentifiers(
  primaryIdentifier: string,
  knownDevice: { deviceId?: string; deviceName?: string } | undefined,
): string[] {
  const unique = new Set<string>();
  const candidates = [primaryIdentifier, knownDevice?.deviceId, knownDevice?.deviceName];

  for (const candidate of candidates) {
    const normalized = candidate?.trim();
    if (normalized) unique.add(normalized);
  }

  return Array.from(unique);
}

async function runPostConnectTasks(
  deviceId: string,
  resolvedDeviceName: string
): Promise<void> {
  try {
    await new Promise((resolve) => setTimeout(resolve, 300));

    if (USE_MESSAGE_PROTOCOL_PPI) {
      try {
        await requestRuntimePpiState({
          timeoutMs: POST_CONNECT_TX_READY_TIMEOUT_MS,
          pollMs: POST_CONNECT_TX_READY_POLL_MS,
        });
      } catch (runtimeStateError) {
        console.warn("[MP] Failed to request runtime PPI state.", runtimeStateError);
      }
    }

    const maxWriteAttempts = 3;
    let timeSyncSucceeded = false;
    try {
      for (let attempt = 1; attempt <= maxWriteAttempts; attempt += 1) {
        timeSyncSucceeded = await writeSystemTime({
          txReadyTimeoutMs: POST_CONNECT_TX_READY_TIMEOUT_MS,
          txReadyPollMs: POST_CONNECT_TX_READY_POLL_MS,
        });
        if (timeSyncSucceeded) break;

        if (attempt < maxWriteAttempts) {
          await new Promise((resolve) => setTimeout(resolve, 700));
        }
      }

      if (!timeSyncSucceeded) {
        console.warn("[BLE] Skipping system time sync after retry window.");
      }
    } catch (timeSyncError) {
      console.warn("[BLE] Failed to write system time over message protocol.", timeSyncError);
    }

    try {
      const scheduleLookupIdentifiers = buildCandidateIdentifiers(deviceId, {
        deviceId,
        deviceName: resolvedDeviceName,
      });
      let scheduleSyncData: DeviceScheduleSyncData | null = null;
      let scheduleSyncIdentifier = deviceId;

      for (const identifier of scheduleLookupIdentifiers) {
        scheduleSyncData = getCachedDeviceScheduleSyncData(identifier);
        if (scheduleSyncData) {
          scheduleSyncIdentifier = identifier;
          break;
        }
      }

      if (scheduleSyncData && scheduleSyncIdentifier !== deviceId) {
        useScheduleStore.getState().storeSchedules(deviceId, scheduleSyncData.schedules);
      }

      const existingDevice = useDeviceStore.getState().getDevice(deviceId);
      const hasScheduleChanged =
        scheduleSyncData?.signature &&
        scheduleSyncData.signature !== existingDevice?.lastScheduleSyncSignature;

      if (scheduleSyncData && hasScheduleChanged) {
        let scheduleSyncSucceeded = false;
        for (let attempt = 1; attempt <= maxWriteAttempts; attempt += 1) {
          scheduleSyncSucceeded = await writeDoseSchedule(scheduleSyncData.payload, {
            txReadyTimeoutMs: POST_CONNECT_TX_READY_TIMEOUT_MS,
            txReadyPollMs: POST_CONNECT_TX_READY_POLL_MS,
          });
          if (scheduleSyncSucceeded) break;

          if (attempt < maxWriteAttempts) {
            await new Promise((resolve) => setTimeout(resolve, 700));
          }
        }

        if (scheduleSyncSucceeded) {
          updateDevice(deviceId, {
            lastScheduleSyncSignature: scheduleSyncData.signature,
            lastScheduleSyncedAt: new Date().toISOString(),
          });
          console.log("[BLE] Updated cached dose schedule via message protocol.");
        } else {
          console.warn("[BLE] Skipping schedule push after retry window.");
        }
      } else if (scheduleSyncData) {
        console.log("[BLE] Cached dose schedule unchanged, skipping message protocol schedule push.");
      } else {
        console.log("[BLE] No cached schedule available to push for this device.", {
          lookupIdentifiers: scheduleLookupIdentifiers,
        });
      }
    } catch (scheduleSyncError) {
      console.warn("[BLE] Failed to sync cached schedule to device.", scheduleSyncError);
    }
  } catch (postConnectError) {
    console.warn("[BLE] Deferred post-connect setup failed.", postConnectError);
  }
}

export async function connectAndSetupDevice(deviceIdentifier: string) {
  if (useDevStore.getState().isMockBleModeEnabled()) {
    const mockDeviceId = deviceIdentifier || `MOCK-${Date.now()}`;
    const mockDeviceName = deviceIdentifier || "MOCK-VALIDOSE";

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

    console.log(`[MOCK BLE] Bypassing BLE setup for ${mockDeviceName}`);
    return { deviceId: mockDeviceId, deviceName: mockDeviceName, status: "success" };
  }

  const scanResponse = await scanLeDevice(1);

  console.log("Scan result:", scanResponse);

  const knownDevice = useDeviceStore.getState().getDevice(deviceIdentifier);
  const candidateIdentifiers = buildCandidateIdentifiers(deviceIdentifier, knownDevice);

  let deviceId = "";
  let resolvedDeviceName = "";
  let lastConnectionError: unknown = null;

  try {
    let connectionResponse: any = null;

    for (const candidate of candidateIdentifiers) {
      try {
        console.log(`Bonding with device ${candidate}`);
        const bondResponse = await bondDevice(candidate);
        console.log("Bond response:", bondResponse);
        if (bondResponse) {
          connectionResponse = bondResponse;
          break;
        }
      } catch (bondError) {
        lastConnectionError = bondError;
        console.warn(`[BLE] bondDevice failed for ${candidate}:`, bondError);
      }

      try {
        console.log(`Connecting to device ${candidate}`);
        const connectResponse = await connect(candidate);
        console.log("Connect response:", connectResponse);
        if (connectResponse) {
          connectionResponse = connectResponse;
          break;
        }
      } catch (connectError) {
        lastConnectionError = connectError;
        if (isAlreadyConnectedError(connectError)) {
          try {
            const connectedDevice = await getConnectedDevice();
            if (connectedDevice) {
              connectionResponse = connectedDevice;
              break;
            }
          } catch (getConnectedDeviceError) {
            lastConnectionError = getConnectedDeviceError;
          }
        }
        console.warn(`[BLE] connect failed for ${candidate}:`, connectError);
      }
    }

    if (!connectionResponse) {
      throw (
        lastConnectionError ??
        new Error(`Failed to connect to ${deviceIdentifier} with available identifiers`)
      );
    }

    deviceId = connectionResponse?.deviceId || knownDevice?.deviceId || deviceIdentifier;
    resolvedDeviceName =
      connectionResponse?.deviceName || knownDevice?.deviceName || deviceIdentifier;

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

    console.log("[MP] Resolved UUIDs from discovery:", resolvedMpUuids);

    stopAndClearMessageProtocol();
    setMessageProtocol(
      new BleMessageProtocol({
        txCharacteristicUUID: resolvedMpUuids.txUuid,
        rxCharacteristicUUID: resolvedMpUuids.rxUuid,
        serviceUUID: resolvedMpUuids.serviceUuid,
        processIntervalMs: MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS,
        // Native BLE layer negotiates MTU up front (247 target on Android); ATT payload is MTU - 3.
        // Use 244-byte packet budget here until MTU is exposed to JS directly.
        maxPacketLength: 244,
        isMaster: true,
        enableSyncControl: true,
        sendAckNak: true,
        autoConsumeRx: true,
        onRxPacket: (packet) => {
          emitMessageProtocolRxPacket(packet);

          const ppiName = PpiId[packet.ppi as PpiId] ?? `PPI_${packet.ppi}`;
          const typeName = PpiType[packet.type as PpiType] ?? `TYPE_${packet.type}`;
          const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);

          console.log("[MP][RX]", {
            deviceId,
            deviceName: resolvedDeviceName,
            ppi: packet.ppi,
            ppiName,
            type: packet.type,
            typeName,
            payloadLen: packet.pktPayloadLen,
            payloadHex: Buffer.from(packet.payload).toString("hex"),
            decoded: decoded.value,
          });
        },
        onRxDataAcked: (packet) => {
          emitMessageProtocolRxDataAcked(packet);

          const packetBytes = getMessageProtocolInstance()?.getLastRxPacketRaw() ?? new Uint8Array(0);
          if (!packetBytes.length) {
            console.warn("[MP][INGEST] Skipping ingest because no raw RX packet was available.", {
              pktCounter: packet.pktCounter,
              sessionId: packet.sessionId,
              ppi: packet.payload.ppi,
              type: packet.payload.type,
            });
            return;
          }
          if (packetBytes.length < MP_MIN_FRAME_LEN_BYTES) {
            console.warn("[MP][INGEST] Skipping ingest because RX packet was shorter than MP frame header.", {
              packetBytesLength: packetBytes.length,
              pktCounter: packet.pktCounter,
              sessionId: packet.sessionId,
            });
            return;
          }

          const pktType = packetBytes[8];
          const pktPayloadLen = packetBytes[12] | (packetBytes[13] << 8);
          if (pktType !== MP_PACKET_TYPE_DATA) {
            console.warn("[MP][INGEST] Skipping ingest because RX packet is not DATA.", {
              pktType,
              pktCounter: packet.pktCounter,
              sessionId: packet.sessionId,
            });
            return;
          }
          if (pktPayloadLen <= 0 || packet.payload.payload.length <= 0) {
            console.warn("[MP][INGEST] Skipping ingest because DATA packet has empty payload.", {
              pktPayloadLen,
              pktCounter: packet.pktCounter,
              sessionId: packet.sessionId,
              ppi: packet.payload.ppi,
              type: packet.payload.type,
            });
            return;
          }
          if (packetBytes.length < MP_MIN_FRAME_LEN_BYTES + pktPayloadLen) {
            console.warn("[MP][INGEST] Skipping ingest because DATA packet appears truncated.", {
              packetBytesLength: packetBytes.length,
              pktPayloadLen,
              pktCounter: packet.pktCounter,
              sessionId: packet.sessionId,
            });
            return;
          }

          const ingestDeviceName = (resolvedDeviceName || "").trim() || deviceId;

          void ingestRawHardwareData({
            packetBytes,
            timestamp: new Date(),
            deviceId: ingestDeviceName,
          }).catch((error) => {
            console.warn("[MP][INGEST] Failed to ingest ACKed packet.", {
              error: error instanceof Error ? error.message : String(error),
              ingestDeviceName,
              pktCounter: packet.pktCounter,
              sessionId: packet.sessionId,
              ppi: packet.payload.ppi,
              type: packet.payload.type,
              packetBytesLength: packetBytes.length,
              packetBase64: Buffer.from(packetBytes).toString("base64"),
            });
          });
        },
        logger: {
          debug: (...args: unknown[]) => relayMessageProtocolConsoleLog("DBG", args),
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
        ? await waitForTxSendable(
            messageProtocol,
            POST_CONNECT_TX_READY_TIMEOUT_MS,
            POST_CONNECT_TX_READY_POLL_MS
          )
        : false;

    console.info("[MP][PROTOCOL] Message protocol started.", {
      mode: "MASTER",
      enableSyncControl: true,
      sendAckNak: true,
      txCharacteristicUuid: resolvedMpUuids.txUuid,
      rxCharacteristicUuid: resolvedMpUuids.rxUuid,
      serviceUuid: resolvedMpUuids.serviceUuid,
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
      try {
        setupMessageProtocolHandlers(deviceId);
      } catch (handlerSetupError) {
        console.warn("[MP] Failed to register runtime protocol handlers.", handlerSetupError);
      }
    } else {
      try {
        await subscribeToDoseEvent(deviceId);
      } catch (doseSubscriptionError) {
        console.warn("[BLE] Failed to subscribe to dose events.", doseSubscriptionError);
      }

      try {
        await subscribeToBatteryLevel(deviceId);
      } catch (batterySubscriptionError) {
        console.warn("[BLE] Failed to subscribe to battery updates.", batterySubscriptionError);
      }

      try {
        await subscribeToError(deviceId);
      } catch (errorSubscriptionError) {
        console.warn("[BLE] Failed to subscribe to device errors.", errorSubscriptionError);
      }
    }

    void runPostConnectTasks(deviceId, resolvedDeviceName);

    return { deviceId, deviceName: resolvedDeviceName, status: "success" };
  } catch (error) {
    console.log("error", error);

    stopAndClearMessageProtocol();

    updateDevice(deviceIdentifier, {
      connected: false,
      color: "",
      batteryLevel: -1,
      error: "",
    });

    return { error, status: "error" };
  }
}
