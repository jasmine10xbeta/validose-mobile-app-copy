# test_baselining_stop_timeout_ppi.py
"""
HIL: Dock baselining (commission new bottle / replace medication) flow test (App <-> Dock message protocol)

This file contains three tests:

Order(1) Calibration pre-check (hard requirement)
  - Request CALIBRATION_DATA
  - Fail if record indicates uncalibrated state (calibration_factor==0, zero_offset==0, etc)

Order(2) STOP procedure behavior (sanity check)
  - Start baselining (start=True), require RE payload[0]==1
  - Observe at least one baselining feedback push (baselining activity is alive)
  - Stop baselining (start=False)
  - STOP is considered successful ONLY when a subsequent BASELINING_FEEDBACK reports state==ERROR
  - (Optional) verify feedback goes quiet after stop

Order(3)  Firmware-timeout STOP behavior
  - Start baselining (start=True), require RE payload[0]==1
  - Intentionally do NOT progress the flow (never remove Ring, no operator actions)
  - Wait for firmware to time out -> must observe BASELINING_FEEDBACK state==ERROR
  - Require feedback quiet afterwards (configurable via env) - sanity check that firmware is actually stopping activity on timeout
  - Pass if ERROR state is observed within expected timeout, fail if not
"""

import logging
import os
import secrets
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
# Note: some helpers are shared with test_calibration_ppi due to overlap in flow and for better organization (stop procedure logic, packet builders/predicates, etc).
# These are not imported to avoid circular dependencies, but are kept in sync manually.
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
    return secrets.randbits(32)


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

    logger = logging.getLogger("pytest.hil.baselining.runtime")
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


def _announce(msg: str, log: logging.Logger) -> None:
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
    No stashing; this dequeues packets just like _wait_for_response.
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
# Shared packets (calibration data used for pre/post checks)
# ===================================================================================


def _build_calibration_data_rq() -> mp_structs.MpPacketPayload:
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_CALIBRATION_DATA
    pkt.pkt_payload_len = 0
    return pkt


def _is_calibration_data_re(pkt: mp_structs.MpPacketPayload) -> bool:
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_RE
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_CALIBRATION_DATA
        and pkt.pkt_payload_len == mp_structs.WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES
    )


def _is_record_calibrated(rec: mp_structs.WeightStackCalibrationRecord) -> bool:
    """
    Heuristic precondition check:
      - calibration_factor != 0
      - (optional) zero_offset != 0
      - (optional) full_assembly_weight_mg != 0
    """
    require_zero = _env_bool("MP_HIL_REQUIRE_ZERO_OFFSET", True)
    require_full_weight = _env_bool("MP_HIL_REQUIRE_FULL_ASSEMBLY_WEIGHT", False)

    if int(rec.calibration_factor) == 0:
        return False
    if require_zero and int(rec.zero_offset) == 0:
        return False
    if require_full_weight and int(rec.full_assembly_weight_mg) == 0:
        return False
    return True


# ===================================================================================
# Baselining protocol packets / predicates
# ===================================================================================
# Builders and predicates for START_BASELINING, BASELINING_FEEDBACK, and VALIDATE_MED packets, as well as helper to resolve baselining state codes and to check
# for all-zero byte sequences (used in some failure states).
def _build_start_baselining_rq(start: bool) -> mp_structs.MpPacketPayload:
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_START_BASELINING
    pkt.pkt_payload_len = mp_structs.START_BASELINING_PARAM_T_SIZE_BYTES
    pkt.payload[0] = 1 if start else 0
    return pkt


def _is_start_baselining_re(pkt: mp_structs.MpPacketPayload) -> bool:
    """
    START semantics:
      - START expects payload[0]==1 for success, 0 for failure
    STOP semantics:
      - STOP does NOT use this RE as success criteria (STOP success comes from ERROR feedback)
    """
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_RE
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_START_BASELINING
        and pkt.pkt_payload_len == mp_structs.START_BASELINING_RESPONSE_T_SIZE_BYTES
    )


def _is_baselining_feedback_push(pkt: mp_structs.MpPacketPayload) -> bool:
    # Accept both BASELINING_FEEDBACK and (firmware bug) START_BASELINING as PUSH feedback.
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_PUSH
        and pkt.pkt_payload_len == mp_structs.BASELINING_FEEDBACK_T_SIZE_BYTES
        and pkt.ppi in (mp_enums.PPI_AD.PPI_AD_BASELINING_FEEDBACK, mp_enums.PPI_AD.PPI_AD_START_BASELINING)
    )


def _is_feedback_state(pkt: mp_structs.MpPacketPayload, state_code: int) -> bool:
    if not _is_baselining_feedback_push(pkt):
        return False
    fb = mp_structs.BaseliningFeedback.from_buffer_copy(bytes(pkt.payload[: pkt.pkt_payload_len]))
    return int(fb.current_state) == int(state_code)


