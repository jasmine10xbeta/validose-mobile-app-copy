import {
  getSchedules,
  getTreatments,
  sendDoseEvent,
} from "@/services/schedule";
import useScheduleStore from "@/store/schedule";
import useTreatmentStore from "@/store/treatment";
import { Treatment } from "@/types/dose";
import { Schedule } from "@/types/schedule";
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
  console.log("\n");
  console.log("[Scheduler] Syncing treatments and schedules..");
  
  const { clearOldSchedules, storeSchedules } = useScheduleStore.getState();
  const now = getLocalISOString();
  const end = getLocalISOString(new Date(Date.now() + 604800000));

  const outdatedTreatments = await getOutdatedTreatments();
  if (outdatedTreatments.length !== 0) {

    console.log("\n[Scheduler] Found empty or outdated treatments");
    console.log("[Scheduler] Attempting to refresh following treatments..", outdatedTreatments);

    let updatedSchedules: Schedule[] = [];
    try {
      updatedSchedules = await updateSchedules(
        outdatedTreatments,
        getLocalISOString(new Date(now)),
        end,
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
  console.log("\n");
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
  const start = getLocalISOString(new Date(now));
  const end = getLocalISOString(new Date(now + SCHEDULE_EXPIRY_DAYS_MS));

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

    console.log("\n");
    console.log("[Scheduler] Schedule refresh completed.");
  } catch (err) {
    console.log("\n");
    console.error("[Scheduler] Failed to refresh schedules:", err);
  }
};

export const getLocalISOString = (date: Date = new Date()): string => {
  const pad = (n: number) => String(n).padStart(2, '0');

  const year = date.getFullYear();
  const month = pad(date.getMonth() + 1);
  const day = pad(date.getDate());
  const hours = pad(date.getHours());
  const minutes = pad(date.getMinutes());
  const seconds = pad(date.getSeconds());

  return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}`;
};
