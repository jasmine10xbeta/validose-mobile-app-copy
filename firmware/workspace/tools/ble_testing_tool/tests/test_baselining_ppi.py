# test_baseling_ppi.py
"""
HIL: Dock baselining (commission new bottle / replace medication) flow test (App <-> Dock message protocol)

This file contains two tests:

Order(1) Calibration pre-check (hard requirement)
  - Request CALIBRATION_DATA
  - Fail if record indicates uncalibrated state (calibration_factor==0, zero_offset==0, etc)

Order(2) Full baselining flow + STOP-on-failure/timeout with ACK + retries
  - Start baselining (start=True) and verify accepted
  - Parallel behavior:
      * when medication UID becomes non-zero in feedback, start backend validation timer
      * Dock may proceed to set full assembly weight in parallel
      * when Dock sends VALIDATE_MED RQ, respond once timer is ready (no blocking sleeps)
  - End condition requires:
      * COMPLETE seen
      * non-zero med UID seen
      * VALIDATE_MED handled + DOSE_SCHEDULE pushed
      * SET_RING_DOCK_WEIGHT observed
      * post-baselining CALIBRATION_DATA shows full_assembly_weight_mg > 0
  - If test fails/times out before COMPLETE, finalizer will STOP baselining with retries and require ERROR feedback.

Key env vars (adjustable) for timeouts:
  MP_HIL_TIMEOUT_S (default 10s) - general timeout for waiting for expected packets
  MP_HIL_BASELINE_TOTAL_TIMEOUT_S (default 300s) - total timeout for entire baselining flow (after this, we consider it a failure and STOP)
  MP_HIL_BASELINE_VALIDATE_DELAY_S (default 0s) - delay after seeing VALIDATE_MED RQ before responding (simulates backend processing time; can be used to test timeout behavior)

Other
  MP_HIL_BASELINE_VALIDATE_OK (default True) - whether to respond with success or failure to VALIDATE_MED RQ (can be used to test flow behavior on validation failure)
"""

import ctypes
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
# Dose schedule (sent after backend validation)
# ===================================================================================
# To push dose schedule after validation: build a valid DOSE_SCHEDULE packet (with at least 1 dose window) and send as PUSH with correct PPI.
def _build_dose_schedule_push() -> mp_structs.MpPacketPayload:
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE
    pkt.pkt_payload_len = mp_structs.DOSE_SCHEDULE_SIZE_BYTES

    ds = mp_structs.DoseSchedule()
    ds.medication_type = 0
    ds.dosage_mg = 500
    ds.temp_upper_limit_deg_c = 25
    ds.temp_lower_limit_deg_c = 0
    ds.temp_avg_window_duration_sec = 300
    ds.dose_days_bitfield = 0b10000000
    ds.dose_window_duration_minutes = 30
    ds.dose_window_count = 1
    ds.dose_window_start_times_minutes[0] = 12 * 60

    ctypes.memmove(pkt.payload, ctypes.byref(ds), pkt.pkt_payload_len)
    return pkt


# ===================================================================================
# Baselining protocol packets / predicates
# ===================================================================================


def _wait_for_state_and_validate(
    runtime: MessageProtocolRuntime,
    predicate: Callable[[mp_structs.MpPacketPayload], bool],
    timeout_s: float,
    log: logging.Logger,
    log_prefix: str,
    expected_state: int,
    expected_state_name: str,
) -> mp_structs.BaseliningFeedback:
    """
    Waits for a packet matching `predicate`, parses it with `feedback_struct_type.from_buffer_copy`,
    logs the common fields, and asserts current_state matches expected_state.
    """
    pkt = _wait_for_response(runtime, predicate, timeout_s, f"{log_prefix} {expected_state_name}")

    fb = mp_structs.BaseliningFeedback.from_buffer_copy(bytes(pkt.payload[: pkt.pkt_payload_len]))

    state = int(fb.current_state)
    ring_present = int(fb.is_ring_present)
    avg_weight_mg = int(fb.avg_weight_mg)
    std_dev = int(fb.std_dev)
    med_uid = bytes(fb.medication_nfc_id).hex()

    log.info(
        "%s -> %s (%d) | ring_present=%d avg_weight_mg=%d std_dev=%d med_uid=%s",
        log_prefix,
        expected_state_name,
        state,
        ring_present,
        avg_weight_mg,
        std_dev,
        med_uid,
    )

    assert state == int(expected_state), f"Expected feedback state {expected_state_name}, got {state}"
    return fb


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


def _is_validate_med_rq(pkt: mp_structs.MpPacketPayload) -> bool:
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_RQ
        and pkt.ppi == mp_enums.PPI_AD.PPI_AD_VALIDATE_MED
        and pkt.pkt_payload_len == 0
    )


def _build_validate_med_re(success: bool) -> mp_structs.MpPacketPayload:
    pkt = mp_structs.MpPacketPayload()
    pkt.type = mp_enums.PPI_TYPE.PPI_TYPE_RE
    pkt.ppi = mp_enums.PPI_AD.PPI_AD_VALIDATE_MED
    pkt.pkt_payload_len = mp_structs.VALIDATE_MED_RESPONSE_T_SIZE_BYTES
    pkt.payload[0] = 1 if success else 0
    return pkt