# ===================================================================================
# STOP helpers (no nested functions inside tests)
# ===================================================================================


@dataclass
class _StopGuard:
    started: bool = False
    done: bool = False


def _stop_baselining_until_error_or_fail(
    runtime: MessageProtocolRuntime,
    error_state_code: int,
    log: logging.Logger,
    reason: str,
) -> None:
    """
    STOP semantics:
      - Send START_BASELINING RQ with start=False
      - STOP is successful only when BASELINING_FEEDBACK with state==ERROR is later observed

    Retry logic:
      - Attempt up to 3 times
      - After each send, wait up to 1s for ERROR feedback
      - If still no ERROR feedback, fail
    """
    pred_error = partial(_is_feedback_state, state_code=error_state_code)

    for attempt in range(1, 4):
        stop_rq = _build_start_baselining_rq(False)
        assert runtime.enqueue_tx_packet(stop_rq), "Failed to enqueue STOP_BASELINING RQ."
        log.warning("Sent STOP_BASELINING RQ (attempt %d/3): %s", attempt, reason)

        err_pkt = _wait_for_optional(runtime, pred_error, timeout_s=1.0)
        if err_pkt is not None:
            log.warning("Observed STOP success via BASELINING_FEEDBACK state==ERROR.")
            return

        log.warning("No ERROR-state feedback within 1.0s after STOP (attempt %d/3).", attempt)

    pytest.fail("STOP_BASELINING did not result in ERROR-state feedback after 3 attempts (1s wait each).")


def _finalize_stop_baselining_if_needed(
    runtime: MessageProtocolRuntime,
    guard: _StopGuard,
    error_state_code: int,
    log: logging.Logger,
    total_timeout_s: float,
) -> None:
    if guard.started and (not guard.done):
        _stop_baselining_until_error_or_fail(
            runtime,
            error_state_code,
            log,
            reason=f"baselining did not complete within {total_timeout_s:.1f}s (or test failed)",
        )


def _assert_feedback_goes_quiet(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    quiet_period_s: float,
    within_s: float,
) -> None:
    quiet_period_s = max(0.0, float(quiet_period_s))
    within_s = max(0.0, float(within_s))

    overall_deadline = time.perf_counter() + within_s
    quiet_start = time.perf_counter()

    while time.perf_counter() < overall_deadline:
        pkt = runtime.try_dequeue_rx_packet()
        now = time.perf_counter()

        if pkt is None:
            if (now - quiet_start) >= quiet_period_s:
                return
            time.sleep(0.01)
            continue

        if predicate(pkt):
            quiet_start = now

    raise AssertionError(f"Feedback did not become quiet for {quiet_period_s:.1f}s within {within_s:.1f}s after stop.")


# ===================================================================================
# Tests
# ===================================================================================
# Global flags to track calibration status and share calibration record (for pre-check and post-check in baselining). Set by test_ppi_is_calibrated, read by test_ppi_ad_baselining_full_flow.
_DOCK_CALIBRATED = False
_BASELINE_CAL_RECORD: Optional[mp_structs.WeightStackCalibrationRecord] = None


@pytest.mark.hil
@pytest.mark.order(1)
def test_ppi_is_calibrated(runtime: MessageProtocolRuntime) -> None:
    """
    Hard precondition for baselining: Dock must be calibrated.
    This test FAILS (does not skip) if uncalibrated.
    """
    global _DOCK_CALIBRATED
    global _BASELINE_CAL_RECORD

    # Set timeout and logger
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    log = logging.getLogger("pytest.hil.baselining.calibration_check")

    # RQ calibration data and check if it appears calibrated.
    rq = _build_calibration_data_rq()
    assert runtime.enqueue_tx_packet(rq), "Failed to enqueue CALIBRATION_DATA RQ."

    # Wait for calibration data RE and parse record
    re = _wait_for_response(runtime, _is_calibration_data_re, timeout_s, "CALIBRATION_DATA RE")
    rec = mp_structs.WeightStackCalibrationRecord.from_buffer_copy(bytes(re.payload[: re.pkt_payload_len]))
    _BASELINE_CAL_RECORD = rec

    log.info(
        "Calibration record: zero_offset=%d calibration_factor=%d full_assembly_weight_mg=%d",
        int(rec.zero_offset),
        int(rec.calibration_factor),
        int(rec.full_assembly_weight_mg),
    )

    _DOCK_CALIBRATED = _is_record_calibrated(rec)
    if not _DOCK_CALIBRATED:
        pytest.fail(
            "Dock does not appear calibrated (calibration_factor/zero_offset indicates uncalibrated). "
            "Run calibration flow first, then re-run baselining."
        )

    _announce("Pre-check OK: Dock appears calibrated. Proceeding to baselining.", log)


