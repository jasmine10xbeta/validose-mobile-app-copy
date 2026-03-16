import {
  getSchedules,
  getTreatments,
  sendDoseEvent,
} from "@/services/schedule";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { Schedule } from "@/types/schedule";
import { Treatment } from "@/types/treatment";
import { toUtcISOString } from "../date";
import { updateNotificationsForSchedules } from "../notifications";

const SCHEDULE_EXPIRY_DAYS_MS = 7 * 24 * 60 * 60 * 1000;

export const syncPendingEvents = async () => {
  const { schedules, markBackendSynced, clearOldSchedules } =
    useScheduleStore.getState();

  for (const [deviceId, doses] of Object.entries(schedules)) {
    for (const dose of doses) {
      if (dose.firmware_acknowledged && !dose.backend_synced) {
        try {
          await sendDoseEvent(dose, deviceId, dose?.medication_code);
          markBackendSynced(deviceId, dose.id);
        } catch (err) {
          console.warn("Sync failed for", dose.id, err);
        }
      }
    }
  }

  clearOldSchedules();
};

const getOutdatedTreatments = async (): Promise<Treatment[]> => {
  const latestTreatments = await getTreatments();
  const { treatments, storeTreatments, storeTreatment } = useTreatmentStore.getState();

  const isEmpty = treatments === null || Object.keys(treatments).length === 0;

  if (isEmpty) {
    console.log("\n[Scheduler] No stored treatments. Syncing..");

    storeTreatments(latestTreatments.treatments);
    return latestTreatments.treatments;
  }

  const outdated: Treatment[] = [];

  for (const latest of latestTreatments.treatments) {
    const stored = treatments?.[latest.id];

    const isNew = !stored;
    const isUpdated = stored?.schedule_updated_at !== latest.schedule_updated_at;

    if (isNew || isUpdated) {
      outdated.push(latest);
      storeTreatment(latest);
    }
  }

  return outdated;
};

export const updateSchedules = async (
  outdatedTreatments: Treatment[],
  start_date: string,
  end_date: string,
  storeSchedules: (deviceId: string, scheduleList: Schedule[]) => void
): Promise<Schedule[]> => {
  const combinedSchedule: Schedule[] = [];

  if (!Array.isArray(outdatedTreatments)) {
    console.error("Expected array for outdatedTreatments but got", outdatedTreatments);
    return [];
  }

  for (const treatment of outdatedTreatments) {
    const { device_id, id: treatment_id } = treatment;
    const schedules = await getSchedules(treatment_id, { device: device_id, event_at: { ">=": start_date, "<": end_date } }); // TODO
    storeSchedules(device_id, schedules.data);

    combinedSchedule.push(...schedules.data);
  }

  return combinedSchedule;
};


export const syncTreatmentsAndSchedules = async () => {
  console.log("[Scheduler] Syncing treatments and schedules..");
  
  const { clearOldSchedules, storeSchedules } = useScheduleStore.getState();
  const now = new Date();
  const end = new Date(Date.now() + 604800000);

  const outdatedTreatments = await getOutdatedTreatments();
  if (outdatedTreatments.length !== 0) {

    console.log("\n[Scheduler] Found empty or outdated treatments");
    console.log("[Scheduler] Attempting to refresh following treatments..", outdatedTreatments);

    let updatedSchedules: Schedule[] = [];
    try {
      updatedSchedules = await updateSchedules(
        outdatedTreatments,
        toUtcISOString(now),
        toUtcISOString(end),
        storeSchedules
      ); // TODO: Refactor start and end date logic here!!!!

      console.log("\n[Scheduler] Updated schedules below..");
      console.log(updatedSchedules);
    } catch (err) {
      console.error(err);
    }

    clearOldSchedules();
    updateNotificationsForSchedules(updatedSchedules);
  }
};

// Refresh locally stored schedules if they are older than SCHEDULE_EXPIRY_DAYS_MS
export const refreshExpiringSchedules = async () => {
  console.log("[Scheduler] Refreshing expiring schedules..");
  
  const { schedules, lastUpdated, setLastUpdated, storeSchedules } = useScheduleStore.getState();
  const { treatments, storeTreatments } = useTreatmentStore.getState();

  const now = Date.now();

  // No treatments stored? Skip
  if (treatments === null) {
    console.log("[Scheduler] No treatments in store. Skipping refresh.");
    return;
  }

  // No schedules stored? Likely first-time sync
  const hasAtLeastOneSchedule = Object.values(schedules).some(
    (list) => list && list.length > 0
  );
  
  if (!hasAtLeastOneSchedule) {
    console.log("[Scheduler] No schedules found. Skipping refresh.");
    return;
  }

  // Is it time to refresh?
  const lastUpdatedValues = Object.values(lastUpdated);

  if (lastUpdatedValues.length === 0) {
    console.log("[Scheduler] No lastUpdated timestamps found. Skipping refresh.");
    return;
  }

  const last = Math.max(...lastUpdatedValues);

  if (now - last < SCHEDULE_EXPIRY_DAYS_MS) {
    const days = Math.floor((now - last) / (1000 * 60 * 60 * 24));
    console.log(`[Scheduler] Less than ${SCHEDULE_EXPIRY_DAYS_MS}ms (~${days} days) since last refresh. Skipping.`);
    return;
  }

  console.log(`[Scheduler] Been ${Math.floor((now - last) / (1000 * 60 * 60 * 24))} days since last refresh.`);

  // const last = Math.max(...Object.values(lastUpdated));
  // if (now - last < SCHEDULE_EXPIRY_DAYS_MS) {
  //   console.log(`[Scheduler] Less than ${SCHEDULE_EXPIRY_DAYS_MS} days since last refresh. Skipping.`);
  //   return;
  // }

  // console.log(`[Scheduler] Been ${Math.floor((now - last) / (1000 * 60 * 60 * 24))} days since last refresh.`);
  console.log("[Scheduler] Proceeding with refresh..");
  // Proceed with refreshing
  const start = toUtcISOString(now);
  const end = toUtcISOString(now + SCHEDULE_EXPIRY_DAYS_MS);

  try {
    const latestTreatments = await getTreatments();
    storeTreatments(latestTreatments.treatments);

    const updatedSchedules = await updateSchedules(
      latestTreatments.treatments,
      start,
      end,
      storeSchedules
    );

    updateNotificationsForSchedules(updatedSchedules);
    setLastUpdated(now);

    console.log("[Scheduler] Schedule refresh completed.");
  } catch (err) {
    console.error("[Scheduler] Failed to refresh schedules:", err);
  }
};

