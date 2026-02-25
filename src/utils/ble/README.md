# BLE Message Protocol (App Side)

This document explains how BLE message protocol works in the app.

It covers:
- packet format and protocol rules
- app-side implementation files
- BLE Debug screen behavior
- testing with nRF app (virtual peripheral)
- testing with real firmware
- common errors and how to fix them

---

## 1) Quick mental model

Think of this protocol as a small "mail system" over one BLE characteristic:

- App sends a framed packet (`TX`) to firmware.
- Firmware sends an `ACK` for reliability.
- Firmware can also send framed data packets (`RX`) back to app.
- Every packet has:
  - session id
  - packet counter
  - packet type
  - payload type (`RQ` / `RE` / `PUSH`)
  - PPI id (what the payload means)
  - CRC (integrity check)

If CRC or framing is invalid, app drops packet.

---

## 2) Glossary

- `TX`: Transmit (app sends out)
- `RX`: Receive (app gets incoming)
- `PPI`: Protocol Payload Identifier (semantic meaning of payload)
- `RQ`: Request (type `0`)
- `RE`: Response (type `1`)
- `PUSH`: Update/event push (type `2`)
- `SYNC_START` / `SYNC_ACK`: session handshake packets
- `ACK`: acknowledgement packet for reliability
- `CRC`: checksum to detect corrupted packets

---

## 3) App files involved

Core protocol:
- `src/utils/ble/messageProtocol.ts`

PPI definitions and encoding/decoding:
- `src/utils/ble/messageProtocolPpi.ts`

App BLE integration (production flow):
- `src/utils/ble/index.ts`

Debug UI:
- `src/app/(tabs)/home/ble-debug.tsx`
- `src/app/(tabs)/home/ble-debug/constants.ts`

BLE native wrapper (with auto discovery retry):
- `modules/tenx-mdk-ble-rn-library/src/index.tsx`

nRF helper script:
- `scripts/mp-nrf-helper.js`

---

## 4) BLE UUIDs used

Service:
- `00001500-EB00-430A-A8FF-C7AD4211BF86`

Message protocol characteristic:
- `00001505-EB00-430A-A8FF-C7AD4211BF86`

For debug/nRF testing, this characteristic must support:
- `Write`
- `Notify`

---

## 5) Packet format (wire/frame)

All values are little-endian where multi-byte.

```
Byte 0..1   : CRC16
Byte 2..3   : pkt_counter (uint16)
Byte 4..7   : session_id (uint32)
Byte 8      : pkt_type (DATA/ACK/NAK/SYNC_*)
Byte 9      : status
Byte 10     : payload.type (RQ/RE/PUSH)
Byte 11     : payload.ppi
Byte 12..13 : payload length (uint16)
Byte 14..N  : payload bytes
```

Packet types used:
- `DATA = 0`
- `ACK = 1`
- `NAK = 2`
- `SYNC_START = 3`
- `SYNC_ACK = 4`
- `SYNC_MISMATCH = 5`

Payload types:
- `RQ = 0`
- `RE = 1`
- `PUSH = 2`

---

## 6) PPI contracts currently used in debug quick actions

### `AD_TIME` (`ppi=3`)
- `RQ` (`type=0`): `len=0`
- `RE` (`type=1`): `len=4` (`uint32` unix seconds)
- `PUSH` (`type=2`): `len=4` (`uint32` unix seconds)

### `AD_DOSE_SCHEDULE` (`ppi=0`)
- `RQ`: `len=0`
- `RE`: `len=30` (`dose_schedule_t`)
- `PUSH`: `len=30` (`dose_schedule_t`)

Length checks are enforced by app-side PPI helpers before send.

---

## 7) Sync and session behavior

There are 2 operating styles in BLE Debug:

### A) Full Sync mode (`Manual nRF Mode = OFF`)
- App acts as master.
- App starts with `SYNC_START`.
- Peripheral must reply `SYNC_ACK` with same `session_id`.
- Then app sends DATA packets.
- Peripheral must send ACK for each DATA.

### B) Manual nRF mode (`Manual nRF Mode = ON`)
- Sync start/ack handshake is skipped.
- App sends DATA directly.
- Peripheral still must ACK each DATA packet.
- Debug implementation uses relaxed ACK matching for easier virtual-peripheral testing.

---

## 8) Timeout/retry behavior (BLE Debug)

Values from debug config:

Common:
- TX-ready wait: `15000 ms`
- Late ACK watch: `90000 ms`

Full Sync mode:
- ACK timeout: `6000 ms`
- max retries: `3`
- completion wait in UI action: `7000 ms`

Manual nRF mode:
- ACK timeout: `30000 ms`
- max retries: `1`
- completion wait in UI action: `60000 ms`

