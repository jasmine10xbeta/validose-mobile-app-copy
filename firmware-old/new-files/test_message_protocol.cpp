#include <gtest/gtest.h>

/**
 * @brief Unit tests for the message protocol (MP) reliability layer.
 *
 * These tests use a lightweight in-memory mock of the link layer to simulate
 * packet delivery between message protocol instances.
 */

/**
 * @todo Verify the mocked link layer implementation.
 */

extern "C"
{
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "comms_driver_interface.h"
#include "crc16.h"
#include "system_time.h"
#include "system_time_interface.h"
#include "utils/message_protocol/message_protocol.h"
}

typedef enum
{
   TEST_PPI_STATUS_UPDATE = 0,
   TEST_PPI_TIME_UPDATE = 1u,
   TEST_PPI_DEBUG_LOG = 3u,
   TEST_PPI_MAX = 3u
} TEST_PPI; // Test-only PPI enum to keep unit tests decoupled from application PPIs.

/***********************************************************************************************************************
 * Test utilities
 **********************************************************************************************************************/

static result_t make_result(uint8_t unit_id, uint8_t err_code)
{
   return (result_t)(((result_t)unit_id << 8u) | (result_t)(err_code & 0xFFu));
}

/**
 * @brief In-memory mock of a link-layer endpoint for MP unit tests.
 *
 * How to read this mock:
 * - Each endpoint represents one side of a "wire" and holds its own RX buffer.
 * - Sending is modeled as a memory copy into the peer's RX buffer.
 * - Receiving is modeled as draining that RX buffer.
 * - There is no real transport or queue; each direction has a single-slot RX buffer.
 *
 * Full duplex behavior:
 * - A->B and B->A can both succeed as long as each peer's RX buffer is empty.
 * - If the peer RX buffer is full, send_packet returns BUSY (backpressure).
 *
 * This keeps the mock simple but still surfaces contention: two endpoints can
 * transmit at the same time only if they are not overwriting the other's RX slot.
 */
typedef struct comms_driver
{
   comms_driver_interface_t interface;        // Vtable used by MP to call into this mock.
   struct comms_driver *peer;                 // Other endpoint; sending writes into peer->rx_buf.
   uint16_t max_packet_len;                   // Simulated MTU (max bytes accepted by send_packet).
   uint32_t send_count;                       // Telemetry: count of successful sends from this endpoint.
   uint16_t last_tx_len;                      // Telemetry: length of last transmitted frame.
   uint8_t last_tx_buf[MP_MAX_PACKET_LENGTH]; // Telemetry: snapshot of last transmitted bytes.
   bool has_data;                             // RX buffer occupancy flag (single-slot RX queue).
   uint16_t rx_len;                           // Length of valid data in rx_buf.
   uint8_t rx_buf[MP_MAX_PACKET_LENGTH];      // RX buffer that peer writes into.
} test_link_endpoint_t;

static result_t
   test_link_send_packet(const comms_driver_interface_t *const interface, const uint8_t *data, uint16_t length)
{
   // Send path: validate inputs, enforce MTU, then copy into peer's RX buffer.
   if((NULL == interface) || (NULL == interface->parent) || (NULL == data))
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_PTR_NULL);
   }

   test_link_endpoint_t *self = (test_link_endpoint_t *)interface->parent;

   if(NULL == self->peer)
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_DEVICE_NOT_FOUND);
   }

   // Enforce the simulated MTU for this endpoint and the global max buffer.
   if((length > self->max_packet_len) || (length > MP_MAX_PACKET_LENGTH))
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_INVALID_TX_LENGTH);
   }

   // Single-slot RX buffer: if the peer still has unread data, we can't send another frame to it.
   if(true == self->peer->has_data)
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_BUSY);
   }

   // "Transmit" by copying data into peer RX buffer and marking it available.
   memcpy(self->peer->rx_buf, data, length);
   if(length < MP_MAX_PACKET_LENGTH)
   {
      memset(&self->peer->rx_buf[length], 0, MP_MAX_PACKET_LENGTH - length);
   }

   self->peer->rx_len = length;
   self->peer->has_data = true;
   self->send_count++;
   self->last_tx_len = length;
   memcpy(self->last_tx_buf, data, length);

   return RESULT_OK;
}

static result_t
   test_link_get_packet(const comms_driver_interface_t *const interface, uint8_t *data, uint16_t data_buffer_size)
{
   // Receive path: if no data, return BUSY (treated as "no packet" by MP).
   if((NULL == interface) || (NULL == interface->parent) || (NULL == data))
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_PTR_NULL);
   }

   // This mock expects a full-size buffer so it can always fill the caller's space.
   if(data_buffer_size < MP_MAX_PACKET_LENGTH)
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_INVALID_TX_LENGTH);
   }

   test_link_endpoint_t *self = (test_link_endpoint_t *)interface->parent;

   if(false == self->has_data)
   {
      memset(data, 0, data_buffer_size);
      // MP treats NFC BUSY as "no data"; use that to avoid propagating errors.
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_BUSY);
   }

   // Drain the RX buffer into the caller's buffer and then clear the RX slot.
   memcpy(data, self->rx_buf, self->rx_len);
   if(self->rx_len < data_buffer_size)
   {
      memset(&data[self->rx_len], 0, data_buffer_size - self->rx_len);
   }

   self->has_data = false;
   self->rx_len = 0u;
   memset(self->rx_buf, 0, sizeof(self->rx_buf));

   return RESULT_OK;
}

static result_t test_link_get_max_packet_length(const comms_driver_interface_t *const interface,
                                                uint16_t *max_packet_len)
{
   // Simple MTU query: just return the configured max_packet_len for this endpoint.
   if((NULL == interface) || (NULL == interface->parent) || (NULL == max_packet_len))
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_PTR_NULL);
   }

   test_link_endpoint_t *self = (test_link_endpoint_t *)interface->parent;
   *max_packet_len = self->max_packet_len;
   return RESULT_OK;
}

static void test_link_endpoint_init(test_link_endpoint_t *endpoint, uint16_t max_packet_len)
{
   // Initialize a clean endpoint with the mock vtable and a starting MTU.
   ASSERT_NE(nullptr, endpoint);
   memset(endpoint, 0, sizeof(*endpoint));
   endpoint->interface.parent = endpoint;
   endpoint->interface.send_packet = test_link_send_packet;
   endpoint->interface.get_packet = test_link_get_packet;
   endpoint->interface.get_max_packet_length = test_link_get_max_packet_length;
   endpoint->max_packet_len = max_packet_len;
}

typedef struct
{
   uint32_t next_id;
} session_id_gen_t;

static session_id_gen_t g_master_session_gen = {0xA5A5A5A5u};

static result_t generate_session_id_master(const message_protocol_t *self, uint32_t *session_id)
{
   (void)self;
   if(NULL == session_id)
   {
      return make_result((uint8_t)SW_UNIT_ID_MESSAGE_PROTOCOL, MSG_PROT_ERROR_NULL_PTR);
   }

   *session_id = g_master_session_gen.next_id;
   g_master_session_gen.next_id += 1u;

   return RESULT_OK;
}

static void build_wire_packet(const mp_packet_t *packet, uint8_t *buffer, uint16_t *length_out)
{
   ASSERT_NE(nullptr, packet);
   ASSERT_NE(nullptr, buffer);
   ASSERT_NE(nullptr, length_out);

   const uint16_t header_len = 2u + 2u + 4u + 1u + 1u; // See mp_packet_header_t
   const uint16_t payload_hdr_len = MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
   const uint16_t packet_length = (uint16_t)(header_len + payload_hdr_len + packet->payload.pkt_payload_len);

   uint8_t crc_offset = sizeof(packet->header.pkt_crc);
   size_t offset = crc_offset;

   buffer[offset++] = (uint8_t)(packet->header.pkt_counter & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.pkt_counter >> 8u) & 0xFFu);
   buffer[offset++] = (uint8_t)(packet->header.session_id & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.session_id >> 8u) & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.session_id >> 16u) & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.session_id >> 24u) & 0xFFu);
   buffer[offset++] = packet->header.pkt_type;
   buffer[offset++] = packet->header.status;

   buffer[offset++] = packet->payload.ppi;
   buffer[offset++] = (uint8_t)(packet->payload.pkt_payload_len & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->payload.pkt_payload_len >> 8u) & 0xFFu);

   if(0u < packet->payload.pkt_payload_len)
   {
      memcpy(&buffer[offset], packet->payload.payload, packet->payload.pkt_payload_len);
   }

   uint16_t crc = 0xFFFF;
   crc = crc16_update(&buffer[crc_offset], packet_length - crc_offset, &crc);
   buffer[0] = (uint8_t)(crc & 0xFFu);
   buffer[1] = (uint8_t)((crc >> 8u) & 0xFFu);

   *length_out = packet_length;
}

