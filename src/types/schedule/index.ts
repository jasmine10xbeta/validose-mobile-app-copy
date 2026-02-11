export interface Schedule {
  id: string;
  treatment_id: string;
  participant_id: string;
  device_id: string;
  medication_code: string;
  dosing_window_min: number;
  timezone: string;
  event_at: string;
  event_at_local: string;
  window_starts_at: string;
  window_starts_at_local: string;
  window_ends_at: string;
  window_ends_at_local: string;
  firmware_acknowledged: boolean;
  firmware_info?: Record<string, any>; // detailed info from firmware
  backend_synced: boolean;
}
