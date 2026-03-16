import { CHARACTERISTIC_UUIDS, SERVICE_UUIDS } from "@/constants/ble";

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

function isUuidEquivalent(uuid: string, expectedUuid: string): boolean {
  return normalizeUuidKey(uuid) === normalizeUuidKey(expectedUuid);
}

function hasProperty(
  characteristic: DiscoveredCharacteristic,
  predicate: (prop: string) => boolean
): boolean {
  return (
    Array.isArray(characteristic.properties) &&
    characteristic.properties.some(
      (prop) => typeof prop === "string" && predicate(prop.toLowerCase())
    )
  );
}

type ServiceResolutionCandidate = {
  txUuid: string;
  rxUuid: string;
  serviceUuid: string;
  score: number;
  source: string;
};

function getServiceResolutionCandidate(
  service: DiscoveredService
): ServiceResolutionCandidate | null {
  if (typeof service?.uuid !== "string" || !service.characteristics?.length) {
    return null;
  }

  const characteristics = service.characteristics.filter(
    (characteristic): characteristic is DiscoveredCharacteristic & { uuid: string } =>
      typeof characteristic?.uuid === "string" && characteristic.uuid.length > 0
  );

  if (!characteristics.length) {
    return null;
  }

  const explicitTx = characteristics.find(
    (characteristic) =>
      isUuidMatchByShortKey(characteristic.uuid, MP_TX_SHORT_UUID) ||
      isUuidEquivalent(characteristic.uuid, CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_TX) ||
      isUuidEquivalent(characteristic.uuid, CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_NUS_TX)
  );
  const explicitRx = characteristics.find(
    (characteristic) =>
      isUuidMatchByShortKey(characteristic.uuid, MP_RX_SHORT_UUID) ||
      isUuidEquivalent(characteristic.uuid, CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_RX) ||
      isUuidEquivalent(characteristic.uuid, CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_NUS_RX)
  );
  const writeCharacteristic = characteristics.find((characteristic) =>
    hasProperty(
      characteristic,
      (prop) => prop === "write" || prop === "writewithoutresponse"
    )
  );
  const notifyCharacteristic = characteristics.find((characteristic) =>
    hasProperty(characteristic, (prop) => prop === "notify" || prop === "indicate")
  );

  const normalizedServiceUuid = normalizeUuidKey(service.uuid);
  const isCustomService =
    isUuidMatchByShortKey(service.uuid, MP_SERVICE_SHORT_UUID) ||
    isUuidEquivalent(service.uuid, SERVICE_UUIDS.MESSAGE_PROTOCOL_SERVICE);
  const isNusService = isUuidEquivalent(service.uuid, SERVICE_UUIDS.MESSAGE_PROTOCOL_NUS_SERVICE);
  const isPreferredService = isCustomService || isNusService;
  const hasWriteNotifyPair = Boolean(writeCharacteristic && notifyCharacteristic);

  const txUuid = explicitTx?.uuid ?? writeCharacteristic?.uuid;
  const rxUuid = explicitRx?.uuid ?? notifyCharacteristic?.uuid;
  const singleCharacteristicUuid =
    !txUuid && !rxUuid && isPreferredService && characteristics.length === 1
      ? characteristics[0].uuid
      : null;

  if (!txUuid && !rxUuid && !singleCharacteristicUuid) {
    return null;
  }
  if (!explicitTx && !explicitRx && !isPreferredService && !hasWriteNotifyPair) {
    return null;
  }

  let score = 0;
  if (isCustomService) score += 40;
  if (isNusService) score += 30;
  if (explicitTx) score += 30;
  if (explicitRx) score += 30;
  if (writeCharacteristic) score += 12;
  if (notifyCharacteristic) score += 12;
  if (singleCharacteristicUuid) score += 3;
  if (
    normalizedServiceUuid === normalizeUuidKey(SERVICE_UUIDS.BATTERY_SERVICE) ||
    normalizedServiceUuid === "180f"
  ) {
    score -= 25;
  }

  const source = singleCharacteristicUuid
    ? "single-characteristic-fallback"
    : explicitTx || explicitRx
      ? "discovery-explicit-uuid"
      : "discovery-properties";

  return {
    txUuid: txUuid ?? singleCharacteristicUuid ?? CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_TX,
    rxUuid: rxUuid ?? singleCharacteristicUuid ?? CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_RX,
    serviceUuid: service.uuid,
    score,
    source,
  };
}

export function resolveMessageProtocolUuidsFromDiscovery(discovery: unknown): {
  txUuid: string;
  rxUuid: string;
  serviceUuid: string;
  source: string;
} {
  let txUuid = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_TX;
  let rxUuid = CHARACTERISTIC_UUIDS.MESSAGE_PROTOCOL_RX;
  let serviceUuid = SERVICE_UUIDS.MESSAGE_PROTOCOL_SERVICE;
  let source = "defaults";

  if (!Array.isArray(discovery)) {
    return { txUuid, rxUuid, serviceUuid, source };
  }

  const services = discovery as DiscoveredService[];
  const candidates = services
    .map(getServiceResolutionCandidate)
    .filter((candidate): candidate is ServiceResolutionCandidate => candidate !== null)
    .sort((a, b) => b.score - a.score);

  const bestCandidate = candidates[0];
  if (bestCandidate) {
    txUuid = bestCandidate.txUuid;
    rxUuid = bestCandidate.rxUuid;
    serviceUuid = bestCandidate.serviceUuid;
    source = bestCandidate.source;
  }

  return { txUuid, rxUuid, serviceUuid, source };
}
