import {
  getSchedules,
  getTreatments,
} from "@/services/schedule";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { Schedule } from "@/types/schedule";
import { Treatment } from "@/types/treatment";
import { toUtcISOString } from "../date";
import { updateNotificationsForSchedules } from "../notifications";

const SCHEDULE_EXPIRY_DAYS_MS = 7 * 24 * 60 * 60 * 1000;

function getLocalDayStart(date: Date): Date {
  return new Date(
    date.getFullYear(),
    date.getMonth(),
    date.getDate(),
    0,
    0,
    0,
    0
  );
}

export const syncPendingEvents = async () => {
  const { schedules, markBackendSynced, clearOldSchedules } =
    useScheduleStore.getState();

  for (const [deviceId, doses] of Object.entries(schedules)) {
    for (const dose of doses) {
      if (dose.firmware_acknowledged && !dose.backend_synced) {
        try {
          // Dose events are backend-synced via /hardware/ingest message protocol flow.
          markBackendSynced(deviceId, dose.id);
        } catch (err) {
          console.warn("Sync failed for", dose.id, err);
        }
      }
    }
  }

  clearOldSchedules();
};

type SyncTreatmentsAndSchedulesOptions = {
  forceScheduleFetch?: boolean;
  reason?: string;
};

function hasFutureOrActiveSchedules(
  scheduleList: Schedule[] | undefined,
  nowMs: number
): boolean {
  if (!Array.isArray(scheduleList) || scheduleList.length === 0) {
    return false;
  }

  return scheduleList.some((schedule) => {
    const windowEnd =
      schedule.window_ends_at_local ||
      schedule.window_ends_at ||
      schedule.event_at_local ||
      schedule.event_at;
    const windowEndMs = new Date(windowEnd).getTime();
    return Number.isFinite(windowEndMs) && windowEndMs >= nowMs;
  });
}

function getTreatmentsNeedingScheduleRefresh(
  latestTreatments: Treatment[],
  storedTreatments: Record<string, Treatment> | null,
  schedulesByDevice: Record<string, Schedule[]>,
  nowMs: number,
  forceScheduleFetch: boolean
): Treatment[] {
  const treatmentsToRefresh: Treatment[] = [];

  for (const treatment of latestTreatments) {
    const storedTreatment = storedTreatments?.[treatment.id];
    const localSchedules = schedulesByDevice[treatment.device_id];

    const isNewTreatment = !storedTreatment;
    const isUpdatedTreatment =
      !!storedTreatment &&
      storedTreatment.schedule_updated_at !== treatment.schedule_updated_at;
    const missingOrExpiredLocalSchedules = !hasFutureOrActiveSchedules(
      localSchedules,
      nowMs
    );

    if (
      forceScheduleFetch ||
      isNewTreatment ||
      isUpdatedTreatment ||
      missingOrExpiredLocalSchedules
    ) {
      treatmentsToRefresh.push(treatment);
    }
  }

  return treatmentsToRefresh;
}

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
    console.log(
      "[Scheduler] Fetching schedules from backend..",
      JSON.stringify({ treatmentId: treatment_id, deviceId: device_id })
    );
    const schedulesResponse = await getSchedules(treatment_id, {
      device: device_id,
      event_at: { ">=": start_date, "<": end_date },
    });
    const fetchedSchedules = Array.isArray(schedulesResponse?.data)
      ? schedulesResponse.data
      : [];

    console.log(
      "[Scheduler] Received schedules from backend.",
      JSON.stringify({
        treatmentId: treatment_id,
        deviceId: device_id,
        scheduleCount: fetchedSchedules.length,
      })
    );

    storeSchedules(device_id, fetchedSchedules);
    combinedSchedule.push(...fetchedSchedules);
  }

  return combinedSchedule;
};


export const syncTreatmentsAndSchedules = async (
  options: SyncTreatmentsAndSchedulesOptions = {}
) => {
  const { forceScheduleFetch = false, reason = "unspecified" } = options;
  console.log(
    `[Scheduler] Syncing treatments and schedules.. (force=${forceScheduleFetch}, reason=${reason})`
  );

  const nowMs = Date.now();
  const nowDate = new Date(nowMs);
  const rangeStartDate = getLocalDayStart(nowDate);
  const endDate = new Date(nowMs + SCHEDULE_EXPIRY_DAYS_MS);
  const scheduleStore = useScheduleStore.getState();
  const treatmentStore = useTreatmentStore.getState();

  const latestTreatmentsResponse = await getTreatments();
  const latestTreatments = Array.isArray(latestTreatmentsResponse?.treatments)
    ? latestTreatmentsResponse.treatments
    : [];

  treatmentStore.storeTreatments(latestTreatments);

  const treatmentsToRefresh = getTreatmentsNeedingScheduleRefresh(
    latestTreatments,
    treatmentStore.treatments,
    scheduleStore.schedules,
    nowMs,
    forceScheduleFetch
  );

  if (treatmentsToRefresh.length === 0) {
    console.log(
      `[Scheduler] Schedule cache is fresh for all ${latestTreatments.length} treatments.`
    );
    scheduleStore.setLastUpdated(nowMs);
    return;
  }

  console.log(
    "[Scheduler] Refreshing schedules for treatments:",
    treatmentsToRefresh.map((treatment) => ({
      treatmentId: treatment.id,
      deviceId: treatment.device_id,
      scheduleUpdatedAt: treatment.schedule_updated_at,
    }))
  );

  let updatedSchedules: Schedule[] = [];
  try {
    updatedSchedules = await updateSchedules(
      treatmentsToRefresh,
      toUtcISOString(rangeStartDate),
      toUtcISOString(endDate),
      scheduleStore.storeSchedules
    );

    scheduleStore.clearOldSchedules();
    scheduleStore.setLastUpdated(nowMs);
    updateNotificationsForSchedules(updatedSchedules);

    console.log(
      `[Scheduler] Schedule sync completed. Updated schedule count: ${updatedSchedules.length}`
    );
  } catch (err) {
    console.error("[Scheduler] Failed to sync treatments and schedules:", err);
    throw err;
  }
};

