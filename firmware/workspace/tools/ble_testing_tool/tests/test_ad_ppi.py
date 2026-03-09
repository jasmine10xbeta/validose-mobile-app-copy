import ctypes
import logging
import os
import secrets
import struct
import time
from typing import Callable

import pytest

from message_protocol_runtime import MessageProtocolRuntime
from utils import mp_enums
from utils import mp_structs

pytestmark = pytest.mark.hil


def _env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def _resolve_session_id() -> int:
    raw = os.getenv("MP_HIL_SESSION_ID")
    if raw is not None and raw.strip() != "":
        session_id = int(raw, 0)
        if session_id < 0 or session_id > 0xFFFFFFFF:
            raise ValueError("MP_HIL_SESSION_ID must be in uint32 range (0..0xFFFFFFFF).")
        return session_id

    # No explicit session provided: use a fresh random uint32 for this run.
    return secrets.randbits(32)


def _build_time_push_packet(unix_time_s: int) -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    packet.ppi = mp_enums.PPI_AD.PPI_AD_TIME
    packet.pkt_payload_len = 4
    packet.payload[:4] = struct.pack("<I", unix_time_s & 0xFFFFFFFF)
    return packet


def _build_time_request_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_TIME
    packet.pkt_payload_len = 0
    return packet


def _build_dock_status_request_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOCK_STATUS
    packet.pkt_payload_len = 0
    return packet


def _build_dose_schedule_push_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE
    packet.pkt_payload_len = mp_structs.DOSE_SCHEDULE_SIZE_BYTES

    dose_schedule = mp_structs.DoseSchedule()
    dose_schedule.medication_type = 0
    dose_schedule.dosage_mg = 500
    dose_schedule.temp_upper_limit_deg_c = 25
    dose_schedule.temp_lower_limit_deg_c = 0
    dose_schedule.temp_avg_window_duration_sec = 300
    dose_schedule.dose_days_bitfield = 0b10000000
    dose_schedule.dose_window_duration_minutes = 30
    dose_schedule.dose_window_count = 1
    dose_schedule.dose_window_start_times_minutes[0] = 12 * 60
    ctypes.memmove(packet.payload, ctypes.byref(dose_schedule), packet.pkt_payload_len)
    return packet


def _build_dose_schedule_request_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE
    packet.pkt_payload_len = 0
    return packet


def _is_time_response(packet: mp_structs.MpPacketPayload) -> bool:
    return packet.type == mp_enums.PPI_TYPE.PPI_TYPE_RE and packet.ppi == mp_enums.PPI_AD.PPI_AD_TIME and packet.pkt_payload_len == 4


def _is_dock_status_response(packet: mp_structs.MpPacketPayload) -> bool:
    return packet.type == mp_enums.PPI_TYPE.PPI_TYPE_RE and packet.ppi == mp_enums.PPI_AD.PPI_AD_DOCK_STATUS and packet.pkt_payload_len == mp_structs.DOCK_STATUS_T_SIZE_BYTES


def _is_dose_schedule_response(packet: mp_structs.MpPacketPayload) -> bool:
    return packet.type == mp_enums.PPI_TYPE.PPI_TYPE_RE and packet.ppi == mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE and packet.pkt_payload_len == mp_structs.DOSE_SCHEDULE_SIZE_BYTES