@pytest.mark.hil
@pytest.mark.order(2)
def test_ppi_ad_baselining_stop_procedure_ack(runtime: MessageProtocolRuntime, request) -> None:
    """
    STOP procedure test:
      - START (expect RE payload[0]==1)
      - Wait for any feedback (proves stream is alive)
      - STOP => require ERROR feedback
      - Optional quiet check
    """
    if not _DOCK_CALIBRATED:
        pytest.fail("Dock calibration pre-check did not pass (required).")

    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    feedback_wait_s = float(os.getenv("MP_HIL_BASELINE_STOP_FEEDBACK_WAIT_S", "10.0"))
    stop_quiet_s = float(os.getenv("MP_HIL_BASELINE_STOP_QUIET_S", "1.0"))
    stop_effect_timeout_s = float(os.getenv("MP_HIL_BASELINE_STOP_EFFECT_TIMEOUT_S", "10.0"))

    log = logging.getLogger("pytest.hil.baselining.stop")

    st_error = mp_enums.BASELINING_STATE.BASELINING_STATE_ERROR

    guard = _StopGuard(started=False, done=False)
    request.addfinalizer(partial(_finalize_stop_baselining_if_needed, runtime, guard, st_error, log, timeout_s))

    # START Baselining and verify RE indicates success
    start_rq = _build_start_baselining_rq(True)
    assert runtime.enqueue_tx_packet(start_rq), "Failed to enqueue START_BASELINING RQ."
    guard.started = True

    start_re = _wait_for_response(runtime, _is_start_baselining_re, timeout_s, "START_BASELINING RE")
    assert int(start_re.payload[0]) == 1, (
        f"START_BASELINING RE indicated failure (payload[0]={int(start_re.payload[0])})."
    )

    # Confirm feedback stream is alive
    fb_pkt = _wait_for_optional(runtime, _is_baselining_feedback_push, timeout_s=feedback_wait_s)
    assert fb_pkt is not None, f"No BASELINING_FEEDBACK within {feedback_wait_s:.1f}s after start."

    # STOP => require ERROR feedback
    _stop_baselining_until_error_or_fail(runtime, st_error, log, reason="stop procedure test")
    guard.done = True

    _assert_feedback_goes_quiet(
        runtime, _is_baselining_feedback_push, quiet_period_s=stop_quiet_s, within_s=stop_effect_timeout_s
    )


@pytest.mark.hil
@pytest.mark.order(3)
def test_ppi_ad_baselining_firmware_timeout_stop_behavior(runtime: MessageProtocolRuntime) -> None:
    """
    Firmware-timeout STOP behavior (baselining):

      - Start baselining (start=True), require RE payload[0]==1
      - Intentionally do NOT progress the flow (no operator actions, do not respond to VALIDATE_MED)
      - Wait for firmware to time out -> must observe BASELINING_FEEDBACK state==ERROR
      - Require feedback quiet afterwards (configurable via env) - sanity check firmware actually stopped activity on timeout
      - Pass if ERROR state is observed within expected timeout, fail if not
    """
    if not _DOCK_CALIBRATED:
        pytest.fail("Dock calibration pre-check did not pass (required).")

    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))

    # How long to wait for firmware to time out into ERROR.
    # Set this to expected firmware timeout (longest timeout in baselining fsm is 5mins) + a small buffer.
    fw_timeout_wait_s = float(os.getenv("MP_HIL_BASELINE_FW_TIMEOUT_WAIT_S", "320.0"))

    # Quiet-window check after firmware timeout (disable by setting quiet to 0.0)
    quiet_s = float(os.getenv("MP_HIL_BASELINE_STOP_QUIET_S", "1.0"))
    quiet_within_s = float(os.getenv("MP_HIL_BASELINE_STOP_EFFECT_TIMEOUT_S", "10.0"))

    st_error = mp_enums.BASELINING_STATE.BASELINING_STATE_ERROR

    # START baselining
    start_rq = _build_start_baselining_rq(True)
    assert runtime.enqueue_tx_packet(start_rq), "Failed to enqueue START_BASELINING RQ."

    start_re = _wait_for_response(runtime, _is_start_baselining_re, timeout_s, "START_BASELINING RE")
    assert int(start_re.payload[0]) == 1, (
        f"START_BASELINING RE indicated failure (payload[0]={int(start_re.payload[0])})."
    )

    # Wait for firmware timeout -> ERROR feedback
    pred_error = partial(_is_feedback_state, state_code=st_error)
    err_pkt = _wait_for_optional(runtime, pred_error, timeout_s=fw_timeout_wait_s)
    assert err_pkt is not None, (
        f"Did not observe BASELINING_FEEDBACK state==ERROR within {fw_timeout_wait_s:.1f}s "
        "(increase MP_HIL_BASELINE_FW_TIMEOUT_WAIT_S if firmware timeout is longer)."
    )

    # After firmware timeout, require feedback to go quiet (sanity check)
    _assert_feedback_goes_quiet(runtime, _is_baselining_feedback_push, quiet_period_s=quiet_s, within_s=quiet_within_s)
