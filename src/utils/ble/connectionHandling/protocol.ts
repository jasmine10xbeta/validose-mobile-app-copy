import {
  MessageProtocolInterface,
  MsgProtError,
} from "../messageProtocol";
import {
  decodeDoseEventPpi,
  decodePpiPayload,
  isTxStatusSendable,
  PpiId,
  PpiType,
} from "../messageProtocolPpi";
import { TX_READY_POLL_MS, TX_READY_TIMEOUT_MS } from "./constants";
import { getMessageProtocolInstance } from "./state";

export function relayMessageProtocolConsoleLog(
  level: "INFO" | "WARN" | "ERR",
  args: unknown[]
): void {
  const prefix = `[MP][PROTOCOL][${level}]`;
  if (!args.length) {
    if (level === "INFO") {
      console.info(prefix);
    } else if (level === "WARN") {
      console.warn(prefix);
    } else {
      console.error(prefix);
    }
    return;
  }

  const [first, ...rest] = args;
  if (typeof first === "string") {
    const message = `${prefix} ${first}`;
    if (level === "INFO") {
      console.info(message, ...rest);
    } else if (level === "WARN") {
      console.warn(message, ...rest);
    } else {
      console.error(message, ...rest);
    }
    return;
  }

  if (level === "INFO") {
    console.info(prefix, ...args);
  } else if (level === "WARN") {
    console.warn(prefix, ...args);
  } else {
    console.error(prefix, ...args);
  }
}

export async function waitForTxSendable(
  protocol: MessageProtocolInterface,
  timeoutMs = TX_READY_TIMEOUT_MS,
  pollMs = TX_READY_POLL_MS
): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() <= deadline) {
    if (isTxStatusSendable(protocol.getTxPacketStatus())) {
      return true;
    }

    // Drive retries/timeouts immediately instead of waiting only for the interval loop.
    await protocol.process();
    await new Promise((resolve) => setTimeout(resolve, pollMs));
  }

  return isTxStatusSendable(protocol.getTxPacketStatus());
}

export async function ensureProtocolReadyForDataSend(
  protocol: MessageProtocolInterface,
  context: string
): Promise<boolean> {
  if (protocol.getCurrentSessionId() === 0) {
    const syncResult = await protocol.startSync();
    if (syncResult !== MsgProtError.NONE) {
      console.warn(`[MP] Could not start sync before ${context}.`, { syncResult });
      return false;
    }
  }

  const txReady = await waitForTxSendable(protocol);
  if (!txReady) {
    console.warn(`[MP] TX not ready for ${context}; waiting for sync/ACK state.`);
  }

  return txReady;
}

export function setupMessageProtocolHandlers(_deviceId: string): void {
  const messageProtocol = getMessageProtocolInstance();
  if (!messageProtocol) {
    return;
  }

  messageProtocol.registerRxHandler(PpiId.AD_DOSE_EVENT_REPORT, PpiType.PUSH, async (packet) => {
    const decoded = decodeDoseEventPpi(packet.payload);
    if (!decoded) {
      console.warn("[MP] Dose event payload size mismatch.");
      return;
    }

    console.log("\n");
    console.log("💊 [MP] Received dose event", decoded);

    // TODO: Map decoded fields to backend payload. The current backend expects
    // dose_amount_mg and an event timestamp. Firmware dose_event_t does not include
    // dose_amount_mg, so this needs alignment before sending.
  });

  messageProtocol.registerRxHandler(PpiId.AD_TIME, PpiType.RE, (packet) => {
    const decoded = decodePpiPayload(packet.ppi, packet.type as PpiType, packet.payload);
    console.log("\n");
    console.log("🕒 [MP] Time update response", decoded.value);
  });
}
