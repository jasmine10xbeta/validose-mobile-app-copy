"""Hardware-in-the-loop regression tests for App<->Dock AD PPIs.

This module verifies that the target responds with correct packet metadata and
payload bytes for AD request/response PPIs, and that expected asynchronous push
packets are emitted after the development command seeds mock dock data.

The tests are intentionally ordered: early tests configure state that later push
tests depend on.
"""

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

# Mark every test in this module as hardware-in-the-loop.
pytestmark = pytest.mark.hil


# -----------------------------------------------------------------------------
# Environment and packet helper utilities
# -----------------------------------------------------------------------------
def _env_bool(name: str, default: bool) -> bool:
    """Read a boolean environment variable with a sensible default."""
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def _resolve_session_id() -> int:
    """Return a uint32 session id from env, or generate one when unset."""
    raw = os.getenv("MP_HIL_SESSION_ID")
    if raw is not None and raw.strip() != "":
        session_id = int(raw, 0)
        if session_id < 0 or session_id > 0xFFFFFFFF:
            raise ValueError("MP_HIL_SESSION_ID must be in uint32 range (0..0xFFFFFFFF).")
        return session_id
    return secrets.randbits(32)


def _packet_payload(packet: mp_structs.MpPacketPayload) -> bytes:
    """Extract only the valid payload bytes from an MP packet."""
    return bytes(packet.payload[: packet.pkt_payload_len])


def _ctypes_to_readable(value: object) -> object:
    """Convert ctypes values into log-friendly Python primitives."""
    if hasattr(value, "_fields_"):
        result: dict[str, object] = {}
        for field_name, _ in value._fields_:
            result[field_name] = _ctypes_to_readable(getattr(value, field_name))
        return result

    if isinstance(value, ctypes.Array):
        elem_type = getattr(value, "_type_", None)
        if elem_type is ctypes.c_uint8:
            return bytes(value).hex()
        return [_ctypes_to_readable(v) for v in value]

    if isinstance(value, (int, str, bool, float)):
        return value

    try:
        return int(value)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return str(value)


def _log_expected_vs_received_packet(
    logger: logging.Logger,
    expected_type: int,
    expected_ppi: int,
    expected_payload_len: int,
    received_packet: mp_structs.MpPacketPayload,
) -> None:
    """Log expected packet metadata alongside the packet that arrived."""
    logger.info(
        "Expected packet: type=%d, ppi=%d, payload_len=%d",
        expected_type,
        expected_ppi,
        expected_payload_len,
    )
    logger.info(
        "Received packet: type=%d, ppi=%d, payload_len=%d",
        int(received_packet.type),
        int(received_packet.ppi),
        int(received_packet.pkt_payload_len),
    )


def _log_expected_vs_received_struct(
    logger: logging.Logger,
    struct_type: type[ctypes.Structure],
    expected_payload: bytes,
    received_payload: bytes,
) -> None:
    """Log a decoded expected struct and decoded received struct for diffing."""
    expected_struct = struct_type.from_buffer_copy(expected_payload)
    received_struct = struct_type.from_buffer_copy(received_payload)
    logger.info("Expected %s: %s", struct_type.__name__, _ctypes_to_readable(expected_struct))
    logger.info("Received %s: %s", struct_type.__name__, _ctypes_to_readable(received_struct))


def _log_received_struct(
    logger: logging.Logger,
    struct_type: type[ctypes.Structure],
    received_payload: bytes,
) -> None:
    """Decode a struct payload and emit it to the test logger."""
    received_struct = struct_type.from_buffer_copy(received_payload)
    logger.info("Received %s: %s", struct_type.__name__, _ctypes_to_readable(received_struct))


def _is_packet(packet: mp_structs.MpPacketPayload, pkt_type: int, ppi: int, payload_len: int) -> bool:
    """Return True when packet type, PPI id, and payload length all match."""
    return packet.type == pkt_type and packet.ppi == ppi and packet.pkt_payload_len == payload_len


def _wait_for_packet(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    timeout_s: float,
    response_label: str,
    poll_interval_s: float = 0.001,
) -> mp_structs.MpPacketPayload:
    """Poll runtime RX queue until a packet satisfying ``predicate`` is received."""
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