Status meanings you will see:
- `WAITING_SYNC_ACK`: waiting for handshake ack
- `SENT_WAITING_ACK`: DATA sent, ACK not seen yet in action window
- `SENT_ACKED`: ACK received
- `TX_ABANDONED`: retries exhausted / send abandoned
- `TX_BUSY`: protocol currently not sendable

---

## 9) Discovery behavior (important)

You should not need to manually call discovery.

Discovery is handled automatically in two places:
- BLE Debug screen tries discovery before protocol actions.
- BLE wrapper retries read/write/subscribe once after auto-calling `discoverServicesAndCharacteristics()` when that specific error appears.

So this error should be auto-recovered:
- `call discoverServicesAndCharacteristics() first`

---

## 10) BLE Debug screen with nRF app (virtual peripheral)

### Step 1: Configure nRF virtual peripheral
Create:
- Service: `00001500-EB00-430A-A8FF-C7AD4211BF86`
- Characteristic: `00001505-EB00-430A-A8FF-C7AD4211BF86`
- Properties: `Write + Notify`

### Step 2: Connect app and enable Manual mode
In app BLE Debug:
1. Connect
2. Turn `Manual nRF Mode` ON

### Step 3: Send an action (example: Time Request)
Tap `Time Request`.
App sends MP DATA with:
- `type=RQ(0)`
- `ppi=3`
- `len=0`

### Step 4: Send ACK from nRF side
Copy incoming hex write from nRF logs, then run:

```bash
node scripts/mp-nrf-helper.js ack "<incoming_hex>"
```

Take `response_hex` and send it as a notification on same MP characteristic.

### Step 5 (optional but recommended): send a Time Response DATA
After ACK, you can send a DATA notify back to app with:
- `type=RE(1)`
- `ppi=3`
- `len=4`
- payload = unix time (`uint32` LE)

Example RE frame (session `0`, counter `2`, payload `69319d69`):
- `b89302000000000000010103040069319d69`

Note: this value changes across requests (counter/session/CRC/time).

### Step 6: Verify in app
You should see:
- TX status updates (`SENT_ACKED` etc.)
- Last Incoming Update decoded in quick actions payload section

---

## 11) BLE Debug screen with real firmware

Use Full Sync mode (Manual OFF) unless firmware test plan says otherwise.

Flow is automatic:
1. Connect
2. Keep Manual mode OFF
3. Tap quick action
4. App sends `SYNC_START` (if needed)
5. Firmware responds `SYNC_ACK`
6. App sends DATA packet
7. Firmware responds `ACK`
8. Firmware may send `PUSH` updates; app decodes and logs them

No manual hex notifications are needed with real firmware.

---

## 12) Using nRF helper script

Current helper commands:

```bash
# Build ACK or SYNC_ACK from incoming frame
node scripts/mp-nrf-helper.js ack "<incoming_frame_hex>"

# Build PUSH notifications for selected PPIs
node scripts/mp-nrf-helper.js notify <dose|dock_batt|ring_batt|dock_dbg|ring_dbg> [flags]
```

If you need custom `AD_TIME RE` frame generation, you can either:
- construct manually from format above, or
- use any local script to build CRC + frame fields.

---

## 13) Common troubleshooting

### `call discoverServicesAndCharacteristics() first`
- Usually auto-recovered now.
- If still seen: disconnect/reconnect and retry once.

### `WAITING_SYNC_ACK`
- You are in Full Sync mode and peripheral did not send `SYNC_ACK`.
- Either send valid `SYNC_ACK` or turn Manual mode ON for virtual testing.

### `SENT_WAITING_ACK`
- DATA was sent but ACK not received in action window.
- Send ACK with matching `session_id` + `pkt_counter`.

### `CRC mismatch; dropping packet`
- Incoming notify frame has invalid CRC or framing.
- Rebuild full frame correctly (do not send payload-only bytes).

### ACK seems late / not applied
- If protocol got reset (disconnect, mode toggle, clear subs), old ACK can no longer complete previous TX.
- Send ACK while same protocol session is still active.

### Duplicate notify ignored
- Exact duplicate raw frame may be suppressed.
- Change `pkt_counter` and/or payload.

---

## 14) FAQ

### Does ACK expire?
- Practically yes (timeouts + retries + potential protocol reset).

### Can I send two `RE` data packets for time?
- Yes.
- If both are valid and not exact duplicates, app can process both.
- UI "Last Incoming Update" shows latest one; logs can show each event.

### Do I need different characteristic UUIDs for request/response?
- No.
- Same MP characteristic carries all packet types (`SYNC`, `DATA`, `ACK`, etc.).

---

## 15) Practical test checklist

Before testing:
- BLE connected
- Correct service/characteristic UUIDs
- MP characteristic has Write + Notify
- Mode selected intentionally (Manual ON for nRF virtual, OFF for real firmware)

For every app DATA write in virtual testing:
- Send ACK back
- Keep `session_id` and `pkt_counter` consistent with incoming request
- Ensure CRC/framing valid