static void build_wire_packet_full(const mp_packet_t *packet, uint8_t *buffer, uint16_t *length_out)
{
   // Build a complete on-wire MP packet including the payload "type" field.
   // This helper mirrors the production encoder so injected packets match the real wire format.
   ASSERT_NE(nullptr, packet);
   ASSERT_NE(nullptr, buffer);
   ASSERT_NE(nullptr, length_out);

   const uint16_t header_len = 2u + 2u + 4u + 1u + 1u; // See mp_packet_header_t
   const uint16_t payload_hdr_len = MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE;
   const uint16_t packet_length = (uint16_t)(header_len + payload_hdr_len + packet->payload.pkt_payload_len);

   uint8_t crc_offset = sizeof(packet->header.pkt_crc);
   size_t offset = crc_offset;

   buffer[offset++] = (uint8_t)(packet->header.pkt_counter & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.pkt_counter >> 8u) & 0xFFu);
   buffer[offset++] = (uint8_t)(packet->header.session_id & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.session_id >> 8u) & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.session_id >> 16u) & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->header.session_id >> 24u) & 0xFFu);
   buffer[offset++] = packet->header.pkt_type;
   buffer[offset++] = packet->header.status;

   buffer[offset++] = packet->payload.type;
   buffer[offset++] = packet->payload.ppi;
   buffer[offset++] = (uint8_t)(packet->payload.pkt_payload_len & 0xFFu);
   buffer[offset++] = (uint8_t)((packet->payload.pkt_payload_len >> 8u) & 0xFFu);

   if(0u < packet->payload.pkt_payload_len)
   {
      memcpy(&buffer[offset], packet->payload.payload, packet->payload.pkt_payload_len);
   }

   uint16_t crc = crc16_update(&buffer[crc_offset], packet_length - crc_offset, NULL);
   buffer[0] = (uint8_t)(crc & 0xFFu);
   buffer[1] = (uint8_t)((crc >> 8u) & 0xFFu);

   *length_out = packet_length;
}

static void link_inject_packet(test_link_endpoint_t *endpoint, const uint8_t *data, uint16_t length)
{
   // Directly stuff bytes into an endpoint RX buffer (bypasses send_packet/MTU checks).
   // This is useful for crafting custom frames (e.g., bad CRC, wrong IDs) in tests.
   ASSERT_NE(nullptr, endpoint);
   ASSERT_NE(nullptr, data);
   ASSERT_FALSE(endpoint->has_data);
   ASSERT_LE(length, MP_MAX_PACKET_LENGTH);

   memcpy(endpoint->rx_buf, data, length);
   if(length < MP_MAX_PACKET_LENGTH)
   {
      memset(&endpoint->rx_buf[length], 0, MP_MAX_PACKET_LENGTH - length);
   }
   endpoint->rx_len = length;
   endpoint->has_data = true;
}

typedef struct
{
   // Single-slot mailbox that intentionally retains data across reads.
   bool has_data;
   uint16_t len;
   uint8_t buf[MP_MAX_PACKET_LENGTH];
} stale_mailbox_t;

static stale_mailbox_t g_stale_mailbox = {0};

static void stale_mailbox_clear(void)
{
   // Reset the simulated mailbox so it returns "no data" on the next read.
   // This keeps each test isolated and predictable.
   memset(&g_stale_mailbox, 0, sizeof(g_stale_mailbox));
}

static void stale_mailbox_load_bytes(const uint8_t *data, uint16_t length)
{
   // Load raw on-wire bytes into the simulated mailbox.
   // The mailbox will keep returning the same bytes until cleared.
   ASSERT_NE(nullptr, data);
   ASSERT_LE(length, MP_MAX_PACKET_LENGTH);
   memcpy(g_stale_mailbox.buf, data, length);
   if(length < MP_MAX_PACKET_LENGTH)
   {
      memset(&g_stale_mailbox.buf[length], 0, MP_MAX_PACKET_LENGTH - length);
   }
   g_stale_mailbox.len = length;
   g_stale_mailbox.has_data = true;
}

static result_t
   stale_link_get_packet(const message_protocol_t *const interface, uint8_t *data, uint16_t data_buffer_size)
{
   // Test-only get_packet() that never clears the mailbox.
   // This simulates a link layer mailbox where reads do not consume data.
   (void)interface;
   if((NULL == data))
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_PTR_NULL);
   }

   if(data_buffer_size < MP_MAX_PACKET_LENGTH)
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_INVALID_TX_LENGTH);
   }

   if(false == g_stale_mailbox.has_data)
   {
      memset(data, 0, data_buffer_size);
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_BUSY);
   }

   // Copy the same bytes on every call to represent a stale mailbox read.
   memcpy(data, g_stale_mailbox.buf, g_stale_mailbox.len);
   if(g_stale_mailbox.len < data_buffer_size)
   {
      memset(&data[g_stale_mailbox.len], 0, data_buffer_size - g_stale_mailbox.len);
   }

   // Intentionally do not clear mailbox contents to simulate a stale read.
   return RESULT_OK;
}

static uint32_t g_mailbox_send_count = 0u;

static result_t mailbox_send_pkt_to_link_layer(const message_protocol_t *const interface, const mp_packet_t *packet)
{
   (void)interface;
   if(NULL == packet)
   {
      return make_result((uint8_t)SW_UNIT_ID_MESSAGE_PROTOCOL, MSG_PROT_ERROR_NULL_PTR);
   }

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);
   g_mailbox_send_count++;

   return RESULT_OK;
}

static result_t mailbox_get_packet(const message_protocol_t *const interface, uint8_t *data, uint16_t data_buffer_size)
{
   // Mailbox read helper: returns OK even when empty (mirrors MP's BUSY->OK behavior).
   (void)interface;
   if((NULL == data))
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_PTR_NULL);
   }

   if(data_buffer_size < MP_MAX_PACKET_LENGTH)
   {
      return make_result((uint8_t)SW_UNIT_ID_NFC_READER_DRV, COMMS_DRIVER_ERROR_INVALID_TX_LENGTH);
   }

   if(false == g_stale_mailbox.has_data)
   {
      memset(data, 0, data_buffer_size);
      return RESULT_OK;
   }

   memcpy(data, g_stale_mailbox.buf, g_stale_mailbox.len);
   if(g_stale_mailbox.len < data_buffer_size)
   {
      memset(&data[g_stale_mailbox.len], 0, data_buffer_size - g_stale_mailbox.len);
   }

   // Do not clear mailbox contents to simulate stale reads.
   return RESULT_OK;
}

static const char *rx_packet_status_to_cstr(MSG_PROT_RX_PACKET_STATUS status)
{
   switch(status)
   {
      case MSG_PROT_RX_PACKET_STATUS_NONE:
         return "RX_NONE";
      case MSG_PROT_RX_PACKET_STATUS_NEW:
         return "RX_NEW";
      case MSG_PROT_RX_PACKET_STATUS_PROCESSED:
         return "RX_PROCESSED";
      default:
         return "RX_UNKNOWN";
   }
}

