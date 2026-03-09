import logging
from typing import Optional

from . import mp_structs  # Import the message protocol structs

LOGGER = logging.getLogger(__name__)

def interpret_rx_packet(
    rx_payload: mp_structs.MpPacketPayload, logger: Optional[logging.Logger] = None
) -> None:
    log = logger if logger is not None else LOGGER
    log.info("Interpreting RX Packet:")
    log.info("Type: %d", rx_payload.type)
    log.info("PPI: %d", rx_payload.ppi)
    log.info("Payload Length: %d", rx_payload.pkt_payload_len)
    payload_data = bytes(rx_payload.payload[:rx_payload.pkt_payload_len])
    log.info("Payload Data (hex): %s", payload_data.hex())
