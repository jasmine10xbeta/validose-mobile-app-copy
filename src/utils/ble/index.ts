import { Buffer } from "buffer";

import { showToast } from "@/components/common/VToast";
import {
  SERVICE_UUIDS,
  CHARACTERISTIC_UUIDS,
  UnitErrorCodes,
  UnitId,
} from "@/constants/ble";
import { sendDoseEvent } from "@/services/schedule";
import { sendTelemetry } from "@/services/telemetry";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { DoseScheduleInput } from "@/types/dose";
import {
  connect,
  bondDevice,
  discoverServicesAndCharacteristics,
  subscribeToCharacteristic,
  writeCharacteristic,
  scanLeDevice,
} from "../../../modules/tenx-mdk-ble-rn-library/src/index";
import { BleMessageProtocol, MessageProtocolInterface } from "./messageProtocol";
import {
  buildPpiPayload,
  decodeDoseEventPpi,
  decodePpiPayload,
  encodeDoseSchedulePpi,
  encodeUint32LE,
  isTxStatusSendable,
  validatePayloadLength,
  PpiId,
  PpiType,
} from "./messageProtocolPpi";

const MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS = 250;
const TX_READY_TIMEOUT_MS = 5000;
const TX_READY_POLL_MS = 50;
// Toggle to route PPI traffic over the message protocol instead of legacy characteristics.
const USE_MESSAGE_PROTOCOL_PPI = true;

let messageProtocol: BleMessageProtocol | null = null;

const { addDevice, updateDevice } = useDeviceStore.getState();

async function waitForTxSendable(
  protocol: MessageProtocolInterface,
  timeoutMs = TX_READY_TIMEOUT_MS,
  pollMs = TX_READY_POLL_MS
): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;

  while (Date.now() <= deadline) {
    if (isTxStatusSendable(protocol.getTxPacketStatus())) {
      return true;
    }

    await new Promise((resolve) => setTimeout(resolve, pollMs));
  }

  return isTxStatusSendable(protocol.getTxPacketStatus());
}

export async function connectAndSetupDevice(deviceName: string) {
  const scanResponse = await scanLeDevice(1);

  console.log("\n");
  console.log("Scan result:", scanResponse);

  let device_id, device_name;

  try {
    const bondResponse = await bondDevice(deviceName);

    console.log("\n");
    console.log(`Bonding with device ${deviceName}`);
    console.log("Response:", bondResponse);

    if (!bondResponse || bondResponse.length === 0) {
      const connectResponse = await connect(deviceName);
      
      if (connectResponse) {
        device_id = connectResponse.deviceId;
        device_name = connectResponse.deviceName;
      } else {
        return { error: "Failed to bond with device", status: "error" };
      }
    } else {
      device_id = bondResponse.deviceId;
      device_name = bondResponse.deviceName;
    }

    const added = addDevice({
      connected: true,
      color: "",
      batteryLevel: -1,
      error: "",
      deviceId: device_id,
      deviceName: device_name,
    });

    console.log("Device successfully added?", added);

    if (!added) {
      updateDevice(device_id, {
        connected: true,
        color: "",
        batteryLevel: -1,
        error: "",
      });
    }

    await discoverServicesAndCharacteristics();

    if (messageProtocol) {
      messageProtocol.stop();
      messageProtocol = null;
    }

    messageProtocol = new BleMessageProtocol({
      txCharacteristicUUID: CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL,
      rxCharacteristicUUID: CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL,
      processIntervalMs: MESSAGE_PROTOCOL_PROCESS_INTERVAL_MS,
      // Native BLE layer negotiates MTU up front (247 target on Android); ATT payload is MTU - 3.
      // Use 244-byte packet budget here until MTU is exposed to JS directly.
      maxPacketLength: 244,
      isMaster: true,
      autoConsumeRx: true,
    });

    await messageProtocol.start();
    // TODO: If/when negotiated MTU is exposed, call messageProtocol.setMtu(mtu) here.

    // await resetBufferCharacteristic();
    if (USE_MESSAGE_PROTOCOL_PPI) {
      setupMessageProtocolHandlers(device_id);
    } else {
      await subscribeToDoseEvent(device_id);
    }
    await subscribeToBatteryLevel(device_id);
    await subscribeToError(device_id);

    await new Promise((res) => setTimeout(res, 300));

    await writeSystemTime();

    // if (treatments && treatments[device_id]) {
    //   const deviceTreatment = treatments[device_id];

    // TODO: parse the schedule skleton to send to device!!!!!
    // await writeDoseSchedule(deviceTreatment);

    await writeDoseSchedule({
      dosage_amount: 2,
      events_per_day: 4,
      max_temperature_threshold: 60,
      temperature_avg_time_window_min: 30,
      window: [
        // 10:30
        { start_min: 630, end_min: 30 },
        // 14:00
        { start_min: 840, end_min: 30 },
        // 17:30
        { start_min: 1050, end_min: 30 },
        // 21:00
        { start_min: 1260, end_min: 30 },

        // { start_min: 465, end_min: 30 },
        // { start_min: 585, end_min: 30 },
      ],
    });
    // }

    return { deviceId: device_id, deviceName: device_name, status: "success" };
  } catch (error) {
    console.log("error", error);

    if (messageProtocol) {
      messageProtocol.stop();
      messageProtocol = null;
    }

    updateDevice(deviceName, {
      connected: false,
      color: "",
      batteryLevel: -1,
      error: "",
    });

    return { error, status: "error" };
  }
}