static const char *tx_packet_status_to_cstr(MSG_PROT_TX_PACKET_STATUS status)
{
   switch(status)
   {
      case MSG_PROT_TX_PACKET_STATUS_NONE:
         return "TX_NONE";
      case MSG_PROT_TX_PACKET_STATUS_NEW:
         return "TX_NEW";
      case MSG_PROT_TX_PACKET_STATUS_COMPLETED:
         return "TX_COMPLETED";
      case MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK:
         return "TX_WAITING_FOR_ACK";
      case MSG_PROT_TX_PACKET_STATUS_ERROR:
         return "TX_ERROR";
      case MSG_PROT_TX_PACKET_STATUS_ABANDONED:
         return "TX_ABANDONED";
      default:
         return "TX_UNKNOWN";
   }
}

static void print_mp_payload(const char *label, const mp_packet_payload_t *payload)
{
   printf("%s ppi=%u len=%u bytes:", label, (unsigned)payload->ppi, (unsigned)payload->pkt_payload_len);
   if(payload->pkt_payload_len == 0u)
   {
      printf(" <empty>");
   }
   else
   {
      for(uint16_t i = 0u; i < payload->pkt_payload_len; i++)
      {
         printf(" %02X", payload->payload[i]);
      }
   }
   printf("\n");
}

static void print_mp_statuses(message_protocol_t *mp_a, message_protocol_t *mp_b)
{
   MSG_PROT_RX_PACKET_STATUS b_rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   MSG_PROT_TX_PACKET_STATUS b_tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   MSG_PROT_TX_PACKET_STATUS a_tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   MSG_PROT_RX_PACKET_STATUS a_rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;

   (void)mp_b->interface.get_rx_packet_status(&mp_b->interface, &b_rx_status);
   (void)mp_b->interface.get_tx_packet_status(&mp_b->interface, &b_tx_status);
   (void)mp_a->interface.get_rx_packet_status(&mp_a->interface, &a_rx_status);
   (void)mp_a->interface.get_tx_packet_status(&mp_a->interface, &a_tx_status);

   printf("  B RX status: %s\n", rx_packet_status_to_cstr(b_rx_status));
   printf("  B TX status: %s\n", tx_packet_status_to_cstr(b_tx_status));
   printf("  A RX status: %s\n", rx_packet_status_to_cstr(a_rx_status));
   printf("  A TX status: %s\n\n", tx_packet_status_to_cstr(a_tx_status));
}

/***********************************************************************************************************************
 * Test fixture
 **********************************************************************************************************************/

class message_protocol_suite: public testing::Test
{
protected:
   void SetUp() override
   {
      ASSERT_TRUE(IS_OK(system_time_init(&m_time)));
      test_link_endpoint_init(&m_link_a, MP_MAX_PACKET_LENGTH);
      test_link_endpoint_init(&m_link_b, MP_MAX_PACKET_LENGTH);
      m_link_a.peer = &m_link_b;
      m_link_b.peer = &m_link_a;
      g_master_session_gen.next_id = 0xA5A5A5A5u;
      stale_mailbox_clear();

      ASSERT_TRUE(
         IS_OK(message_protocol_init(&m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, NULL)));
      // Prime the instance so it can query link layer MTU and set its internal max payload length.
      prime_process(&m_mp_a.interface);
   }

   void prime_process(message_protocol_interface_t *ifc)
   {
      ASSERT_NE(nullptr, ifc);
      // Prime the instance so it can query link layer MTU and set its internal max payload length.
      (void)ifc->process(ifc);
   }

   void pump_process(message_protocol_interface_t *ifc_a, message_protocol_interface_t *ifc_b, uint8_t steps)
   {
      ASSERT_NE(nullptr, ifc_a);
      ASSERT_NE(nullptr, ifc_b);
      for(uint8_t i = 0u; i < steps; i++)
      {
         (void)ifc_a->process(ifc_a);
         (void)ifc_b->process(ifc_b);
      }
   }

   system_time_t m_time{};
   test_link_endpoint_t m_link_a{};
   test_link_endpoint_t m_link_b{};
   message_protocol_t m_mp_a{};
   message_protocol_t m_mp_b{};
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
// =====================================================================================================================
// Initialization & API guardrails
// =====================================================================================================================

TEST_F(message_protocol_suite, init_null_checks)
{
   // Edge case: NULL arguments should return MSG_PROT_ERROR_NULL_PTR.
   result_t result = RESULT_OK;

   result = message_protocol_init(NULL, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, NULL);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_NULL_PTR, GET_ERR_CODE(result));

   result = message_protocol_init(&m_mp_a, NULL, &m_link_a.interface, true, NULL, NULL, NULL, NULL);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_NULL_PTR, GET_ERR_CODE(result));

   result = message_protocol_init(&m_mp_a, &m_time.interface, NULL, true, NULL, NULL, NULL, NULL);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_NULL_PTR, GET_ERR_CODE(result));
}

TEST_F(message_protocol_suite, get_rx_packet_no_new_returns_error)
{
   // Edge case: get_rx_packet() called without a NEW packet should return MSG_PROT_ERROR_NO_NEW_PACKET.
   mp_packet_payload_t rx_payload = {0};
   result_t result = m_mp_a.interface.get_rx_packet(&m_mp_a.interface, &rx_payload);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_NO_NEW_PACKET, GET_ERR_CODE(result));
}

TEST_F(message_protocol_suite, send_rejects_when_tx_busy)
{
   // Edge case: send() should reject when a TX packet is already in-flight.
   m_mp_a._tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK;

   mp_packet_payload_t payload = {0};
   payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   payload.pkt_payload_len = 1u;
   payload.payload[0] = 0xAAu;

   result_t result = m_mp_a.interface.send(&m_mp_a.interface, &payload);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_BUSY, GET_ERR_CODE(result));
}

TEST_F(message_protocol_suite, send_and_get_tx_status_blocked_while_syncing)
{
   // Edge case: while syncing, send() must reject new packets and TX status must report ERROR.
   m_mp_a._tx_packet.header.status = (uint8_t)MSG_PROT_TX_PACKET_STATUS_COMPLETED;
   m_mp_a._is_syncing = true;

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_ERROR, tx_status);

   mp_packet_payload_t payload = {0};
   payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   payload.pkt_payload_len = 1u;
   payload.payload[0] = 0xA5u;

   result_t result = m_mp_a.interface.send(&m_mp_a.interface, &payload);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_BUSY, GET_ERR_CODE(result));

   // Once syncing clears, status should reflect the real TX state and send should be allowed again.
   m_mp_a._is_syncing = false;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_COMPLETED, tx_status);
   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &payload)));
}

TEST_F(message_protocol_suite, send_rejects_payload_too_large)
{
   // Edge case: payload larger than the negotiated MP payload size should return MSG_PROT_ERROR_PAYLOAD_TOO_BIG.
   m_link_a.max_packet_len
      = (uint16_t)(sizeof(mp_packet_header_t) + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE
                   + 2u); // I.e. Max actual payload that the link layer now can accommodate is 2 bytes
   prime_process(&m_mp_a.interface);

   mp_packet_payload_t payload = {0};
   payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   payload.pkt_payload_len = 6u; // Intentionally exceeds the tiny max payload size.
   payload.payload[0] = 0x10u;
   payload.payload[1] = 0x11u;
   payload.payload[2] = 0x12u;
   payload.payload[3] = 0x13u;
   payload.payload[4] = 0x14u;
   payload.payload[5] = 0x15u;

   result_t result = m_mp_a.interface.send(&m_mp_a.interface, &payload);
   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_PAYLOAD_TOO_BIG, GET_ERR_CODE(result));
}

TEST_F(message_protocol_suite, send_allows_zero_length_payload)
{
   // Edge case: zero-length payloads should be accepted and transmitted.
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_b.interface);

   // Give both MP instances the same session ID
   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x1111u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 0u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   pump_process(&m_mp_a.interface, &m_mp_b.interface, 4u);

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_COMPLETED, tx_status);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_b.interface.get_rx_packet_status(&m_mp_b.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);
}

