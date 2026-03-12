from __future__ import annotations

import atexit
import ctypes
import json
import logging
import os
import secrets
import struct
import sys
import threading
import time
from collections import deque
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Deque, Optional

from flask import Flask, Response, jsonify, render_template, request, stream_with_context

APP_DIR = Path(__file__).resolve().parent
TESTS_DIR = APP_DIR.parent / "tests"
if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from message_protocol_runtime import MessageProtocolRuntime  # noqa: E402
from utils import mp_enums  # noqa: E402
from utils import mp_structs  # noqa: E402

_BATTERY_CHARGE_STATUS_NAMES = {
    0: "SOC_GOOD",
    1: "SOC_LOW",
    2: "CHARGING",
    3: "CHARGING_COMPLETED",
    4: "ERROR",
}
_MAX_INCOMING_MESSAGES = 500


def _env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


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


def _format_timestamp_utc(timestamp_unix_s: int) -> str:
    try:
        return datetime.fromtimestamp(timestamp_unix_s, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    except (OverflowError, OSError, ValueError):
        return f"{timestamp_unix_s} (invalid unix timestamp)"


def _format_timestamp_utc_or_unset(timestamp_unix_s: int) -> str:
    if timestamp_unix_s == 0:
        return "0 (unset)"
    return _format_timestamp_utc(timestamp_unix_s)


def _format_nfc_id_hex(raw_id: bytes | bytearray | ctypes.Array) -> str:
    return ":".join(f"{byte:02X}" for byte in bytes(raw_id))


def _coerce_int(name: str, value: object, min_value: int, max_value: int) -> int:
    try:
        parsed = int(value)  # type: ignore[arg-type]
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid value for '{name}': {value}") from exc

    if parsed < min_value or parsed > max_value:
        raise ValueError(f"'{name}' out of range [{min_value}, {max_value}]: {parsed}")

    return parsed


def _empty_dose_schedule_dict() -> dict:
    return {
        "medication_type": 0,
        "dosage_mg": 0,
        "temp_upper_limit_deg_c": 0,
        "temp_lower_limit_deg_c": 0,
        "temp_avg_window_duration_sec": 0,
        "dose_days_bitfield": 0,
        "dose_window_duration_minutes": 0,
        "dose_window_count": 0,
        "dose_window_start_times_minutes": [0] * mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY,
    }


def _copy_dose_schedule_dict(schedule: dict) -> dict:
    copied = dict(schedule)
    copied["dose_window_start_times_minutes"] = list(schedule["dose_window_start_times_minutes"])
    return copied


def _copy_dock_status_dict(dock_status: dict) -> dict:
    copied = dict(dock_status)
    copied["hardware_version"] = dict(dock_status.get("hardware_version", {}))
    copied["firmware_version"] = dict(dock_status.get("firmware_version", {}))
    return copied


def _copy_ring_status_dict(ring_status: dict) -> dict:
    copied = dict(ring_status)
    copied["hardware_version"] = dict(ring_status.get("hardware_version", {}))
    copied["firmware_version"] = dict(ring_status.get("firmware_version", {}))
    return copied


def _copy_dose_event_dict(dose_event: dict) -> dict:
    return dict(dose_event)


def _copy_weight_measurement_dict(weight_measurement: dict) -> dict:
    return dict(weight_measurement)


def _time_dict_from_unix(unix_time_s: int) -> dict:
    normalized_unix_time_s = int(unix_time_s) & 0xFFFFFFFF
    return {
        "unix_time_s": normalized_unix_time_s,
        "unix_time_utc": _format_timestamp_utc_or_unset(normalized_unix_time_s),
    }


def _copy_time_dict(time_data: dict) -> dict:
    return {
        "unix_time_s": int(time_data.get("unix_time_s", 0)),
        "unix_time_utc": str(time_data.get("unix_time_utc", _format_timestamp_utc_or_unset(0))),
    }


def _semantic_version_to_dict(version: mp_structs.SemanticVersion) -> dict:
    major = int(version.major)
    minor = int(version.minor)
    patch = int(version.patch)
    return {
        "major": major,
        "minor": minor,
        "patch": patch,
        "value": f"{major}.{minor}.{patch}",
    }


def _dock_status_struct_to_dict(dock_status: mp_structs.DockStatus) -> dict:
    timestamp_unix_s = int(dock_status.timestamp_unix_s)
    ship_mode_exit_timestamp_unix = int(dock_status.ship_mode_exit_timestamp_unix)

    return {
        "hardware_version": _semantic_version_to_dict(dock_status.hardware_version),
        "firmware_version": _semantic_version_to_dict(dock_status.firmware_version),
        "mac_hex": _format_nfc_id_hex(dock_status.mac),
        "timestamp_unix_s": timestamp_unix_s,
        "timestamp_utc": _format_timestamp_utc_or_unset(timestamp_unix_s),
        "ship_mode_exit_timestamp_unix": ship_mode_exit_timestamp_unix,
        "ship_mode_exit_timestamp_utc": _format_timestamp_utc_or_unset(ship_mode_exit_timestamp_unix),
        "uptime_s": int(dock_status.uptime_s),
    }


def _battery_charge_status_name(charge_status: int) -> str:
    return _BATTERY_CHARGE_STATUS_NAMES.get(charge_status, f"UNKNOWN({charge_status})")


def _ring_status_struct_to_dict(ring_status: mp_structs.RingStatus) -> dict:
    timestamp_unix_s = int(ring_status.timestamp_unix_s)
    ship_mode_exit_timestamp_unix = int(ring_status.ship_mode_exit_timestamp_unix)
    battery_charge_status = int(ring_status.battery_charge_status)

    return {
        "hardware_version": _semantic_version_to_dict(ring_status.hardware_version),
        "firmware_version": _semantic_version_to_dict(ring_status.firmware_version),
        "mac_hex": _format_nfc_id_hex(ring_status.mac),
        "timestamp_unix_s": timestamp_unix_s,
        "timestamp_utc": _format_timestamp_utc_or_unset(timestamp_unix_s),
        "ship_mode_exit_timestamp_unix": ship_mode_exit_timestamp_unix,
        "ship_mode_exit_timestamp_utc": _format_timestamp_utc_or_unset(ship_mode_exit_timestamp_unix),
        "battery_charge_status": battery_charge_status,
        "battery_charge_status_name": _battery_charge_status_name(battery_charge_status),
        "uptime_s": int(ring_status.uptime_s),
        "temperature_celsius": int(ring_status.temperature_celsius),
        "dose_fifo_used_percent": int(ring_status.dose_fifo_used_percent),
        "battery_fifo_used_percent": int(ring_status.battery_fifo_used_percent),
        "imu_fifo_used_percent": int(ring_status.imu_fifo_used_percent),
        "error_fifo_used_percent": int(ring_status.error_fifo_used_percent),
        "dose_fifo_used_percent_watermark": int(ring_status.dose_fifo_used_percent_watermark),
        "battery_fifo_used_percent_watermark": int(ring_status.battery_fifo_used_percent_watermark),
        "imu_fifo_used_percent_watermark": int(ring_status.imu_fifo_used_percent_watermark),
        "error_fifo_used_percent_watermark": int(ring_status.error_fifo_used_percent_watermark),
        "battery_sample_frequency_millihz": int(ring_status.battery_sample_frequency_millihz),
    }


def _normalize_dose_schedule_dict(raw: dict) -> dict:
    if not isinstance(raw, dict):
        raise ValueError("Dose schedule payload must be a JSON object.")

    normalized = _empty_dose_schedule_dict()

    normalized["medication_type"] = _coerce_int("medication_type", raw.get("medication_type", 0), 0, 0xFF)
    normalized["dosage_mg"] = _coerce_int("dosage_mg", raw.get("dosage_mg", 0), 0, 0xFFFF)
    normalized["temp_upper_limit_deg_c"] = _coerce_int(
        "temp_upper_limit_deg_c", raw.get("temp_upper_limit_deg_c", 0), -128, 127
    )
    normalized["temp_lower_limit_deg_c"] = _coerce_int(
        "temp_lower_limit_deg_c", raw.get("temp_lower_limit_deg_c", 0), -128, 127
    )
    normalized["temp_avg_window_duration_sec"] = _coerce_int(
        "temp_avg_window_duration_sec", raw.get("temp_avg_window_duration_sec", 0), 0, 0xFFFF
    )
    normalized["dose_days_bitfield"] = _coerce_int("dose_days_bitfield", raw.get("dose_days_bitfield", 0), 0, 0xFF)
    normalized["dose_window_duration_minutes"] = _coerce_int(
        "dose_window_duration_minutes", raw.get("dose_window_duration_minutes", 0), 0, 0xFF
    )
    normalized["dose_window_count"] = _coerce_int(
        "dose_window_count", raw.get("dose_window_count", 0), 0, mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY
    )

    raw_times = raw.get("dose_window_start_times_minutes", [])
    if raw_times is None:
        raw_times = []
    if not isinstance(raw_times, list):
        raise ValueError("'dose_window_start_times_minutes' must be a list.")
    if len(raw_times) > mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY:
        raise ValueError(
            f"'dose_window_start_times_minutes' can contain at most {mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY} values."
        )

    times: list[int] = []
    for idx, value in enumerate(raw_times):
        times.append(_coerce_int(f"dose_window_start_times_minutes[{idx}]", value, 0, 0xFFFF))
    while len(times) < mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY:
        times.append(0)

    normalized["dose_window_start_times_minutes"] = times
    return normalized


def _dose_schedule_struct_to_dict(dose_schedule: mp_structs.DoseSchedule) -> dict:
    return {
        "medication_type": int(dose_schedule.medication_type),
        "dosage_mg": int(dose_schedule.dosage_mg),
        "temp_upper_limit_deg_c": int(dose_schedule.temp_upper_limit_deg_c),
        "temp_lower_limit_deg_c": int(dose_schedule.temp_lower_limit_deg_c),
        "temp_avg_window_duration_sec": int(dose_schedule.temp_avg_window_duration_sec),
        "dose_days_bitfield": int(dose_schedule.dose_days_bitfield),
        "dose_window_duration_minutes": int(dose_schedule.dose_window_duration_minutes),
        "dose_window_count": int(dose_schedule.dose_window_count),
        "dose_window_start_times_minutes": [
            int(dose_schedule.dose_window_start_times_minutes[i])
            for i in range(mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY)
        ],
    }


def _dose_schedule_dict_to_struct(schedule: dict) -> mp_structs.DoseSchedule:
    dose_schedule = mp_structs.DoseSchedule()
    dose_schedule.medication_type = schedule["medication_type"]
    dose_schedule.dosage_mg = schedule["dosage_mg"]
    dose_schedule.temp_upper_limit_deg_c = schedule["temp_upper_limit_deg_c"]
    dose_schedule.temp_lower_limit_deg_c = schedule["temp_lower_limit_deg_c"]
    dose_schedule.temp_avg_window_duration_sec = schedule["temp_avg_window_duration_sec"]
    dose_schedule.dose_days_bitfield = schedule["dose_days_bitfield"]
    dose_schedule.dose_window_duration_minutes = schedule["dose_window_duration_minutes"]
    dose_schedule.dose_window_count = schedule["dose_window_count"]
    for i in range(mp_structs.DOSE_SCHEDULE_MAX_DOSES_PER_DAY):
        dose_schedule.dose_window_start_times_minutes[i] = schedule["dose_window_start_times_minutes"][i]
    return dose_schedule


def _build_dose_schedule_push_packet(schedule: dict) -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_PUSH
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE
    packet.pkt_payload_len = mp_structs.DOSE_SCHEDULE_SIZE_BYTES

    dose_schedule = _dose_schedule_dict_to_struct(schedule)
    ctypes.memmove(packet.payload, ctypes.byref(dose_schedule), packet.pkt_payload_len)
    return packet


def _build_dose_schedule_request_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE
    packet.pkt_payload_len = 0
    return packet


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


def _build_ring_status_request_packet() -> mp_structs.MpPacketPayload:
    packet = mp_structs.MpPacketPayload()
    packet.type = mp_enums.PPI_TYPE.PPI_TYPE_RQ
    packet.ppi = mp_enums.PPI_AD.PPI_AD_RING_STATUS
    packet.pkt_payload_len = 0
    return packet


def _extract_dose_schedule_from_response(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_RE)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_DOSE_SCHEDULE)
        and int(packet.pkt_payload_len) == int(mp_structs.DOSE_SCHEDULE_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.DoseSchedule.from_buffer_copy(payload)
        return _dose_schedule_struct_to_dict(parsed)
    return None


def _extract_temperature_log_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_DOCK_TEMP_LOG)
        and int(packet.pkt_payload_len) == int(mp_structs.TEMPERATURE_LOG_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.TemperatureLog.from_buffer_copy(payload)
        timestamp_unix_s = int(parsed.timestamp_unix_s)
        temperature_deg_c = int(parsed.temperature_deg_c)
        timestamp_utc = _format_timestamp_utc(timestamp_unix_s)

        return {
            "timestamp_unix_s": timestamp_unix_s,
            "timestamp_utc": timestamp_utc,
            "temperature_deg_c": temperature_deg_c,
        }
    return None


def _extract_dock_battery_level_log_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_DOCK_BATT_LEVEL_LOG)
        and int(packet.pkt_payload_len) == int(mp_structs.BATTERY_LEVEL_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.BatteryLevel.from_buffer_copy(payload)
        timestamp_unix_s = int(parsed.timestamp_unix_s)
        battery_level = int(parsed.battery_level)
        timestamp_utc = _format_timestamp_utc(timestamp_unix_s)

        return {
            "timestamp_unix_s": timestamp_unix_s,
            "timestamp_utc": timestamp_utc,
            "battery_level": battery_level,
        }
    return None


def _extract_ring_battery_level_log_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_RING_BATT_LEVEL_LOG)
        and int(packet.pkt_payload_len) == int(mp_structs.BATTERY_LEVEL_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.BatteryLevel.from_buffer_copy(payload)
        timestamp_unix_s = int(parsed.timestamp_unix_s)
        battery_level = int(parsed.battery_level)
        timestamp_utc = _format_timestamp_utc(timestamp_unix_s)

        return {
            "timestamp_unix_s": timestamp_unix_s,
            "timestamp_utc": timestamp_utc,
            "battery_level": battery_level,
        }
    return None


def _extract_dose_event_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_DOSE_EVENT_REPORT)
        and int(packet.pkt_payload_len) == int(mp_structs.DOSE_EVENT_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.DoseEvent.from_buffer_copy(payload)
        days_since_epoch = int(parsed.event_id.days_since_epoch)
        event_ctr = int(parsed.event_id.event_ctr)
        start_timestamp_unix_s = int(parsed.start_timestamp_unix_s)
        duration_s = int(parsed.duration_s)
        dose_completed_in_time = int(parsed.dose_completed_in_time)
        tilt_count = int(parsed.tilt_count)

        return {
            "event_id_days_since_epoch": days_since_epoch,
            "event_id_event_ctr": event_ctr,
            "event_id_label": f"{days_since_epoch}:{event_ctr}",
            "start_timestamp_unix_s": start_timestamp_unix_s,
            "start_timestamp_utc": _format_timestamp_utc_or_unset(start_timestamp_unix_s),
            "duration_s": duration_s,
            "dose_completed_in_time": dose_completed_in_time,
            "dose_completed_in_time_bool": dose_completed_in_time == 1,
            "tilt_count": tilt_count,
        }
    return None


def _extract_weight_measurement_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_DOCK_WEIGHT_LOG)
        and int(packet.pkt_payload_len) == int(mp_structs.DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.WeightMeasurement.from_buffer_copy(payload)
        timestamp_unix_s = int(parsed.timestamp_unix_s)
        temperature_deg_c_div10 = int(parsed.temperature_deg_c_div10)
        temperature_deg_c = temperature_deg_c_div10 / 10.0

        return {
            "weight_mg": int(parsed.weight_mg),
            "total_dispensed_mg": int(parsed.total_dispensed_mg),
            "std_dev": int(parsed.std_dev),
            "temperature_deg_c_div10": temperature_deg_c_div10,
            "temperature_deg_c": temperature_deg_c,
            "timestamp_unix_s": timestamp_unix_s,
            "timestamp_utc": _format_timestamp_utc(timestamp_unix_s),
        }
    return None


def _extract_ring_docked_status_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_RING_DOCKED_STATUS)
        and int(packet.pkt_payload_len) == int(mp_structs.RING_DOCKED_STATUS_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.RingDockedStatus.from_buffer_copy(payload)
        timestamp_unix_s = int(parsed.timestamp_unix_s)
        docked_status = int(parsed.docked_status)

        if docked_status == 0:
            docked_status_name = "UNDOCKED"
            is_docked = False
        elif docked_status == 1:
            docked_status_name = "DOCKED"
            is_docked = True
        else:
            docked_status_name = f"UNKNOWN({docked_status})"
            is_docked = False

        return {
            "timestamp_unix_s": timestamp_unix_s,
            "timestamp_utc": _format_timestamp_utc(timestamp_unix_s),
            "docked_status": docked_status,
            "docked_status_name": docked_status_name,
            "is_docked": is_docked,
            "ring_nfc_id_hex": _format_nfc_id_hex(parsed.ring_nfc_id),
            "medication_nfc_id_hex": _format_nfc_id_hex(parsed.medication_nfc_id),
        }
    return None


def _extract_ring_status_from_push(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) in (
            int(mp_enums.PPI_TYPE.PPI_TYPE_PUSH),
            int(mp_enums.PPI_TYPE.PPI_TYPE_RE),
        )
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_RING_STATUS)
        and int(packet.pkt_payload_len) == int(mp_structs.RING_STATUS_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.RingStatus.from_buffer_copy(payload)
        return _ring_status_struct_to_dict(parsed)
    return None


def _extract_dock_status_from_response(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_RE)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_DOCK_STATUS)
        and int(packet.pkt_payload_len) == int(mp_structs.DOCK_STATUS_T_SIZE_BYTES)
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        parsed = mp_structs.DockStatus.from_buffer_copy(payload)
        return _dock_status_struct_to_dict(parsed)
    return None