function setupMessageProtocolHandlers(device_id: string) {
  if (!messageProtocol) {
    return;
  }

  // Handle PUSH dose events via message protocol.
  messageProtocol.registerRxHandler(PpiId.AD_DOSE_EVENT_REPORT, PpiType.PUSH, async (packet) => {
    const decoded = decodeDoseEventPpi(packet.payload);
    if (!decoded) {
      console.warn("[MP] Dose event payload size mismatch.");
      return;
    }

    console.log("\n");
    console.log("💊 [MP] Received dose event", decoded);

    // TODO: Map decoded fields to backend payload. The current backend expects
    // dose_amount_mg and an event timestamp. Firmware dose_event_t does not include
    // dose_amount_mg, so this needs alignment before sending.
  });

  // Example handler for time response (RE).
  messageProtocol.registerRxHandler(PpiId.AD_TIME, PpiType.RE, (packet) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    console.log("\n");
    console.log("🕒 [MP] Time update response", decoded.value);
  });
}

// Legacy characteristic subscription for dose events.
// Prefer using message protocol PPI_AD_DOSE_EVENT_REPORT when firmware supports it.
async function subscribeToDoseEvent(device_id: string) {
  // Subscribe to Dose Events
  await subscribeToCharacteristic(
    CHARACTERISTIC_UUIDS.DOSE_EVENT,
    SERVICE_UUIDS.CUSTOM_SERVICE,
    async ({ uuid, hex, deviceId }) => {
      if (uuid.toLowerCase() !== CHARACTERISTIC_UUIDS.DOSE_EVENT || deviceId !== device_id) return;

      const parsed = decodeDoseEvent(hex);
      console.log("\n");
      console.log(`💊 [BLE] Recieved dose event: (Hex: ${hex})`, parsed);

      if (parsed) {
        const device = useDeviceStore.getState().getDevice(device_id);
        const deviceName = device?.deviceName ?? "Unknown Device";

        const treatment = useTreatmentStore.getState().getDeviceTreatment(deviceName);

        if (treatment?.medication_code) {
          const res = await sendDoseEvent(parsed, deviceName, treatment?.medication_code);
          if (res) {
            const ack_response = useScheduleStore.getState().acknowledgeDoseEvent(deviceName, res.event_id, parsed);
            console.log("\n");
            console.log(`Locally acknowledged dose event?`, ack_response);
            if (ack_response !== null) {
              showToast(
                "success",
                `Dose recorded for ${treatment?.medication_code}`,
              );
            } else {
              showToast(
                "error",
                `Dose detected but was outside valid dosing window`,
              );
            }
          }
        }
      }
    }
  );
}

async function subscribeToError(device_id: string) {
  // Subscribe to Error Notifications
  await subscribeToCharacteristic(
    CHARACTERISTIC_UUIDS.ERROR_CODE,
    SERVICE_UUIDS.CUSTOM_SERVICE,
    ({ uuid, hex, deviceId }) => {
      if (uuid.toLowerCase() !== CHARACTERISTIC_UUIDS.ERROR_CODE || deviceId !== device_id) return;

      const error = decodeErrorNotification(hex);
      if (error) {
        // TODO: Send data to backend to telemetry endpoint
        console.log("\n");
        console.log(`⚠️ [BLE] Received and parsed the following error from device..`);
        console.log(`Unit: ${error.unitName} (${error.unitId})`);
        console.log(`Code: ${error.errorNumber} \nMessage: ${error.errorMessage}`);
        console.log(`Hex: ${hex}`);

        const device = useDeviceStore.getState().getDevice(device_id);
        const deviceName = device?.deviceName ?? "Unknown Device";
        const treatment = useTreatmentStore.getState().getDeviceTreatment(deviceName);
        if (treatment?.id) {
          sendTelemetry(treatment.id, {
            device_id: deviceName,
            characteristic: "ERROR",
            value: error.errorMessage ?? `Code ${error.errorNumber}`,
          }).catch((telemetryErr) => {
            console.log("\n");
            console.warn("[Telemetry] Failed to log error event", telemetryErr)
          });
        }

        updateDevice(device_id, { error: `Error: ${error.errorMessage}` });
        return;
      }

      console.log("ℹ️ [BLE] Unrecognized data format.");
    }
  );
}

