import { ValidoseDevice } from "../device";

export interface VMedicationItemProps {
  item: ValidoseDevice;
}

export interface DoseLabelResult {
  mainLabel: string;
  timeLabel: string;
  detailsLabel: string;
  color?: string;
}

export interface VMedicationInfoProps {
  detailsLabel: string;
  state: number;
}

export type DoseScheduleEvent = {
  start_min: number;
  end_min: number;
}

export type DoseScheduleInput = {
  dosage_amount: number;
  events_per_day: number;
  max_temperature_threshold: number;
  temperature_avg_time_window_min: number;
  window: DoseScheduleEvent[];
}

export type Treatment = {
  id: string;
  participant_id: string;
  device_id: string;
  medication_code: string;
  schedule_updated_at?: string | null;
}

export type TreatmentsResponse = { treatments: Treatment[] }


export type DoseScheduleItem = {
  id: string;
  treatment_id: string;
  participant_id: string;
  device_id: string;
  medication_code: string;
  dosing_window_min: number;
  timezone: string;
  event_at: string;              // UTC ISO
  event_at_local: string;        // local ISO (server computed)
  window_starts_at: string;      // UTC
  window_starts_at_local: string;
  window_ends_at: string;        // UTC
  window_ends_at_local: string;
}

type ISO = string;

type DeviceScheduleBucket = {
  events: DoseScheduleItem[];             // always sorted by event_at asc
  fetchedStart?: ISO;                 // inclusive
  fetchedEnd?: ISO;                   // exclusive
  lastTreatmentUpdatedAt?: ISO | null;
}

export type ScheduleState = {
  byDevice: Record<string, DeviceScheduleBucket>;
  upsertEvents: (deviceId: string, range: { start: ISO; end: ISO }, events: DoseScheduleItem[]) => void;
  clearDevice: (deviceId: string) => void;
  setTreatmentUpdatedAt: (deviceId: string, ts: ISO | null | undefined) => void;
  getTodayEvents: (deviceId: string, todayStartISO: ISO, tomorrowStartISO: ISO) => DoseScheduleItem[];
  getMeta: (deviceId: string) => DeviceScheduleBucket | undefined;
}





export type TreatmentMeta = {
  id: string;                 // treatment_id
  participant_id: string;
  device_id: string;
  medication_code: string;
  schedule_updated_at: string; // ISO string
}

export type ScheduleEvent = {
  id: string;
  treatment_id: string;
  participant_id: string;
  device_id: string;
  medication_code: string;
  dosing_window_min: number;
  timezone: string;
  event_at: string;                 // UTC ISO
  event_at_local: string;           // local ISO
  window_starts_at: string;
  window_starts_at_local: string;
  window_ends_at: string;
  window_ends_at_local: string;
}

export type DateRange = { start: string; end: string }

export type PendingDose = {
  id: string;                 // client uuid
  device_id: string;
  medication_code: string;
  event_id: string;           // UTC ISO scheduled time
  taken_at: string;           // UTC ISO actual taken time
  payload?: any;              // raw hardware payload if needed
  sent: boolean;
  error?: string | null;
  dose_id?: string | null;
  dose_state: "ADMINISTERED" | "MISSED" | "POTENTIAL";
  dose_amount_mg: number;
  event_at: string;
}

export type DeviceBucket = {
  events: ScheduleEvent[];
  fetchedStart?: string | null;   // earliest ISO fetched
  fetchedEnd?: string | null;     // latest ISO fetched

  treatment?: TreatmentMeta | null;
  lastTreatmentCheckedAt?: string | null;
  lastTreatmentUpdatedAt?: string | null; // from schedule_updated_at

  lastScheduleRefreshedAt?: string | null;
}

export type DoseEventPayload = {
  // immutable identifiers (for idempotency server-side)
  idempotency_key: string;         // e.g. your local `p.id` (UUID)
  event_id: { days_since_epoch: number; event_ctr: number } | null; // if you have it
  device_id: string;
  medication_code: string;
  dose_id?: string | null;
  dose_state: "ADMINISTERED" | "MISSED" | "POTENTIAL"; // match backend enum
  dose_amount_mg: number;
  dose_event_at: string;           // ISO 8601 UTC: 2025-01-01T12:34:56.000Z
  // optional telemetry
  device_sent_at?: string;         // ISO 8601 UTC when the phone sent it
  device_timezone?: string;        // e.g. "America/New_York"
}
