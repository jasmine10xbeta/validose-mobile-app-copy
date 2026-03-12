import ctypes

MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN = 256
MAC_ADDRESS_SIZE_BYTES = 10
DOCK_STATUS_T_SIZE_BYTES = 28
DOSE_SCHEDULE_MAX_DOSES_PER_DAY = 10
DOSE_SCHEDULE_SIZE_BYTES = 30
TEMPERATURE_LOG_T_SIZE_BYTES = 6
BATTERY_LEVEL_T_SIZE_BYTES = 5
RING_DOCKED_STATUS_T_SIZE_BYTES = 25
RING_STATUS_T_SIZE_BYTES = 41
EVENT_ID_T_SIZE_BYTES = 3
DOSE_EVENT_T_SIZE_BYTES = 12
DOCK_WEIGHT_MEASUREMENT_T_SIZE_BYTES = 16
START_CALIBRATION_PARAM_T_SIZE_BYTES = 5
START_BASELINING_PARAM_T_SIZE_BYTES = 1
START_CALIBRATION_RESPONSE_T_SIZE_BYTES = 1
START_BASELINING_RESPONSE_T_SIZE_BYTES = 1
CALIBRATION_FEEDBACK_T_SIZE_BYTES = 9
WEIGHT_STACK_CALIBRATION_RECORD_T_SIZE_BYTES = 12
CALIBRATION_WEIGHT_PRESENT_RESPONSE_T_SIZE_BYTES = 1
BASELINING_FEEDBACK_T_SIZE_BYTES = 19
MEDICATION_NFC_ID_SIZE_BYTES = 10
RING_NFC_ID_SIZE_BYTES = 10
VALIDATE_MED_RESPONSE_T_SIZE_BYTES = 1
DOCK_CHARGE_STATUS_T_SIZE_BYTES = 5
CAP_DETECTION_SAMPLE_RATE_SIZE_BYTES = 2
CAP_DETECTION_CONFIG_SIZE_BYTES = 4
CAP_DETECTION_STATUS_T_SIZE_BYTES = 15

MAX_SUPPORTED_VARARGS = 6
RAW_DEBUG_LOG_T_SIZE_BYTES = 8 + (MAX_SUPPORTED_VARARGS * 4)

# Define ctypes structures
class MpPacketPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("type", ctypes.c_uint8),
        ("ppi", ctypes.c_uint8),
        ("pkt_payload_len", ctypes.c_uint16),
        ("payload", ctypes.c_uint8 * MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN),
    ]


class SemanticVersion(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("major", ctypes.c_uint8),
        ("minor", ctypes.c_uint8),
        ("patch", ctypes.c_uint8),
    ]


class DockStatus(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("hardware_version", SemanticVersion),
        ("firmware_version", SemanticVersion),
        ("mac", ctypes.c_uint8 * MAC_ADDRESS_SIZE_BYTES),
        ("timestamp_unix_s", ctypes.c_uint32),
        ("ship_mode_exit_timestamp_unix", ctypes.c_uint32),
        ("uptime_s", ctypes.c_uint32),
    ]


class DoseSchedule(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("medication_type", ctypes.c_uint8),
        ("dosage_mg", ctypes.c_uint16),
        ("temp_upper_limit_deg_c", ctypes.c_int8),
        ("temp_lower_limit_deg_c", ctypes.c_int8),
        ("temp_avg_window_duration_sec", ctypes.c_uint16),
        ("dose_days_bitfield", ctypes.c_uint8),
        ("dose_window_duration_minutes", ctypes.c_uint8),
        ("dose_window_count", ctypes.c_uint8),
        ("dose_window_start_times_minutes", ctypes.c_uint16 * DOSE_SCHEDULE_MAX_DOSES_PER_DAY),
    ]


class StartCalibrationParam(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("start", ctypes.c_uint8),
        ("calibration_weight_mg", ctypes.c_uint32),
    ]


class CalibrationFeedback(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("current_state", ctypes.c_uint8),
        ("motion_state", ctypes.c_uint8),
        ("std_dev", ctypes.c_uint16),
        ("avg_weight_mg", ctypes.c_int32),
        ("is_ring_present", ctypes.c_uint8),  # represent bool as uint8
    ]


