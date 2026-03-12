# test_calibration_ppi.py
"""
HIL: Dock calibration flow test (App <-> Dock message protocol)

Order(1)  Full operator-gated calibration flow
  - Start calibration (start=True) and verify accepted
  - Follow the state machine through operator prompts:
      WAIT_FOR_RING_REMOVAL -> SET_DOCK_WEIGHT -> WAIT_FOR_CALIBRATION_WEIGHT -> CALIBRATING -> COMPLETE
  - Confirm weight placement by pressing "W"
  - Require CALIBRATION_DATA PUSH after COMPLETE
  - If the test fails or times out before COMPLETE, it will STOP calibration with retries:
      send start=False, wait up to 1s for feedback state==ERROR, retry 2 more times, then fail if no ERROR feedback.

Key env vars (adjustable) for timeouts:
  MP_HIL_TIMEOUT_S                      (default 10) - generic timeout for individual waits (start RE, feedback, data push, etc)
  MP_HIL_CAL_TOTAL_TIMEOUT_S            (default 300) - total timeout for the entire flow (after this, finalizer will attempt STOP if not done)
"""

import logging
import os
import secrets
import struct
import time
from dataclasses import dataclass
from functools import partial
from typing import Callable, Optional

import pytest

from message_protocol_runtime import MessageProtocolRuntime
from utils import mp_enums
from utils import mp_structs

pytestmark = pytest.mark.hil

# ===================================================================================
# Shared helpers / runtime fixture
# ===================================================================================


# Helper for boolean env vars with common truthy values (1, true, yes, on)
def _env_bool(name: str, default: bool) -> bool:
    """Parse common boolean env-var values."""
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


# Helper to resolve session ID with env override or random fallback, ensuring uint32 range
def _resolve_session_id() -> int:
    """Use a stable HIL session id if provided, otherwise generate a random uint32."""
    raw = os.getenv("MP_HIL_SESSION_ID")
    if raw is not None and raw.strip() != "":
        session_id = int(raw, 0)
        if session_id < 0 or session_id > 0xFFFFFFFF:
            raise ValueError("MP_HIL_SESSION_ID must be in uint32 range (0..0xFFFFFFFF).")
        return session_id
    return secrets.randbits(32)


# Fixture for HIL message protocol runtime (one per module)
@pytest.fixture(scope="module")
def runtime() -> MessageProtocolRuntime:
    """
    One runtime per module: starts the HIL message protocol runtime and closes it after tests finish.
    """
    if not _env_bool("MP_HIL_ENABLED", False):
        pytest.skip("HIL disabled. Set MP_HIL_ENABLED=1 to run hardware-in-the-loop tests.")

    host = os.getenv("MP_HIL_HOST", "localhost").encode("ascii")
    port = int(os.getenv("MP_HIL_PORT", "5000"))
    is_master = _env_bool("MP_HIL_IS_MASTER", True)

    raw_session_id = os.getenv("MP_HIL_SESSION_ID")
    session_id = _resolve_session_id()
    session_id_source = "MP_HIL_SESSION_ID" if raw_session_id is not None and raw_session_id.strip() != "" else "random"
    process_interval_s = float(os.getenv("MP_HIL_PROCESS_INTERVAL_S", "0.001"))

    logger = logging.getLogger("pytest.hil.calibration.runtime")
    logger.info("Using HIL session_id=%#010x (source=%s)", session_id, session_id_source)

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


# Helper to announce important operator actions with both print (for immediate visibility) and logging (for record)
def _announce(msg: str, log: logging.Logger) -> None:
    """
    For time-sensitive operator actions, print (immediate visibility with `pytest -s`) and log.
    """
    print(msg, flush=True)
    log.info(msg)