async function subscribeToBatteryLevel(device_id: string) {
  // Subscribe to Battery Level
  await subscribeToCharacteristic(
    CHARACTERISTIC_UUIDS.BATTERY_LEVEL,
    SERVICE_UUIDS.BATTERY_SERVICE,
    ({ uuid, hex, deviceId }) => {
      if (uuid.toLowerCase() !== CHARACTERISTIC_UUIDS.BATTERY_LEVEL || deviceId !== device_id) return;
      
      try {
        const battery = Buffer.from(hex, "hex").readUInt8(0);
        updateDevice(device_id, { batteryLevel: battery });

        // TODO: Optimize logging to reduce noise & get device name from store
        console.log("\n");
        console.log(`🔋 [BLE] Received and parsed battery level from device.. `);
        console.log(`Battery level: ${battery}%`);
        console.log(`Hex: ${hex}`);

        const device = useDeviceStore.getState().getDevice(device_id);
        const deviceName = device?.deviceName ?? "Unknown Device";
        const treatment = useTreatmentStore.getState().getDeviceTreatment(deviceName);
        if (treatment?.id) {
          sendTelemetry(treatment.id, {
            device_id: deviceName,
            characteristic: "BATTERY",
            value: battery,
          }).catch((telemetryErr) => {
            console.log("\n");
            console.warn("[Telemetry] Failed to log battery event", telemetryErr)
          });
        }

      } catch (err) {
        console.error("Error parsing battery level:", err);
      }
    }
  );
}

function decodeErrorNotification(hex: string) {
  const buffer = Buffer.from(hex, "hex");
  if (buffer.length < 2) return null;

  const unitId = buffer.readUInt8(0);
  const rawErrorByte = buffer.readUInt8(1);
  // Mask out highest bit if it’s a status or reserved bit
  const errorNumber = rawErrorByte & 0x7f;

  // Safely get unitName string; fallback to "UNKNOWN_UNIT"
  const unitName = UnitId[unitId] ?? "UNKNOWN_UNIT";

  // Retrieve error message if exists, else fallback
  const errorMessage =
    UnitErrorCodes[unitId as UnitId]?.[errorNumber] ??
    `Unknown error ${errorNumber}`;

  return {
    unitId,
    unitName,
    errorNumber,
    errorMessage,
  };
}

// Legacy characteristic encoding for dose schedules (variable length).
// Prefer encodeDoseSchedulePpi() which matches fixed-size firmware struct.
function encodeDoseSchedule({
  dosage_amount,
  events_per_day,
  max_temperature_threshold,
  temperature_avg_time_window_min,
  window,
}: DoseScheduleInput): Uint8Array {
  const size = 5 + window.length * 4;
  const buffer = new ArrayBuffer(size);
  const view = new DataView(buffer);

  view.setUint8(0, dosage_amount);
  view.setUint8(1, events_per_day);
  view.setInt8(2, max_temperature_threshold);
  view.setUint16(3, temperature_avg_time_window_min, true); // little-endian

  let offset = 5;
  window.forEach((ev) => {
    view.setUint16(offset, ev.start_min, true);
    view.setUint16(offset + 2, ev.end_min, true);
    offset += 4;
  });

  return new Uint8Array(buffer);
}

async function writeSystemTime(): Promise<boolean> {
  const unixTime = Math.floor(Date.now() / 1000);

  console.log("\n");
  console.log(`📝 [BLE] Writing system time to device..`);
  console.log(`Unix time: ${unixTime}`);

  try {
    if (USE_MESSAGE_PROTOCOL_PPI && messageProtocol) {
      const payload = encodeUint32LE(unixTime);
      const txReady = await waitForTxSendable(messageProtocol);

      if (!txReady) {
        console.warn("[MP] TX busy; cannot send time update yet.");
        return false;
      }

      if (payload.length > messageProtocol.getMaxPayloadLength()) {
        console.warn("[MP] Payload exceeds negotiated max length.");
        return false;
      }

      if (!validatePayloadLength(PpiId.AD_TIME, PpiType.RQ, payload)) {
        console.warn("[MP] Payload length mismatch for PPI_AD_TIME request.");
        return false;
      }

      const result = messageProtocol.send(buildPpiPayload(PpiId.AD_TIME, PpiType.RQ, payload));
      console.log(`Message protocol send result: ${result}`);
      return result === 0;
    }

    // Legacy characteristic write path.
    const buffer = Buffer.alloc(4);
    buffer.writeUInt32LE(unixTime, 0);
    const base64Time = buffer.toString("base64");
    console.log(`Payload (base64): ${base64Time}`);

    const result = await writeCharacteristic(CHARACTERISTIC_UUIDS.TIME, base64Time);
    const success = result === true;
    console.log(`Success? ${success}`);
    return success;
  } catch (err) {
    console.error("Error writing system time:", err);
    throw err;
  }
}