TEST_F(message_protocol_suite, get_max_payload_length_returns_clamped_value)
{
   // Edge case: verify clamp at MP limit and clamp at LL MTU limit.
   const uint16_t overhead = (uint16_t)(sizeof(mp_packet_header_t) + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE);
   uint16_t max_payload_len = 0u;
   uint16_t expected = 0u;

   // Case 1: LL MTU is large enough to hit the MP max payload cap.
   m_link_a.max_packet_len = (uint16_t)(overhead + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN + 10u);
   prime_process(&m_mp_a.interface);
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_max_payload_length(&m_mp_a.interface, &max_payload_len)));
   expected = MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN;
   ASSERT_EQ(expected, max_payload_len);

   // Case 2: LL MTU is smaller than the MP max payload cap.
   m_link_a.max_packet_len = (uint16_t)(overhead + 8u);
   prime_process(&m_mp_a.interface);
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_max_payload_length(&m_mp_a.interface, &max_payload_len)));
   expected = 8u;
   ASSERT_EQ(expected, max_payload_len);
}

// =====================================================================================================================
// ACK / NAK / retry behavior
// =====================================================================================================================

TEST_F(message_protocol_suite, master_to_slave_data_exchange_happy_path)
{
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   (void)m_mp_a.interface.process(&m_mp_a.interface);
   (void)m_mp_b.interface.process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x1111u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 2u;
   tx_payload.payload[0] = 0xABu;
   tx_payload.payload[1] = 0xCDu;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   // Explicit ACK path: DATA -> ACK -> TX completed.
   pump_process(&m_mp_a.interface, &m_mp_b.interface, 4u);

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_COMPLETED, tx_status);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_b.interface.get_rx_packet_status(&m_mp_b.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_b.interface.get_rx_packet(&m_mp_b.interface, &rx_payload)));
   ASSERT_EQ(tx_payload.ppi, rx_payload.ppi);
   ASSERT_EQ(tx_payload.pkt_payload_len, rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(tx_payload.payload, rx_payload.payload, tx_payload.pkt_payload_len));
}

TEST_F(message_protocol_suite, ack_with_wrong_id_does_not_complete)
{
   // Edge case: ACK with a mismatched packet counter should not complete the TX.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0x3333u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x55u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   (void)m_mp_a.interface.process(&m_mp_a.interface); // Todo: why is this cast to void?

   mp_packet_t ack_packet = {0};
   ack_packet.header.pkt_counter = (uint16_t)(m_mp_a._pending_id + 1u);
   ack_packet.header.session_id = m_mp_a._current_session_id;
   ack_packet.header.pkt_type = MP_PACKET_TYPE_ACK;
   ack_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   ack_packet.payload.ppi = 0u;
   ack_packet.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet(&ack_packet, wire_buf, &wire_len);
   link_inject_packet(&m_link_a, wire_buf, wire_len);

   (void)m_mp_a.interface.process(&m_mp_a.interface);

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);
}

TEST_F(message_protocol_suite, nack_triggers_immediate_resend_and_eventual_ack)
{
   // Edge case: when RX is busy, the receiver must NAK and the sender must resend.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x1111u;

   m_mp_b._rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_NEW;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x77u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   // DATA sent -> NAK returned -> immediate resend.
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   ASSERT_GE(m_link_a.send_count, 2u);

   // Free the RX buffer and allow the resend to be accepted and ACKed.
   m_mp_b._rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED;
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_COMPLETED, tx_status);
}

TEST_F(message_protocol_suite, timeout_after_retries_abandons_packet)
{
   // Edge case: no ACKs received should exhaust retries and abandon the TX packet.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0x1234u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x5Au;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   result_t result = RESULT_OK;
   for(uint8_t i = 0u; i <= MSG_PROT_MAX_RETRIES; i++)
   {
      ASSERT_TRUE(IS_OK(m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(ACK_TIMEOUT_MS + 1u))));
      result = m_mp_a.interface.process(&m_mp_a.interface);
   }

   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_TIMEOUT, GET_ERR_CODE(result));

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_ABANDONED, tx_status);
}

// =====================================================================================================================
// CRC and corruption handling
// =====================================================================================================================

TEST_F(message_protocol_suite, corrupted_frame_is_ignored)
{
   // Edge case: corrupted CRC is ignored, then retry triggers NAK (busy RX), then resend is accepted.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x1111u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x42u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   // Initial send: corrupt the frame before the slave processes it so it is ignored.
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   m_link_b.rx_buf[0] ^= 0xFFu;
   (void)m_mp_b.interface.process(&m_mp_b.interface);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_b.interface.get_rx_packet_status(&m_mp_b.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_PROCESSED, rx_status);

   // Advance time to force a retry from the sender.
   ASSERT_TRUE(IS_OK(m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(ACK_TIMEOUT_MS + 1u))));
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_GE(m_link_a.send_count, 2u);
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);

   // Receiver is busy on the retry, so it NAKs and the sender immediately resends.
   m_mp_b._rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_NEW;
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   // Free the RX buffer so the next resend is accepted.
   m_mp_b._rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_PROCESSED;
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   ASSERT_TRUE(IS_OK(m_mp_b.interface.get_rx_packet_status(&m_mp_b.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_COMPLETED, tx_status);
}

// =====================================================================================================================
// Duplicate / stale packet handling
// =====================================================================================================================

TEST_F(message_protocol_suite, stale_data_packet_is_dropped)
{
   // Scenario: the link-layer mailbox keeps returning the same DATA bytes.
   // Expectation: MP accepts the DATA once (and ACKs once), then drops repeats.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, false, stale_link_get_packet, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0x1111u;

   // Step 1: Craft a valid DATA packet with a small payload.
   mp_packet_t data_packet = {0};
   data_packet.header.pkt_counter = 1u;
   data_packet.header.session_id = m_mp_a._current_session_id;
   data_packet.header.pkt_type = MP_PACKET_TYPE_DATA;
   data_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   data_packet.payload.type = 0x10u;
   data_packet.payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   data_packet.payload.pkt_payload_len = 2u;
   data_packet.payload.payload[0] = 0xAAu;
   data_packet.payload.payload[1] = 0xBBu;

   // Step 2: Encode to raw bytes and load into the stale mailbox.
   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&data_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // Step 3: First process() should accept the DATA and send exactly one ACK.
   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before + 1u, m_link_a.send_count);

   // Step 4: RX status must be NEW because the payload was delivered.
   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_rx_packet_status(&m_mp_a.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   // Step 5: Second process() reads the same stale bytes and must drop them.
   // Verify by ensuring no additional send occurs.
   send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before, m_link_a.send_count);

   // Step 6: RX status should still be NEW (original packet not overwritten).
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_rx_packet_status(&m_mp_a.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   // Step 7: Read back payload and ensure it matches the original DATA.
   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_rx_packet(&m_mp_a.interface, &rx_payload)));
   ASSERT_EQ(data_packet.payload.ppi, rx_payload.ppi);
   ASSERT_EQ(data_packet.payload.pkt_payload_len, rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(data_packet.payload.payload, rx_payload.payload, data_packet.payload.pkt_payload_len));

   // Step 8: Even after consuming RX, stale bytes must still be dropped.
   send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before, m_link_a.send_count);
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_rx_packet_status(&m_mp_a.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_PROCESSED, rx_status);
}