def _wait_for_expected_payload_packet(
    runtime: MessageProtocolRuntime,
    packet_predicate: Callable[[mp_structs.MpPacketPayload], bool],
    expected_payload: bytes,
    timeout_s: float,
    response_label: str,
    poll_interval_s: float = 0.001,
) -> mp_structs.MpPacketPayload:
    """Wait for a packet that matches both packet metadata and exact payload."""
    deadline = time.perf_counter() + timeout_s

    while time.perf_counter() < deadline:
        if runtime.runtime_error is not None:
            pytest.fail(f"Runtime failed while waiting for {response_label}: {runtime.runtime_error}")

        packet = runtime.try_dequeue_rx_packet()
        if packet is None:
            time.sleep(poll_interval_s)
            continue

        if packet_predicate(packet) and _packet_payload(packet) == expected_payload:
            return packet

    pytest.fail(f"Did not receive {response_label} with expected payload within {timeout_s:.1f}s.")


def _build_time_push_packet(unix_time_s: int) -> mp_structs.MpPacketPayload:
    """Build a TIME push packet containing a little-endian uint32 unix timestamp."""
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    packet.ppi = mp_enums.PPI_AD.PPI_AD_TIME
    packet.pkt_payload_len = 4
    packet.payload[:4] = struct.pack("<I", unix_time_s & 0xFFFFFFFF)
    return packet


def _build_time_request_packet() -> mp_structs.MpPacketPayload:
    """Build a TIME request packet (no payload)."""
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_TIME
    packet.pkt_payload_len = 0
    return packet


def _build_dock_status_request_packet() -> mp_structs.MpPacketPayload:
    """Build a DOCK_STATUS request packet."""
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOCK_STATUS
    packet.pkt_payload_len = 0
    return packet


def _build_ring_status_request_packet() -> mp_structs.MpPacketPayload:
    """Build a RING_STATUS request packet."""
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_RING_STATUS
    packet.pkt_payload_len = 0
    return packet


def _build_dose_schedule_push_packet() -> mp_structs.MpPacketPayload:
    """Build a deterministic DOSE_SCHEDULE push packet used for round-trip checks."""
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

    # Copy the packed struct bytes directly into the transport payload buffer.
    ctypes.memmove(packet.payload, ctypes.byref(dose_schedule), packet.pkt_payload_len)
    return packet


def _build_dose_schedule_request_packet() -> mp_structs.MpPacketPayload:
    """Build a DOSE_SCHEDULE request packet."""
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE
    packet.pkt_payload_len = 0
    return packet


def _build_dev_cmd_dock_push_data_request_packet() -> mp_structs.MpPacketPayload:
    """Build DEV_CMD request asking firmware to emit representative dock push data."""
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DEVELOPMENT_CMD
    packet.payload[0] = mp_enums.APP2DOCK_COMMANDS.APP2DOCK_COMMANDS_POPULATE_DOCK_PUSH_DATA
    packet.pkt_payload_len = 1
    return packet


# -----------------------------------------------------------------------------
# Packet match predicates
# -----------------------------------------------------------------------------
def _is_time_response(packet: mp_structs.MpPacketPayload) -> bool:
    """Match TIME response packets."""
    return _is_packet(packet, mp_enums.PPI_TYPE.PPI_TYPE_RE, mp_enums.PPI_AD.PPI_AD_TIME, 4)


def _is_dock_status_response(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_STATUS response packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_DOCK_STATUS,
        mp_structs.DOCK_STATUS_T_SIZE_BYTES,
    )


