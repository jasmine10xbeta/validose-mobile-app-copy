import { BleMessageProtocol, MessageProtocolInterface } from "../messageProtocol";

let messageProtocol: BleMessageProtocol | null = null;

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
