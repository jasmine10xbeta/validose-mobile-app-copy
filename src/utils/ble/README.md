# BLE Message Protocol (App Side)

This document explains how Message Protocol works in the app after the dual-characteristic firmware update.

---

## 1) Quick mental model

Message Protocol uses one custom service with two one-way characteristics:

- App writes DATA/ACK/NAK to `UUID_DATA_RX` (`0x1508`)
- App receives notifications from `UUID_DATA_TX` (`0x1509`)

Normal payload exchange is DATA/ACK/NAK only.

For each outgoing DATA packet:
- app queues frame
- peripheral replies ACK
- retries happen on timeout until max retries

---

## 2) BLE UUIDs

Service:
- `00001500-EB00-430A-A8FF-C7AD4211BF86`

Characteristics:
- `00001508-EB00-430A-A8FF-C7AD4211BF86` (`UUID_DATA_RX`, App -> device)
- `00001509-EB00-430A-A8FF-C7AD4211BF86` (`UUID_DATA_TX`, device -> App, Notify)

---

## 3) Packet format

All values are little-endian.

```text
Byte 0..1   : CRC16
Byte 2..3   : pkt_counter (uint16)
Byte 4..7   : session_id (uint32)
Byte 8      : pkt_type
Byte 9      : status
Byte 10     : payload.type (RQ/RE/PUSH)
Byte 11     : payload.ppi
Byte 12..13 : payload length (uint16)
Byte 14..N  : payload bytes
```

Packet types:
- `DATA = 0`
- `ACK = 1`
- `NAK = 2`

---

## 4) App files involved

Core protocol:
- `src/utils/ble/messageProtocol.ts`

PPI encoding/decoding:
- `src/utils/ble/messageProtocolPpi.ts`

Production BLE integration:
- `src/utils/ble/index.ts`

Debug screens:
- `src/app/(tabs)/home/ble-debug-console.tsx` (scan/connect VAL devices)
- `src/app/(tabs)/home/ble-debug.tsx` (protocol actions)
- `src/app/(tabs)/home/ble-debug-logs.tsx`

---

## 5) Debug quick actions currently exposed

- Time: `AD_TIME` (`RQ`, `RE`, `PUSH`)
- Dose schedule: `AD_DOSE_SCHEDULE` (`RQ`, `RE`, `PUSH`)
- Dock status: `AD_DOCK_STATUS` (`RQ`)
- Ring status: `AD_RING_STATUS` (`RQ`)
- Dock battery: `AD_DOCK_BATT_LEVEL_LOG` (`RQ`)
- Ring battery: `AD_RING_BATT_LEVEL_LOG` (`RQ`)

---

## 6) TX ready and protocol running (UI definitions)

In BLE Debug:

- **Message protocol running** means:
  - RX notifications are subscribed on `0x1509`
  - protocol process loop is active
  - protocol instance is initialized and can send/receive frames

- **TX ready** means:
  - current TX packet state is `COMPLETED` or `ABANDONED`
  - next DATA frame can be queued safely

If TX is not ready, action returns `TX_BUSY`.

---

## 7) Timeout / retry behavior

Configured in debug flow:

- ACK timeout: `1000 ms` (firmware `ACK_TIMEOUT_MS`)
- Max retries: `20` (firmware `MSG_PROT_MAX_RETRIES`)
- TX-ready wait timeout: `23000 ms` (debug guard around firmware worst-case abandon at ~21000 ms)
- TX-completion wait window: `23000 ms` (same rationale)
- Late ACK watch window: `23000 ms`

Common statuses:
- `SENT_ACKED`
- `SENT_WAITING_ACK`
- `TX_ABANDONED`
- `TX_BUSY`

---

## 8) Debug workflow

1. Open **BLE Debug Console**
2. Scan devices and filter by names starting with `VAL`
3. Tap **Connect** (bond attempt + connect + discovery)
4. Screen navigates to BLE Debug actions
5. Run quick actions and inspect payload preview + logs

---

## 9) nRF virtual peripheral notes

If testing with a virtual peripheral:

- Expose service `0x1500`
- Expose write characteristic `0x1508`
- Expose notify characteristic `0x1509`
- For each DATA write from app, notify a valid ACK frame back

---

## 10) Troubleshooting

### `call discoverServicesAndCharacteristics() first`
- Usually auto-recovered.
- If persistent: reconnect device and retry.

### `TX_BUSY`
- Previous TX has not reached `COMPLETED`/`ABANDONED` yet.
- Wait, or inspect logs for stuck ACK path.

### `SENT_WAITING_ACK`
- DATA sent; ACK not received in action window.
- Ensure peripheral notifies ACK for the sent frame.

### `CRC mismatch; dropping packet`
- Incoming notify frame is malformed.
- Rebuild full frame (CRC + header + payload), not payload-only.

---

## 11) FAQ

### Do request/response use different UUIDs now?
- Yes.
- App writes to `0x1508` and listens on `0x1509`.

### Who owns session control?
- App runs as master for session_id generation.
- App runtime keeps SYNC control flow disabled.
