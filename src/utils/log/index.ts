import * as FileSystem from "expo-file-system";
import * as Sharing from "expo-sharing";
import type { AxiosError, AxiosResponse } from "axios";

const ENABLE_VERBOSE_API_LOGS = false;

function summarizeApiData(data: unknown): string {
  if (data === null || data === undefined) return "-";
  if (typeof data === "string") {
    return data.length > 160 ? `${data.slice(0, 160)}...` : data;
  }
  if (typeof data !== "object") return String(data);
  if (Array.isArray(data)) return `Array(len=${data.length})`;

  const record = data as Record<string, unknown>;
  const keys = Object.keys(record);
  const summaryParts: string[] = [];
  if (Array.isArray(record.data)) summaryParts.push(`data=${record.data.length}`);
  if (Array.isArray(record.treatments)) summaryParts.push(`treatments=${record.treatments.length}`);
  if (Array.isArray(record.device_ids)) summaryParts.push(`device_ids=${record.device_ids.length}`);
  if (record.pagination && typeof record.pagination === "object") {
    const pagination = record.pagination as Record<string, unknown>;
    if (typeof pagination.total === "number") {
      summaryParts.push(`total=${pagination.total}`);
    }
  }

  const keyPreview = keys.slice(0, 6).join(", ");
  return summaryParts.length
    ? `Object(keys=[${keyPreview}]; ${summaryParts.join(", ")})`
    : `Object(keys=[${keyPreview}])`;
}

export function logAPIRequest(config: any) {
  const fullUrl = `${config.baseURL || ""}${config.url || ""}`;

  console.log("➡️ [API Request]");
  console.log(`${config.method?.toUpperCase() || ""} ${fullUrl}`);
  console.log(`${"Headers:"} ${config.headers}`);
  if (ENABLE_VERBOSE_API_LOGS) {
    console.log(`${"Payload:"}`, config?.data ? config?.data : "-");
    return;
  }
  console.log(`${"Payload:"} ${summarizeApiData(config?.data)}`);
}

export function logAPIResponse(response: AxiosResponse) {
  const fullUrl = `${response.config.baseURL || ""}${response.config.url || ""}`;

  console.log("✅ [API Response]");
  console.log(`${response.config.method?.toUpperCase() || ""} ${fullUrl}`);
  console.log(`${"Status:"} ${response.status}`);
  if (ENABLE_VERBOSE_API_LOGS) {
    console.log(`${"Data:"}`, response.data);
    return;
  }
  console.log(`${"Data:"} ${summarizeApiData(response.data)}`);
}

export function logAPIError(error: AxiosError) {
  const fullUrl = `${error.config?.baseURL || ""}${error.config?.url || ""}`;

  console.log("❌ [API Error]");

  if (fullUrl) console.log(`${"URL:"} ${fullUrl}`);

  if (error.response) {
    console.log(`${"Status:"} ${error.response.status}`);
    if (ENABLE_VERBOSE_API_LOGS) {
      console.log(`${"Data:"}`, error.response.data);
    } else {
      console.log(`${"Data:"} ${summarizeApiData(error.response.data)}`);
    }
  } else {
    console.log(`${"Message:"} ${error.message}`);
  }
}

// Try to extract JSON substring from a message
export function extractJson(msg: string) {
  // Find first { ... } or [ ... ]
  const match = msg.match(/(\{.*\}|\[.*\])/s); // 's' for dotall: allow . to match newlines
  if (!match) return null;
  try {
    const json = JSON.parse(match[0]);
    return { raw: match[0], obj: json, index: match.index ?? 0 };
  } catch {
    return null;
  }
}

// Convert logs array to string
export const exportLogsToFile = async (logs: string[]) => {
  const logString = logs.join("\n");
  const fileUri = FileSystem.documentDirectory + "logs.txt";

  await FileSystem.writeAsStringAsync(fileUri, logString, {
    encoding: FileSystem.EncodingType.UTF8,
  });

  // Check if sharing is available (iOS/Android only)
  if (await Sharing.isAvailableAsync()) {
    await Sharing.shareAsync(fileUri);
  } else {
    alert("Sharing not available on this device");
  }
};

// Helper function to determine style based on log content
export function getLogStyle(message: string) {
  const lowerMsg = message.toLowerCase();

  if (lowerMsg.includes("error")) {
    return { color: "#D7263D", fontWeight: "bold" as const };
  }
  if (lowerMsg.includes("warn")) {
    return { color: "#FF9F1C", fontWeight: "normal" as const };
  }
  if (lowerMsg.includes("success")) {
    return { color: "#2EC4B6", fontWeight: "normal" as const };
  }
  return { color: "#252F3B", fontWeight: "normal" as const };
}
