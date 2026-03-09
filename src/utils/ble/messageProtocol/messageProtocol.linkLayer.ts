import { Buffer } from "buffer";

import { SERVICE_UUIDS } from "@/constants/ble";

import { MessageProtocolTransport } from "./messageProtocol.interface";

type BleNotificationData = {
  uuid: string;
  fullUuid: string;
  hex: string;
  deviceId: string;
};

type BleModule = {
  subscribeToCharacteristic: (
    characteristicUUID: string,
    serviceUUID: string,
    callback: (data: BleNotificationData) => void
  ) => Promise<() => void>;
  writeCharacteristic: (characteristicUUID: string, value: any) => Promise<boolean>;
};

let bleModuleCache: BleModule | null = null;

function getBleModule(): BleModule {
  if (bleModuleCache) {
    return bleModuleCache;
  }

  // Lazy require to keep tests from importing native modules at file-load time.
  // eslint-disable-next-line @typescript-eslint/no-var-requires
  const mod = require("../../../../modules/tenx-mdk-ble-rn-library/src/index") as BleModule;
  bleModuleCache = mod;
  return mod;
}

export async function subscribeToBleCharacteristic(
  rxCharacteristicUUID: string,
  onRawBytes: (bytes: Uint8Array) => void,
  serviceUUID = SERVICE_UUIDS.CUSTOM_SERVICE
): Promise<() => void> {
  const { subscribeToCharacteristic } = getBleModule();

  return subscribeToCharacteristic(rxCharacteristicUUID, serviceUUID, ({ hex }) => {
    if (!hex) {
      return;
    }

    const cleanedHex = hex.replace(/0x/gi, "").replace(/[^0-9a-fA-F]/g, "").trim();
    if (!cleanedHex || cleanedHex.length % 2 !== 0) {
      return;
    }

    try {
      const raw = Buffer.from(cleanedHex, "hex");
      if (raw.length === 0) {
        return;
      }
      onRawBytes(raw);
    } catch {
      // Ignore malformed notifications.
    }
  });
}

export async function writeBytesToBleCharacteristic(
  txCharacteristicUUID: string,
  bytes: Uint8Array
): Promise<boolean> {
  const base64Value = Buffer.from(bytes).toString("base64");
  const { writeCharacteristic } = getBleModule();
  return writeCharacteristic(txCharacteristicUUID, base64Value);
}

export function createBleCharacteristicTransport(options: {
  txCharacteristicUUID: string;
  rxCharacteristicUUID: string;
  serviceUUID?: string;
}): MessageProtocolTransport {
  return {
    sendPacket: (bytes: Uint8Array) => writeBytesToBleCharacteristic(options.txCharacteristicUUID, bytes),
    subscribe: (handler: (bytes: Uint8Array) => void) =>
      subscribeToBleCharacteristic(options.rxCharacteristicUUID, handler, options.serviceUUID),
  };
}
