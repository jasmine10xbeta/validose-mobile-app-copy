#ifndef _DEFAULT_SOURCE
#   define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#   define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "message_protocol.h"
#include "message_protocol_wrapper.h"
#include "system_time.h"

static system_time_t systick;
static message_protocol_t mp;
static tcp_link_t tcp_link;
static bool mp_initialized = false;
static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_DOCK_COMMS_DRIVER;

#define MP_WIRE_HEADER_LEN         (2u + 2u + 4u + 1u + 1u)
#define MP_WIRE_PAYLOAD_HDR_LEN    (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE)
#define MP_WIRE_MIN_PACKET_LEN     (MP_WIRE_HEADER_LEN + MP_WIRE_PAYLOAD_HDR_LEN)
#define MP_WIRE_PAYLOAD_LEN_OFFSET (12u)
#define MP_ACK_TIMEOUT_MS          (1000u)

typedef struct
{
   uint32_t next_id;
} session_id_gen_t;

static session_id_gen_t g_master_session_gen = {0x00000000u};

static void tcp_drop_staging_prefix(tcp_link_t *self, size_t byte_count)
{
   if((NULL == self) || (0u == byte_count) || (0u == self->rx_staging_len))
   {
      return;
   }

   if(byte_count >= self->rx_staging_len)
   {
      self->rx_staging_len = 0u;
      return;
   }

   memmove(self->rx_staging, &self->rx_staging[byte_count], self->rx_staging_len - byte_count);
   self->rx_staging_len -= byte_count;
}

static result_t tcp_try_extract_mp_packet(tcp_link_t *self, uint8_t *data, uint16_t buflen)
{
   if((NULL == self) || (NULL == data))
   {
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_PTR_NULL);
   }

   while(self->rx_staging_len >= MP_WIRE_MIN_PACKET_LEN)
   {
      const uint16_t payload_len = (uint16_t)((uint16_t)self->rx_staging[MP_WIRE_PAYLOAD_LEN_OFFSET]
                                              | ((uint16_t)self->rx_staging[MP_WIRE_PAYLOAD_LEN_OFFSET + 1u] << 8u));
      const uint16_t packet_len = (uint16_t)(MP_WIRE_MIN_PACKET_LEN + payload_len);

      if(packet_len > MP_MAX_PACKET_LENGTH)
      {
         printf("[tcp_recv] Invalid MP packet length %u. Dropping one byte for resync.\n", packet_len);
         tcp_drop_staging_prefix(self, 1u);
         continue;
      }

      if(packet_len > buflen)
      {
         return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_OVERFLOW);
      }

      if(self->rx_staging_len < packet_len)
      {
         return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_BUSY);
      }

      memcpy(data, self->rx_staging, packet_len);
      tcp_drop_staging_prefix(self, packet_len);
      return RESULT_OK;
   }

   return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_BUSY);
}

result_t message_protocol_wrapper_send(mp_packet_payload_t *payload)
{
   if(!mp_initialized)
      return 0xFFFF;
   mp_packet_payload_t c_payload;
   memset(&c_payload, 0, sizeof(c_payload));
   c_payload.type = payload->type;
   c_payload.ppi = payload->ppi;
   c_payload.pkt_payload_len = payload->pkt_payload_len;
   if(c_payload.pkt_payload_len > sizeof(c_payload.payload))
      c_payload.pkt_payload_len = sizeof(c_payload.payload);
   memcpy(c_payload.payload, payload->payload, c_payload.pkt_payload_len);
   return mp.interface.send(&mp.interface, &c_payload);
}

result_t message_protocol_wrapper_process(void)
{
   if(!mp_initialized)
      return 0xFFFF;
   return mp.interface.process(&mp.interface);
}

result_t message_protocol_wrapper_get_rx_packet_status(MSG_PROT_RX_PACKET_STATUS *status)
{
   if(!mp_initialized)
      return 0xFFFF;

   return mp.interface.get_rx_packet_status(&mp.interface, status);
}