def _minutes_to_hhmm(total_minutes: int) -> str:
    hours = (total_minutes // 60) % 24
    minutes = total_minutes % 60
    return f"{hours:02d}:{minutes:02d}"


def _packet_log_direction(packet: mp_structs.MpPacketPayload) -> str:
    if packet.type == mp_enums.PPI_TYPE.PPI_TYPE_RE:
        return "Received"
    return "Sent"


def _log_packet(logger: logging.Logger, packet: mp_structs.MpPacketPayload) -> None:
    payload = bytes(packet.payload[: packet.pkt_payload_len])
    logger.info(
        "%s packet, type=%d, ppi=%d, payload_len=%d, payload_hex=%s",
        _packet_log_direction(packet),
        packet.type,
        packet.ppi,
        packet.pkt_payload_len,
        payload.hex(),
    )


def _log_dose_schedule(logger: logging.Logger, packet: mp_structs.MpPacketPayload) -> None:
    payload = bytes(packet.payload[: packet.pkt_payload_len])
    dose_schedule = mp_structs.DoseSchedule.from_buffer_copy(payload)
    window_count = min(dose_schedule.dose_window_count, mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY)
    dose_times = [int(dose_schedule.dose_window_start_times_minutes[i]) for i in range(window_count)]
    direction = _packet_log_direction(packet)

    _log_packet(logger, packet)
    logger.info(
        "%s dose_schedule, medication_type=%d, dosage_mg=%d, temp_upper_limit_deg_c=%d, temp_lower_limit_deg_c=%d, temp_avg_window_duration_sec=%d, dose_days_bitfield=0b%s, dose_window_duration_minutes=%d, dose_window_count=%d, dose_window_start_times_minutes=%s",
        direction,
        dose_schedule.medication_type,
        dose_schedule.dosage_mg,
        dose_schedule.temp_upper_limit_deg_c,
        dose_schedule.temp_lower_limit_deg_c,
        dose_schedule.temp_avg_window_duration_sec,
        format(dose_schedule.dose_days_bitfield, "08b"),
        dose_schedule.dose_window_duration_minutes,
        dose_schedule.dose_window_count,
        dose_times,
    )


def _wait_for_response(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    timeout_s: float,
    response_label: str,
    poll_interval_s: float = 0.005,
) -> mp_structs.MpPacketPayload:
    deadline = time.perf_counter() + timeout_s

    while time.perf_counter() < deadline:
        if runtime.runtime_error is not None:
            pytest.fail(f"Runtime failed while waiting for {response_label}: {runtime.runtime_error}")

        packet = runtime.try_dequeue_rx_packet()
        if packet is None:
            time.sleep(poll_interval_s)
            continue

        if predicate(packet):
            return packet

    pytest.fail(f"Did not receive {response_label} within {timeout_s:.1f}s.")


@pytest.fixture(scope="module")
def runtime() -> MessageProtocolRuntime:
    if not _env_bool("MP_HIL_ENABLED", False):
        pytest.skip("HIL disabled. Set MP_HIL_ENABLED=1 to run hardware-in-the-loop tests.")

    host = os.getenv("MP_HIL_HOST", "localhost").encode("ascii")
    port = int(os.getenv("MP_HIL_PORT", "5000"))
    is_master = _env_bool("MP_HIL_IS_MASTER", True)
    raw_session_id = os.getenv("MP_HIL_SESSION_ID")
    session_id = _resolve_session_id()
    session_id_source = "MP_HIL_SESSION_ID" if raw_session_id is not None and raw_session_id.strip() != "" else "random"
    process_interval_s = float(os.getenv("MP_HIL_PROCESS_INTERVAL_S", "0.001"))

    logger = logging.getLogger("pytest.hil.time_set")
    logger.info("Using HIL session_id=%#010x (source=%s)", session_id, session_id_source)
    logger.info(
        "Starting HIL runtime with host=%s port=%d is_master=%s session_id=%#010x",
        host.decode("ascii"),
        port,
        is_master,
        session_id,
    )
    rt = MessageProtocolRuntime(
        serial_dev=host,
        port=port,
        master=is_master,
        session_id=session_id,
        process_interval_s=process_interval_s,
        logger=logger,
    )
    rt.start()

    try:
        yield rt
    finally:
        rt.close()


@pytest.mark.order(1)
def test_ppi_ad_time(runtime: MessageProtocolRuntime) -> None:
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    max_time_delta_s = int(os.getenv("MP_HIL_TIME_TOLERANCE_S", "3"))
    pushed_unix_time_s = int(time.time()) & 0xFFFFFFFF
    push_packet = _build_time_push_packet(pushed_unix_time_s)
    request_packet = _build_time_request_packet()
    time_logger = logging.getLogger("pytest.hil.time")

    assert runtime.enqueue_tx_packet(push_packet), "Failed to enqueue time push packet."
    _log_packet(time_logger, push_packet)

    assert runtime.enqueue_tx_packet(request_packet), "Failed to enqueue time request packet."
    _log_packet(time_logger, request_packet)

    matched_response = _wait_for_response(runtime, _is_time_response, timeout_s, "time response")
    _log_packet(time_logger, matched_response)

    received_unix_time_s = struct.unpack("<I", bytes(matched_response.payload[:4]))[0]
    delta_s = received_unix_time_s - pushed_unix_time_s
    assert abs(delta_s) <= max_time_delta_s, (
        f"Time response out of expected range after push. pushed={pushed_unix_time_s} received={received_unix_time_s} delta_s={delta_s} max_time_delta_s={max_time_delta_s}"
    )


@pytest.mark.order(2)
def test_ppi_dock_status_req(runtime: MessageProtocolRuntime) -> None:
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    request_packet = _build_dock_status_request_packet()
    dock_status_logger = logging.getLogger("pytest.hil.dock_status_rq")

    assert runtime.enqueue_tx_packet(request_packet), "Failed to enqueue dock-status request packet."

    _log_packet(dock_status_logger, request_packet)

    matched_response = _wait_for_response(runtime, _is_dock_status_response, timeout_s, "dock-status response")
    _log_packet(dock_status_logger, matched_response)
    dock_status = mp_structs.DockStatus.from_buffer_copy(bytes(matched_response.payload[: mp_structs.DOCK_STATUS_T_SIZE_BYTES]))
    mac_str = ":".join(f"{byte:02X}" for byte in dock_status.mac)

    def _fmt_unix_ts(ts: int) -> str:
        if ts == 0:
            return "0 (unset)"
        return f"{ts} ({time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime(ts))})"

    dock_status_logger.info(
        "Dock status. hw=%d.%d.%d, fw=%d.%d.%d, mac=%s, timestamp=%s, ship_mode_exit=%s, uptime_s=%d",
        dock_status.hardware_version.major,
        dock_status.hardware_version.minor,
        dock_status.hardware_version.patch,
        dock_status.firmware_version.major,
        dock_status.firmware_version.minor,
        dock_status.firmware_version.patch,
        mac_str,
        _fmt_unix_ts(dock_status.timestamp_unix_s),
        _fmt_unix_ts(dock_status.ship_mode_exit_timestamp_unix),
        dock_status.uptime_s,
    )


@pytest.mark.order(3)
def test_ppi_dose_schedule(runtime: MessageProtocolRuntime) -> None:
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    push_packet = _build_dose_schedule_push_packet()
    dose_schedule_logger = logging.getLogger("pytest.hil.dose_schedule")

    assert runtime.enqueue_tx_packet(push_packet), "Failed to enqueue dose-schedule push packet."

    _log_dose_schedule(dose_schedule_logger, push_packet)

    request_packet = _build_dose_schedule_request_packet()

    assert runtime.enqueue_tx_packet(request_packet), "Failed to enqueue dose-schedule request packet."

    matched_response = _wait_for_response(runtime, _is_dose_schedule_response, timeout_s, "dose-schedule response")
    _log_dose_schedule(dose_schedule_logger, matched_response)

    expected_payload = bytes(push_packet.payload[: push_packet.pkt_payload_len])
    actual_payload = bytes(matched_response.payload[: matched_response.pkt_payload_len])

    assert actual_payload == expected_payload, f"Dose-schedule response payload mismatch. expected={expected_payload.hex()} actual={actual_payload.hex()}"
