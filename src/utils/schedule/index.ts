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

export const syncPendingEvents = async () => {
  const { schedules, markBackendSynced, clearOldSchedules } =
    useScheduleStore.getState();

  for (const [deviceId, doses] of Object.entries(schedules)) {
    for (const dose of doses) {
      if (dose.firmware_acknowledged && !dose.backend_synced) {
        try {
          await sendDoseEvent(dose, deviceId);
          markBackendSynced(deviceId, dose.id);
        } catch (err) {
          console.warn("Sync failed for", dose.id, err);
        }
      }
    }
  }

  clearOldSchedules();
};

export const getOutdatedTreatments = async (): Promise<Treatment[]> => {
  const latestTreatments = await getTreatments();
  const { treatments, storeTreatments, storeTreatment } = useTreatmentStore.getState();

  const isEmpty = treatments === null || Object.keys(treatments).length === 0;

  if (isEmpty) {
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
  const now = Date.now();
  const end = new Date(now + SEVEN_DAYS_MS).toISOString();

  const outdatedTreatments = await getOutdatedTreatments();
  if (outdatedTreatments.length !== 0) {
    console.log("[Scheduler] Found outdated treatments");
    console.log("[Scheduler] Attempting to refresh following treatments..");
    console.log(outdatedTreatments);

    let updatedSchedules: Schedule[] = [];
    try {
      updatedSchedules = await updateSchedules(
        outdatedTreatments,
        new Date(now).toISOString(),
        end,
        storeSchedules
      );

      console.log("\n");
      console.log("[Scheduler] Updated schedules below..");
      console.log(updatedSchedules);
    } catch (err) {
      console.error(err);
    }

    clearOldSchedules();
    updateNotificationsForSchedules(updatedSchedules);
  }
};

// Refresh the schedules if they are older than 7 days
const SEVEN_DAYS_MS = 7 * 24 * 60 * 60 * 1000;
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
  const last = Math.max(...Object.values(lastUpdated));
  if (now - last < SEVEN_DAYS_MS) {
    console.log("[Scheduler] Less than 7 days since last refresh. Skipping.");
    return;
  }

  // Proceed with refreshing
  const start = new Date(now).toISOString();
  const end = new Date(now + SEVEN_DAYS_MS).toISOString();

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