def _extract_time_from_response(packet: mp_structs.MpPacketPayload) -> Optional[dict]:
    if (
        int(packet.type) == int(mp_enums.PPI_TYPE.PPI_TYPE_RE)
        and int(packet.ppi) == int(mp_enums.PPI_AD.PPI_AD_TIME)
        and int(packet.pkt_payload_len) == 4
    ):
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        unix_time_s = struct.unpack("<I", payload)[0]
        return _time_dict_from_unix(unix_time_s)
    return None


@dataclass(frozen=True)
class PacketEvent:
    id: int
    received_at_utc: str
    session_id: str
    type: int
    type_name: str
    ppi: int
    ppi_name: str
    payload_len: int
    payload_hex: str
    dose_schedule: Optional[dict] = None
    dock_status: Optional[dict] = None
    ring_status: Optional[dict] = None
    dose_event: Optional[dict] = None
    weight_measurement: Optional[dict] = None
    temperature_log: Optional[dict] = None
    dock_battery_level_log: Optional[dict] = None
    ring_battery_level_log: Optional[dict] = None
    ring_docked_status: Optional[dict] = None
    unix_time: Optional[dict] = None


class SimpleRuntimeMonitor:
    def __init__(self, max_messages: int = _MAX_INCOMING_MESSAGES) -> None:
        self._host = os.getenv("MP_HIL_HOST", "localhost")
        self._port = int(os.getenv("MP_HIL_PORT", "5000"))
        self._is_master = _env_bool("MP_HIL_IS_MASTER", True)
        self._process_interval_s = float(os.getenv("MP_HIL_PROCESS_INTERVAL_S", "0.001"))
        self._session_id, self._session_id_source = _resolve_session_id()

        self._runtime: Optional[MessageProtocolRuntime] = None
        self._lock = threading.Lock()

        self._messages: Deque[PacketEvent] = deque(maxlen=max(1, min(max_messages, _MAX_INCOMING_MESSAGES)))
        self._next_id = 1
        self._dose_schedule = _empty_dose_schedule_dict()
        self._unix_time = _time_dict_from_unix(0)
        self._dock_status: Optional[dict] = None
        self._ring_status: Optional[dict] = None
        self._dose_events: Deque[dict] = deque(
            maxlen=int(os.getenv("MP_MONITOR_MAX_DOSE_EVENTS", "500"))
        )
        self._weight_measurements: Deque[dict] = deque(
            maxlen=int(os.getenv("MP_MONITOR_MAX_WEIGHT_MEASUREMENTS", "500"))
        )
        self._temperature_points: Deque[dict] = deque(
            maxlen=int(os.getenv("MP_MONITOR_MAX_TEMP_POINTS", "2000"))
        )
        self._dock_battery_level_points: Deque[dict] = deque(
            maxlen=int(os.getenv("MP_MONITOR_MAX_DOCK_BATT_LEVEL_POINTS", "2000"))
        )
        self._ring_battery_level_points: Deque[dict] = deque(
            maxlen=int(os.getenv("MP_MONITOR_MAX_RING_BATT_LEVEL_POINTS", "2000"))
        )
        self._ring_docked_status_points: Deque[dict] = deque(
            maxlen=int(os.getenv("MP_MONITOR_MAX_RING_DOCKED_STATUS_POINTS", "2000"))
        )

        self._logger = logging.getLogger("mp_monitor.runtime")

    def ensure_started(self) -> None:
        with self._lock:
            if self._runtime is not None:
                return
            self._start_unlocked()

    def start(self) -> None:
        with self._lock:
            if self._runtime is not None:
                return
            self._start_unlocked()

    def _start_unlocked(self) -> None:
        self._logger.info(
            "Starting runtime host=%s port=%d is_master=%s session_id=%#010x",
            self._host,
            self._port,
            self._is_master,
            self._session_id,
        )

        self._runtime = MessageProtocolRuntime(
            serial_dev=self._host.encode("ascii"),
            port=self._port,
            master=self._is_master,
            session_id=self._session_id,
            process_interval_s=self._process_interval_s,
            logger=self._logger,
        )
        self._runtime.start()

    def close(self) -> None:
        with self._lock:
            if self._runtime is not None:
                self._runtime.close()
                self._runtime = None

    def _drain_runtime_rx_unlocked(self) -> None:
        runtime = self._runtime
        if runtime is None:
            return

        while True:
            packet = runtime.try_dequeue_rx_packet()
            if packet is None:
                return

            dose_schedule = _extract_dose_schedule_from_response(packet)
            if dose_schedule is not None:
                self._dose_schedule = _copy_dose_schedule_dict(dose_schedule)
            dock_status = _extract_dock_status_from_response(packet)
            if dock_status is not None:
                self._dock_status = _copy_dock_status_dict(dock_status)
            ring_status = _extract_ring_status_from_push(packet)
            if ring_status is not None:
                self._ring_status = _copy_ring_status_dict(ring_status)
            dose_event = _extract_dose_event_from_push(packet)
            if dose_event is not None:
                self._dose_events.append(_copy_dose_event_dict(dose_event))
            weight_measurement = _extract_weight_measurement_from_push(packet)
            if weight_measurement is not None:
                self._weight_measurements.append(_copy_weight_measurement_dict(weight_measurement))
            temperature_log = _extract_temperature_log_from_push(packet)
            if temperature_log is not None:
                self._temperature_points.append(dict(temperature_log))
            dock_battery_level_log = _extract_dock_battery_level_log_from_push(packet)
            if dock_battery_level_log is not None:
                self._dock_battery_level_points.append(dict(dock_battery_level_log))
            ring_battery_level_log = _extract_ring_battery_level_log_from_push(packet)
            if ring_battery_level_log is not None:
                self._ring_battery_level_points.append(dict(ring_battery_level_log))
            ring_docked_status = _extract_ring_docked_status_from_push(packet)
            if ring_docked_status is not None:
                self._ring_docked_status_points.append(dict(ring_docked_status))
            unix_time = _extract_time_from_response(packet)
            if unix_time is not None:
                self._unix_time = _copy_time_dict(unix_time)

            event = self._packet_to_event(
                packet,
                self._next_id,
                dose_schedule,
                dock_status,
                ring_status,
                dose_event,
                weight_measurement,
                temperature_log,
                dock_battery_level_log,
                ring_battery_level_log,
                ring_docked_status,
                unix_time,
            )
            self._messages.append(event)
            self._next_id += 1

    def _packet_to_event(
        self,
        packet: mp_structs.MpPacketPayload,
        event_id: int,
        dose_schedule: Optional[dict],
        dock_status: Optional[dict],
        ring_status: Optional[dict],
        dose_event: Optional[dict],
        weight_measurement: Optional[dict],
        temperature_log: Optional[dict],
        dock_battery_level_log: Optional[dict],
        ring_battery_level_log: Optional[dict],
        ring_docked_status: Optional[dict],
        unix_time: Optional[dict],
    ) -> PacketEvent:
        payload = bytes(packet.payload[: packet.pkt_payload_len])
        now_utc = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
        return PacketEvent(
            id=event_id,
            received_at_utc=now_utc,
            session_id=f"0x{self._session_id:08X}",
            type=int(packet.type),
            type_name=_enum_name(mp_enums.PPI_TYPE, int(packet.type)),
            ppi=int(packet.ppi),
            ppi_name=_enum_name(mp_enums.PPI_AD, int(packet.ppi)),
            payload_len=int(packet.pkt_payload_len),
            payload_hex=payload.hex(),
            dose_schedule=_copy_dose_schedule_dict(dose_schedule) if dose_schedule is not None else None,
            dock_status=_copy_dock_status_dict(dock_status) if dock_status is not None else None,
            ring_status=_copy_ring_status_dict(ring_status) if ring_status is not None else None,
            dose_event=_copy_dose_event_dict(dose_event) if dose_event is not None else None,
            weight_measurement=(
                _copy_weight_measurement_dict(weight_measurement)
                if weight_measurement is not None
                else None
            ),
            temperature_log=dict(temperature_log) if temperature_log is not None else None,
            dock_battery_level_log=dict(dock_battery_level_log) if dock_battery_level_log is not None else None,
            ring_battery_level_log=dict(ring_battery_level_log) if ring_battery_level_log is not None else None,
            ring_docked_status=dict(ring_docked_status) if ring_docked_status is not None else None,
            unix_time=_copy_time_dict(unix_time) if unix_time is not None else None,
        )

    def get_events(self, after_id: int, limit: int) -> list[PacketEvent]:
        with self._lock:
            self._drain_runtime_rx_unlocked()
            events = [event for event in self._messages if event.id > after_id]
        return events[:limit]

    def get_messages(self) -> list[PacketEvent]:
        with self._lock:
            self._drain_runtime_rx_unlocked()
            return list(self._messages)

    def get_export_snapshot(self) -> dict:
        with self._lock:
            self._drain_runtime_rx_unlocked()
            session_id = f"0x{self._session_id:08X}"
            return {
                "session_id": session_id,
                "messages": [asdict(event) for event in self._messages],
                "temperature_points": [dict(point) for point in self._temperature_points],
                "dock_battery_level_points": [dict(point) for point in self._dock_battery_level_points],
                "ring_battery_level_points": [dict(point) for point in self._ring_battery_level_points],
                "ring_docked_status_points": [dict(point) for point in self._ring_docked_status_points],
                "dose_events": [_copy_dose_event_dict(event) for event in self._dose_events],
                "weight_measurements": [_copy_weight_measurement_dict(event) for event in self._weight_measurements],
            }

    def get_state(self) -> dict:
        with self._lock:
            runtime = self._runtime
            last_message_id = self._messages[-1].id if self._messages else 0

            runtime_error_code = None if runtime is None else runtime.runtime_error
            runtime_error = None if runtime_error_code is None else f"Runtime error code: {runtime_error_code}"

        return {
            "runtime_started": runtime is not None and runtime_error_code is None,
            "runtime_starting": False,
            "runtime_error": runtime_error,
            "runtime_error_code": runtime_error_code,
            "host": self._host,
            "port": self._port,
            "is_master": self._is_master,
            "session_id": f"0x{self._session_id:08X}",
            "session_id_source": self._session_id_source,
            "process_interval_s": self._process_interval_s,
            "last_message_id": last_message_id,
        }

    def get_dose_schedule(self) -> dict:
        with self._lock:
            return _copy_dose_schedule_dict(self._dose_schedule)

    def get_unix_time(self) -> dict:
        with self._lock:
            return _copy_time_dict(self._unix_time)

    def get_dock_status(self) -> Optional[dict]:
        with self._lock:
            if self._dock_status is None:
                return None
            return _copy_dock_status_dict(self._dock_status)

    def get_ring_status(self) -> Optional[dict]:
        with self._lock:
            if self._ring_status is None:
                return None
            return _copy_ring_status_dict(self._ring_status)

    def get_dose_events(self) -> list[dict]:
        with self._lock:
            return [_copy_dose_event_dict(event) for event in self._dose_events]

    def get_weight_measurements(self) -> list[dict]:
        with self._lock:
            return [_copy_weight_measurement_dict(event) for event in self._weight_measurements]

    def get_temperature_points(self) -> list[dict]:
        with self._lock:
            return [dict(point) for point in self._temperature_points]

    def get_dock_battery_level_points(self) -> list[dict]:
        with self._lock:
            return [dict(point) for point in self._dock_battery_level_points]

    def get_ring_battery_level_points(self) -> list[dict]:
        with self._lock:
            return [dict(point) for point in self._ring_battery_level_points]

    def get_ring_docked_status_points(self) -> list[dict]:
        with self._lock:
            return [dict(point) for point in self._ring_docked_status_points]

    def enqueue_dose_schedule_push(self, schedule: dict) -> bool:
        with self._lock:
            runtime = self._runtime
            if runtime is None:
                return False

            packet = _build_dose_schedule_push_packet(schedule)
            queued = runtime.enqueue_tx_packet(packet)
            if queued:
                self._dose_schedule = _copy_dose_schedule_dict(schedule)
            return queued

    def enqueue_dose_schedule_request(self) -> bool:
        with self._lock:
            runtime = self._runtime
            if runtime is None:
                return False
            packet = _build_dose_schedule_request_packet()
            return runtime.enqueue_tx_packet(packet)

    def enqueue_time_push(self, unix_time_s: int) -> bool:
        with self._lock:
            runtime = self._runtime
            if runtime is None:
                return False
            packet = _build_time_push_packet(unix_time_s)
            queued = runtime.enqueue_tx_packet(packet)
            if queued:
                self._unix_time = _time_dict_from_unix(unix_time_s)
            return queued

    def enqueue_time_request(self) -> bool:
        with self._lock:
            runtime = self._runtime
            if runtime is None:
                return False
            packet = _build_time_request_packet()
            return runtime.enqueue_tx_packet(packet)

    def enqueue_dock_status_request(self) -> bool:
        with self._lock:
            runtime = self._runtime
            if runtime is None:
                return False
            packet = _build_dock_status_request_packet()
            return runtime.enqueue_tx_packet(packet)

    def enqueue_ring_status_request(self) -> bool:
        with self._lock:
            runtime = self._runtime
            if runtime is None:
                return False
            packet = _build_ring_status_request_packet()
            return runtime.enqueue_tx_packet(packet)


