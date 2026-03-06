import { Buffer } from "buffer";

import { CHARACTERISTIC_UUIDS } from "@/constants/ble";
import { DoseScheduleInput } from "@/types/dose";
import { writeCharacteristic } from "../../../../modules/tenx-mdk-ble-rn-library/src/index";
import { MsgProtError } from "../messageProtocol";
import {
  buildPpiPayload,
  encodeDoseSchedulePpi,
  encodeUint32LE,
  MAX_DOSES_PER_DAY,
  PpiId,
  PpiType,
} from "../messageProtocolPpi";
import { USE_MESSAGE_PROTOCOL_PPI } from "./constants";
import { ensureProtocolReadyForDataSend } from "./protocol";
import { getMessageProtocolInstance } from "./state";

function encodeDoseScheduleLegacy({
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
  view.setUint16(3, temperature_avg_time_window_min, true);

  let offset = 5;
  window.forEach((event) => {
    view.setUint16(offset, event.start_min, true);
    view.setUint16(offset + 2, event.end_min, true);
    offset += 4;
  });

  return new Uint8Array(buffer);
}

export async function resetBufferCharacteristic(): Promise<void> {
  try {
    const buffer = Buffer.alloc(1);
    buffer.writeUInt8(0x02, 0);
    const base64Value = buffer.toString("base64");

    const result = await writeCharacteristic(CHARACTERISTIC_UUIDS.RESET, base64Value);

    console.log("\n📝 [BLE] Writing reset buffer flag (0x02) to device..");
    console.log(`Successful? ${result}`);
    console.log(`Value (base64): ${base64Value}`);
  } catch (error) {
    console.log("Error writing reset buffer flag:", error);
  }
}

export async function writeSystemTime(): Promise<boolean> {
  const unixTime = Math.floor(Date.now() / 1000);
  const messageProtocol = getMessageProtocolInstance();

  console.log("\n");
  console.log("📝 [BLE] Writing system time to device..");
  console.log(`Unix time: ${unixTime}`);

  try {
    if (USE_MESSAGE_PROTOCOL_PPI && messageProtocol) {
      const payload = encodeUint32LE(unixTime);
      const txReady = await ensureProtocolReadyForDataSend(messageProtocol, "time update");

      if (!txReady) {
        console.warn("[MP] TX busy; cannot send time update yet.");
        return false;
      }

      const result = messageProtocol.send(buildPpiPayload(PpiId.AD_TIME, PpiType.PUSH, payload));
      console.log(`Message protocol send result: ${result}`);
      if (result === MsgProtError.NONE) {
        await messageProtocol.process();
      }
      return result === MsgProtError.NONE;
    }

    const buffer = Buffer.alloc(4);
    buffer.writeUInt32LE(unixTime, 0);
    const base64Time = buffer.toString("base64");
    console.log(`Payload (base64): ${base64Time}`);

    const result = await writeCharacteristic(CHARACTERISTIC_UUIDS.TIME, base64Time);
    const success = result === true;
    console.log(`Success? ${success}`);
    return success;
  } catch (error) {
    console.error("Error writing system time:", error);
    throw error;
  }
}

export async function writeDoseSchedule(doseSchedule: any): Promise<void> {
  const messageProtocol = getMessageProtocolInstance();

  try {
    const rawWindows = Array.isArray(doseSchedule?.window) ? doseSchedule.window : [];
    const doseWindowStartTimesMinutes = rawWindows
      .map((event: any) => Math.max(0, Math.trunc(Number(event?.start_min ?? 0))))
      .slice(0, MAX_DOSES_PER_DAY);

    const doseWindowDurationMinutes = Math.max(
      0,
      Math.trunc(
        Number(
          doseSchedule?.dose_window_duration_minutes ??
            doseSchedule?.dosing_window_min ??
            rawWindows[0]?.duration ??
            rawWindows[0]?.end_min ??
            0
        )
      )
    );

    const doseWindowCount = Math.min(
      MAX_DOSES_PER_DAY,
      Math.max(
        0,
        Math.trunc(
          Number(doseSchedule?.dose_window_count ?? doseWindowStartTimesMinutes.length)
        )
      )
    );

    const tempAvgWindowDurationSec = Math.max(
      0,
      Math.trunc(
        Number(
          doseSchedule?.temp_avg_window_duration_sec ??
            (doseSchedule?.temperature_avg_time_window_min ?? 0) * 60
        )
      )
    );

    const schedule = encodeDoseSchedulePpi({
      medication_type: Math.max(0, Math.trunc(Number(doseSchedule?.medication_type ?? 0))),
      dosage_mg: Math.max(
        0,
        Math.trunc(Number(doseSchedule?.dosage_mg ?? doseSchedule?.dosage_amount ?? 0))
      ),
      temp_upper_limit_deg_c: Math.trunc(
        Number(
          doseSchedule?.temp_upper_limit_deg_c ??
            doseSchedule?.max_temperature_threshold ??
            60
        )
      ),
      temp_lower_limit_deg_c: Math.trunc(Number(doseSchedule?.temp_lower_limit_deg_c ?? 0)),
      temp_avg_window_duration_sec: tempAvgWindowDurationSec,
      dose_days_bitfield: Math.max(
        0,
        Math.trunc(Number(doseSchedule?.dose_days_bitfield ?? 0x7f))
      ),
      dose_window_duration_minutes: doseWindowDurationMinutes,
      dose_window_count: doseWindowCount,
      dose_window_start_times_minutes: doseWindowStartTimesMinutes,
    });

    if (USE_MESSAGE_PROTOCOL_PPI && messageProtocol) {
      const txReady = await ensureProtocolReadyForDataSend(messageProtocol, "dose schedule update");
      if (!txReady) {
        console.warn("[MP] TX busy; cannot send dose schedule yet.");
        return;
      }

      const result = messageProtocol.send(
        buildPpiPayload(PpiId.AD_DOSE_SCHEDULE, PpiType.PUSH, schedule)
      );
      if (result === MsgProtError.NONE) {
        await messageProtocol.process();
      }
      console.log(`\n📝 [MP] Sent dose schedule. result=${result}`);
      return;
    }

    const legacyPayload = encodeDoseScheduleLegacy({
      dosage_amount: Number(doseSchedule?.dosage_amount ?? 0),
      events_per_day: Number(doseSchedule?.events_per_day ?? 0),
      max_temperature_threshold: Number(doseSchedule?.max_temperature_threshold ?? 0),
      temperature_avg_time_window_min: Number(doseSchedule?.temperature_avg_time_window_min ?? 0),
      window: Array.isArray(doseSchedule?.window) ? doseSchedule.window : [],
    });

    const base64DoseSchedule = Buffer.from(legacyPayload).toString("base64");

    const result = await writeCharacteristic(CHARACTERISTIC_UUIDS.DOSE_SCHEDULE, base64DoseSchedule);

    console.log("\n📝 [BLE] Attempting to write dose schedule to device..");
    console.log(`Successful? ${result}`);
    console.log(`Value (base64): ${base64DoseSchedule}`);
  } catch (error) {
    console.log("Error writing dose schedule:", error);
  }

}