class BaseliningFeedback(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("current_state", ctypes.c_uint8),
        ("motion_state", ctypes.c_uint8),
        ("std_dev", ctypes.c_uint16),
        ("avg_weight_mg", ctypes.c_int32),
        ("is_ring_present", ctypes.c_uint8),  # represent bool as uint8
        ("medication_nfc_id", ctypes.c_uint8 * MEDICATION_NFC_ID_SIZE_BYTES),
    ]


class WeightStackCalibrationRecord(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("zero_offset", ctypes.c_int32),
        ("calibration_factor", ctypes.c_int32),
        ("full_assembly_weight_mg", ctypes.c_uint32),
    ]


class TemperatureLog(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("timestamp_unix_s", ctypes.c_uint32),
        ("temperature_deg_c", ctypes.c_int16),
    ]


class BatteryLevel(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("timestamp_unix_s", ctypes.c_uint32),
        ("battery_level", ctypes.c_uint8),
    ]


class RingDockedStatus(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("timestamp_unix_s", ctypes.c_uint32),
        ("docked_status", ctypes.c_uint8),
        ("ring_nfc_id", ctypes.c_uint8 * RING_NFC_ID_SIZE_BYTES),
        ("medication_nfc_id", ctypes.c_uint8 * MEDICATION_NFC_ID_SIZE_BYTES),
    ]


class RingStatus(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("hardware_version", SemanticVersion),
        ("firmware_version", SemanticVersion),
        ("mac", ctypes.c_uint8 * MAC_ADDRESS_SIZE_BYTES),
        ("timestamp_unix_s", ctypes.c_uint32),
        ("ship_mode_exit_timestamp_unix", ctypes.c_uint32),
        ("battery_charge_status", ctypes.c_uint8),
        ("uptime_s", ctypes.c_uint32),
        ("temperature_celsius", ctypes.c_int16),
        ("dose_fifo_used_percent", ctypes.c_uint8),
        ("battery_fifo_used_percent", ctypes.c_uint8),
        ("imu_fifo_used_percent", ctypes.c_uint8),
        ("error_fifo_used_percent", ctypes.c_uint8),
        ("dose_fifo_used_percent_watermark", ctypes.c_uint8),
        ("battery_fifo_used_percent_watermark", ctypes.c_uint8),
        ("imu_fifo_used_percent_watermark", ctypes.c_uint8),
        ("error_fifo_used_percent_watermark", ctypes.c_uint8),
        ("battery_sample_frequency_millihz", ctypes.c_uint16),
    ]


class EventID(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("days_since_epoch", ctypes.c_uint16),
        ("event_ctr", ctypes.c_uint8),
    ]


class DoseEvent(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("event_id", EventID),
        ("start_timestamp_unix_s", ctypes.c_uint32),
        ("duration_s", ctypes.c_uint16),
        ("dose_completed_in_time", ctypes.c_uint8),
        ("tilt_count", ctypes.c_uint16),
    ]

class DockWeightMeasurement(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("weight_mg", ctypes.c_int32),
        ("total_dispensed_mg", ctypes.c_uint32),
        ("std_dev", ctypes.c_uint16),
        ("temperature_deg_c_div10", ctypes.c_int16),
        ("timestamp_unix_s", ctypes.c_uint32),
    ]

class CapDetectionSampleRate(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("polling_period_ms", ctypes.c_uint16),
    ]

class CapDetectionStatus(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("config_threshhold", ctypes.c_uint16),
        ("config_hysteresis", ctypes.c_uint16),
        ("prox_value", ctypes.c_uint16),
        ("is_cap_closed", ctypes.c_uint8),
        ("timestamp_unix_s", ctypes.c_uint32),
        ("poll_period_ms", ctypes.c_uint32),
    ]

class CapDetectionConfig(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("threshhold", ctypes.c_uint16),
        ("hysteresis", ctypes.c_uint16),
    ]
class DockChargeStatus(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("timestamp_unix_s", ctypes.c_uint32),
        ("charge_status", ctypes.c_uint8),
    ]

class RawDebugLog(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("level", ctypes.c_uint8),
        ("timestamp", ctypes.c_uint32),
        ("module_id", ctypes.c_uint8),
        ("line", ctypes.c_uint16),
        ("arg_values", ctypes.c_uint32 * MAX_SUPPORTED_VARARGS),
    ]