TEST_F(message_protocol_suite, duplicate_stale_session_data_on_slave_resends_sync_mismatch)
{
   // Scenario: slave receives repeated stale-session DATA (same bytes) because master did not receive SYNC_MISMATCH.
   // Expectation: slave re-sends SYNC_MISMATCH for each duplicate stale-session DATA packet.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, false, stale_link_get_packet, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0x1111u;

   mp_packet_t stale_data = {0};
   stale_data.header.pkt_counter = 7u;
   stale_data.header.session_id = 0x2222u; // Mismatch to force SYNC_MISMATCH on slave.
   stale_data.header.pkt_type = MP_PACKET_TYPE_DATA;
   stale_data.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   stale_data.payload.type = 0x10u;
   stale_data.payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   stale_data.payload.pkt_payload_len = 1u;
   stale_data.payload.payload[0] = 0xA5u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&stale_data, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // First process(): mismatch DATA -> SYNC_MISMATCH
   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before + 1u, m_link_a.send_count);
   ASSERT_GE(m_link_a.last_tx_len, (uint16_t)(2u + 2u + 4u + 1u + 1u + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE));
   ASSERT_EQ(MP_PACKET_TYPE_SYNC_MISMATCH, m_link_a.last_tx_buf[8]);
   const uint32_t first_tx_session_id = (uint32_t)m_link_a.last_tx_buf[4] | ((uint32_t)m_link_a.last_tx_buf[5] << 8u)
                                        | ((uint32_t)m_link_a.last_tx_buf[6] << 16u)
                                        | ((uint32_t)m_link_a.last_tx_buf[7] << 24u);
   ASSERT_EQ(m_mp_a._current_session_id, first_tx_session_id);

   // Clear peer RX slot in the single-slot mock transport so a second send can be observed.
   // (Without this, send_packet() returns BUSY and send_count won't increment.)
   m_link_b.has_data = false;
   m_link_b.rx_len = 0u;
   memset(m_link_b.rx_buf, 0, sizeof(m_link_b.rx_buf));

   // Second process(): duplicate stale-session DATA -> SYNC_MISMATCH again
   uint32_t send_after_first = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_after_first + 1u, m_link_a.send_count);
   ASSERT_GE(m_link_a.last_tx_len, (uint16_t)(2u + 2u + 4u + 1u + 1u + MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE));
   ASSERT_EQ(MP_PACKET_TYPE_SYNC_MISMATCH, m_link_a.last_tx_buf[8]);
   const uint32_t second_tx_session_id = (uint32_t)m_link_a.last_tx_buf[4] | ((uint32_t)m_link_a.last_tx_buf[5] << 8u)
                                         | ((uint32_t)m_link_a.last_tx_buf[6] << 16u)
                                         | ((uint32_t)m_link_a.last_tx_buf[7] << 24u);
   ASSERT_EQ(m_mp_a._current_session_id, second_tx_session_id);
}

TEST_F(message_protocol_suite, mailbox_echoed_tx_packet_is_dropped)
{
   // Scenario: mailbox returns the sender's own TX bytes until a peer overwrites them.
   // Expectation: MP drops the echoed TX packet and does not deliver or ACK it.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           mailbox_get_packet,
                                           mailbox_send_pkt_to_link_layer,
                                           NULL,
                                           NULL)));

   prime_process(&m_mp_a.interface);
   stale_mailbox_clear();
   g_mailbox_send_count = 0u;
   m_mp_a._current_session_id = 0x4444u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0xCCu;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   // First process() sends the packet into the mailbox.
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(1u, g_mailbox_send_count);

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);

   // Second process() reads the echoed TX bytes and should drop them.
   uint32_t send_before = g_mailbox_send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before, g_mailbox_send_count);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_rx_packet_status(&m_mp_a.interface, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_PROCESSED, rx_status);

   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);
}

TEST_F(message_protocol_suite, mailbox_echoed_ack_packet_is_dropped)
{
   // Scenario: mailbox returns the MP's own ACK bytes.
   // Expectation: echoed ACK is dropped and does not complete the pending TX.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           mailbox_get_packet,
                                           mailbox_send_pkt_to_link_layer,
                                           NULL,
                                           NULL)));

   prime_process(&m_mp_a.interface);
   stale_mailbox_clear();
   g_mailbox_send_count = 0u;
   m_mp_a._current_session_id = 0x5555u;

   // Create a pending TX so an echoed ACK could complete it.
   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0xA1u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);

   uint16_t pending_id = m_mp_a._pending_id;

   // Clear mailbox and inject peer DATA so MP emits ACK with matching pkt_counter.
   stale_mailbox_clear();
   mp_packet_t data_packet = {0};
   data_packet.header.pkt_counter = pending_id;
   data_packet.header.session_id = m_mp_a._current_session_id;
   data_packet.header.pkt_type = MP_PACKET_TYPE_DATA;
   data_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   data_packet.payload.type = 0x11u;
   data_packet.payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   data_packet.payload.pkt_payload_len = 1u;
   data_packet.payload.payload[0] = 0xB2u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&data_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   uint32_t send_before = g_mailbox_send_count;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_EQ(send_before + 1u, g_mailbox_send_count);

   // Echoed ACK should be dropped, leaving TX pending.
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_EQ(send_before + 1u, g_mailbox_send_count);
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);
}

TEST_F(message_protocol_suite, mailbox_echoed_nak_packet_is_dropped)
{
   // Scenario: mailbox returns the MP's own NAK bytes.
   // Expectation: echoed NAK is dropped and does not trigger a resend.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           mailbox_get_packet,
                                           mailbox_send_pkt_to_link_layer,
                                           NULL,
                                           NULL)));

   prime_process(&m_mp_a.interface);
   stale_mailbox_clear();
   g_mailbox_send_count = 0u;
   m_mp_a._current_session_id = 0x6666u;

   // Create a pending TX.
   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0xC3u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   (void)m_mp_a.interface.process(&m_mp_a.interface);

   uint32_t send_after_tx = g_mailbox_send_count;

   // Clear mailbox and inject peer DATA while RX buffer is busy to force NAK.
   stale_mailbox_clear();
   m_mp_a._rx_packet.header.status = (uint8_t)MSG_PROT_RX_PACKET_STATUS_NEW;

   mp_packet_t data_packet = {0};
   data_packet.header.pkt_counter = 7u;
   data_packet.header.session_id = m_mp_a._current_session_id;
   data_packet.header.pkt_type = MP_PACKET_TYPE_DATA;
   data_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   data_packet.payload.type = 0x22u;
   data_packet.payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   data_packet.payload.pkt_payload_len = 1u;
   data_packet.payload.payload[0] = 0xD4u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&data_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_EQ(send_after_tx + 1u, g_mailbox_send_count);

   uint32_t send_after_nak = g_mailbox_send_count;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_EQ(send_after_nak, g_mailbox_send_count);
}

TEST_F(message_protocol_suite, mailbox_echoed_sync_start_packet_is_dropped)
{
   // Scenario: mailbox returns the MP's own SYNC_START bytes.
   // Expectation: echoed SYNC_START is dropped and does not surface as an error.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           mailbox_get_packet,
                                           mailbox_send_pkt_to_link_layer,
                                           NULL,
                                           generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   stale_mailbox_clear();
   g_mailbox_send_count = 0u;
   m_mp_a._current_session_id = 0x7777u;

   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   mp_packet_t ack_packet = {0};
   ack_packet.header.pkt_counter = 1u;
   ack_packet.header.session_id = 0x8888u; // Mismatch to force sync start.
   ack_packet.header.pkt_type = MP_PACKET_TYPE_ACK;
   ack_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   ack_packet.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&ack_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   uint32_t send_before = g_mailbox_send_count;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_GT(g_mailbox_send_count, send_before);

   uint32_t send_after_first = g_mailbox_send_count;
   result_t result = m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(send_after_first, g_mailbox_send_count);
}

TEST_F(message_protocol_suite, mailbox_echoed_sync_ack_packet_is_dropped)
{
   // Scenario: mailbox returns the MP's own SYNC_ACK bytes.
   // Expectation: echoed SYNC_ACK is dropped and does not surface as an error.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           false,
                                           mailbox_get_packet,
                                           mailbox_send_pkt_to_link_layer,
                                           NULL,
                                           NULL)));

   prime_process(&m_mp_a.interface);
   stale_mailbox_clear();
   g_mailbox_send_count = 0u;

   mp_packet_t sync_start = {0};
   sync_start.header.pkt_counter = 1u;
   sync_start.header.session_id = 0x9999u;
   sync_start.header.pkt_type = MP_PACKET_TYPE_SYNC_START;
   sync_start.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   sync_start.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&sync_start, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   uint32_t send_before = g_mailbox_send_count;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_EQ(send_before + 1u, g_mailbox_send_count);

   uint32_t send_after_first = g_mailbox_send_count;
   result_t result = m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(send_after_first, g_mailbox_send_count);
}

