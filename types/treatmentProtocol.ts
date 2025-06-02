export interface Medication {
  name: string;
  dose_times: string[]; // e.g., ["08:00", "20:00"]
  days_of_week?: string[]; // Optional: ["Monday", "Tuesday", ...]
}

export interface TreatmentProtocol {
  protocol_id: string;
  regimen_id: string;
  version: number;
  updated_at: string;
  source: string;
  timezone: string;
  medications: Medication[];
}