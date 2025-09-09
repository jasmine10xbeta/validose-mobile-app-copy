import { AxiosError, AxiosResponse } from "axios";
import * as FileSystem from "expo-file-system";
import * as Sharing from "expo-sharing";

const BOLD = (text: string) => `\x1b[1m${text}\x1b[22m`;

export function logAPIRequest(config: any) {
  const fullUrl = `${config.baseURL || ""}${config.url || ""}`;

  console.log("\n");
  console.log(BOLD("➡️ [API Request]"));
  console.log(`${BOLD(config.method?.toUpperCase() || "")} ${fullUrl}`);
  console.log(`${BOLD("Headers:")} ${config.headers}`);
  console.log(`${BOLD("Payload:")} ${config?.data ? config?.data: "-"}`);
}

export function logAPIResponse(response: AxiosResponse) {
  const fullUrl = `${response.config.baseURL || ""}${response.config.url || ""}`;

  console.log("\n");
  console.log(BOLD("✅ [API Response]"));
  console.log(`${BOLD(response.config.method?.toUpperCase() || "")} ${fullUrl}`);
  console.log(`${BOLD("Status:")} ${response.status}`);
  console.log(`${BOLD("Data:")}`, response.data);
}

export function logAPIError(error: AxiosError) {
  const fullUrl = `${error.config?.baseURL || ""}${error.config?.url || ""}`;

  console.log("\n");
  console.log(BOLD("❌ [API Error]"));

  if (fullUrl) console.log(`${BOLD("URL:")} ${fullUrl}`);

  if (error.request) console.log(`${BOLD("Request:")} ${String(error.request)}`);

  if (error.response) {
    console.log(`${BOLD("Status:")} ${error.response.status}`);
    console.log(`${BOLD("Data:")}`, error.response.data);
  } else {
    console.log(`${BOLD("Message:")} ${error.message}`);
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