def _wait_for_response(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    timeout_s: float,
    response_label: str,
    poll_interval_s: float = 0.001,
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


def _wait_for_optional(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    timeout_s: float,
    poll_interval_s: float = 0.001,
) -> Optional[mp_structs.MpPacketPayload]:
    """
    Optional version of _wait_for_response(): returns None on timeout.
    """
    deadline = time.perf_counter() + timeout_s

    while time.perf_counter() < deadline:
        if runtime.runtime_error is not None:
            pytest.fail(f"Runtime failed while waiting for packet: {runtime.runtime_error}")

        packet = runtime.try_dequeue_rx_packet()
        if packet is None:
            time.sleep(poll_interval_s)
            continue

        if predicate(packet):
            return packet

    return None


# ===================================================================================
# Calibration packet helpers / predicates
# ===================================================================================


def _wait_for_state_and_validate(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    timeout_s: float,
    log: logging.Logger,
    log_prefix: str,
    expected_state: int,
    expected_state_name: str,
) -> mp_structs.CalibrationFeedback:
    """
    Waits for a packet matching `predicate`, parses it with `feedback_struct_type.from_buffer_copy`,
    logs the common fields, and asserts current_state matches expected_state.
    """
    pkt = _wait_for_response(runtime, predicate, timeout_s, f"{log_prefix} {expected_state_name}")

    fb = mp_structs.CalibrationFeedback.from_buffer_copy(bytes(pkt.payload[: pkt.pkt_payload_len]))

    state = int(fb.current_state)
    ring_present = int(fb.is_ring_present)
    avg_weight_mg = int(fb.avg_weight_mg)
    std_dev = int(fb.std_dev)

    log.info(
        "%s -> %s (%d) | ring_present=%d avg_weight_mg=%d std_dev=%d",
        log_prefix,
        expected_state_name,
        state,
        ring_present,
        avg_weight_mg,
        std_dev,
    )

    assert state == int(expected_state), f"Expected feedback state {expected_state_name}, got {state}"
    return fb


# Helper to get calibration weight from env vars, with mg/g support and uint32 enforcement
def _get_cal_weight_mg() -> int:
    """
    Calibration weight passed to Dock.
    Override with:
      MP_HIL_CAL_WEIGHT_MG=<int>  (preferred)
    """
    raw_mg = os.getenv("MP_HIL_CAL_WEIGHT_MG")
    if raw_mg is not None:
        return int(raw_mg, 0) & 0xFFFFFFFF

    return 20000  # default: 20g


# Packet builders and predicates for calibration PPIs (START, weight present, feedback, data, etc.)
def _build_start_calibration_rq(start: bool, cal_weight_mg: int) -> mp_structs.MpPacketPayload:
    """
    PPI_AD_START_CALIBRATION (RQ)
      payload[0] = start (1/0)
      payload[1..4] = cal_weight_mg (uint32 LE)
    """
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_START_CALIBRATION
    payload = struct.pack("<BI", 1 if start else 0, cal_weight_mg & 0xFFFFFFFF)
    pkt.pkt_payload_len = len(payload)
    pkt.payload[: pkt.pkt_payload_len] = payload
    return pkt


def _build_weight_present_push(is_present: bool) -> mp_structs.MpPacketPayload:
    """
    PPI_AD_CALIBRATION_WEIGHT_PRESENT (PUSH)
      payload[0] = is_present (1/0)
    """
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_CALIBRATION_WEIGHT_PRESENT
    pkt.pkt_payload_len = 1
    pkt.payload[0] = 1 if is_present else 0
    return pkt


def _build_calibration_data_rq() -> mp_structs.MpPacketPayload:
    """PPI_AD_CALIBRATION_DATA (RQ) no payload."""
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_CALIBRATION_DATA
    pkt.pkt_payload_len = 0
    return pkt


def _is_start_calibration_re(pkt: mp_structs.MpPacketPayload) -> bool:
    """
    Start/Stop shares the same RE, but semantics differ:
      - START expects payload[0]==1 for success
      - STOP does NOT use this RE for success (STOP success comes from ERROR feedback)
    """
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_RE
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_START_CALIBRATION
        and pkt.pkt_payload_len == mp_structs.START_CALIBRATION_RESPONSE_T_SIZE_BYTES
    )


def _is_cal_feedback_push(pkt: mp_structs.MpPacketPayload) -> bool:
    """Calibration feedback PUSH with strict payload length."""
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_PUSH
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_CALIBRATION_FEEDBACK
        and pkt.pkt_payload_len == mp_structs.CALIBRATION_FEEDBACK_T_SIZE_BYTES
    )


