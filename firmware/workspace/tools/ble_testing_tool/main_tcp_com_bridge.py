#!/usr/bin/env python3
"""
mp_packet_framer.py

Deframes mp_packet_t packets from a serial (COM) byte stream.

On-wire format (little-endian):
  mp_packet_t = mp_packet_header_t + mp_packet_payload_t (variable length payload)

CRC:
  - CRC-16/IBM-3740
  - poly 0x1021, init 0xFFFF, refin=false, refout=false, xorout=0x0000
  - calculated over EVERYTHING except the first 2 CRC bytes (i.e. over frame[2:])
"""

from __future__ import annotations

import argparse
import dataclasses

import struct
from typing import Callable, List, Optional, Tuple
import serial
import socket
import sys


# Set this to match your build
MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN = 512  # update to your real MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN


# mp_packet_header_t:
#   uint16 pkt_crc
#   uint16 pkt_counter
#   uint32 session_id
#   uint8  pkt_type
#   uint8  status
HEADER_FMT = "<HHIBB"
HEADER_LEN = struct.calcsize(HEADER_FMT)  # 10

# mp_packet_payload_t fields before payload bytes:
#   uint8  type
#   uint8  ppi
#   uint16 pkt_payload_len
PAYLOAD_HDR_FMT = "<BBH"
PAYLOAD_HDR_LEN = struct.calcsize(PAYLOAD_HDR_FMT)  # 4

MIN_PACKET_LEN = HEADER_LEN + PAYLOAD_HDR_LEN


# -----------------------------
# CRC-16/IBM-3740 (aka CRC-16/AUTOSAR in many libs)
# poly 0x1021, init 0xFFFF, refin=false, refout=false, xorout=0x0000
# -----------------------------
def crc16_ibm_3740(data: bytes, poly: int = 0x1021, init: int = 0xFFFF) -> int:
    crc = init & 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ poly) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


# -----------------------------
# Data classes
# -----------------------------
@dataclasses.dataclass(frozen=True)
class MpPacketHeader:
    pkt_crc: int
    pkt_counter: int
    session_id: int
    pkt_type: int
    status: int


@dataclasses.dataclass(frozen=True)
class MpPacketPayload:
    type: int
    ppi: int
    pkt_payload_len: int
    payload: bytes


@dataclasses.dataclass(frozen=True)
class MpPacket:
    header: MpPacketHeader
    payload: MpPacketPayload


# -----------------------------
# Packing (for sending)
# CRC rule: crc over frame[2:] (everything except first 2 CRC bytes)
# -----------------------------
def pack_mp_packet(
    pkt_counter: int,
    session_id: int,
    pkt_type: int,
    status: int,
    payload_type: int,
    ppi: int,
    payload_bytes: bytes,
    crc_func: Callable[[bytes], int] = crc16_ibm_3740,
) -> bytes:
    if len(payload_bytes) > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN:
        raise ValueError(
            f"payload too large: {len(payload_bytes)} > {MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN}"
        )

    payload_len = len(payload_bytes)

    # Build with CRC placeholder
    header_wo_crc = struct.pack(
        HEADER_FMT,
        0x0000,  # placeholder
        pkt_counter & 0xFFFF,
        session_id & 0xFFFFFFFF,
        pkt_type & 0xFF,
        status & 0xFF,
    )
    payload_hdr = struct.pack(PAYLOAD_HDR_FMT, payload_type & 0xFF, ppi & 0xFF, payload_len & 0xFFFF)

    frame_wo_crc = header_wo_crc + payload_hdr + payload_bytes

    # Compute CRC over everything except first 2 bytes
    crc = crc_func(frame_wo_crc[2:]) & 0xFFFF

    # Insert CRC
    header_with_crc = struct.pack(
        HEADER_FMT,
        crc,
        pkt_counter & 0xFFFF,
        session_id & 0xFFFFFFFF,
        pkt_type & 0xFF,
        status & 0xFF,
    )
    return header_with_crc + payload_hdr + payload_bytes


# -----------------------------
# Parsing / deframing
# -----------------------------
def _try_parse_one(
    buf: bytearray,
    start: int,
    crc_func: Callable[[bytes], int],
) -> Optional[Tuple[MpPacket, int]]:
    remaining = len(buf) - start
    if remaining < MIN_PACKET_LEN:
        return None

    header_bytes = bytes(buf[start : start + HEADER_LEN])
    payload_hdr_bytes = bytes(buf[start + HEADER_LEN : start + HEADER_LEN + PAYLOAD_HDR_LEN])

    pkt_crc, pkt_counter, session_id, pkt_type, status = struct.unpack(HEADER_FMT, header_bytes)
    p_type, ppi, pkt_payload_len = struct.unpack(PAYLOAD_HDR_FMT, payload_hdr_bytes)

    # Sanity checks for resync scanning
    if pkt_payload_len > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN:
        return None

    total_len = HEADER_LEN + PAYLOAD_HDR_LEN + pkt_payload_len
    if remaining < total_len:
        return None

    frame = bytes(buf[start : start + total_len])

    # Validate CRC over everything except first 2 bytes
    computed = crc_func(frame[2:]) & 0xFFFF
    if pkt_crc != computed:
        return None

    payload_bytes = frame[HEADER_LEN + PAYLOAD_HDR_LEN : total_len]

    pkt = MpPacket(
        header=MpPacketHeader(
            pkt_crc=pkt_crc,
            pkt_counter=pkt_counter,
            session_id=session_id,
            pkt_type=pkt_type,
            status=status,
        ),
        payload=MpPacketPayload(
            type=p_type,
            ppi=ppi,
            pkt_payload_len=pkt_payload_len,
            payload=payload_bytes,
        ),
    )
    return pkt, total_len


