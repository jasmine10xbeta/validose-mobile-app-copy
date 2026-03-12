# test_calibration_stop_timeout_ppi.py
"""
HIL: Dock calibration flow test (App <-> Dock message protocol)

This file contains two tests:

Order(1)  STOP procedure behavior (sanity check)
  - Start calibration (start=True)
  - Observe at least one calibration feedback push (means calibration activity is alive)
  - Stop calibration (start=False)
  - STOP is considered successful ONLY when a subsequent CALIBRATION_FEEDBACK reports state==ERROR
  - (Optional) verify feedback goes quiet after stop (configurable via env)

Order(2)  Firmware-timeout STOP behavior
  - Start calibration (start=True), require RE payload[0]==1
  - Intentionally do NOT progress the flow (never send weight-present TRUE, no operator actions)
  - Wait for firmware to time out -> must observe CALIBRATION_FEEDBACK state==ERROR
  - Require feedback quiet afterwards (configurable via env) - sanity check that firmware is actually stopping activity on timeout
  - Pass if ERROR state is observed within expected timeout, fail if not
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
# Helper to get calibration weight from env vars, with mg/g support and uint32 enforcement
def _get_cal_weight_mg() -> int:
    """
    Calibration weight passed to Dock.
    Override with:
      MP_HIL_CAL_WEIGHT_MG=<int>  (preferred)
      MP_HIL_CAL_WEIGHT_G=<int>   (converted to mg)
    """
    raw_mg = os.getenv("MP_HIL_CAL_WEIGHT_MG")
    if raw_mg is not None:
        return int(raw_mg, 0) & 0xFFFFFFFF

    raw_g = os.getenv("MP_HIL_CAL_WEIGHT_G")
    if raw_g is not None:
        return (int(raw_g, 0) * 1000) & 0xFFFFFFFF

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


# Helper to require a quiet feedback window after STOP (used for sanity check in stop test, optional in full flow test)
def _assert_feedback_goes_quiet(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    quiet_period_s: float,
    within_s: float,
) -> None:
    """
    Optional post-stop check: require a contiguous quiet window (no feedback packets) of quiet_period_s,
    observed within within_s.

    If firmware continues to push feedback while in ERROR, this may fail; adjust env vars accordingly.
    """
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


@pytest.mark.hil
@pytest.mark.order(1)
def test_ppi_ad_calibration_stop_procedure_ack(runtime: MessageProtocolRuntime, request) -> None:
    """
    STOP procedure test (sanity):
      - START (ok==1)
      - Observe any feedback
      - STOP (start=False) and require ERROR feedback
      - Optional quiet check
    """
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    feedback_wait_s = float(os.getenv("MP_HIL_CAL_STOP_FEEDBACK_WAIT_S", "10.0"))
    stop_quiet_s = float(os.getenv("MP_HIL_CAL_STOP_QUIET_S", "1.0"))
    stop_effect_timeout_s = float(os.getenv("MP_HIL_CAL_STOP_EFFECT_TIMEOUT_S", "10.0"))

    cal_weight_mg = _get_cal_weight_mg()
    log = logging.getLogger("pytest.hil.calibration.stop")

    st_error = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_ERROR

    guard = _StopGuard(started=False, done=False)
    request.addfinalizer(
        partial(_finalize_stop_calibration_if_needed, runtime, guard, cal_weight_mg, st_error, log, timeout_s)
    )

    start_rq = _build_start_calibration_rq(True, cal_weight_mg)
    assert runtime.enqueue_tx_packet(start_rq), "Failed to enqueue START_CALIBRATION RQ."
    guard.started = True
    log.info(
        "Sent START_CALIBRATION RQ: payload_hex=%s cal_weight_mg=%d",
        bytes(start_rq.payload[: start_rq.pkt_payload_len]).hex(),
        cal_weight_mg,
    )

    start_re = _wait_for_response(runtime, _is_start_calibration_re, timeout_s, "START_CALIBRATION RE")
    log.info("Received START_CALIBRATION RE: ok=%d", int(start_re.payload[0]))
    assert int(start_re.payload[0]) == 1, (
        f"START_CALIBRATION RE indicated failure (payload[0]={int(start_re.payload[0])})."
    )

    fb_pkt = _wait_for_response(runtime, _is_cal_feedback_push, feedback_wait_s, "any CALIBRATION_FEEDBACK")
    fb = mp_structs.CalibrationFeedback.from_buffer_copy(bytes(fb_pkt.payload[: fb_pkt.pkt_payload_len]))
    log.info(
        "Observed CALIBRATION_FEEDBACK prior to stop: state=%d ring_present=%d avg_weight_mg=%d std_dev=%d",
        int(fb.current_state),
        int(fb.is_ring_present),
        int(fb.avg_weight_mg),
        int(fb.std_dev),
    )

    _stop_calibration(runtime, cal_weight_mg, st_error, log, reason="stop procedure test")
    guard.done = True
    log.info("STOP_CALIBRATION succeeded (observed CALIBRATION_FEEDBACK state==ERROR).")

    _assert_feedback_goes_quiet(
        runtime, _is_cal_feedback_push, quiet_period_s=stop_quiet_s, within_s=stop_effect_timeout_s
    )
    log.info(
        "STOP_CALIBRATION verified: feedback became quiet for %.1fs within %.1fs.",
        stop_quiet_s,
        stop_effect_timeout_s,
    )


@pytest.mark.hil
@pytest.mark.order(2)
def test_ppi_ad_calibration_firmware_timeout_stop_behavior(runtime: MessageProtocolRuntime) -> None:
    """
    Firmware-timeout STOP behavior:
      - START (ok==1)
      - Do not progress flow
      - Expect ERROR feedback within fw_timeout_wait_s
      - Optional quiet check afterwards
    """
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    fw_timeout_wait_s = float(os.getenv("MP_HIL_CAL_FW_TIMEOUT_WAIT_S", "320.0"))
    quiet_s = float(os.getenv("MP_HIL_CAL_STOP_QUIET_S", "1.0"))
    quiet_within_s = float(os.getenv("MP_HIL_CAL_STOP_EFFECT_TIMEOUT_S", "10.0"))

    cal_weight_mg = _get_cal_weight_mg()
    log = logging.getLogger("pytest.hil.calibration.fw_timeout")

    st_error = mp_enums.CALIBRATION_STATE.CALIBRATION_STATE_ERROR

    start_rq = _build_start_calibration_rq(True, cal_weight_mg)
    assert runtime.enqueue_tx_packet(start_rq), "Failed to enqueue START_CALIBRATION RQ."
    log.info(
        "Sent START_CALIBRATION RQ: payload_hex=%s cal_weight_mg=%d",
        bytes(start_rq.payload[: start_rq.pkt_payload_len]).hex(),
        cal_weight_mg,
    )

    start_re = _wait_for_response(runtime, _is_start_calibration_re, timeout_s, "START_CALIBRATION RE")
    log.info("Received START_CALIBRATION RE: ok=%d", int(start_re.payload[0]))
    assert int(start_re.payload[0]) == 1, (
        f"START_CALIBRATION RE indicated failure (payload[0]={int(start_re.payload[0])})."
    )

    err_pkt = _wait_for_optional(
        runtime,
        partial(_is_cal_feedback_state, state_code=st_error),
        timeout_s=fw_timeout_wait_s,
        poll_interval_s=0.01,
    )
    assert err_pkt is not None, (
        f"Did not observe CALIBRATION_FEEDBACK state==ERROR within {fw_timeout_wait_s:.1f}s "
        "(increase MP_HIL_CAL_FW_TIMEOUT_WAIT_S if firmware timeout is longer)."
    )

    fb = mp_structs.CalibrationFeedback.from_buffer_copy(bytes(err_pkt.payload[: err_pkt.pkt_payload_len]))
    log.info(
        "Observed firmware timeout via CALIBRATION_FEEDBACK state==ERROR: state=%d ring_present=%d avg_weight_mg=%d std_dev=%d",
        int(fb.current_state),
        int(fb.is_ring_present),
        int(fb.avg_weight_mg),
        int(fb.std_dev),
    )

    _assert_feedback_goes_quiet(runtime, _is_cal_feedback_push, quiet_period_s=quiet_s, within_s=quiet_within_s)
    log.info(
        "Firmware-timeout stop verified: feedback became quiet for %.1fs within %.1fs.",
        quiet_s,
        quiet_within_s,
    )