def _is_cal_data_push(pkt: mp_structs.MpPacketPayload) -> bool:
    """Calibration record PUSH (expected at COMPLETE)."""
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_PUSH
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_CALIBRATION_DATA
        and pkt.pkt_payload_len == mp_structs.WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES
    )


def _is_cal_data_re(pkt: mp_structs.MpPacketPayload) -> bool:
    """Calibration record RE (used for baseline capture and diagnostic polling)."""
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_RE
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_CALIBRATION_DATA
        and pkt.pkt_payload_len == mp_structs.WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES
    )


# Helper to compare calibration records for update verification (used at end of full flow test)
def _records_differ(a: mp_structs.WeightStackCalibrationRecord, b: mp_structs.WeightStackCalibrationRecord) -> bool:
    """Field-by-field difference check for record update verification."""
    return (
        int(a.zero_offset) != int(b.zero_offset)
        or int(a.calibration_factor) != int(b.calibration_factor)
        or int(a.full_assembly_weight_mg) != int(b.full_assembly_weight_mg)
    )


# Helper to check if a feedback packet indicates a specific calibration state (used for STOP success and flow tracking)
def _is_cal_feedback_state(pkt: mp_structs.MpPacketPayload, state_code: int) -> bool:
    """
    Predicate: calibration feedback push AND parsed state equals state_code.
    Used for STOP success (state==ERROR).
    """
    if not _is_cal_feedback_push(pkt):
        return False
    fb = mp_structs.CalibrationFeedback.from_buffer_copy(bytes(pkt.payload[: pkt.pkt_payload_len]))
    return int(fb.current_state) == int(state_code)


# Helper to check if the operator pressed 'W' for weight placement confirmation, with cross-platform support
def _operator_pressed_w() -> bool:
    """
    Operator confirmation for weight placement.
      - POSIX: type 'W' then Enter
      - Windows: press 'W' (no Enter)
    """
    try:
        if os.name == "nt":
            import msvcrt  # type: ignore

            if msvcrt.kbhit():
                ch = msvcrt.getwch()
                return ch.lower() == "w"
            return False

        import select
        import sys

        if sys.stdin is None:
            return False
        r, _, _ = select.select([sys.stdin], [], [], 0)
        if not r:
            return False
        line = sys.stdin.readline()
        return line.strip().lower() == "w"
    except Exception:
        return False


def _wait_for_operator_w(timeout_s: float) -> None:
    """Block (polling) until operator presses W, or timeout."""
    deadline = time.perf_counter() + timeout_s
    while time.perf_counter() < deadline:
        if _operator_pressed_w():
            return
        time.sleep(0.01)
    pytest.fail(f"Timed out waiting for operator to press W within {timeout_s:.1f}s.")


# ===================================================================================
# STOP helpers (no nested functions inside tests)
# ===================================================================================


@dataclass
class _StopGuard:
    """
    Used by teardown-finalizers:
      started=True once START accepted
      done=True once test reaches its success terminal condition (COMPLETE for full flow; successful stop for stop test)
    """

    started: bool = False
    done: bool = False


# Helper to perform STOP procedure with retries and require ERROR feedback for success
def _stop_calibration(
    runtime: MessageProtocolRuntime,
    cal_weight_mg: int,
    error_state_code: int,
    log: logging.Logger,
    reason: str,
) -> None:
    """
    STOP semantics (per updated protocol):
      - Send START_CALIBRATION RQ with start=False
      - STOP is successful only when CALIBRATION_FEEDBACK with state==ERROR is observed after STOP command

    Retry logic:
      - Attempt up to 3 times
      - After each send, wait up to 1s for ERROR feedback
      - If still no ERROR feedback, fail
    """

    pred_error = partial(_is_cal_feedback_state, state_code=error_state_code)

    for attempt in range(1, 4):
        stop_rq = _build_start_calibration_rq(False, cal_weight_mg)
        assert runtime.enqueue_tx_packet(stop_rq), "Failed to enqueue STOP_CALIBRATION RQ."
        log.warning("Sent STOP_CALIBRATION RQ (attempt %d/3): %s", attempt, reason)

        err_pkt = _wait_for_optional(runtime, pred_error, timeout_s=1.0)
        if err_pkt is not None:
            log.warning("Observed STOP success via CALIBRATION_FEEDBACK state==ERROR.")
            return

        log.warning("No ERROR-state feedback within 1.0s after STOP (attempt %d/3).", attempt)

    pytest.fail("STOP_CALIBRATION did not result in ERROR-state feedback after 3 attempts (1s wait each).")


