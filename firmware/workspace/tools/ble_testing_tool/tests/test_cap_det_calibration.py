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

def _build_start_cap_detection_calibration_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    packet.ppi = mp_enums.PPI_AD.PPI_AD_START_CAP_DETECTION_CALIBRATION_INTERVAL
    packet.pkt_payload_len = 0
    
    return packet

def _build_set_cap_detection_config_packet(threshold: int, hysteresis_ms: int) -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    packet.ppi = mp_enums.PPI_AD.PPI_AD_CAP_DETECTION_CONFIG
    packet.pkt_payload_len = mp_structs.CAP_DETECTION_CONFIG_SIZE_BYTES

    # Set cap detection threshold and hysteresis to the specified values
    config_struct = mp_structs.CapDetectionConfig()
    config_struct.threshold = threshold
    config_struct.hysteresis = hysteresis_ms
    ctypes.memmove(packet.payload, ctypes.byref(config_struct), packet.pkt_payload_len)
    
    return packet

# -----------------------------------------------------------------------------
# Packet match predicates
# -----------------------------------------------------------------------------
def _is_cap_detection_status_response(packet: mp_structs.MpPacketPayload) -> bool:
    return packet.type == mp_enums.PPI_TYPE.PPI_TYPE_PUSH and packet.ppi == mp_enums.PPI_AD.PPI_AD_CAP_DETECTION_STATUS and packet.pkt_payload_len == mp_structs.CAP_DETECTION_STATUS_T_SIZE_BYTES


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
def test_ppi_set_cap_detection_sample_rate(runtime: MessageProtocolRuntime) -> None:
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "20.0"))
    set_cap_detection_sample_rate_logger = logging.getLogger("pytest.hil.set_cap_detection_sample_rate")

    start_calibration_packet = _build_start_cap_detection_calibration_packet()
    set_cap_detection_config_packet = _build_set_cap_detection_config_packet(5000, 250)
    

    assert runtime.enqueue_tx_packet(start_calibration_packet), "Failed to enqueue start calibration push packet."
    
    for _ in range(5):
        # Wait for the cap detection status response and log it
        response = _wait_for_packet(runtime, _is_cap_detection_status_response, timeout_s, "cap-detection-status response")
        
        # Print the cap detection status in a human-readable format
        proximity_status = mp_structs.CapDetectionStatus.from_buffer_copy(bytes(response.payload[: mp_structs.CAP_DETECTION_STATUS_T_SIZE_BYTES]))
        set_cap_detection_sample_rate_logger.info(
            "Cap Detection Status. Threshhold=%d, Hysteresis=%d, prox=%d, is_cap_closed=%d",
            proximity_status.config_threshold,
            proximity_status.config_hysteresis,
            proximity_status.prox_value,
            proximity_status.is_cap_closed,
        )
    
    # Set the cap detection config status
    assert runtime.enqueue_tx_packet(set_cap_detection_config_packet), "Failed to enqueue set-cap-detection-config push packet."
    
    for _ in range(4):
        # Wait for the cap detection status response and log it
        response = _wait_for_packet(runtime, _is_cap_detection_status_response, timeout_s, "cap-detection-status response")
        
        # Print the cap detection status in a human-readable format
        proximity_status = mp_structs.CapDetectionStatus.from_buffer_copy(bytes(response.payload[: mp_structs.CAP_DETECTION_STATUS_T_SIZE_BYTES]))
        set_cap_detection_sample_rate_logger.info(
            "Cap Detection Status. Threshhold=%d, Hysteresis=%d, prox=%d, is_cap_closed=%d",
            proximity_status.config_threshold,
            proximity_status.config_hysteresis,
            proximity_status.prox_value,
            proximity_status.is_cap_closed,
        )

    # Wait for the cap detection status response and log it
    response = _wait_for_packet(runtime, _is_cap_detection_status_response, timeout_s, "cap-detection-status response")
    
    # Print the cap detection status in a human-readable format
    proximity_status = mp_structs.CapDetectionStatus.from_buffer_copy(bytes(response.payload[: mp_structs.CAP_DETECTION_STATUS_T_SIZE_BYTES]))
    set_cap_detection_sample_rate_logger.info(
        "Cap Detection Status. Threshhold=%d, Hysteresis=%d, prox=%d, is_cap_closed=%d",
        proximity_status.config_threshold,
        proximity_status.config_hysteresis,
        proximity_status.prox_value,
        proximity_status.is_cap_closed,
    )
    
    assert proximity_status.config_threshold == 5000, f"Expected threshold to remain at 5000, but got {proximity_status.config_threshold}."
    assert proximity_status.config_hysteresis == 250, f"Expected hysteresis to remain at 250, but got {proximity_status.config_hysteresis}."

    total = 0
    for i in range(120):
        # Wait for the cap detection status response and log it
        response = _wait_for_packet(runtime, _is_cap_detection_status_response, timeout_s, "cap-detection-status response")
        
        # print i to track how many responses have been received
        set_cap_detection_sample_rate_logger.info("Received cap detection status response #%d", i + 10)
        total = i

    assert total >= 115, f"Expected to receive at least 115 cap detection status responses, but only received {total}."

        
