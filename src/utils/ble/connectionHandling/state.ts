import {
  AckedRxDataPacket,
  BleMessageProtocol,
  MessageProtocolInterface,
  MpPacketPayload,
} from "../messageProtocol";

let messageProtocol: BleMessageProtocol | null = null;
const rxPacketListeners = new Set<(packet: MpPacketPayload) => void>();
const rxDataAckedListeners = new Set<(packet: AckedRxDataPacket) => void>();

export function setMessageProtocol(protocol: BleMessageProtocol | null): void {
  messageProtocol = protocol;
}

export function stopAndClearMessageProtocol(): void {
  if (messageProtocol) {
    messageProtocol.stop();
    messageProtocol = null;
  }
}

export function getMessageProtocolInstance(): BleMessageProtocol | null {
  return messageProtocol;
}

export function getMessageProtocol(): MessageProtocolInterface | null {
  return messageProtocol;
}

export function emitMessageProtocolRxPacket(packet: MpPacketPayload): void {
  rxPacketListeners.forEach((listener) => {
    try {
      listener(packet);
    } catch (error) {
      console.warn("[MP][STATE] RX packet listener failed.", error);
    }
  });
}

export function emitMessageProtocolRxDataAcked(packet: AckedRxDataPacket): void {
  rxDataAckedListeners.forEach((listener) => {
    try {
      listener(packet);
    } catch (error) {
      console.warn("[MP][STATE] RX-ACK listener failed.", error);
    }
  });
}

export function subscribeMessageProtocolRxPackets(
  listener: (packet: MpPacketPayload) => void
): () => void {
  rxPacketListeners.add(listener);
  return () => {
    rxPacketListeners.delete(listener);
  };
}

export function subscribeMessageProtocolRxDataAcked(
  listener: (packet: AckedRxDataPacket) => void
): () => void {
  rxDataAckedListeners.add(listener);
  return () => {
    rxDataAckedListeners.delete(listener);
  };
}