async function resetBufferCharacteristic() {
  try {
    const buf = Buffer.alloc(1);
    buf.writeUInt8(0x02, 0);
    const base64Value = buf.toString("base64");

    const result = await writeCharacteristic(CHARACTERISTIC_UUIDS.RESET, base64Value);

    console.log(`\n📝 [BLE] Writing reset buffer flag (0x02) to device..`);
    console.log(`Successful? ${result}`);
    console.log(`Value (base64): ${base64Value}`);
  } catch (err) {
    console.log("Error writing reset buffer flag:", err);
  }
}

async function writeDoseSchedule(doseSchedule: any) {
  try {
    const schedule = encodeDoseSchedulePpi({
      dosage_amount: doseSchedule.dosage_amount,
      events_per_day: doseSchedule.events_per_day,
      temperature_threshold_deg_c: doseSchedule.max_temperature_threshold,
      temperature_avg_time_window_minutes: doseSchedule.temperature_avg_time_window_min,
      // Firmware expects a fixed list of 10 windows (duration, not end_min).
      // The existing app shape uses end_min as a duration; keep that mapping.
      window: (doseSchedule.window ?? []).map((ev: any) => ({
        start_min: ev.start_min,
        duration: ev.end_min,
      })),
    });

    if (USE_MESSAGE_PROTOCOL_PPI && messageProtocol) {
      const txReady = await waitForTxSendable(messageProtocol);
      if (!txReady) {
        console.warn("[MP] TX busy; cannot send dose schedule yet.");
        return;
      }

      if (schedule.length > messageProtocol.getMaxPayloadLength()) {
        console.warn("[MP] Payload exceeds negotiated max length.");
        return;
      }

      if (!validatePayloadLength(PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, schedule)) {
        console.warn("[MP] Payload length mismatch for PPI_AD_DOSE_SCHEDULE.");
        return;
      }

      const result = messageProtocol.send(
        buildPpiPayload(PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, schedule)
      );
      console.log(`\n📝 [MP] Sent dose schedule. result=${result}`);
      return;
    }

    // Legacy characteristic write path.
    const buffer = Buffer.from(schedule);
    const base64DoseSchedule = buffer.toString("base64");

    const doseResult = await writeCharacteristic(CHARACTERISTIC_UUIDS.DOSE_SCHEDULE, base64DoseSchedule);

    console.log(`\n📝 [BLE] Attempting to write dose schedule to device..`);
    console.log(`Successful? ${doseResult}`);
    console.log(`Value (base64): ${base64DoseSchedule}`);
  } catch (err) {
    console.log("Error writing dose schedule:", err); // TODO: Send errors to Sentry or log on backend
  }
}

export function decodeDoseEvent(hex: string) {
  const buffer = Buffer.from(hex, "hex");

  // Expected size: 10 bytes
  if (buffer.length !== 10) {
    console.warn(`Dose event data must be 10 bytes, got ${buffer.length}:`, hex);
    return null;
  }

  // event_id fields
  const days_since_epoch = buffer.readUInt16LE(0); // bytes 0–1
  const event_ctr = buffer.readUInt8(2);           // byte 2

  // dose_amount_mg (2 bytes, LE)
  const dose_amount_mg = buffer.readUInt16LE(3);   // bytes 3–4

  // timestamp_unix (4 bytes, LE)
  const timestamp_unix = buffer.readUInt32LE(5);   // bytes 5–8

  // dose_event_type (1 byte)
  const dose_event_type = buffer.readUInt8(9);     // byte 9

  return {
    event_id: {
      days_since_epoch,
      event_ctr,
    },
    dose_amount_mg,
    timestamp_unix,                               // seconds since epoch (UTC)
    dose_event_type
  };
}

export function getMessageProtocol(): MessageProtocolInterface | null {
  return messageProtocol;
}

export * from "./messageProtocol";
export * from "./messageProtocolPpi";
