import { Buffer } from "buffer";

import {
  SERVICE_UUIDS,
  CHARACTERISTIC_UUIDS,
  UnitErrorCodes,
  UnitId,
} from "@/constants/ble";
import { sendDoseEvent } from "@/services/schedule";
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

const { addDevice, updateDevice } = useDeviceStore.getState();

export async function connectAndSetupDevice(deviceName: string) {
  const scanResponse = await scanLeDevice(1);

  // console.log("\n");
  // console.log("\can result:", scanResponse);

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
    await subscribeToDoseEvent(device_id);
    await subscribeToBatteryLevel(device_id);
    await subscribeToError(device_id);

    await writeSystemTime();

    // if (treatments && treatments[device_id]) {
    //   const deviceTreatment = treatments[device_id];

    // TODO: parse the schedule skleton to send to device!!!!!
    // await writeDoseSchedule(deviceTreatment);

    await writeDoseSchedule({
      dosage_amount: 2,
      events_per_day: 7,
      max_temperature_threshold: 25,
      temperature_avg_time_window_min: 30,
      window: [
        { start_min: 465, end_min: 30 },
        { start_min: 585, end_min: 30 },
        { start_min: 705, end_min: 30 },
        { start_min: 885, end_min: 30 },
        { start_min: 1005, end_min: 30 },
        { start_min: 1185, end_min: 30 },
        { start_min: 1305, end_min: 30 },
      ],
    });
    // }

    return { deviceId: device_id, deviceName: device_name, status: "success" };
  } catch (error) {
    console.log("error", error);
    updateDevice(deviceName, {
      connected: false,
      color: "",
      batteryLevel: -1,
      error: "",
    });

    return { error, status: "error" };
  }
}

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
        // TODO: Send data to backend to telemetry endpoint
        const battery = Buffer.from(hex, "hex").readUInt8(0);
        updateDevice(device_id, { batteryLevel: battery });

        console.log("\n");
        console.log(`🔋 [BLE] Received and parsed battery level from device.. `);
        console.log(`Battery level: ${battery}%`);
        console.log(`Hex: ${hex}`);

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

async function writeSystemTime() {
  try {
    const unixTime = Math.floor(Date.now() / 1000);
    const buf = Buffer.alloc(4);
    buf.writeUInt32LE(unixTime, 0);
    const base64Time = buf.toString("base64");

    const result = await writeCharacteristic(CHARACTERISTIC_UUIDS.TIME, base64Time);

    console.log("\n");
    console.log(`📝 [BLE] Attempting to write time to device..`);
    console.log(`Successful? ${result}`);
    console.log(`Value (base64): ${base64Time}`);
  } catch (err) {
    console.log("Error writing system time:", err);
  }
}

async function writeDoseSchedule(doseSchedule: any) {
  try {
    const schedule = encodeDoseSchedule(doseSchedule);
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