app = Flask(__name__)
monitor = SimpleRuntimeMonitor(
    max_messages=min(_MAX_INCOMING_MESSAGES, int(os.getenv("MP_MONITOR_MAX_MESSAGES", str(_MAX_INCOMING_MESSAGES)))),
)


@app.get("/")
def index() -> str:
    monitor.ensure_started()
    return render_template("index.html")


@app.get("/api/state")
def api_state():
    monitor.ensure_started()
    return jsonify(monitor.get_state())


@app.get("/api/messages")
def api_get_messages():
    monitor.ensure_started()
    state = monitor.get_state()
    return jsonify(
        {
            "session_id": state["session_id"],
            "messages": [asdict(event) for event in monitor.get_messages()],
        }
    )


@app.get("/api/export_snapshot")
def api_export_snapshot():
    monitor.ensure_started()
    return jsonify(monitor.get_export_snapshot())


@app.get("/api/dose_schedule")
def api_get_dose_schedule():
    monitor.ensure_started()
    return jsonify({"dose_schedule": monitor.get_dose_schedule()})


@app.get("/api/dock_status")
def api_get_dock_status():
    monitor.ensure_started()
    return jsonify({"dock_status": monitor.get_dock_status()})


@app.get("/api/ring_status")
def api_get_ring_status():
    monitor.ensure_started()
    return jsonify({"ring_status": monitor.get_ring_status()})