def _is_ring_status_response(packet: mp_structs.MpPacketPayload) -> bool:
    """Match RING_STATUS response packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_RING_STATUS,
        mp_structs.RING_STATUS_T_SIZE_BYTES,
    )


def _is_dose_schedule_response(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOSE_SCHEDULE response packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE,
        mp_structs.DOSE_SCHEDULE_SIZE_BYTES,
    )


def _is_dev_cmd_dock_push_data_response(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DEVELOPMENT_CMD response packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_DEVELOPMENT_CMD,
        1,
    )


def _is_dose_event_report_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOSE_EVENT_REPORT push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOSE_EVENT_REPORT,
        mp_structs.DOSE_EVENT_T_SIZE_BYTES,
    )


def _is_ring_docked_status_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match RING_DOCKED_STATUS push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_DOCKED_STATUS,
        mp_structs.RING_DOCKED_STATUS_T_SIZE_BYTES,
    )


def _is_dock_weight_measurement_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_WEIGHT_LOG push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_WEIGHT_LOG,
        mp_structs.DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES,
    )


def _is_temperature_log_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_TEMP_LOG push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_TEMP_LOG,
        mp_structs.TEMPERATURE_LOG_T_SIZE_BYTES,
    )


def _is_ring_battery_level_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match RING_BATT_LEVEL_LOG push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_BATT_LEVEL_LOG,
        mp_structs.BATTERY_LEVEL_T_SIZE_BYTES,
    )


def _is_dock_battery_level_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_BATT_LEVEL_LOG push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_BATT_LEVEL_LOG,
        mp_structs.BATTERY_LEVEL_T_SIZE_BYTES,
    )


def _is_dock_charge_status_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_CHARGE_STATUS push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_CHARGE_STATUS,
        mp_structs.DOCK_CHARGE_STATUS_T_SIZE_BYTES,
    )


def _is_ring_status_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match RING_STATUS push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_STATUS,
        mp_structs.RING_STATUS_T_SIZE_BYTES,
    )


def _is_dock_status_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_STATUS push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_STATUS,
        mp_structs.DOCK_STATUS_T_SIZE_BYTES,
    )


def _is_dock_debug_log_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match DOCK_DEBUG_LOG push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_DEBUG_LOG,
        mp_structs.RAW_DEBUG_LOG_T_SIZE_BYTES,
    )


def _is_ring_debug_log_push(packet: mp_structs.MpPacketPayload) -> bool:
    """Match RING_DEBUG_LOG push packets."""
    return _is_packet(
        packet,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_DEBUG_LOG,
        mp_structs.RAW_DEBUG_LOG_T_SIZE_BYTES,
    )


# -----------------------------------------------------------------------------
# Runtime fixture
# -----------------------------------------------------------------------------
@pytest.fixture(scope="module")
def runtime() -> MessageProtocolRuntime:
    """Create and manage a shared HIL runtime for all tests in this module."""
    if not _env_bool("MP_HIL_ENABLED", False):
        pytest.skip("HIL disabled. Set MP_HIL_ENABLED=1 to run hardware-in-the-loop tests.")

    host = os.getenv("MP_HIL_HOST", "localhost").encode("ascii")
    port = int(os.getenv("MP_HIL_PORT", "5000"))
    is_master = _env_bool("MP_HIL_IS_MASTER", True)
    process_interval_s = float(os.getenv("MP_HIL_PROCESS_INTERVAL_S", "0.001"))
    queue_size = int(os.getenv("MP_HIL_QUEUE_SIZE", "200"))
    session_id = _resolve_session_id()

    rt = MessageProtocolRuntime(
        serial_dev=host,
        port=port,
        master=is_master,
        session_id=session_id,
        queue_size=queue_size,
        process_interval_s=process_interval_s,
    )
    rt.start()

    try:
        yield rt
    finally:
        rt.close()


# -----------------------------------------------------------------------------
# Ordered AD PPI scenarios
# -----------------------------------------------------------------------------
@pytest.mark.order(1)
def test_ppi_ad_time(runtime: MessageProtocolRuntime) -> None:
    """Push current unix time and verify TIME request returns that value."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    max_time_delta_s = int(os.getenv("MP_HIL_TIME_TOLERANCE_S", "3"))
    logger = logging.getLogger("pytest.hil.time")

    pushed_unix_time_s = int(time.time()) & 0xFFFFFFFF
    assert runtime.enqueue_tx_packet(_build_time_push_packet(pushed_unix_time_s)), "Failed to enqueue time push packet."
    assert runtime.enqueue_tx_packet(_build_time_request_packet()), "Failed to enqueue time request packet."

    response = _wait_for_packet(runtime, _is_time_response, timeout_s, "time response")
    received_unix_time_s = struct.unpack("<I", _packet_payload(response))[0]
    delta_s = received_unix_time_s - pushed_unix_time_s
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_TIME,
        4,
        response,
    )
    logger.info("Expected time_unix_s=%d", pushed_unix_time_s)
    logger.info("Received time_unix_s=%d", received_unix_time_s)

    assert abs(delta_s) <= max_time_delta_s, (
        f"Time response out of expected range after push. pushed={pushed_unix_time_s} "
        f"received={received_unix_time_s} delta_s={delta_s} max_time_delta_s={max_time_delta_s}"
    )