type DeviceScheduleSyncData = {
  treatment: Treatment;
  schedules: Schedule[];
  signature: string;
  payload: {
    medication_type: number;
    dosage_mg: number;
    temp_upper_limit_deg_c: number;
    temp_lower_limit_deg_c: number;
    temp_avg_window_duration_sec: number;
    dose_days_bitfield: number;
    dose_window_duration_minutes: number;
    dose_window_count: number;
    dose_window_start_times_minutes: number[];
  };
};

function toMinutesSinceMidnight(isoValue: string): number {
  const parsed = new Date(isoValue);
  if (Number.isNaN(parsed.getTime())) return 0;
  return parsed.getHours() * 60 + parsed.getMinutes();
}

function buildDoseWindowStartTimes(schedules: Schedule[]): number[] {
  const unique = new Set<number>();

  for (const schedule of schedules) {
    unique.add(toMinutesSinceMidnight(schedule.event_at_local || schedule.event_at));
  }

  return Array.from(unique)
    .filter((value) => Number.isFinite(value) && value >= 0)
    .sort((a, b) => a - b)
    .slice(0, 10);
}

function buildDeviceScheduleSignature(
  deviceId: string,
  treatment: Treatment,
  schedules: Schedule[],
): string {
  const normalized = schedules
    .map((schedule) => ({
      id: schedule.id,
      event_at: schedule.event_at,
      event_at_local: schedule.event_at_local,
      dosing_window_min: schedule.dosing_window_min,
      medication_code: schedule.medication_code,
    }))
    .sort((a, b) => a.event_at.localeCompare(b.event_at));

  return JSON.stringify({
    deviceId,
    treatmentId: treatment.id,
    scheduleUpdatedAt: treatment.schedule_updated_at ?? "",
    schedules: normalized,
  });
}

function buildDeviceDoseSchedulePayload(schedules: Schedule[]) {
  const sorted = [...schedules].sort((a, b) => a.event_at.localeCompare(b.event_at));
  const doseWindowStartTimesMinutes = buildDoseWindowStartTimes(sorted);
  const firstSchedule = sorted[0];
  const doseWindowDurationMinutes = Math.max(
    1,
    Math.min(255, Number(firstSchedule?.dosing_window_min) || 30),
  );

  if (!doseWindowStartTimesMinutes.length) {
    return null;
  }

  return {
    medication_type: 0,
    dosage_mg: 0,
    temp_upper_limit_deg_c: 60,
    temp_lower_limit_deg_c: 0,
    temp_avg_window_duration_sec: 30 * 60,
    dose_days_bitfield: 0x7f,
    dose_window_duration_minutes: doseWindowDurationMinutes,
    dose_window_count: doseWindowStartTimesMinutes.length,
    dose_window_start_times_minutes: doseWindowStartTimesMinutes,
  };
}

export async function fetchAndStoreDeviceSchedules(deviceId: string): Promise<Schedule[]> {
  const now = new Date();
  const end = new Date(Date.now() + 604800000);

  const latestTreatments = await getTreatments();
  useTreatmentStore.getState().storeTreatments(latestTreatments.treatments);

  const treatment = latestTreatments.treatments.find((t: Treatment) => t.device_id === deviceId);
  if (!treatment) {
    useScheduleStore.getState().storeSchedules(deviceId, []);
    return [];
  }

  const schedulesResponse = await getSchedules(treatment.id, {
    device: deviceId,
    event_at: { ">=": toUtcISOString(now), "<": toUtcISOString(end) },
  });

  const scheduleList = Array.isArray(schedulesResponse?.data) ? schedulesResponse.data : [];
  useScheduleStore.getState().storeSchedules(deviceId, scheduleList);
  return scheduleList;
}

export async function getDeviceScheduleSyncData(
  deviceId: string,
): Promise<DeviceScheduleSyncData | null> {
  const schedules = await fetchAndStoreDeviceSchedules(deviceId);
  if (!schedules.length) return null;

  const treatment = useTreatmentStore.getState().getDeviceTreatment(deviceId);
  if (!treatment) return null;

  const payload = buildDeviceDoseSchedulePayload(schedules);
  if (!payload) return null;

  return {
    treatment,
    schedules,
    payload,
    signature: buildDeviceScheduleSignature(deviceId, treatment, schedules),
  };
}

export { toUtcISOString } from "../date";