@app.get("/api/time")
def api_get_time():
    monitor.ensure_started()
    return jsonify({"time": monitor.get_unix_time()})


@app.get("/api/temperature_points")
def api_get_temperature_points():
    monitor.ensure_started()
    return jsonify({"temperature_points": monitor.get_temperature_points()})


@app.get("/api/dock_battery_level_points")
def api_get_dock_battery_level_points():
    monitor.ensure_started()
    return jsonify({"dock_battery_level_points": monitor.get_dock_battery_level_points()})


@app.get("/api/ring_battery_level_points")
def api_get_ring_battery_level_points():
    monitor.ensure_started()
    return jsonify({"ring_battery_level_points": monitor.get_ring_battery_level_points()})


@app.get("/api/dose_events")
def api_get_dose_events():
    monitor.ensure_started()
    return jsonify({"dose_events": monitor.get_dose_events()})


@app.get("/api/weight_measurements")
def api_get_weight_measurements():
    monitor.ensure_started()
    return jsonify({"weight_measurements": monitor.get_weight_measurements()})


@app.get("/api/ring_docked_status_points")
def api_get_ring_docked_status_points():
    monitor.ensure_started()
    return jsonify({"ring_docked_status_points": monitor.get_ring_docked_status_points()})


@app.post("/api/dose_schedule/update")
def api_update_dose_schedule():
    monitor.ensure_started()
    payload = request.get_json(silent=True)
    if payload is None:
        return jsonify({"ok": False, "error": "Missing JSON payload."}), 400

    dose_schedule_payload = payload.get("dose_schedule", payload) if isinstance(payload, dict) else payload
    try:
        schedule = _normalize_dose_schedule_dict(dose_schedule_payload)
    except ValueError as exc:
        return jsonify({"ok": False, "error": str(exc)}), 400

    if not monitor.enqueue_dose_schedule_push(schedule):
        return jsonify({"ok": False, "error": "Failed to enqueue dose schedule push packet."}), 503

    return jsonify({"ok": True, "dose_schedule": monitor.get_dose_schedule()})