TEST_F(message_protocol_suite, mailbox_echoed_sync_mismatch_packet_is_dropped)
{
   // Scenario: mailbox returns the MP's own SYNC_MISMATCH bytes.
   // Expectation: echoed SYNC_MISMATCH is dropped and does not surface as an error.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           false,
                                           mailbox_get_packet,
                                           mailbox_send_pkt_to_link_layer,
                                           NULL,
                                           NULL)));

   prime_process(&m_mp_a.interface);
   stale_mailbox_clear();
   g_mailbox_send_count = 0u;
   m_mp_a._current_session_id = 0xAAAAu;

   mp_packet_t data_packet = {0};
   data_packet.header.pkt_counter = 2u;
   data_packet.header.session_id = 0xBBBBu; // Mismatch to force SYNC_MISMATCH.
   data_packet.header.pkt_type = MP_PACKET_TYPE_DATA;
   data_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   data_packet.payload.type = 0x33u;
   data_packet.payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   data_packet.payload.pkt_payload_len = 1u;
   data_packet.payload.payload[0] = 0xE5u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&data_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   uint32_t send_before = g_mailbox_send_count;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface)));
   ASSERT_EQ(send_before + 1u, g_mailbox_send_count);

   uint32_t send_after_first = g_mailbox_send_count;
   result_t result = m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(send_after_first, g_mailbox_send_count);
}

TEST_F(message_protocol_suite, stale_ack_packet_does_not_trigger_multiple_sync_starts)
{
   // Scenario: mailbox repeatedly returns an ACK with a mismatched session ID.
   // Expectation: MP initiates SYNC_START once, then drops the duplicate ACK.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           stale_link_get_packet,
                                           NULL,
                                           NULL,
                                           generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0x1111u;

   // Allow sync start (respect MAX_TIME_BEFORE_SYNC_RETRY_MS guard).
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   // Step 1: Craft an ACK with a mismatched session ID.
   mp_packet_t ack_packet = {0};
   ack_packet.header.pkt_counter = 1u;
   ack_packet.header.session_id = 0x2222u; // Mismatch to force sync start on master.
   ack_packet.header.pkt_type = MP_PACKET_TYPE_ACK;
   ack_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   ack_packet.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&ack_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // Step 2: First process() should send SYNC_START exactly once.
   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_GT(m_link_a.send_count, send_before);

   uint32_t send_after_first = m_link_a.send_count;
   // Step 3: Second process() sees the same stale ACK and must drop it.
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_after_first, m_link_a.send_count);
}

TEST_F(message_protocol_suite, startup_stale_session_data_on_master_triggers_immediate_sync_start)
{
   // Scenario: master just booted and receives stale-session DATA before MAX_TIME_BEFORE_SYNC_RETRY_MS.
   // Expectation: first mismatch must trigger SYNC_START immediately; duplicate retries are then dropped.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           stale_link_get_packet,
                                           NULL,
                                           NULL,
                                           generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   uint64_t startup_time_ms = 0u;
   ASSERT_TRUE(IS_OK(m_time.interface.get_time_ms(&m_time.interface, &startup_time_ms)));
   ASSERT_EQ(0u, startup_time_ms); // Reproduce startup timing (no delay before first inbound packet).

   mp_packet_t stale_data = {0};
   stale_data.header.pkt_counter = 1u;
   stale_data.header.session_id = 0x2222u; // Mismatch with master's startup session_id (0).
   stale_data.header.pkt_type = MP_PACKET_TYPE_DATA;
   stale_data.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   stale_data.payload.type = 0x10u;
   stale_data.payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   stale_data.payload.pkt_payload_len = 1u;
   stale_data.payload.payload[0] = 0x5Au;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&stale_data, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before + 1u, m_link_a.send_count);
   ASSERT_TRUE(m_mp_a._is_syncing);
   ASSERT_EQ(MP_PACKET_TYPE_SYNC_START, m_link_a.last_tx_buf[8]);

   const uint32_t tx_session_id = (uint32_t)m_link_a.last_tx_buf[4] | ((uint32_t)m_link_a.last_tx_buf[5] << 8u)
                                  | ((uint32_t)m_link_a.last_tx_buf[6] << 16u)
                                  | ((uint32_t)m_link_a.last_tx_buf[7] << 24u);
   ASSERT_EQ(m_mp_a._current_session_id, tx_session_id);

   // Clear peer RX slot in the single-slot mock transport to detect accidental re-send.
   m_link_b.has_data = false;
   m_link_b.rx_len = 0u;
   memset(m_link_b.rx_buf, 0, sizeof(m_link_b.rx_buf));

   uint32_t send_after_first = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_after_first, m_link_a.send_count);
}

TEST_F(message_protocol_suite, stale_nak_packet_does_not_trigger_multiple_resends)
{
   // Scenario: mailbox repeatedly returns a valid NAK for a pending TX.
   // Expectation: MP resends once on the first NAK, then drops duplicates.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, stale_link_get_packet, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0x3333u;

   // Step 1: Send one DATA so we have a pending TX to NAK.
   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x5Au;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   (void)m_mp_a.interface.process(&m_mp_a.interface); // Send initial DATA

   uint32_t send_after_tx = m_link_a.send_count;

   // Step 2: Craft a NAK for the pending packet ID.
   mp_packet_t nak_packet = {0};
   nak_packet.header.pkt_counter = m_mp_a._pending_id;
   nak_packet.header.session_id = m_mp_a._current_session_id;
   nak_packet.header.pkt_type = MP_PACKET_TYPE_NAK;
   nak_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   nak_packet.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&nak_packet, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // Step 3: First process() should resend exactly once.
   (void)m_mp_a.interface.process(&m_mp_a.interface); // First NAK triggers resend
   ASSERT_EQ(send_after_tx + 1u, m_link_a.send_count);

   // Step 4: Second process() sees same NAK again and must drop it.
   uint32_t send_after_first_nak = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface); // Duplicate NAK dropped
   ASSERT_EQ(send_after_first_nak, m_link_a.send_count);
}

TEST_F(message_protocol_suite, stale_sync_start_packet_is_dropped_after_first_ack)
{
   // Scenario: mailbox repeatedly returns SYNC_START to a slave.
   // Expectation: MP replies with SYNC_ACK once, then drops duplicates.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, false, stale_link_get_packet, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);

   // Step 1: Craft SYNC_START bytes from a master.
   mp_packet_t sync_start = {0};
   sync_start.header.pkt_counter = 1u;
   sync_start.header.session_id = 0x9876u;
   sync_start.header.pkt_type = MP_PACKET_TYPE_SYNC_START;
   sync_start.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   sync_start.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&sync_start, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // Step 2: First process() should send SYNC_ACK and adopt the session ID.
   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_before + 1u, m_link_a.send_count);
   ASSERT_EQ(m_mp_a._current_session_id, sync_start.header.session_id);

   // Step 3: Second process() sees stale SYNC_START again and must drop it.
   uint32_t send_after_first = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_after_first, m_link_a.send_count);
}

TEST_F(message_protocol_suite, stale_sync_mismatch_packet_is_dropped)
{
   // Scenario: mailbox repeatedly returns SYNC_MISMATCH to a master.
   // Expectation: MP starts sync once, then drops duplicates.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           stale_link_get_packet,
                                           NULL,
                                           NULL,
                                           generate_session_id_master)));

   prime_process(&m_mp_a.interface);

   // Allow sync start (respect MAX_TIME_BEFORE_SYNC_RETRY_MS guard).
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   // Step 1: Craft SYNC_MISMATCH bytes from a slave.
   mp_packet_t sync_mismatch = {0};
   sync_mismatch.header.pkt_counter = 1u;
   sync_mismatch.header.session_id = 0x1234u;
   sync_mismatch.header.pkt_type = MP_PACKET_TYPE_SYNC_MISMATCH;
   sync_mismatch.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   sync_mismatch.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&sync_mismatch, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // Step 2: First process() should send SYNC_START.
   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_GT(m_link_a.send_count, send_before);

   uint32_t send_after_first = m_link_a.send_count;
   // Step 3: Second process() sees the same stale SYNC_MISMATCH and must drop it.
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_EQ(send_after_first, m_link_a.send_count);
}