result_t message_protocol_wrapper_get_rx_packet(mp_packet_payload_t *payload)
{
   if(!mp_initialized)
      return 0xFFFF;
   mp_packet_payload_t c_payload;
   memset(&c_payload, 0, sizeof(c_payload));
   result_t result = mp.interface.get_rx_packet(&mp.interface, &c_payload);
   payload->type = c_payload.type;
   payload->ppi = c_payload.ppi;
   payload->pkt_payload_len = c_payload.pkt_payload_len;
   if(payload->pkt_payload_len > sizeof(payload->payload))
      payload->pkt_payload_len = sizeof(payload->payload);
   memcpy(payload->payload, c_payload.payload, payload->pkt_payload_len);
   return result;
}

result_t message_protocol_wrapper_get_tx_packet_status(MSG_PROT_TX_PACKET_STATUS *status)
{
   if(!mp_initialized)
      return 0xFFFF;

   return mp.interface.get_tx_packet_status(&mp.interface, status);
}

result_t message_protocol_wrapper_get_max_payload_length(uint16_t *max_len)
{
   if(!mp_initialized)
      return 0xFFFF;

   return mp.interface.get_max_payload_length(&mp.interface, max_len);
}

result_t message_protocol_wrapper_set_session_id(uint32_t session_id)
{
   if(!mp_initialized)
      return 0xFFFF;
   g_master_session_gen = (session_id_gen_t){session_id};
   mp._current_session_id = session_id;
   return RESULT_OK;
}

result_t message_protocol_wrapper_get_session_id(uint32_t *session_id)
{
   if(!mp_initialized)
      return 0xFFFF;
   if(NULL == session_id)
      return -1;
   *session_id = mp._current_session_id;
   return RESULT_OK;
}

result_t message_protocol_wrapper_inc_time_by_set_val_ms(uint16_t value_ms)
{
   if(!mp_initialized)
      return 0xFFFF;
   return systick.interface.inc_time_by_set_val_ms(&systick.interface, value_ms);
}

result_t message_protocol_wrapper_set_time_ms(uint64_t value_ms)
{
   if(!mp_initialized)
      return 0xFFFF;
   return systick.interface.set_time_ms(&systick.interface, value_ms);
}

result_t message_protocol_wrapper_get_time_ms(uint64_t *current_system_time)
{
   if(!mp_initialized)
      return 0xFFFF;
   return systick.interface.get_time_ms(&systick.interface, current_system_time);
}

static result_t generate_session_id_master(const message_protocol_t *self, uint32_t *session_id)
{
   UNUSED_PARAMETER(self);
   if(NULL == session_id)
   {
      return -1;
   }

   *session_id = g_master_session_gen.next_id;
   g_master_session_gen.next_id += 1u;

   return RESULT_OK;
}

result_t create_message_protocol(const char *host, int port, bool is_master)
{
   if(mp_initialized)
      return 0xFFFF;

   result_t result = system_time_init(&systick);
   if(IS_ERR(result))
   {
      printf("system_time_init failed, result: %u\n", result);
   }

   IF_OK_RUN_AND_UPDATE(result, tcp_link_init(&tcp_link, host, port, is_master));

   if(IS_ERR(result))
   {
      printf("tcp_link_init failed, result: %u\n", result);
   }

   if(is_master)
   {
      IF_OK_RUN_AND_UPDATE(result,
                           message_protocol_init(&mp,
                                                 &systick.interface,
                                                 &tcp_link.interface,
                                                 is_master,
                                                 MP_ACK_TIMEOUT_MS,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 generate_session_id_master));
   }
   else
   {
      IF_OK_RUN_AND_UPDATE(
         result,
         message_protocol_init(
            &mp, &systick.interface, &tcp_link.interface, is_master, MP_ACK_TIMEOUT_MS, NULL, NULL, NULL, NULL));
   }

   if(IS_ERR(result))
   {
      printf("[create_message_protocol] message_protocol_init failed, result: %u\n", result);
      tcp_link_close(&tcp_link);
   }

   mp_initialized = IS_OK(result);
   return result;
}

