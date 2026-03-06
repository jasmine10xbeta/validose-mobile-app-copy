import { CHARACTERISTIC_UUIDS } from "@/constants/ble";

import { MP_RX_SHORT_UUID, MP_SERVICE_SHORT_UUID, MP_TX_SHORT_UUID } from "./constants";

type DiscoveredCharacteristic = {
  uuid?: string;
  properties?: string[];
};

type DiscoveredService = {
  uuid?: string;
  characteristics?: DiscoveredCharacteristic[];
};

function normalizeUuidKey(uuid: string): string {
  return uuid.replace(/[^0-9a-fA-F]/g, "").toLowerCase();
}

function isUuidMatchByShortKey(uuid: string, shortUuid: string): boolean {
  const normalized = normalizeUuidKey(uuid);
  const shortKey = shortUuid.toLowerCase();

  if (!normalized) return false;
  if (normalized === shortKey) return true;
  return normalized.startsWith(`0000${shortKey}`);
}

export function resolveMessageProtocolUuidsFromDiscovery(discovery: unknown): {
  txUuid: string;
  rxUuid: string;
  source: string;
} {
  let txUuid = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_TX;
  let rxUuid = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_RX;
  let source = "defaults";

  if (!Array.isArray(discovery)) {
    return { txUuid, rxUuid, source };
  }

  const services = discovery as DiscoveredService[];
  const customService = services.find(
    (service) =>
      typeof service?.uuid === "string" &&
      isUuidMatchByShortKey(service.uuid, MP_SERVICE_SHORT_UUID)
  );

  if (!customService?.characteristics?.length) {
    return { txUuid, rxUuid, source };
  }

  const characteristics = customService.characteristics.filter(
    (characteristic): characteristic is DiscoveredCharacteristic & { uuid: string } =>
      typeof characteristic?.uuid === "string" && characteristic.uuid.length > 0
  );

  const hasProperty = (
    characteristic: DiscoveredCharacteristic,
    predicate: (prop: string) => boolean
  ) =>
    Array.isArray(characteristic.properties) &&
    characteristic.properties.some(
      (prop) => typeof prop === "string" && predicate(prop.toLowerCase())
    );

  const writeCharacteristic = characteristics.find((characteristic) =>
    hasProperty(
      characteristic,
      (prop) => prop === "write" || prop === "writewithoutresponse"
    )
  );
  const notifyCharacteristic = characteristics.find((characteristic) =>
    hasProperty(
      characteristic,
      (prop) => prop === "notify" || prop === "indicate"
    )
  );

  const explicitTx = characteristics.find((characteristic) =>
    isUuidMatchByShortKey(characteristic.uuid, MP_TX_SHORT_UUID)
  );
  const explicitRx = characteristics.find((characteristic) =>
    isUuidMatchByShortKey(characteristic.uuid, MP_RX_SHORT_UUID)
  );

  if (explicitTx?.uuid) {
    txUuid = explicitTx.uuid;
    source = "discovery-explicit-uuid";
  }
  if (explicitRx?.uuid) {
    rxUuid = explicitRx.uuid;
    source = source === "discovery-explicit-uuid" ? source : "discovery-explicit-uuid";
  }

  if (!explicitTx?.uuid && writeCharacteristic?.uuid) {
    txUuid = writeCharacteristic.uuid;
    source = source === "defaults" ? "discovery-properties" : `${source}+properties`;
  }
  if (!explicitRx?.uuid && notifyCharacteristic?.uuid) {
    rxUuid = notifyCharacteristic.uuid;
    source = source === "defaults" ? "discovery-properties" : `${source}+properties`;
  }

  if (
    !explicitTx?.uuid &&
    !explicitRx?.uuid &&
    !writeCharacteristic?.uuid &&
    !notifyCharacteristic?.uuid &&
    characteristics.length === 1
  ) {
    txUuid = characteristics[0].uuid;
    rxUuid = characteristics[0].uuid;
    source = "single-characteristic-fallback";
  }

  return { txUuid, rxUuid, source };
}