# Teardown finalizer to ensure STOP is attempted if test started calibration but did not complete. Require ERROR feedback for cleanup success
def _finalize_stop_calibration_if_needed(
    runtime: MessageProtocolRuntime,
    guard: _StopGuard,
    cal_weight_mg: int,
    error_state_code: int,
    log: logging.Logger,
    total_timeout_s: float,
) -> None:
    """
    Finalizer called at teardown:
      - If test started calibration but did not complete, attempt STOP with retries and require ERROR feedback.
    """
    if guard.started and (not guard.done):
        _stop_calibration(
            runtime,
            cal_weight_mg,
            error_state_code,
            log,
            reason=f"calibration did not complete within {total_timeout_s:.1f}s (or test failed)",
        )


# ===================================================================================
# Tests
# ===================================================================================
# The full calibration flow
@pytest.mark.hil
@pytest.mark.order(1)
def test_ppi_ad_calibration_full_flow(runtime: MessageProtocolRuntime, request) -> None:
    """
    Full calibration flow:
      - START (expect RE payload[0]==1)
      - Follow feedback states to prompt operator actions
      - Send weight-present PUSH when operator confirms (W)
      - Require COMPLETE and CALIBRATION_DATA PUSH
      - On failure/timeout before COMPLETE, finalizer will STOP and require ERROR feedback.
    """
    # Set all timeouts from env with sane defaults
    # generic timeout for individual waits (start RE, feedback, data push, etc)
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    # total timeout for the entire flow (after this, finalizer will attempt STOP if not done)
    total_timeout_s = float(os.getenv("MP_HIL_CAL_TOTAL_TIMEOUT_S", "300.0"))

    # Get calibration weight and logger
    cal_weight_mg = _get_cal_weight_mg()
    log = logging.getLogger("pytest.hil.calibration")

    # Resolve state codes from enums for readability (used for state tracking and prompts)
    # Ignore CALIBRATION_STATE_STORE_UPDATED_PARAM state since it's an internal detail that may be skipped or merged with COMPLETE
    st_wait_ring_removed = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_WAIT_FOR_RING_REMOVAL
    st_set_dock_weight = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_SET_DOCK_WEIGHT
    st_wait_cal_weight = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_WAIT_FOR_CALIBRATION_WEIGHT
    st_calibrating = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_CALIBRATING
    st_complete = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_COMPLETE
    st_error = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_ERROR

    # Stop-on-failure guard via finalizer
    guard = _StopGuard(started=False, done=False)
    request.addfinalizer(
        partial(_finalize_stop_calibration_if_needed, runtime, guard, cal_weight_mg, st_error, log, total_timeout_s)
    )

    # Capture baseline record (best effort; used to confirm update at end)
    baseline_record = None
    runtime.enqueue_tx_packet(_build_calibration_data_rq())
    base_pkt = _wait_for_optional(runtime, _is_cal_data_re, timeout_s=3.0)
    if base_pkt is not None:
        baseline_record = mp_structs.WeightStackCalibrationRecord.from_buffer_copy(
            bytes(base_pkt.payload[: base_pkt.pkt_payload_len])
        )
        log.info(
            "Baseline CALIBRATION_DATA RE: zero_offset=%d calibration_factor=%d full_assembly_weight_mg=%d",
            int(baseline_record.zero_offset),
            int(baseline_record.calibration_factor),
            int(baseline_record.full_assembly_weight_mg),
        )
        _announce("Captured baseline calibration record. Starting flow...", log)

    # START Calibration
    start_rq = _build_start_calibration_rq(True, cal_weight_mg)
    assert runtime.enqueue_tx_packet(start_rq), "Failed to enqueue START_CALIBRATION RQ."
    guard.started = True
    log.info(
        "Sent START_CALIBRATION RQ: payload_hex=%s cal_weight_mg=%d",
        bytes(start_rq.payload[: start_rq.pkt_payload_len]).hex(),
        cal_weight_mg,
    )

    # Expect RE with payload[0]==1 for successful start
    start_re = _wait_for_response(runtime, _is_start_calibration_re, timeout_s, "START_CALIBRATION RE")
    assert int(start_re.payload[0]) == 1, (
        f"START_CALIBRATION RE indicated failure (payload[0]={int(start_re.payload[0])})."
    )
    _announce("Received successful START_CALIBRATION RE. Follow prompts to complete calibration.", log)

    # Step 1: WAIT_FOR_RING_REMOVAL -> prompt remove ring
    _announce("Remove Ring From Dock", log)
    _wait_for_state_and_validate(
        runtime,
        partial(_is_cal_feedback_state, state_code=st_wait_ring_removed),
        timeout_s,
        log,
        "CAL STATE",
        st_wait_ring_removed,
        "WAIT_FOR_RING_REMOVAL",
    )

    # Step 2: SET_DOCK_WEIGHT -> prompt place dock on stable surface
    _wait_for_state_and_validate(
        runtime,
        partial(_is_cal_feedback_state, state_code=st_set_dock_weight),
        timeout_s * 5,  # allow extra time for weight stabilization
        log,
        "CAL STATE",
        st_set_dock_weight,
        "SET_DOCK_WEIGHT",
    )
    _announce("Place Dock on a stable, level surface", log)

    # Step 3: WAIT_FOR_CALIBRATION_WEIGHT -> prompt place calibration weight, confirm with W
    _wait_for_state_and_validate(
        runtime,
        partial(_is_cal_feedback_state, state_code=st_wait_cal_weight),
        timeout_s,
        log,
        "CAL STATE",
        st_wait_cal_weight,
        "WAIT_FOR_CALIBRATION_WEIGHT",
    )
    _announce("Place Calibration Weight on Dock", log)
    _announce("Press W then Enter to confirm calibration weight is placed.", log)

    # Operator confirms weight
    _wait_for_operator_w(timeout_s=total_timeout_s)
    wp = _build_weight_present_push(True)
    assert runtime.enqueue_tx_packet(wp), "Failed to enqueue CALIBRATION_WEIGHT_PRESENT PUSH."
    _announce("Sent CALIBRATION_WEIGHT_PRESENT (true) [operator confirmed with W]", log)

    # Step 4: CALIBRATING
    _wait_for_state_and_validate(
        runtime,
        partial(_is_cal_feedback_state, state_code=st_calibrating),
        timeout_s * 5,  # allow extra time for calibration to complete
        log,
        "CAL STATE",
        st_calibrating,
        "CALIBRATING",
    )
    _announce("Calibrating", log)

    # Step 5: COMPLETE
    _wait_for_state_and_validate(
        runtime,
        partial(_is_cal_feedback_state, state_code=st_complete),
        timeout_s,
        log,
        "CAL STATE",
        st_complete,
        "COMPLETE",
    )
    _announce("Observed COMPLETE state (from feedback).", log)

    # Require CALIBRATION_DATA PUSH after COMPLETE
    data_push_pkt = _wait_for_response(runtime, _is_cal_data_push, timeout_s, "CALIBRATION_DATA PUSH")
    rec = mp_structs.WeightStackCalibrationRecord.from_buffer_copy(
        bytes(data_push_pkt.payload[: data_push_pkt.pkt_payload_len])
    )
    _announce(
        f"Received CALIBRATION_DATA PUSH: zero_offset={int(rec.zero_offset)} "
        f"calibration_factor={int(rec.calibration_factor)} "
        f"full_assembly_weight_mg={int(rec.full_assembly_weight_mg)}",
        log,
    )

    log.info("Note: full_assembly_weight_mg is unchanged by calibration (set during baselining).")

    assert int(rec.zero_offset) != 0, "Calibration record PUSH has zero_offset == 0 (unexpected)."
    assert int(rec.calibration_factor) != 0, "Calibration record PUSH has calibration_factor == 0 (unexpected)."

    if baseline_record is not None:
        assert _records_differ(rec, baseline_record), "Calibration record did not update vs baseline record."
    guard.done = True