void destroy_message_protocol(void)
{
   if(!mp_initialized)
   {
      printf("[destroy_message_protocol] Not initialized, nothing to destroy.\n");
      return;
   }
   // printf("[destroy_message_protocol] Destroying protocol and closing TCP link.\n");
   tcp_link_close(&tcp_link);
   mp_initialized = false;
}

static result_t tcp_send(const comms_driver_interface_t *iface, const uint8_t *data, uint16_t len)
{
   if((NULL == iface) || (NULL == iface->parent) || (NULL == data))
   {
      printf("[tcp_send] Invalid input pointers.\n");
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_PTR_NULL);
   }

   tcp_link_t *self = (tcp_link_t *)iface->parent;
   if(self->sockfd < 0)
   {
      printf("[tcp_send] Invalid socket state: sockfd=%d\n", self->sockfd);
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND);
   }

   if(0u == len)
   {
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_INVALID_TX_LENGTH);
   }

   // printf("[tcp_send] Sending %u bytes: ", len);
   // for(uint16_t i = 0; i < len; ++i)
   //    printf("%02X ", data[i]);
   // printf("\n");

   size_t sent = 0u;
   while(sent < len)
   {
      int flags = 0;
#ifdef MSG_NOSIGNAL
      flags |= MSG_NOSIGNAL;
#endif

      ssize_t n = send(self->sockfd, &data[sent], (size_t)(len - sent), flags);
      if(n > 0)
      {
         sent += (size_t)n;
         continue;
      }

      if((n < 0) && (EINTR == errno))
      {
         continue;
      }

      if((n < 0) && ((EAGAIN == errno) || (EWOULDBLOCK == errno)))
      {
         return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_BUSY);
      }

      if((n < 0) && ((EPIPE == errno) || (ECONNRESET == errno) || (ENOTCONN == errno)))
      {
         printf("[tcp_send] Connection lost: errno=%d (%s)\n", errno, strerror(errno));
         close(self->sockfd);
         self->sockfd = -1;
         return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND);
      }

      printf("[tcp_send] send error: errno=%d (%s)\n", errno, strerror(errno));
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_COMM_TX);
   }

   // printf("[tcp_send] send() returned %u\n", (unsigned int)sent);
   return RESULT_OK;
}

static result_t tcp_recv(const comms_driver_interface_t *iface, uint8_t *data, uint16_t buflen)
{
   if((NULL == iface) || (NULL == iface->parent) || (NULL == data))
   {
      printf("[tcp_recv] Invalid input pointers.\n");
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_PTR_NULL);
   }

   tcp_link_t *self = (tcp_link_t *)iface->parent;
   if(self->sockfd < 0)
   {
      printf("[tcp_recv] Invalid socket state: sockfd=%d\n", self->sockfd);
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND);
   }

   if(buflen < MP_WIRE_MIN_PACKET_LEN)
   {
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_OVERFLOW);
   }

   // Deliver already-buffered complete packets first.
   result_t result = tcp_try_extract_mp_packet(self, data, buflen);
   if(IS_OK(result))
   {
      return RESULT_OK;
   }
   if(GET_ERR_CODE(result) != COMMS_DRIVER_ERROR_BUSY)
   {
      return result;
   }

   uint8_t rx_buf[MP_MAX_PACKET_LENGTH] = {0};
   ssize_t n = recv(self->sockfd, rx_buf, sizeof(rx_buf), MSG_DONTWAIT);

   if(n > 0)
   {
      // printf("[tcp_recv] Received %zd bytes: ", n);
      // for(ssize_t i = 0; i < n; ++i)
      // {
      //    printf("%02X ", rx_buf[i]);
      // }
      // printf("\n");

      const size_t rx_size = (size_t)n;
      const size_t remaining = TCP_LINK_RX_STAGING_CAPACITY - self->rx_staging_len;
      if(rx_size > remaining)
      {
         printf("[tcp_recv] RX staging overflow (%zu + %zu > %zu). Flushing staging buffer.\n",
                self->rx_staging_len,
                rx_size,
                (size_t)TCP_LINK_RX_STAGING_CAPACITY);
         self->rx_staging_len = 0u;
         return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_OVERFLOW);
      }

      memcpy(&self->rx_staging[self->rx_staging_len], rx_buf, rx_size);
      self->rx_staging_len += rx_size;

      return tcp_try_extract_mp_packet(self, data, buflen);
   }

   if(0 == n)
   {
      printf("[tcp_recv] n=0 (FIN from peer)\n");
      close(self->sockfd);
      self->sockfd = -1;
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND);
   }

   if((EAGAIN == errno) || (EWOULDBLOCK == errno) || (EINTR == errno))
   {
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_BUSY);
   }

   if((ECONNRESET == errno) || (ENOTCONN == errno))
   {
      printf("[tcp_recv] Connection lost: errno=%d (%s)\n", errno, strerror(errno));
      close(self->sockfd);
      self->sockfd = -1;
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND);
   }

   printf("[tcp_recv] recv error: errno=%d (%s)\n", errno, strerror(errno));
   return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_COMM_RX);
}

