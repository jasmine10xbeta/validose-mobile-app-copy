#ifndef MESSAGE_PROTOCOL_WRAPPER_H
#define MESSAGE_PROTOCOL_WRAPPER_H

#include "message_protocol.h"
#include <arpa/inet.h>
#include <stddef.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>

#define TCP_LINK_RX_STAGING_CAPACITY (MP_MAX_PACKET_LENGTH * 8u)

typedef struct
{
   int sockfd;
   struct sockaddr_in server_addr;
   uint8_t rx_staging[TCP_LINK_RX_STAGING_CAPACITY];
   size_t rx_staging_len;
   comms_driver_interface_t interface;
} tcp_link_t;

result_t tcp_link_init(tcp_link_t *link, const char *host, int port, bool is_master);
void tcp_link_close(tcp_link_t *link);

// Factory function to create and initialize the protocol instance
result_t create_message_protocol(const char *host, int port, bool is_master);
// Destroy function to clean up the protocol instance
void destroy_message_protocol(void);

// Wrapper functons for Python
result_t message_protocol_wrapper_send(mp_packet_payload_t *payload);
result_t message_protocol_wrapper_process(void);
result_t message_protocol_wrapper_get_rx_packet_status(MSG_PROT_RX_PACKET_STATUS *status);
result_t message_protocol_wrapper_get_rx_packet(mp_packet_payload_t *payload);
result_t message_protocol_wrapper_get_tx_packet_status(MSG_PROT_TX_PACKET_STATUS *status);
result_t message_protocol_wrapper_get_max_payload_length(uint16_t *max_len);
result_t message_protocol_wrapper_set_session_id(uint32_t session_id);
result_t message_protocol_wrapper_get_session_id(uint32_t *session_id);
result_t message_protocol_wrapper_inc_time_by_set_val_ms(uint16_t value_ms);
result_t message_protocol_wrapper_set_time_ms(uint64_t value_ms);
result_t message_protocol_wrapper_get_time_ms(uint64_t *current_system_time);

#endif // MESSAGE_PROTOCOL_WRAPPER_H