@pytest.mark.order(2)
def test_ppi_dock_status_req(runtime: MessageProtocolRuntime) -> None:
    """Request DOCK_STATUS and log the decoded struct for inspection."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dock_status_req")

    assert runtime.enqueue_tx_packet(_build_dock_status_request_packet()), "Failed to enqueue dock-status request packet."
    response = _wait_for_packet(runtime, _is_dock_status_response, timeout_s, "dock-status response")
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_DOCK_STATUS,
        mp_structs.DOCK_STATUS_T_SIZE_BYTES,
        response,
    )
    _log_received_struct(logger, mp_structs.DockStatus, _packet_payload(response))


@pytest.mark.order(3)
def test_ppi_ring_status_req(runtime: MessageProtocolRuntime) -> None:
    """Request RING_STATUS and log the decoded struct for inspection."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.ring_status_req")

    assert runtime.enqueue_tx_packet(_build_ring_status_request_packet()), "Failed to enqueue ring-status request packet."
    response = _wait_for_packet(runtime, _is_ring_status_response, timeout_s, "ring-status response")
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_RING_STATUS,
        mp_structs.RING_STATUS_T_SIZE_BYTES,
        response,
    )
    _log_received_struct(logger, mp_structs.RingStatus, _packet_payload(response))


@pytest.mark.order(4)
def test_ppi_dose_schedule(runtime: MessageProtocolRuntime) -> None:
    """Push a deterministic dose schedule and verify an exact round-trip payload."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dose_schedule")

    push_packet = _build_dose_schedule_push_packet()
    assert runtime.enqueue_tx_packet(push_packet), "Failed to enqueue dose-schedule push packet."
    assert runtime.enqueue_tx_packet(_build_dose_schedule_request_packet()), "Failed to enqueue dose-schedule request packet."

    expected_payload = bytes(push_packet.payload[: push_packet.pkt_payload_len])
    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dose_schedule_response,
        expected_payload=expected_payload,
        timeout_s=timeout_s,
        response_label="dose-schedule response",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE,
        mp_structs.DOSE_SCHEDULE_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.DoseSchedule, expected_payload, _packet_payload(response))


@pytest.mark.order(5)
def test_ppi_dev_cmd_populate_dock_push_data(runtime: MessageProtocolRuntime) -> None:
    """Trigger development command that primes downstream push-data scenarios."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dev_cmd_populate")

    assert runtime.enqueue_tx_packet(
        _build_dev_cmd_dock_push_data_request_packet()
    ), "Failed to enqueue DEV CMD dock push data packet."

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dev_cmd_dock_push_data_response,
        expected_payload=bytes((1,)),
        timeout_s=timeout_s,
        response_label="DEV CMD dock push data response",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_RE,
        mp_enums.PPI_AD.PPI_AD_DEVELOPMENT_CMD,
        1,
        response,
    )
    logger.info("Expected development command response payload=%s", bytes((1,)).hex())
    logger.info("Received development command response payload=%s", _packet_payload(response).hex())


@pytest.mark.order(6)
def test_ppi_dose_event_push(runtime: MessageProtocolRuntime) -> None:
    """Verify DOSE_EVENT_REPORT push payload bytes against known fixture data."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dose_event_push")

    # Values are expected from the development command executed in order(5).
    expected = mp_structs.DoseEvent()
    expected.event_id.days_since_epoch = 12345
    expected.event_id.event_ctr = 20
    expected.start_timestamp_unix_s = 1697049600
    expected.duration_s = 240
    expected.dose_completed_in_time = 1
    expected.tilt_count = 3

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dose_event_report_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="dose event push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOSE_EVENT_REPORT,
        mp_structs.DOSE_EVENT_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.DoseEvent, bytes(expected), _packet_payload(response))


@pytest.mark.order(7)
def test_ppi_ring_docked_status_push(runtime: MessageProtocolRuntime) -> None:
    """Verify RING_DOCKED_STATUS push bytes and decode for readable logging."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.ring_docked_status_push")

    expected = mp_structs.RingDockedStatus()
    expected.timestamp_unix_s = 1697049600
    expected.docked_status = 1
    expected.ring_nfc_id[:] = (0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05)
    expected.medication_nfc_id[:] = (0xBA, 0xAD, 0xF0, 0x0D, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05)

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_ring_docked_status_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="ring docked status push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_DOCKED_STATUS,
        mp_structs.RING_DOCKED_STATUS_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.RingDockedStatus, bytes(expected), _packet_payload(response))