static result_t tcp_max_len(const comms_driver_interface_t *iface, uint16_t *maxlen)
{
   if((NULL == iface) || (NULL == iface->parent) || (NULL == maxlen))
   {
      return RESULT_THIS_UNIT_ERROR(COMMS_DRIVER_ERROR_PTR_NULL);
   }
   *maxlen = MP_MAX_PACKET_LENGTH;
   return RESULT_OK;
}

result_t tcp_link_init(tcp_link_t *link, const char *host, int port, bool is_master)
{
   if(!link)
      return -1;
   memset(link, 0, sizeof(*link));
   link->sockfd = -1;
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(sockfd < 0)
      return -2;
   // Allow immediate reuse of the port after program exit
   int optval = 1;
   setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
   if(is_master)
   {
      // Server mode
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
      addr.sin_port = htons(port);
      if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
      {
         close(sockfd);
         return -3;
      }
      if(listen(sockfd, 1) < 0)
      {
         close(sockfd);
         return -4;
      }
      // printf("[tcp_link_init] Waiting for client to connect on port %d...\n", port);
      struct sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);
      int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
      // printf("[tcp_link_init] Client connected from %s:%d (fd=%d)\n",
      //        inet_ntoa(client_addr.sin_addr),
      //        ntohs(client_addr.sin_port),
      //        clientfd);
      if(clientfd < 0)
      {
         close(sockfd);
         return -5;
      }
      close(sockfd); // No longer need the listening socket
      link->sockfd = clientfd;
      // printf("[tcp_link_init] Client connected!\n");
   }
   else
   {
      // Client mode
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(port);
      struct hostent *he = gethostbyname(host);
      if(he == NULL || he->h_addrtype != AF_INET)
      {
         close(sockfd);
         return -6;
      }
      memcpy(&server_addr.sin_addr, he->h_addr, he->h_length);
      if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
      {
         close(sockfd);
         return -7;
      }
      link->sockfd = sockfd;
      printf("[tcp_link_init] Connected to server %s:%d\n", host, port);
   }
   link->interface.parent = (void *)link;
   link->interface.send_packet = tcp_send;
   link->interface.get_packet = tcp_recv;
   link->interface.get_max_packet_length = tcp_max_len;
   return 0;
}

void tcp_link_close(tcp_link_t *link)
{
   if(link && link->sockfd >= 0)
   {
      // printf("[tcp_link_close] Closing socket %d\n", link->sockfd);
      close(link->sockfd);
      link->sockfd = -1;
   }
   else
   {
      printf("[tcp_link_close] Nothing to close (link=%p, sockfd=%d)\n", link, link ? link->sockfd : -1);
   }
}
