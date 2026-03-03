#!/usr/bin/env python3
"""Monitor incoming message protocol packets using the MessageProtocolRuntime"""

import argparse
from datetime import datetime, timezone
import logging
import os
import secrets
import signal
import sys
import time

from message_protocol_runtime import MessageProtocolRuntime
from utils import mp_enums, mp_structs


def _resolve_session_id() -> tuple[int, str]:
    raw = os.getenv("MP_HIL_SESSION_ID")
    if raw is not None and raw.strip() != "":
        session_id = int(raw, 0)
        if session_id < 0 or session_id > 0xFFFFFFFF:
            raise ValueError("MP_HIL_SESSION_ID must be in uint32 range (0..0xFFFFFFFF).")
        return session_id, "MP_HIL_SESSION_ID"

    return secrets.randbits(32), "random"


def _enum_name(enum_cls: type, value: int) -> str:
    try:
        return enum_cls(value).name
    except ValueError:
        return f"UNKNOWN({value})"


def _format_packet(packet: mp_structs.MpPacketPayload) -> str:
    payload = bytes(packet.payload[: packet.pkt_payload_len])
    ptype_name = _enum_name(mp_enums.PPI_TYPE, int(packet.type))
    ppi_name = _enum_name(mp_enums.PPI_AD, int(packet.ppi))
    now_utc = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
    return (
        f"[{now_utc}] "
        f"type={int(packet.type)}({ptype_name}) "
        f"ppi={int(packet.ppi)}({ppi_name}) "
        f"payload_len={int(packet.pkt_payload_len)} "
        f"payload_hex={payload.hex()}"
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Monitor incoming message protocol packets.")
    parser.add_argument("--host", default="localhost", help="Runtime host")
    parser.add_argument("--port", type=int, default=5000, help="Runtime port")

    role = parser.add_mutually_exclusive_group()
    role.add_argument("--master", dest="is_master", action="store_true", help="Run as master")
    role.add_argument("--slave", dest="is_master", action="store_false", help="Run as slave")
    parser.set_defaults(is_master=True)

    parser.add_argument("--process-interval-s", type=float, default=0.001, help="Runtime loop sleep interval")
    parser.add_argument("--poll-interval-s", type=float, default=0.01, help="Monitor RX polling interval")
    parser.add_argument(
        "--session-id", type=lambda v: int(v, 0), default=None, help="Session ID (uint32; decimal or 0x-prefixed hex)"
    )
    parser.add_argument(
        "--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"], help="Logging level"
    )

    return parser.parse_args()


def main() -> int:
    args = _parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    logger = logging.getLogger("mp.monitor")

    if args.session_id is not None:
        if args.session_id < 0 or args.session_id > 0xFFFFFFFF:
            raise ValueError("--session-id must be in uint32 range (0..0xFFFFFFFF).")
        session_id = args.session_id
        session_source = "--session-id"
    else:
        session_id, session_source = _resolve_session_id()

    logger.info("Using session_id=%#010x (source=%s)", session_id, session_source)
    logger.info(
        "Starting runtime host=%s port=%d is_master=%s session_id=%#010x",
        args.host,
        args.port,
        args.is_master,
        session_id,
    )

    runtime = MessageProtocolRuntime(
        serial_dev=args.host.encode("ascii"),
        port=args.port,
        master=args.is_master,
        session_id=session_id,
        process_interval_s=args.process_interval_s,
        logger=logging.getLogger("mp.monitor.runtime"),
    )

    stop_requested = False

    def _handle_signal(signum: int, _frame: object) -> None:
        nonlocal stop_requested
        stop_requested = True
        logger.info("Received signal %d; shutting down...", signum)

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    runtime.start()
    print("Monitoring RX packets. Press Ctrl+C to stop.")

    exit_code = 0
    try:
        while not stop_requested:
            if runtime.runtime_error is not None:
                logger.error("Runtime failed with error code: %d", runtime.runtime_error)
                exit_code = 1
                break

            packet = runtime.try_dequeue_rx_packet()
            if packet is None:
                time.sleep(args.poll_interval_s)
                continue

            print(_format_packet(packet), flush=True)
    finally:
        runtime.close()

    return exit_code


if __name__ == "__main__":
    err = main()
    sys.exit(err)