@pytest.mark.order(8)
def test_ppi_dock_weight_measurement_push(runtime: MessageProtocolRuntime) -> None:
    """Verify DOCK_WEIGHT_LOG push payload exactly matches expected struct bytes."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dock_weight_measurement_push")

    expected = mp_structs.DockWeightMeasurement()
    expected.weight_mg = 50000
    expected.total_dispensed_mg = 10
    expected.std_dev = 100
    expected.temperature_deg_c_div10 = 250
    expected.timestamp_unix_s = 1697049600

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dock_weight_measurement_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="dock weight measurement push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_WEIGHT_LOG,
        mp_structs.DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.DockWeightMeasurement, bytes(expected), _packet_payload(response))


@pytest.mark.order(9)
def test_ppi_temperature_log_push(runtime: MessageProtocolRuntime) -> None:
    """Verify DOCK_TEMP_LOG push payload exactly matches expected struct bytes."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.temperature_log_push")

    expected = mp_structs.TemperatureLog()
    expected.temperature_deg_c = 25
    expected.timestamp_unix_s = 1697049600

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_temperature_log_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="temperature log push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_TEMP_LOG,
        mp_structs.TEMPERATURE_LOG_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.TemperatureLog, bytes(expected), _packet_payload(response))


@pytest.mark.order(10)
def test_ppi_ring_battery_level_push(runtime: MessageProtocolRuntime) -> None:
    """Verify RING_BATT_LEVEL_LOG push payload exactly matches expected bytes."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.ring_battery_level_push")

    expected = mp_structs.BatteryLevel()
    expected.timestamp_unix_s = 1697049600
    expected.battery_level = 85

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_ring_battery_level_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="ring battery level push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_BATT_LEVEL_LOG,
        mp_structs.BATTERY_LEVEL_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.BatteryLevel, bytes(expected), _packet_payload(response))


@pytest.mark.order(11)
def test_ppi_dock_battery_level_push(runtime: MessageProtocolRuntime) -> None:
    """Verify DOCK_BATT_LEVEL_LOG push payload exactly matches expected bytes."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dock_battery_level_push")

    expected = mp_structs.BatteryLevel()
    expected.timestamp_unix_s = 1697049600
    expected.battery_level = 70

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dock_battery_level_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="dock battery level push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_BATT_LEVEL_LOG,
        mp_structs.BATTERY_LEVEL_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.BatteryLevel, bytes(expected), _packet_payload(response))


@pytest.mark.order(12)
def test_ppi_dock_charge_status_push(runtime: MessageProtocolRuntime) -> None:
    """Verify DOCK_CHARGE_STATUS push payload exactly matches expected bytes."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dock_charge_status_push")

    expected = mp_structs.DockChargeStatus()
    expected.timestamp_unix_s = 1772616072
    expected.charge_status = mp_enums.BATTERY_STATE.BATTERY_STATE_CHARGING

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dock_charge_status_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="dock charge status push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_CHARGE_STATUS,
        mp_structs.DOCK_CHARGE_STATUS_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.DockChargeStatus, bytes(expected), _packet_payload(response))


@pytest.mark.order(13)
def test_ppi_ring_status_push(runtime: MessageProtocolRuntime) -> None:
    """Verify full RING_STATUS push payload against a fixed expected snapshot."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "30.0"))
    logger = logging.getLogger("pytest.hil.ring_status_push")

    expected = mp_structs.RingStatus()
    expected.hardware_version.major = 1
    expected.hardware_version.minor = 0
    expected.hardware_version.patch = 1
    expected.firmware_version.major = 1
    expected.firmware_version.minor = 0
    expected.firmware_version.patch = 1
    expected.mac[:] = (0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05)
    expected.timestamp_unix_s = 1772616496
    expected.ship_mode_exit_timestamp_unix = 1697049600
    expected.battery_charge_status = mp_enums.BATTERY_STATE.BATTERY_STATE_CHARGING
    expected.uptime_s = 3600
    expected.temperature_celsius = 25
    expected.dose_fifo_used_percent = 50
    expected.battery_fifo_used_percent = 30
    expected.imu_fifo_used_percent = 20
    expected.error_fifo_used_percent = 10
    expected.dose_fifo_used_percent_watermark = 80
    expected.battery_fifo_used_percent_watermark = 60
    expected.imu_fifo_used_percent_watermark = 40
    expected.error_fifo_used_percent_watermark = 20
    expected.battery_sample_frequency_millihz = 1000

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_ring_status_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="ring status push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_STATUS,
        mp_structs.RING_STATUS_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.RingStatus, bytes(expected), _packet_payload(response))


