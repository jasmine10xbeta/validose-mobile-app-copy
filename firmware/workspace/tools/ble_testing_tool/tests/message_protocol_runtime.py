import ctypes
import logging
import os
import signal
import threading
import time
from queue import Empty, Full, Queue
from typing import Optional

from utils import mp_enums
from utils import mp_structs

RESULT_OK = 0
LOGGER = logging.getLogger(__name__)


class MessageProtocolRuntime:
    """Background message-protocol runtime with queue-based TX/RX API.

    Public API methods for external callers are:
    - `start()`
    - `close()`
    - `get_time_ms()`
    - `set_time_ms()`
    - `enqueue_tx_packet()`
    - `try_dequeue_rx_packet()`

    Methods prefixed with `_` are internal runtime implementation details.
    """

    def __init__(
        self,
        serial_dev: bytes,
        port: int,
        master: bool,
        session_id: int,
        queue_size: int = 50,
        process_interval_s: float = 0.001,
        logger: Optional[logging.Logger] = None,
    ) -> None:
        """Public API. Create and initialize the runtime and C protocol instance."""
        self.tx_queue: Queue[mp_structs.MpPacketPayload] = Queue(maxsize=queue_size)
        self.rx_queue: Queue[mp_structs.MpPacketPayload] = Queue(maxsize=queue_size)

        self._stop_event = threading.Event()
        self._process_thread: Optional[threading.Thread] = None
        self._is_open = False
        self._process_interval_s = process_interval_s
        self._logger = logger if logger is not None else LOGGER
        self._tx_inflight: Optional[mp_structs.MpPacketPayload] = None

        self.runtime_error: Optional[int] = None

        self.libmp = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "../build/message_protocol_wrapper.so"))
        self._configure_function_signatures()
        self._create_protocol(serial_dev, port, master, session_id)

    def start(self) -> None:
        """Public API. Start the background processing thread."""
        if not self._is_open:
            raise RuntimeError("Cannot start runtime: protocol is not open")

        if self._process_thread is None:
            self._process_thread = threading.Thread(target=self._process_loop, daemon=True)
            self._logger.info("Waiting for client to connect")
            self._process_thread.start()

    def close(self) -> None:
        """Public API. Stop background processing and destroy the protocol instance."""
        self._stop_event.set()
        if self._process_thread is not None:
            self._process_thread.join()
            self._process_thread = None
        if self._is_open:
            self.libmp.destroy_message_protocol()
            self._is_open = False
            self._logger.info("Protocol instance destroyed")

    def get_time_ms(self) -> Optional[int]:
        """Public API. Return current protocol/system time in milliseconds, or None on error."""
        current_time = ctypes.c_uint64()
        result = self.libmp.message_protocol_wrapper_get_time_ms(ctypes.byref(current_time))
        if result != RESULT_OK:
            return None
        return int(current_time.value)

    def set_time_ms(self, unix_time_ms: int) -> bool:
        """Public API. Set protocol/system time in milliseconds."""
        result = self.libmp.message_protocol_wrapper_set_time_ms(ctypes.c_uint64(unix_time_ms))
        if result != RESULT_OK:
            self._logger.error("Failed to set time: %d", result)
            return False
        return True

    def enqueue_tx_packet(self, packet: mp_structs.MpPacketPayload) -> bool:
        """Public API. Queue one TX packet for background transmission."""
        try:
            self.tx_queue.put_nowait(packet)
            return True
        except Full:
            return False

    def try_dequeue_rx_packet(self) -> Optional[mp_structs.MpPacketPayload]:
        """Public API. Return one RX packet if available, else None."""
        try:
            return self.rx_queue.get_nowait()
        except Empty:
            return None

    def _configure_function_signatures(self) -> None:
        """Define ctypes signatures for wrapper functions used by the runtime."""
        self.libmp.create_message_protocol.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_bool]
        self.libmp.create_message_protocol.restype = ctypes.c_uint16
        self.libmp.destroy_message_protocol.argtypes = []
        self.libmp.destroy_message_protocol.restype = None

        self.libmp.message_protocol_wrapper_send.argtypes = [ctypes.POINTER(mp_structs.MpPacketPayload)]
        self.libmp.message_protocol_wrapper_send.restype = ctypes.c_uint16
        self.libmp.message_protocol_wrapper_process.argtypes = []
        self.libmp.message_protocol_wrapper_process.restype = ctypes.c_uint16

        self.libmp.message_protocol_wrapper_get_rx_packet_status.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        self.libmp.message_protocol_wrapper_get_rx_packet_status.restype = ctypes.c_uint16
        self.libmp.message_protocol_wrapper_get_rx_packet.argtypes = [ctypes.POINTER(mp_structs.MpPacketPayload)]
        self.libmp.message_protocol_wrapper_get_rx_packet.restype = ctypes.c_uint16

        self.libmp.message_protocol_wrapper_get_tx_packet_status.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        self.libmp.message_protocol_wrapper_get_tx_packet_status.restype = ctypes.c_uint16

        self.libmp.message_protocol_wrapper_set_session_id.argtypes = [ctypes.c_uint32]
        self.libmp.message_protocol_wrapper_set_session_id.restype = ctypes.c_uint16

        self.libmp.message_protocol_wrapper_inc_time_by_set_val_ms.argtypes = [ctypes.c_uint16]
        self.libmp.message_protocol_wrapper_inc_time_by_set_val_ms.restype = ctypes.c_uint16

        self.libmp.message_protocol_wrapper_set_time_ms.argtypes = [ctypes.c_uint64]
        self.libmp.message_protocol_wrapper_set_time_ms.restype = ctypes.c_uint16

        self.libmp.message_protocol_wrapper_get_time_ms.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        self.libmp.message_protocol_wrapper_get_time_ms.restype = ctypes.c_uint16

    def _create_protocol(self, serial_dev: bytes, port: int, master: bool, session_id: int) -> None:
        """Create the underlying C runtime instance and perform initial setup."""
        result = self.libmp.create_message_protocol(serial_dev, port, master)
        if result != RESULT_OK:
            raise RuntimeError(f"create_message_protocol failed: {result}")
        self._logger.info("Protocol instance created, result: %d", result)

        result = self.libmp.message_protocol_wrapper_set_session_id(session_id)
        if result != RESULT_OK:
            self.libmp.destroy_message_protocol()
            raise RuntimeError(f"message_protocol_wrapper_set_session_id failed: {result}")
        self._logger.info("Session ID set to: %#010x, result: %d", session_id, result)

        result = self.libmp.message_protocol_wrapper_process()
        if result != RESULT_OK:
            self.libmp.destroy_message_protocol()
            raise RuntimeError(f"Initial message_protocol_wrapper_process failed: {result}")
        self._logger.info("Initial process result: %d", result)

        self._is_open = True

    def _inc_time_elapsed_ms(self, elapsed_ms: int) -> int:
        """Increment C-side time by elapsed milliseconds, chunked to uint16 range."""
        remaining = elapsed_ms
        while remaining > 0:
            step_ms = min(remaining, 0xFFFF)
            result = self.libmp.message_protocol_wrapper_inc_time_by_set_val_ms(step_ms)
            if result != RESULT_OK:
                return int(result)
            remaining -= step_ms
        return RESULT_OK

    def _process_loop(self) -> None:
        """Worker loop: advance time, process protocol, and service TX/RX queues."""
        last_tick_s = time.perf_counter()
        pending_ms = 0.0

        while not self._stop_event.is_set():
            now_s = time.perf_counter()
            pending_ms += (now_s - last_tick_s) * 1000.0
            last_tick_s = now_s

            elapsed_ms = int(pending_ms)
            if elapsed_ms > 0:
                time_result = self._inc_time_elapsed_ms(elapsed_ms)
                if time_result != RESULT_OK:
                    self.runtime_error = time_result
                    self._logger.error("Runtime time increment error: %d", time_result)
                    self._stop_event.set()
                    break
                pending_ms -= elapsed_ms

            result = self._process_once()
            if result != RESULT_OK:
                self.runtime_error = result
                self._logger.error("Runtime process error: %d", result)
                self._stop_event.set()
                break
            time.sleep(self._process_interval_s)

    def _try_send_from_queue(self) -> int:
        """Send one queued packet when the protocol TX state is ready."""
        status_tx = ctypes.c_uint8()
        result = self.libmp.message_protocol_wrapper_get_tx_packet_status(ctypes.byref(status_tx))
        if result != RESULT_OK:
            return int(result)

        tx_completed = mp_enums.MSG_PROT_TX_PACKET_STATUS.MSG_PROT_TX_PACKET_STATUS_COMPLETED
        tx_abandoned = mp_enums.MSG_PROT_TX_PACKET_STATUS.MSG_PROT_TX_PACKET_STATUS_ABANDONED

        # Keep one packet in-flight and only consider it dequeued on COMPLETED.
        if self._tx_inflight is not None:
            if status_tx.value == tx_completed:
                self._tx_inflight = None
            elif status_tx.value == tx_abandoned:
                resend_result = self.libmp.message_protocol_wrapper_send(ctypes.byref(self._tx_inflight))
                if resend_result != RESULT_OK:
                    return int(resend_result)
                return RESULT_OK
            else:
                return RESULT_OK

        if status_tx.value not in (tx_completed, tx_abandoned):
            return RESULT_OK

        try:
            packet = self.tx_queue.get_nowait()
        except Empty:
            return RESULT_OK

        send_result = self.libmp.message_protocol_wrapper_send(ctypes.byref(packet))
        if send_result != RESULT_OK:
            try:
                self.tx_queue.put_nowait(packet)
            except Full:
                pass
            return int(send_result)

        self._tx_inflight = packet
        return RESULT_OK

    def _poll_rx_to_queue(self) -> int:
        """Move one newly received packet from C-side storage into rx_queue."""
        status_rx = ctypes.c_uint8()
        result = self.libmp.message_protocol_wrapper_get_rx_packet_status(ctypes.byref(status_rx))
        if result != RESULT_OK:
            return int(result)

        if status_rx.value != mp_enums.MSG_PROT_RX_PACKET_STATUS.MSG_PROT_RX_PACKET_STATUS_NEW:
            return RESULT_OK

        rx_packet = mp_structs.MpPacketPayload()
        result = self.libmp.message_protocol_wrapper_get_rx_packet(ctypes.byref(rx_packet))
        if result != RESULT_OK:
            return int(result)

        try:
            self.rx_queue.put_nowait(rx_packet)
        except Full:
            pass

        return RESULT_OK

    def _process_once(self) -> int:
        """Execute one protocol cycle: process core state, then TX and RX queues."""
        process_result = self.libmp.message_protocol_wrapper_process()
        if process_result != RESULT_OK:
            return int(process_result)

        send_result = self._try_send_from_queue()
        if send_result != RESULT_OK:
            return int(send_result)

        rx_result = self._poll_rx_to_queue()
        if rx_result != RESULT_OK:
            return int(rx_result)

        return RESULT_OK