TEST_F(message_protocol_suite, stale_sync_ack_packet_is_dropped_without_resync)
{
   // Scenario: mailbox repeatedly returns a valid SYNC_ACK to a master.
   // Expectation: MP clears syncing once, then drops duplicate SYNC_ACK packets without resyncing.
   ASSERT_TRUE(IS_OK(message_protocol_init(&m_mp_a,
                                           &m_time.interface,
                                           &m_link_a.interface,
                                           true,
                                           stale_link_get_packet,
                                           NULL,
                                           NULL,
                                           generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   m_mp_a._current_session_id = 0xBEEFu;
   m_mp_a._is_syncing = true;
   const uint32_t session_before = m_mp_a._current_session_id;

   // Allow sync start (respect MAX_TIME_BEFORE_SYNC_RETRY_MS guard).
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   // Step 1: Craft a valid SYNC_ACK for the current session.
   mp_packet_t sync_ack = {0};
   sync_ack.header.pkt_counter = 1u;
   sync_ack.header.session_id = m_mp_a._current_session_id;
   sync_ack.header.pkt_type = MP_PACKET_TYPE_SYNC_ACK;
   sync_ack.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   sync_ack.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&sync_ack, wire_buf, &wire_len);
   stale_mailbox_load_bytes(wire_buf, wire_len);

   // Step 2: First process() should clear syncing and send nothing.
   uint32_t send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_FALSE(m_mp_a._is_syncing);
   ASSERT_EQ(send_before, m_link_a.send_count);

   uint32_t send_after_first = m_link_a.send_count;
   // Step 3: Second process() sees stale SYNC_ACK again, must drop duplicate, and must not resync.
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_FALSE(m_mp_a._is_syncing);
   ASSERT_EQ(session_before, m_mp_a._current_session_id);
   ASSERT_EQ(send_after_first, m_link_a.send_count);
}

// =====================================================================================================================
// Synchronization / session management
// =====================================================================================================================

TEST_F(message_protocol_suite, master_slave_sync_on_session_mismatch)
{
   // Edge case: session mismatch should trigger SYNC_MISMATCH -> SYNC_START -> SYNC_ACK.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   // Set time to some random time in the future to simulate the device running for a while before this interaction
   // occurs. I.e. time != 0
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x2222u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x99u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   pump_process(&m_mp_a.interface, &m_mp_b.interface, 8u);

   ASSERT_FALSE(m_mp_a._is_syncing);
   ASSERT_EQ(m_mp_a._current_session_id, m_mp_b._current_session_id);
}

TEST_F(message_protocol_suite, sync_start_while_tx_waiting_marks_tx_abandoned_not_completed)
{
   // Scenario: master has an in-flight DATA packet and receives a mismatched ACK that starts sync.
   // Expectation: reset_mp_state marks TX as ABANDONED so app layer does not treat it as COMPLETED.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x1111u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x9Cu;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface))); // Send DATA, now waiting for ACK.

   MSG_PROT_TX_PACKET_STATUS tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_WAITING_FOR_ACK, tx_status);

   // Clear peer RX slot so master can transmit SYNC_START in this single-slot mock transport.
   m_link_b.has_data = false;
   m_link_b.rx_len = 0u;
   memset(m_link_b.rx_buf, 0, sizeof(m_link_b.rx_buf));

   // Allow sync start (respect MAX_TIME_BEFORE_SYNC_RETRY_MS guard).
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   mp_packet_t mismatch_ack = {0};
   mismatch_ack.header.pkt_counter = m_mp_a._pending_id;
   mismatch_ack.header.session_id = 0x2222u; // Mismatch triggers master sync start.
   mismatch_ack.header.pkt_type = MP_PACKET_TYPE_ACK;
   mismatch_ack.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   mismatch_ack.payload.type = 0u;
   mismatch_ack.payload.ppi = 0u;
   mismatch_ack.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet_full(&mismatch_ack, wire_buf, &wire_len);
   link_inject_packet(&m_link_a, wire_buf, wire_len);

   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface))); // Trigger sync start + reset_mp_state().
   ASSERT_TRUE(m_mp_a._is_syncing);
   ASSERT_EQ((uint8_t)MSG_PROT_TX_PACKET_STATUS_ABANDONED, m_mp_a._tx_packet.header.status);
   ASSERT_NE((uint8_t)MSG_PROT_TX_PACKET_STATUS_COMPLETED, m_mp_a._tx_packet.header.status);

   // Complete sync handshake to check externally visible TX status after syncing clears.
   ASSERT_TRUE(IS_OK(m_mp_b.interface.process(&m_mp_b.interface))); // Handle SYNC_START -> send SYNC_ACK
   ASSERT_TRUE(IS_OK(m_mp_a.interface.process(&m_mp_a.interface))); // Handle SYNC_ACK
   ASSERT_FALSE(m_mp_a._is_syncing);

   tx_status = MSG_PROT_TX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_a.interface.get_tx_packet_status(&m_mp_a.interface, &tx_status)));
   ASSERT_EQ(MSG_PROT_TX_PACKET_STATUS_ABANDONED, tx_status);
}

TEST_F(message_protocol_suite, sync_steps_slave_detects_mismatch_first)
{
   // Edge case: slave detects session mismatch first and responds with SYNC_MISMATCH before master initiates sync.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x2222u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x5Bu;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   // Master sends DATA; slave should respond with SYNC_MISMATCH.
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   uint32_t slave_send_before = m_link_b.send_count;
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   ASSERT_GT(m_link_b.send_count, slave_send_before);

   // Advance time so the master is allowed to start a new sync.
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   // Master handles SYNC_MISMATCH and initiates SYNC_START.
   uint32_t master_send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_TRUE(m_mp_a._is_syncing);
   ASSERT_GT(m_link_a.send_count, master_send_before);

   // Slave handles SYNC_START and sends SYNC_ACK, adopting master's session ID.
   slave_send_before = m_link_b.send_count;
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   ASSERT_GT(m_link_b.send_count, slave_send_before);
   ASSERT_EQ(m_mp_a._current_session_id, m_mp_b._current_session_id);

   // Master handles SYNC_ACK and clears syncing flag.
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_FALSE(m_mp_a._is_syncing);
}

TEST_F(message_protocol_suite, sync_steps_master_detects_mismatch_first)
{
   // Edge case: master detects session mismatch first and initiates SYNC_START immediately.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x4444u;
   m_mp_b._current_session_id = 0x5555u;

   // Inject an ACK with a mismatched session ID into the master.
   mp_packet_t ack_packet = {0};
   ack_packet.header.pkt_counter = 1u;
   ack_packet.header.session_id = m_mp_b._current_session_id;
   ack_packet.header.pkt_type = MP_PACKET_TYPE_ACK;
   ack_packet.header.status = MSG_PROT_TX_PACKET_STATUS_NEW;
   ack_packet.payload.ppi = 0u;
   ack_packet.payload.pkt_payload_len = 0u;

   uint8_t wire_buf[MP_MAX_PACKET_LENGTH] = {0};
   uint16_t wire_len = 0u;
   build_wire_packet(&ack_packet, wire_buf, &wire_len);
   link_inject_packet(&m_link_a, wire_buf, wire_len);

   // Advance time so the master is allowed to start a new sync.
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   // Master processes mismatched ACK and initiates sync.
   uint32_t master_send_before = m_link_a.send_count;
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_TRUE(m_mp_a._is_syncing);
   ASSERT_GT(m_link_a.send_count, master_send_before);

   // Slave handles SYNC_START and sends SYNC_ACK.
   uint32_t slave_send_before = m_link_b.send_count;
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   ASSERT_GT(m_link_b.send_count, slave_send_before);

   // Master handles SYNC_ACK and clears syncing flag.
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   ASSERT_FALSE(m_mp_a._is_syncing);
   ASSERT_EQ(m_mp_a._current_session_id, m_mp_b._current_session_id);
}