@pytest.mark.order(14)
def test_ppi_dock_status_push(runtime: MessageProtocolRuntime) -> None:
    """Verify full DOCK_STATUS push payload against a fixed expected snapshot."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    logger = logging.getLogger("pytest.hil.dock_status_push")

    expected = mp_structs.DockStatus()
    expected.hardware_version.major = 1
    expected.hardware_version.minor = 0
    expected.hardware_version.patch = 1
    expected.firmware_version.major = 1
    expected.firmware_version.minor = 0
    expected.firmware_version.patch = 1
    expected.mac[:] = (0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05)
    expected.timestamp_unix_s = 1772616496
    expected.ship_mode_exit_timestamp_unix = 1697049600
    expected.uptime_s = 3600

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dock_status_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="dock status push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_STATUS,
        mp_structs.DOCK_STATUS_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.DockStatus, bytes(expected), _packet_payload(response))


@pytest.mark.order(14)
def test_ppi_dock_debug_log_push(runtime: MessageProtocolRuntime) -> None:
    """Verify DOCK_DEBUG_LOG push payload exactly matches expected debug frame."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "60.0"))
    logger = logging.getLogger("pytest.hil.dock_debug_log_push")

    expected = mp_structs.RawDebugLog()
    expected.level = mp_enums.DEBUG_LEVEL.DEBUG_LEVEL_INFO
    expected.timestamp = 1772616496
    expected.module_id = mp_enums.SW_UNIT_ID.SW_UNIT_ID_GENERAL_CONTROL_DOCK
    expected.line = 3358
    expected.arg_values[:] = (0xDEADBEEF, 0xCAFEBABE, 0xFEEDFACE, 0xBAADF00D, 0x0D15EA5E, 0xC001D00D)

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_dock_debug_log_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="dock debug log push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_DOCK_DEBUG_LOG,
        mp_structs.RAW_DEBUG_LOG_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.RawDebugLog, bytes(expected), _packet_payload(response))


@pytest.mark.order(15)
def test_ppi_ring_debug_log_push(runtime: MessageProtocolRuntime) -> None:
    """Verify RING_DEBUG_LOG push payload exactly matches expected debug frame."""
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "60.0"))
    logger = logging.getLogger("pytest.hil.ring_debug_log_push")

    expected = mp_structs.RawDebugLog()
    expected.level = mp_enums.DEBUG_LEVEL.DEBUG_LEVEL_INFO
    expected.timestamp = 1772616496
    expected.module_id = mp_enums.SW_UNIT_ID.SW_UNIT_ID_GENERAL_CONTROL_RING
    expected.line = 3358
    expected.arg_values[:] = (0xDEADBEEF, 0xCAFEBABE, 0xFEEDFACE, 0xBAADF00D, 0x0D15EA5E, 0xC001D00D)

    response = _wait_for_expected_payload_packet(
        runtime=runtime,
        packet_predicate=_is_ring_debug_log_push,
        expected_payload=bytes(expected),
        timeout_s=timeout_s,
        response_label="ring debug log push",
    )
    _log_expected_vs_received_packet(
        logger,
        mp_enums.PPI_TYPE.PPI_TYPE_PUSH,
        mp_enums.PPI_AD.PPI_AD_RING_DEBUG_LOG,
        mp_structs.RAW_DEBUG_LOG_T_SIZE_BYTES,
        response,
    )
    _log_expected_vs_received_struct(logger, mp_structs.RawDebugLog, bytes(expected), _packet_payload(response))