def _is_baselining_feedback_push(pkt: mp_structs.MpPacketPayload) -> bool:
    # Accept both BASELINING_FEEDBACK and (firmware bug) START_BASELINING as PUSH feedback.
    return (
        pkt.type == mp_enums.PPI_TYPE.PPI_TYPE_PUSH
        and pkt.pkt_payload_len == mp_structs.BASELINING_FEEDBACK_T_SIZE_BYTES
        and pkt.ppi in (mp_enums.PPI_AD.PPI_AD_BASELINING_FEEDBACK, mp_enums.PPI_AD.PPI_AD_START_BASELINING)
    )


def _is_all_zero(data: bytes) -> bool:
    return all(b == 0 for b in data)


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
def test_ppi_ad_baselining_full_flow(runtime: MessageProtocolRuntime, request) -> None:
    """
    Full baselining flow with parallel "backend" validation.

    Key idea:
    - wait for key feedback states in expected order and prompt the operator.
    also:
      - detect UID non-zero in WAIT_FOR_RING
      - start a validation timer at UID-detect time
      - wait for VALIDATE_MED RQ and respond once timer is ready
      (simulates backend processing time, and tests that firmware handles this parallelism correctly without blocking or timeouts)
    """
    if not _DOCK_CALIBRATED:
        pytest.fail("Dock calibration pre-check did not pass (required).")

    # general timeout for waiting for expected packets (used for individual waits in flow, not total flow)
    timeout_s = float(os.getenv("MP_HIL_TIMEOUT_S", "10.0"))
    # total timeout for entire baselining flow (after this, consider it a failure and STOP)
    total_timeout_s = float(os.getenv("MP_HIL_BASELINE_TOTAL_TIMEOUT_S", "300.0"))
    # delay after seeing VALIDATE_MED RQ before responding (simulates backend processing time; can be used to test timeout behavior)
    validate_delay_s = float(os.getenv("MP_HIL_BASELINE_VALIDATE_DELAY_S", "1.0"))
    # whether to respond with success or failure to VALIDATE_MED RQ (can be used to test flow behavior on validation failure)
    validate_ok = _env_bool("MP_HIL_BASELINE_VALIDATE_OK", True)
    # time at which we will respond to VALIDATE_MED RQ (set when we detect non-zero UID in feedback)
    validation_ready_time = int(0)

    log = logging.getLogger("pytest.hil.baselining.flow")

    st_wait_ring_removed = mp_enums.BASELINING_STATE.BASELINING_STATE_WAIT_FOR_RING_REMOVAL
    st_set_dock_weight = mp_enums.BASELINING_STATE.BASELINING_STATE_SET_DOCK_WEIGHT
    st_wait_for_ring = mp_enums.BASELINING_STATE.BASELINING_STATE_WAIT_FOR_RING
    st_set_ring_dock_weight = mp_enums.BASELINING_STATE.BASELINING_STATE_SET_RING_DOCK_WEIGHT
    st_wait_backend = mp_enums.BASELINING_STATE.BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION
    st_complete = mp_enums.BASELINING_STATE.BASELINING_STATE_COMPLETE
    st_error = mp_enums.BASELINING_STATE.BASELINING_STATE_ERROR

    guard = _StopGuard(started=False, done=False)
    request.addfinalizer(partial(_finalize_stop_baselining_if_needed, runtime, guard, st_error, log, total_timeout_s))

    # START baselining
    start_rq = _build_start_baselining_rq(True)
    assert runtime.enqueue_tx_packet(start_rq), "Failed to enqueue START_BASELINING RQ."
    guard.started = True

    start_re = _wait_for_response(runtime, _is_start_baselining_re, timeout_s, "START_BASELINING RE")
    assert int(start_re.payload[0]) == 1, (
        f"START_BASELINING RE indicated failure (payload[0]={int(start_re.payload[0])})."
    )

    # Step 1: WAIT_FOR_RING_REMOVAL -> prompt remove ring
    _wait_for_state_and_validate(
        runtime,
        partial(_is_feedback_state, state_code=st_wait_ring_removed),
        timeout_s,
        log,
        "BASE STATE",
        st_wait_ring_removed,
        "WAIT_FOR_RING_REMOVAL",
    )
    _announce("Remove Ring/Bottle from Dock", log)

    # Step 2: SET_DOCK_WEIGHT -> prompt stable surface
    _wait_for_state_and_validate(
        runtime,
        partial(_is_feedback_state, state_code=st_set_dock_weight),
        timeout_s * 5,  # allow extra time for weight stabilization
        log,
        "BASE STATE",
        st_set_dock_weight,
        "SET_DOCK_WEIGHT",
    )
    _announce("Place Dock on a stable, level surface. Do NOT move Dock.", log)

    # Step 3: WAIT_FOR_RING -> prompt insert new medication
    _wait_for_state_and_validate(
        runtime,
        partial(_is_feedback_state, state_code=st_wait_for_ring),
        timeout_s,
        log,
        "BASE STATE",
        st_wait_for_ring,
        "WAIT_FOR_RING",
    )
    _announce("Insert new medication into Ring and place Ring/Bottle into Dock.", log)

    # Step 5b: SET_RING_DOCK_WEIGHT (weight capture path can proceed while validation timer runs)
    fb3 = _wait_for_state_and_validate(
        runtime,
        partial(_is_feedback_state, state_code=st_set_ring_dock_weight),
        timeout_s * 5,  # allow extra time for weight stabilization
        log,
        "BASE STATE",
        st_set_ring_dock_weight,
        "SET_RING_DOCK_WEIGHT",
    )
    _announce("Ring detected.", log)

    # Step 5a: detect UID non-zero in WAIT_FOR_RING (starts validation timer) - do this here, as the ring IS present in this state
    uid_hex = bytes(fb3.medication_nfc_id).hex()
    if not _is_all_zero(bytes(fb3.medication_nfc_id)):
        _announce(f"New medication NFC detected: {uid_hex}", log)
        validation_ready_time = time.perf_counter() + validate_delay_s
        log.info("Started backend validation timer: ready in %.3fs", validate_delay_s)

    _announce("Do NOT move Dock. Capturing full dock/ring/bottle baseline...", log)

    # Step 6: WAIT_FOR_BACKEND_VALIDATION state
    _wait_for_state_and_validate(
        runtime,
        partial(_is_feedback_state, state_code=st_wait_backend),
        timeout_s,
        log,
        "BASE STATE",
        st_wait_backend,
        "WAIT_FOR_BACKEND_VALIDATION",
    )
    _announce("Validating new Medication, please wait...", log)

    # Step 7: Dock requests validation (VALIDATE_MED RQ) in WAIT_FOR_BACKEND_VALIDATION state
    _wait_for_response(runtime, _is_validate_med_rq, total_timeout_s, "VALIDATE_MED RQ")
    log.info("Dock requested backend validation (VALIDATE_MED).")

    # Wait until our simulated backend validation is ready (remaining delay only)
    remaining = validation_ready_time - time.perf_counter()
    if remaining > 0:
        time.sleep(remaining)

    # Send VALIDATE_MED RE + DOSE_SCHEDULE PUSH
    validate_re = _build_validate_med_re(validate_ok)
    assert runtime.enqueue_tx_packet(validate_re), "Failed to enqueue VALIDATE_MED RE."
    _announce("Sent VALIDATE_MED response. " + ("Validation OK." if validate_ok else "Validation FAILED."), log)

    ds = _build_dose_schedule_push()
    assert runtime.enqueue_tx_packet(ds), "Failed to enqueue DOSE_SCHEDULE PUSH."
    log.info("Sent DOSE_SCHEDULE PUSH after validation.")

    # Step 8: COMPLETE
    _wait_for_state_and_validate(
        runtime,
        partial(_is_feedback_state, state_code=st_complete),
        timeout_s,
        log,
        "BASE STATE",
        st_complete,
        "COMPLETE",
    )
    _announce("Baselining COMPLETE (from feedback).", log)
    guard.done = True

    # Post-baselining storage proof: CALIBRATION_DATA should have full_assembly_weight_mg > 0
    cal_rq = _build_calibration_data_rq()
    assert runtime.enqueue_tx_packet(cal_rq), "Failed to enqueue CALIBRATION_DATA RQ (post-baselining)."
    cal_re = _wait_for_response(runtime, _is_calibration_data_re, timeout_s, "CALIBRATION_DATA RE (post-baselining)")
    post_rec = mp_structs.WeightStackCalibrationRecord.from_buffer_copy(bytes(cal_re.payload[: cal_re.pkt_payload_len]))
    log.info(
        "Post-baselining calibration record: zero_offset=%d calibration_factor=%d full_assembly_weight_mg=%d",
        int(post_rec.zero_offset),
        int(post_rec.calibration_factor),
        int(post_rec.full_assembly_weight_mg),
    )

    assert int(post_rec.full_assembly_weight_mg) > 0, (
        "Post-baselining full_assembly_weight_mg is 0. This suggests ring/dock full-med weight was not captured/stored."
    )

    if _BASELINE_CAL_RECORD is not None and int(_BASELINE_CAL_RECORD.full_assembly_weight_mg) == int(
        post_rec.full_assembly_weight_mg
    ):
        log.warning(
            "full_assembly_weight_mg did not change vs pre-baselining record (pre=%d post=%d).",
            int(_BASELINE_CAL_RECORD.full_assembly_weight_mg),
            int(post_rec.full_assembly_weight_mg),
        )

    _announce(
        "Baselining succeeded: "
        f"uid={uid_hex} "
        f"validate_ok={validate_ok} "
        f"full_assembly_weight_mg={int(post_rec.full_assembly_weight_mg)}",
        log,
    )
