import { Buffer } from "buffer";

import { showToast } from "@/components/common/VToast";
import {
  CHARACTERISTIC_UUIDS,
  SERVICE_UUIDS,
  UnitErrorCodes,
  UnitId,
} from "@/constants/ble";
import useDeviceStore from "@/store/device";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { subscribeToCharacteristic } from "../../../../modules/tenx-mdk-ble-rn-library/src/index";

const { updateDevice } = useDeviceStore.getState();

export function decodeDoseEvent(hex: string) {
  const buffer = Buffer.from(hex, "hex");

  if (buffer.length !== 10) {
    console.warn(`Dose event data must be 10 bytes, got ${buffer.length}:`, hex);
    return null;
  }

  const days_since_epoch = buffer.readUInt16LE(0);
  const event_ctr = buffer.readUInt8(2);
  const dose_amount_mg = buffer.readUInt16LE(3);
  const timestamp_unix = buffer.readUInt32LE(5);
  const dose_event_type = buffer.readUInt8(9);

  return {
    event_id: {
      days_since_epoch,
      event_ctr,
    },
    dose_amount_mg,
    timestamp_unix,
    dose_event_type,
  };
}

function decodeErrorNotification(hex: string) {
  const buffer = Buffer.from(hex, "hex");
  if (buffer.length < 2) return null;

  const unitId = buffer.readUInt8(0);
  const rawErrorByte = buffer.readUInt8(1);
  const errorNumber = rawErrorByte & 0x7f;

  const unitName = UnitId[unitId] ?? "UNKNOWN_UNIT";
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

export async function subscribeToDoseEvent(deviceId: string): Promise<void> {
  await subscribeToCharacteristic(
    CHARACTERISTIC_UUIDS.DOSE_EVENT,
    SERVICE_UUIDS.CUSTOM_SERVICE,
    async ({ uuid, hex, deviceId: callbackDeviceId }) => {
      if (uuid.toLowerCase() !== CHARACTERISTIC_UUIDS.DOSE_EVENT || callbackDeviceId !== deviceId) return;

      const parsed = decodeDoseEvent(hex);
      console.log(`💊 [BLE] Recieved dose event: (Hex: ${hex})`, parsed);

      if (!parsed) {
        return;
      }

      const device = useDeviceStore.getState().getDevice(deviceId);
      const resolvedDeviceId = device?.deviceId ?? deviceId;
      const treatment =
        useTreatmentStore.getState().getDeviceTreatment(resolvedDeviceId) ??
        useTreatmentStore.getState().getDeviceTreatment(device?.deviceName ?? "");

      if (!treatment?.medication_code) {
        return;
      }

      const acknowledged = useScheduleStore
        .getState()
        .acknowledgeDoseEventByTimestamp(
          resolvedDeviceId,
          new Date(parsed.timestamp_unix * 1000).toISOString(),
          parsed
        );

      console.log("Locally acknowledged dose event?", acknowledged);

      if (acknowledged !== null) {
        showToast("success", `Dose recorded for ${treatment.medication_code}`);
      } else {
        showToast("error", "Dose detected but was outside valid dosing window");
      }
    }
  );
}

export async function subscribeToError(deviceId: string): Promise<void> {
  await subscribeToCharacteristic(
    CHARACTERISTIC_UUIDS.ERROR_CODE,
    SERVICE_UUIDS.CUSTOM_SERVICE,
    ({ uuid, hex, deviceId: callbackDeviceId }) => {
      if (uuid.toLowerCase() !== CHARACTERISTIC_UUIDS.ERROR_CODE || callbackDeviceId !== deviceId) return;

      const error = decodeErrorNotification(hex);
      if (!error) {
        console.log("ℹ️ [BLE] Unrecognized data format.");
        return;
      }

      console.log("⚠️ [BLE] Received and parsed the following error from device..");
      console.log(`Unit: ${error.unitName} (${error.unitId})`);
      console.log(`Code: ${error.errorNumber} \nMessage: ${error.errorMessage}`);
      console.log(`Hex: ${hex}`);

      updateDevice(deviceId, { error: `Error: ${error.errorMessage}` });
    }
  );
}

export async function subscribeToBatteryLevel(deviceId: string): Promise<void> {
  await subscribeToCharacteristic(
    CHARACTERISTIC_UUIDS.BATTERY_LEVEL,
    SERVICE_UUIDS.BATTERY_SERVICE,
    ({ uuid, hex, deviceId: callbackDeviceId }) => {
      if (uuid.toLowerCase() !== CHARACTERISTIC_UUIDS.BATTERY_LEVEL || callbackDeviceId !== deviceId) return;

      try {
        const battery = Buffer.from(hex, "hex").readUInt8(0);
        updateDevice(deviceId, { batteryLevel: battery });

        console.log("🔋 [BLE] Received and parsed battery level from device..");
        console.log(`Battery level: ${battery}%`);
        console.log(`Hex: ${hex}`);
      } catch (error) {
        console.error("Error parsing battery level:", error);
      }
    }
  );
}