@app.post("/api/dose_schedule/request")
def api_request_dose_schedule():
    monitor.ensure_started()
    if not monitor.enqueue_dose_schedule_request():
        return jsonify({"ok": False, "error": "Failed to enqueue dose schedule request packet."}), 503
    return jsonify({"ok": True})


@app.post("/api/time/push")
@app.post("/api/time/update")
def api_push_time():
    monitor.ensure_started()
    payload = request.get_json(silent=True)
    if payload is None:
        return jsonify({"ok": False, "error": "Missing JSON payload."}), 400

    time_payload = payload.get("time", payload) if isinstance(payload, dict) else payload
    unix_time_payload = time_payload.get("unix_time_s") if isinstance(time_payload, dict) else time_payload
    try:
        unix_time_s = _coerce_int("unix_time_s", unix_time_payload, 0, 0xFFFFFFFF)
    except ValueError as exc:
        return jsonify({"ok": False, "error": str(exc)}), 400

    if not monitor.enqueue_time_push(unix_time_s):
        return jsonify({"ok": False, "error": "Failed to enqueue unix time push packet."}), 503

    return jsonify({"ok": True, "time": monitor.get_unix_time()})


@app.post("/api/time/request")
def api_request_time():
    monitor.ensure_started()
    if not monitor.enqueue_time_request():
        return jsonify({"ok": False, "error": "Failed to enqueue unix time request packet."}), 503
    return jsonify({"ok": True})