def _candidate_total_len(buf: bytearray, start: int) -> Optional[int]:
    """Return candidate frame length at `start` when header/payload-len are readable and sane."""
    remaining = len(buf) - start
    if remaining < MIN_PACKET_LEN:
        return None

    payload_hdr_bytes = bytes(buf[start + HEADER_LEN : start + HEADER_LEN + PAYLOAD_HDR_LEN])
    _, _, pkt_payload_len = struct.unpack(PAYLOAD_HDR_FMT, payload_hdr_bytes)
    if pkt_payload_len > MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN:
        return None

    return HEADER_LEN + PAYLOAD_HDR_LEN + pkt_payload_len


class MpPacketFramer:
    """
    Deframe mp_packet_t from a serial byte stream.

    No sync/magic field -> resync by scanning forward until:
      - length looks sane, and
      - CRC validates
    """

    def __init__(self, crc_func: Callable[[bytes], int] = crc16_ibm_3740):
        self._buf = bytearray()
        self._crc_func = crc_func

    def feed(self, data: bytes) -> List[MpPacket]:
        self._buf.extend(data)
        out: List[MpPacket] = []

        while True:
            if len(self._buf) < MIN_PACKET_LEN:
                break

            parsed0 = _try_parse_one(self._buf, 0, self._crc_func)
            if parsed0 is not None:
                pkt, used = parsed0
                out.append(pkt)
                del self._buf[:used]
                continue

            # Track whether offset 0 is a plausible-but-incomplete frame start.
            total0 = _candidate_total_len(self._buf, 0)
            start0_partial = total0 is not None and len(self._buf) < total0

            # Scan forward for a valid start
            found_full = None
            found_partial = None
            scan_limit = len(self._buf) - MIN_PACKET_LEN
            for i in range(1, scan_limit + 1):
                total_i = _candidate_total_len(self._buf, i)
                if total_i is None:
                    continue

                if (len(self._buf) - i) < total_i:
                    if found_partial is None:
                        found_partial = i
                    continue

                parsed_i = _try_parse_one(self._buf, i, self._crc_func)
                if parsed_i is not None:
                    found_full = i
                    break

            if found_full is not None:
                del self._buf[:found_full]
                continue

            # Keep waiting for the offset-0 candidate if no complete frame was found ahead.
            if start0_partial:
                break

            if found_partial is not None:
                # Keep from the earliest plausible partial start.
                del self._buf[:found_partial]
                break

            # No plausible start found; keep a short tail for boundary-spanning packets.
            if len(self._buf) > (MIN_PACKET_LEN - 1):
                # Keep tail to avoid buffer blow-up and allow boundary-spanning packets
                self._buf[:] = self._buf[-(MIN_PACKET_LEN - 1) :]
            break

        return out


def _hexdump(b: bytes, max_len: int = 64) -> str:
    return (b[:max_len].hex(" ") + (" …" if len(b) > max_len else ""))


def run_reader(port: str, baud: int, timeout: float) -> None:
    framer = MpPacketFramer()

    # TCP client setup
    TCP_HOST = 'localhost'  # Change as needed
    TCP_PORT = 5000         # Change as needed
    tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        tcp_sock.connect((TCP_HOST, TCP_PORT))
        print(f"Connected to TCP server at {TCP_HOST}:{TCP_PORT}")
    except Exception as e:
        print(f"Failed to connect to TCP server: {e}")
        return

    import threading

    def tcp_to_com_thread(ser, tcp_sock):
        while True:
            try:
                data = tcp_sock.recv(4096)
                if not data:
                    print("[TCP->COM] TCP connection closed by server.")
                    break
                ser.write(data)
                ser.flush()
                print(f"[TCP->COM] Forwarded {len(data)} bytes to COM port: {data.hex(' ')}")
            except Exception as e:
                print(f"[TCP->COM] Error: {e}")
                break

    with serial.Serial(port, baudrate=baud, timeout=timeout) as ser:
        print(f"Opened {port} @ {baud}. Reading… (Ctrl+C to stop)")

        # Start TCP->COM forwarding thread
        t = threading.Thread(target=tcp_to_com_thread, args=(ser, tcp_sock), daemon=True)
        t.start()

        while True:
            chunk = ser.read(4096)
            if not chunk:
                continue

            for pkt in framer.feed(chunk):
                print(
                    f"crc=0x{pkt.header.pkt_crc:04X} "
                    f"ctr={pkt.header.pkt_counter} "
                    f"sess=0x{pkt.header.session_id:08X} "
                    f"pkt_type={pkt.header.pkt_type} status={pkt.header.status} "
                    f"pl_type={pkt.payload.type} ppi={pkt.payload.ppi} "
                    f"len={pkt.payload.pkt_payload_len} "
                    f"payload={_hexdump(pkt.payload.payload)}"
                )
                # Send the valid packet to the TCP server
                frame_bytes = pack_mp_packet(
                    pkt.header.pkt_counter,
                    pkt.header.session_id,
                    pkt.header.pkt_type,
                    pkt.header.status,
                    pkt.payload.type,
                    pkt.payload.ppi,
                    pkt.payload.payload,
                )
                try:
                    tcp_sock.sendall(frame_bytes)
                    print(f"[COM -> TCP] Sent {len(frame_bytes)} bytes to TCP server: {frame_bytes.hex(' ')}")
                except Exception as e:
                    print(f"[COM -> TCP] Failed to send to server: {e}")
                    sys.exit(1)


def main() -> int:
    ap = argparse.ArgumentParser(description="Deframe mp_packet_t from a serial stream.")
    ap.add_argument("--port", required=True, help="COM7 or /dev/ttyACM0 etc.")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=0.1)
    args = ap.parse_args()

    run_reader(args.port, args.baud, args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())