TEST_F(message_protocol_suite, two_masters_conflict_on_sync_start)
{
   // Edge case: two masters should reject SYNC_START from another master.
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master)));
   ASSERT_TRUE(IS_OK(message_protocol_init(
      &m_mp_b, &m_time.interface, &m_link_b.interface, true, NULL, NULL, NULL, generate_session_id_master)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   // Set time to some random time in the future to simulate the device running for a while before this interaction
   // occurs. I.e. time != 0
   ASSERT_TRUE(IS_OK(
      m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(MAX_TIME_BEFORE_SYNC_RETRY_MS + 1u))));

   m_mp_a._current_session_id = 0xAAAAu;
   m_mp_b._current_session_id = 0xBBBBu;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x01u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   (void)m_mp_a.interface.process(&m_mp_a.interface);
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   result_t result = m_mp_a.interface.process(&m_mp_a.interface);

   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_INVALID_PACKET_STATUS, GET_ERR_CODE(result));
}

TEST_F(message_protocol_suite, two_slaves_conflict_on_sync_mismatch)
{
   // Edge case: a slave receiving SYNC_MISMATCH should error.
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_a, &m_time.interface, &m_link_a.interface, false, NULL, NULL, NULL, NULL)));
   ASSERT_TRUE(
      IS_OK(message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL)));

   prime_process(&m_mp_a.interface);
   prime_process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0x1111u;
   m_mp_b._current_session_id = 0x2222u;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = (uint8_t)TEST_PPI_STATUS_UPDATE;
   tx_payload.pkt_payload_len = 1u;
   tx_payload.payload[0] = 0x02u;

   ASSERT_TRUE(IS_OK(m_mp_a.interface.send(&m_mp_a.interface, &tx_payload)));

   (void)m_mp_a.interface.process(&m_mp_a.interface);
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   result_t result = m_mp_a.interface.process(&m_mp_a.interface);

   ASSERT_EQ(SW_UNIT_ID_MESSAGE_PROTOCOL, GET_ERR_UNIT(result));
   ASSERT_EQ(MSG_PROT_ERROR_INVALID_PACKET_STATUS, GET_ERR_CODE(result));
}

/**
 * This test is less of a test and more of a step-by-step trace of a complex interaction scenario.
 * It is intended to document and verify the behavior of the message protocol when handling overlapping
 * request and async push operations that contend for the link. This assumes a full duplex link where
 * both sides can send and receive independently, but the message protocol logic must still manage
 * contention for the link when both sides have data to send at the same time.
 */
TEST_F(message_protocol_suite, request_and_async_push_overlap_delays_push_until_retry)
{
   // Scenario: A (master) sends a request to B (slave) while B also has an async push ready for A.
   // Expectation: request is received, ACKed, async push is retried after link busy clears, then B responds.
   result_t init_result_a = message_protocol_init(
      &m_mp_a, &m_time.interface, &m_link_a.interface, true, NULL, NULL, NULL, generate_session_id_master);
   result_t init_result_b
      = message_protocol_init(&m_mp_b, &m_time.interface, &m_link_b.interface, false, NULL, NULL, NULL, NULL);
   printf("Init results: A unit=%u code=%u, B unit=%u code=%u\n",
          (unsigned)GET_ERR_UNIT(init_result_a),
          (unsigned)GET_ERR_CODE(init_result_a),
          (unsigned)GET_ERR_UNIT(init_result_b),
          (unsigned)GET_ERR_CODE(init_result_b));

   (void)m_mp_a.interface.process(&m_mp_a.interface);
   (void)m_mp_b.interface.process(&m_mp_b.interface);

   m_mp_a._current_session_id = 0xABCDu;
   m_mp_b._current_session_id = 0xABCDu;

   mp_packet_payload_t request_payload = {0};
   request_payload.ppi = (uint8_t)TEST_PPI_TIME_UPDATE;
   request_payload.pkt_payload_len = 2u;
   request_payload.payload[0] = 0x10u;
   request_payload.payload[1] = 0x11u;

   mp_packet_payload_t async_payload = {0};
   async_payload.ppi = (uint8_t)TEST_PPI_DEBUG_LOG;
   async_payload.pkt_payload_len = 3u;
   async_payload.payload[0] = 0xA0u;
   async_payload.payload[1] = 0xA1u;
   async_payload.payload[2] = 0xA2u;

   // Use helpers instead of lambdas for clearer step-by-step tracing.

   // Send a data request from one side and async push unrelated data from the other side at the same time.
   result_t send_request_result = m_mp_a.interface.send(&m_mp_a.interface, &request_payload);
   result_t send_async_result = m_mp_b.interface.send(&m_mp_b.interface, &async_payload);
   printf("Initial send results: A request unit=%u code=%u, B async unit=%u code=%u\n",
          (unsigned)GET_ERR_UNIT(send_request_result),
          (unsigned)GET_ERR_CODE(send_request_result),
          (unsigned)GET_ERR_UNIT(send_async_result),
          (unsigned)GET_ERR_CODE(send_async_result));
   print_mp_payload("A request payload:", &request_payload);
   print_mp_payload("B async payload:", &async_payload);
   printf("Sending: A2B request + B2A async push at the same time...\n");
   print_mp_statuses(&m_mp_a, &m_mp_b);

   mp_packet_payload_t response_payload = {0};
   bool response_pending = false;

   printf("Step 1: A.process() -> send request\n");
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   printf("Step 2: B.process() -> receive request, ACK it, attempt async push\n");
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   MSG_PROT_RX_PACKET_STATUS b_rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   (void)m_mp_b.interface.get_rx_packet_status(&m_mp_b.interface, &b_rx_status);
   if(MSG_PROT_RX_PACKET_STATUS_NEW == b_rx_status)
   {
      mp_packet_payload_t b_rx = {0};
      (void)m_mp_b.interface.get_rx_packet(&m_mp_b.interface, &b_rx);
      print_mp_payload("B handled request:", &b_rx);

      response_payload.ppi = b_rx.ppi;
      response_payload.pkt_payload_len = b_rx.pkt_payload_len;
      for(uint16_t i = 0u; i < b_rx.pkt_payload_len; i++)
      {
         response_payload.payload[i] = (uint8_t)(b_rx.payload[i] + 1u);
      }
      print_mp_payload("B prepared response:", &response_payload);
      response_pending = true;
   }

   printf("Step 3: A.process() -> receive ACK for request\n");
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   printf("Advance time > ACK_TIMEOUT_MS to trigger retry of async push\n");
   (void)m_time.interface.inc_time_by_set_val_ms(&m_time.interface, (uint16_t)(ACK_TIMEOUT_MS + 1u));

   printf("Step 4: B.process() -> retry async push\n");
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   printf("Step 5: A.process() -> receive async push, send ACK\n");
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   MSG_PROT_RX_PACKET_STATUS a_rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   (void)m_mp_a.interface.get_rx_packet_status(&m_mp_a.interface, &a_rx_status);
   if(MSG_PROT_RX_PACKET_STATUS_NEW == a_rx_status)
   {
      mp_packet_payload_t a_rx = {0};
      (void)m_mp_a.interface.get_rx_packet(&m_mp_a.interface, &a_rx);
      print_mp_payload("A handled async push:", &a_rx);
   }

   printf("Step 6: B.process() -> receive ACK for async push\n");
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   if(response_pending)
   {
      printf("B sending response to request\n");
      result_t send_result = m_mp_b.interface.send(&m_mp_b.interface, &response_payload);
      printf("  send response result: unit=%u code=%u\n",
             (unsigned)GET_ERR_UNIT(send_result),
             (unsigned)GET_ERR_CODE(send_result));
      response_pending = false;
   }

   printf("Step 7: B.process() -> send response\n");
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   printf("Step 8: A.process() -> receive response, send ACK\n");
   (void)m_mp_a.interface.process(&m_mp_a.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);

   (void)m_mp_a.interface.get_rx_packet_status(&m_mp_a.interface, &a_rx_status);
   if(MSG_PROT_RX_PACKET_STATUS_NEW == a_rx_status)
   {
      mp_packet_payload_t a_rx = {0};
      (void)m_mp_a.interface.get_rx_packet(&m_mp_a.interface, &a_rx);
      print_mp_payload("A handled response:", &a_rx);
   }

   printf("Step 9: B.process() -> receive ACK for response\n");
   (void)m_mp_b.interface.process(&m_mp_b.interface);
   print_mp_statuses(&m_mp_a, &m_mp_b);
}