@app.post("/api/dock_status/request")
def api_request_dock_status():
    monitor.ensure_started()
    if not monitor.enqueue_dock_status_request():
        return jsonify({"ok": False, "error": "Failed to enqueue dock status request packet."}), 503
    return jsonify({"ok": True})


@app.post("/api/ring_status/request")
def api_request_ring_status():
    monitor.ensure_started()
    if not monitor.enqueue_ring_status_request():
        return jsonify({"ok": False, "error": "Failed to enqueue ring status request packet."}), 503
    return jsonify({"ok": True})


@app.get("/api/stream")
def api_stream():
    monitor.ensure_started()

    after_id_raw = request.args.get("after_id", "0")
    last_event_id_raw = request.headers.get("Last-Event-ID", "")
    try:
        after_id = max(0, int(after_id_raw))
    except ValueError:
        after_id = 0
    try:
        last_event_id = max(0, int(last_event_id_raw)) if last_event_id_raw != "" else 0
    except ValueError:
        last_event_id = 0

    def _generate():
        cursor = max(after_id, last_event_id)

        while True:
            state = monitor.get_state()
            if state["runtime_error_code"] is not None:
                payload = json.dumps({"error": state["runtime_error"]})
                yield f"event: runtime_error\ndata: {payload}\n\n"
                return

            events = monitor.get_events(after_id=cursor, limit=512)
            if events:
                for event in events:
                    cursor = max(cursor, event.id)
                    payload = json.dumps(asdict(event), separators=(",", ":"))
                    yield f"id: {event.id}\nevent: message\ndata: {payload}\n\n"
                continue

            time.sleep(0.01)

    return Response(
        stream_with_context(_generate()),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no",
        },
    )


@atexit.register
def _cleanup() -> None:
    monitor.close()


if __name__ == "__main__":
    logging.basicConfig(
        level=os.getenv("MP_MONITOR_LOG_LEVEL", "INFO").upper(),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    monitor.ensure_started()
    bind_host = os.getenv("MP_MONITOR_BIND_HOST", "127.0.0.1")
    bind_port = int(os.getenv("MP_MONITOR_BIND_PORT", "8080"))
    app.run(host=bind_host, port=bind_port, debug=False, use_reloader=False, threaded=True)
