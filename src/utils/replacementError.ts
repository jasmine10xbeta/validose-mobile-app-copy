type ReplacementErrorReason = {
  title: string;
  message: string;
};

type ReplacementErrorUnit = {
  name: string;
  reasons: Record<string, ReplacementErrorReason>;
};

type ReplacementErrorMap = {
  defaults: ReplacementErrorReason;
  units: Record<string, ReplacementErrorUnit>;
};

type DecodedReplacementError = {
  unitHex: string;
  reasonHex: string;
  unitCode: number;
  reasonCode: number;
  unitName: string;
  title: string;
  message: string;
};

const replacementErrorMap =
  require("../constants/replacementErrorMap.json") as ReplacementErrorMap;

export function decodeReplacementErrorFromHex(hex: string): DecodedReplacementError | null {
  const cleanHex = hex.trim().toLowerCase();

  // Error frame format assumption:
  // first 2 bytes => unit, last 2 bytes => reason (minimum 4 bytes / 8 hex chars).
  if (!/^[0-9a-f]+$/.test(cleanHex) || cleanHex.length < 8) {
    return null;
  }

  const unitHex = cleanHex.slice(0, 4).toUpperCase();
  const reasonHex = cleanHex.slice(-4).toUpperCase();

  const unitCode = Number.parseInt(unitHex, 16);
  const reasonCode = Number.parseInt(reasonHex, 16);

  if (Number.isNaN(unitCode) || Number.isNaN(reasonCode)) {
    return null;
  }

  const unit = replacementErrorMap.units[unitHex];
  const reason = unit?.reasons?.[reasonHex];

  return {
    unitHex,
    reasonHex,
    unitCode,
    reasonCode,
    unitName: unit?.name ?? "UNKNOWN_UNIT",
    title: reason?.title ?? replacementErrorMap.defaults.title,
    message: reason?.message ?? replacementErrorMap.defaults.message,
  };
}

export function buildReplacementErrorRoute(params: {
  startedAtMs: number;
  step: number;
  progress: number;
  title: string;
  message: string;
  unitHex: string;
  reasonHex: string;
}): string {
  const query = [
    `startedAt=${params.startedAtMs}`,
    `step=${params.step}`,
    `progress=${params.progress}`,
    `title=${encodeURIComponent(params.title)}`,
    `message=${encodeURIComponent(params.message)}`,
    `unit=${params.unitHex}`,
    `reason=${params.reasonHex}`,
  ].join("&");

  return `/home/dashboard/replace-medication-error?${query}`;
}