// Refresh locally stored schedules if they are older than SCHEDULE_EXPIRY_DAYS_MS
export const refreshExpiringSchedules = async () => {
  console.log("[Scheduler] Refreshing expiring schedules..");

  const { schedules, lastUpdated } = useScheduleStore.getState();

  const now = Date.now();

  // No schedules stored? Likely first-time sync
  const hasAtLeastOneSchedule = Object.values(schedules).some(
    (list) => list && list.length > 0
  );

  if (!hasAtLeastOneSchedule) {
    console.log(
      "[Scheduler] No schedules found in store. Triggering forced backend schedule sync."
    );
    await syncTreatmentsAndSchedules({
      forceScheduleFetch: true,
      reason: "refresh-expiring-empty-schedule-cache",
    });
    return;
  }

  if (!Number.isFinite(lastUpdated) || lastUpdated <= 0) {
    console.log(
      "[Scheduler] lastUpdated timestamp missing. Triggering forced backend schedule sync."
    );
    await syncTreatmentsAndSchedules({
      forceScheduleFetch: true,
      reason: "refresh-expiring-missing-last-updated",
    });
    return;
  }

  if (now - lastUpdated < SCHEDULE_EXPIRY_DAYS_MS) {
    const days = Math.floor((now - lastUpdated) / (1000 * 60 * 60 * 24));
    console.log(
      `[Scheduler] Less than ${SCHEDULE_EXPIRY_DAYS_MS}ms (~${days} days) since last refresh. Skipping.`
    );
    return;
  }

  console.log(
    `[Scheduler] Been ${Math.floor((now - lastUpdated) / (1000 * 60 * 60 * 24))} days since last refresh.`
  );
  await syncTreatmentsAndSchedules({
    forceScheduleFetch: true,
    reason: "refresh-expiring-stale-cache",
  });
};

export type DeviceScheduleSyncData = {
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
  const canonicalDeviceId =
    (typeof treatment.device_id === "string" && treatment.device_id.trim()) || deviceId;
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
    deviceId: canonicalDeviceId,
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
  const rangeStartDate = getLocalDayStart(now);
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
    event_at: { ">=": toUtcISOString(rangeStartDate), "<": toUtcISOString(end) },
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

function normalizeIdentifier(value: string): string {
  return value.trim().toLowerCase();
}

function resolveCachedSchedulesForDevice(deviceId: string): {
  deviceKey: string;
  schedules: Schedule[];
} | null {
  const scheduleState = useScheduleStore.getState().schedules;
  const normalizedTarget = normalizeIdentifier(deviceId);
  if (!normalizedTarget) return null;

  const directSchedules = scheduleState[deviceId];
  if (Array.isArray(directSchedules) && directSchedules.length) {
    return {
      deviceKey: deviceId,
      schedules: directSchedules,
    };
  }

  for (const [scheduleKey, scheduleList] of Object.entries(scheduleState)) {
    if (!Array.isArray(scheduleList) || !scheduleList.length) continue;
    if (normalizeIdentifier(scheduleKey) === normalizedTarget) {
      return {
        deviceKey: scheduleKey,
        schedules: scheduleList,
      };
    }
  }

  for (const [scheduleKey, scheduleList] of Object.entries(scheduleState)) {
    if (!Array.isArray(scheduleList) || !scheduleList.length) continue;
    const matchesScheduleDeviceId = scheduleList.some((schedule) => {
      if (typeof schedule.device_id !== "string") return false;
      return normalizeIdentifier(schedule.device_id) === normalizedTarget;
    });
    if (matchesScheduleDeviceId) {
      return {
        deviceKey: scheduleKey,
        schedules: scheduleList,
      };
    }
  }

  return null;
}

function resolveCachedTreatmentForDevice(
  deviceId: string,
  schedules: Schedule[]
): Treatment | null {
  const treatmentStore = useTreatmentStore.getState();
  const directTreatment = treatmentStore.getDeviceTreatment(deviceId);
  if (directTreatment) return directTreatment;

  const scheduleTreatmentId = schedules[0]?.treatment_id;
  if (scheduleTreatmentId) {
    const byTreatmentId = treatmentStore.getTreatmentById(scheduleTreatmentId);
    if (byTreatmentId) return byTreatmentId;
  }

  const scheduleDeviceId = schedules[0]?.device_id;
  if (scheduleDeviceId) {
    const byScheduleDeviceId = treatmentStore.getDeviceTreatment(scheduleDeviceId);
    if (byScheduleDeviceId) return byScheduleDeviceId;
  }

  return null;
}

export function getCachedDeviceScheduleSyncData(
  deviceId: string
): DeviceScheduleSyncData | null {
  const resolved = resolveCachedSchedulesForDevice(deviceId);
  if (!resolved || !resolved.schedules.length) return null;

  const treatment = resolveCachedTreatmentForDevice(deviceId, resolved.schedules);
  if (!treatment) return null;

  const payload = buildDeviceDoseSchedulePayload(resolved.schedules);
  if (!payload) return null;

  const signatureSourceDeviceId =
    (typeof treatment.device_id === "string" && treatment.device_id.trim()) ||
    resolved.deviceKey;

  return {
    treatment,
    schedules: resolved.schedules,
    payload,
    signature: buildDeviceScheduleSignature(
      signatureSourceDeviceId,
      treatment,
      resolved.schedules
    ),
  };
}

export { toUtcISOString } from "../date";
