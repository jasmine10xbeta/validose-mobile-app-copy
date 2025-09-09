export interface AuthorizedDevice {
  deviceId: string;
}

export interface ValidoseDevice {
  deviceId: string;   // BLE address/identifier
  deviceName: string; // Starts with VAL-OP XXXX
  connected: boolean;
  color: string;
  error: string;
  batteryLevel: number;
}

export type StatusType = "network" | "connection" | "error" | "battery"
