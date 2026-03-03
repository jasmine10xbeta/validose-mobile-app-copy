#include <cstdio>
#include <gtest/gtest.h>

extern "C"
{
#include "common.h"

#include "message_protocol.h"
#include "message_protocol_interface.h"

#include "dock_data_manager.h"
#include "dock_data_manager_interface.h"

#include "system_time.h"
#include "system_time_interface.h"

#include "queue.h"
#include "queue_interface.h"

#include "app_manager.h"
#include "app_manager_interface.h"

#include "debug.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
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

#define DUMMY_DOSE_SCHEDULE_DEFAULT                                                                                    \
   {.medication_type = MEDICATION_TYPE_UNDEFINED,                                                                      \
    .dosage_mg = 2u,                                                                                                   \
    .temp_upper_limit_deg_c = 30,                                                                                      \
    .temp_lower_limit_deg_c = 0,                                                                                       \
    .temp_avg_window_duration_sec = 3600u,                                                                             \
    .dose_days_bitfield = 0b10000000,                                                                                  \
    .dose_window_duration_minutes = 60u,                                                                               \
    .dose_window_count = 4u,                                                                                           \
    .dose_window_start_times_minutes = {480u, 720u, 1080u, 1320u}}
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

static queue_t m_queue_app_manager_rx = {0};
static queue_interface_t *m_app_manager_rx_queue_ifc;

static queue_t m_queue_app_manager_tx = {0};
static queue_interface_t *m_app_manager_tx_queue_ifc;

static uint8_t
   m_buffer_app_manager_rx[20 * (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)]
   = {0};
static uint8_t
   m_buffer_app_manager_tx[20 * (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)]
   = {0};

static system_time_t m_systick = {0};
static system_time_interface_t *m_systick_ifc;

static dock_data_manager_t m_dock_data_manager = {0};
static dock_data_manager_interface_t *m_dock_data_manager_ifc;

static test_link_endpoint_t m_link_dock = {0};
static test_link_endpoint_t m_link_app = {0};

static message_protocol_t m_mp_dock = {0};
static message_protocol_interface_t *m_mp_dock_ifc;

static message_protocol_t m_mp_app = {0};
static message_protocol_interface_t *m_mp_app_ifc;
static app_manager_t m_app_manager = {0};
static app_manager_interface_t *m_app_manager_ifc;

/***********************************************************************************************************************
 * Test utilities
 **********************************************************************************************************************/
static result_t make_result(uint8_t unit_id, uint8_t err_code)
{
   return (result_t)(((result_t)unit_id << 8u) | (result_t)(err_code & 0xFFu));
}

/***********************************************************************************************************************
 * Link layer Endpoint Mock
 **********************************************************************************************************************/
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
/***********************************************************************************************************************
 * Helper Functions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Test Fixture
 **********************************************************************************************************************/
class AppManager: public testing::Test
{
protected:
   void SetUp() override
   {
      result_t result = RESULT_OK;

      // Initialize systick
      result = system_time_init(&m_systick);
      ASSERT_EQ(RESULT_OK, result);
      m_systick_ifc = &m_systick.interface;

      // Initialize link layer endpoints
      test_link_endpoint_init(&m_link_dock, MP_MAX_PACKET_LENGTH);
      test_link_endpoint_init(&m_link_app, MP_MAX_PACKET_LENGTH);
      m_link_dock.peer = &m_link_app;
      m_link_app.peer = &m_link_dock;

      // Initialize mock message protocol interfaces with the link layer endpoints
      result = message_protocol_init(&m_mp_dock, m_systick_ifc, &m_link_dock.interface, true, NULL, NULL, NULL, NULL);
      ASSERT_EQ(RESULT_OK, result);
      m_mp_dock_ifc = &m_mp_dock.interface;

      result = message_protocol_init(&m_mp_app, m_systick_ifc, &m_link_app.interface, false, NULL, NULL, NULL, NULL);
      ASSERT_EQ(RESULT_OK, result);
      m_mp_app_ifc = &m_mp_app.interface;

      m_mp_dock._current_session_id = 0x1111u;
      m_mp_app._current_session_id = 0x1111u;

      m_mp_dock_ifc->process(m_mp_dock_ifc);
      m_mp_app_ifc->process(m_mp_app_ifc);

      // Initize dock data manager
      result = dock_data_manager_init(&m_dock_data_manager);
      ASSERT_EQ(RESULT_OK, result);
      m_dock_data_manager_ifc = &m_dock_data_manager.interface;

      // Initialize queues
      IF_OK_RUN_AND_UPDATE(result,
                           queue_init(&m_queue_app_manager_rx,
                                      m_buffer_app_manager_rx,
                                      sizeof(m_buffer_app_manager_rx),
                                      (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)));
      m_app_manager_rx_queue_ifc = &m_queue_app_manager_rx.interface;

      IF_OK_RUN_AND_UPDATE(result,
                           queue_init(&m_queue_app_manager_tx,
                                      m_buffer_app_manager_tx,
                                      sizeof(m_buffer_app_manager_tx),
                                      (MESSAGE_PROTOCOL_MIN_PAYLOAD_STRUCT_SIZE + MESSAGE_PROTOCOL_MAX_PAYLOAD_LEN)));
      m_app_manager_tx_queue_ifc = &m_queue_app_manager_tx.interface;

      result = app_manager_init(&m_app_manager,
                                m_mp_dock_ifc,
                                m_app_manager_rx_queue_ifc,
                                m_app_manager_tx_queue_ifc,
                                &(m_link_dock.interface),
                                m_systick_ifc,
                                m_dock_data_manager_ifc);
      ASSERT_EQ(RESULT_OK, result);
      m_app_manager_ifc = &m_app_manager.interface;
   }

   void process_till_new_msg_on_app(uint8_t steps)
   {
      ASSERT_NE(nullptr, m_mp_dock_ifc);
      ASSERT_NE(nullptr, m_mp_app_ifc);
      ASSERT_NE(nullptr, m_app_manager_ifc);
      for(uint8_t i = 0u; i < steps; i++)
      {
         (void)m_mp_dock_ifc->process(m_mp_dock_ifc);
         (void)m_app_manager_ifc->process(&m_app_manager.interface);
         (void)m_mp_app_ifc->process(m_mp_app_ifc);

         MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
         ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
         if(MSG_PROT_RX_PACKET_STATUS_NEW == rx_status)
         {
            break;
         }
      }
      (void)m_mp_dock_ifc->process(m_mp_dock_ifc);
      (void)m_app_manager_ifc->process(&m_app_manager.interface);
      (void)m_mp_app_ifc->process(m_mp_app_ifc);
   }

   void process_till_new_msg_on_dock(uint8_t steps)
   {
      ASSERT_NE(nullptr, m_mp_dock_ifc);
      ASSERT_NE(nullptr, m_mp_app_ifc);
      ASSERT_NE(nullptr, m_app_manager_ifc);
      for(uint8_t i = 0u; i < steps; i++)
      {
         (void)m_mp_dock_ifc->process(m_mp_dock_ifc);
         (void)m_app_manager_ifc->process(&m_app_manager.interface);
         (void)m_mp_app_ifc->process(m_mp_app_ifc);

         MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
         ASSERT_TRUE(IS_OK(m_mp_dock_ifc->get_rx_packet_status(m_mp_dock_ifc, &rx_status)));
         if(MSG_PROT_RX_PACKET_STATUS_NEW == rx_status)
         {
            break;
         }
      }
      (void)m_mp_dock_ifc->process(m_mp_dock_ifc);
      (void)m_app_manager_ifc->process(&m_app_manager.interface);
      (void)m_mp_app_ifc->process(m_mp_app_ifc);
   }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
TEST_F(AppManager, init_null_checks)
{
   result_t result = app_manager_init(NULL,
                                      m_mp_dock_ifc,
                                      m_app_manager_rx_queue_ifc,
                                      m_app_manager_tx_queue_ifc,
                                      &(m_link_dock.interface),
                                      m_systick_ifc,
                                      m_dock_data_manager_ifc);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));

   result = app_manager_init(&m_app_manager,
                             NULL,
                             m_app_manager_rx_queue_ifc,
                             m_app_manager_tx_queue_ifc,
                             &(m_link_dock.interface),
                             m_systick_ifc,
                             m_dock_data_manager_ifc);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));

   result = app_manager_init(&m_app_manager,
                             m_mp_dock_ifc,
                             NULL,
                             m_app_manager_tx_queue_ifc,
                             &(m_link_dock.interface),
                             m_systick_ifc,
                             m_dock_data_manager_ifc);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));

   result = app_manager_init(&m_app_manager,
                             m_mp_dock_ifc,
                             m_app_manager_rx_queue_ifc,
                             NULL,
                             &(m_link_dock.interface),
                             m_systick_ifc,
                             m_dock_data_manager_ifc);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));

   result = app_manager_init(&m_app_manager,
                             m_mp_dock_ifc,
                             m_app_manager_rx_queue_ifc,
                             m_app_manager_tx_queue_ifc,
                             NULL,
                             m_systick_ifc,
                             m_dock_data_manager_ifc);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));

   result = app_manager_init(&m_app_manager,
                             m_mp_dock_ifc,
                             m_app_manager_rx_queue_ifc,
                             m_app_manager_tx_queue_ifc,
                             &(m_link_dock.interface),
                             NULL,
                             m_dock_data_manager_ifc);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));

   result = app_manager_init(&m_app_manager,
                             m_mp_dock_ifc,
                             m_app_manager_rx_queue_ifc,
                             m_app_manager_tx_queue_ifc,
                             &(m_link_dock.interface),
                             m_systick_ifc,
                             NULL);

   ASSERT_EQ(SW_UNIT_ID_APP_MANAGER, GET_ERR_UNIT(result));
   ASSERT_EQ(APP_MANAGER_ERROR_NULL, GET_ERR_CODE(result));
}

TEST_F(AppManager, init_success)
{
   result_t result = app_manager_init(&m_app_manager,
                                      m_mp_dock_ifc,
                                      m_app_manager_rx_queue_ifc,
                                      m_app_manager_tx_queue_ifc,
                                      &(m_link_dock.interface),
                                      m_systick_ifc,
                                      m_dock_data_manager_ifc);

   ASSERT_EQ(RESULT_OK, result);
}

// This tests the tx and rx queue used by GC and the app, in the shape of dose_schedule (data that is not tracked
// in Dock Data Manager)
TEST_F(AppManager, tx_rx_queue_test)
{
   // First we test SENDING a message from Dock to App via the app manager's tx queue.
   dose_schedule_t dummy_dose_schedule = DUMMY_DOSE_SCHEDULE_DEFAULT;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = PPI_AD_DOSE_SCHEDULE;
   tx_payload.type = PPI_TYPE_PUSH;
   tx_payload.pkt_payload_len = sizeof(dose_schedule_t);
   memcpy(tx_payload.payload, &dummy_dose_schedule, sizeof(dose_schedule_t));

   m_app_manager_tx_queue_ifc->enqueue(m_app_manager_tx_queue_ifc, &tx_payload);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_SCHEDULE, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_schedule_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_schedule, rx_payload.payload, sizeof(dose_schedule_t)));
}

TEST_F(AppManager, rx_queue_test)
{
   // First we test SENDING a message from Dock to App via the app manager's tx queue.
   dose_schedule_t dummy_dose_schedule = DUMMY_DOSE_SCHEDULE_DEFAULT;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = PPI_AD_DOSE_SCHEDULE;
   tx_payload.type = PPI_TYPE_PUSH;
   tx_payload.pkt_payload_len = sizeof(dose_schedule_t);
   memcpy(tx_payload.payload, &dummy_dose_schedule, sizeof(dose_schedule_t));

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   // Now we test RECEIVING a message from App to Dock via the app manager's rx queue.
   m_mp_app_ifc->send(m_mp_app_ifc, &tx_payload);

   process_till_new_msg_on_dock(8);

   size_t count = 0;
   ASSERT_TRUE(IS_OK(m_app_manager_rx_queue_ifc->get_count(m_app_manager_rx_queue_ifc, &count)));
   ASSERT_EQ(1, count);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_app_manager_rx_queue_ifc->dequeue(m_app_manager_rx_queue_ifc, &rx_payload)));
   ASSERT_EQ(tx_payload.ppi, rx_payload.ppi);
   ASSERT_EQ(tx_payload.pkt_payload_len, rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(tx_payload.payload, rx_payload.payload, sizeof(dose_schedule_t)));
}

TEST_F(AppManager, push_dock_status)
{
   dock_status_t dummy_dock_status = {
      .hardware_version = {4, 5, 6},
      .firmware_version = {1, 2, 3},
      .mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99},
      .timestamp_unix_s = 1625097600,
      .ship_mode_exit_timestamp_unix = 1625094000,
      .uptime_s = 3600,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOCK_STATUS, &dummy_dock_status, sizeof(dock_status_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_STATUS, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_STATUS, rx_payload.ppi);
   ASSERT_EQ(sizeof(dock_status_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dock_status, rx_payload.payload, sizeof(dock_status_t)));

   // Ensure the dock status was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_STATUS, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_ring_status)
{
   ring_status_t dummy_ring_status = {
      .hardware_version = {4, 5, 6},
      .firmware_version = {1, 2, 3},
      .mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99},
      .timestamp_unix_s = 1625097600,
      .ship_mode_exit_timestamp_unix = 1625094000,
      .battery_charge_status = 80,
      .uptime_s = 3600,
      .temperature_celsius = 25,
      .dose_fifo_used_percent = 50,
      .battery_fifo_used_percent = 20,
      .imu_fifo_used_percent = 10,
      .error_fifo_used_percent = 0,
      .dose_fifo_used_percent_watermark = 75,
      .battery_fifo_used_percent_watermark = 80,
      .imu_fifo_used_percent_watermark = 90,
      .error_fifo_used_percent_watermark = 100,
      .battery_sample_frequency_millihz = 1000,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_RING_STATUS, &dummy_ring_status, sizeof(ring_status_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_STATUS, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_RING_STATUS, rx_payload.ppi);
   ASSERT_EQ(sizeof(ring_status_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_ring_status, rx_payload.payload, sizeof(ring_status_t)));

   // Ensure the ring status was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_STATUS, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_dose_event)
{
   dose_event_t dummy_dose_event = {
      .event_id = 12345,
      .start_timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .duration_s = 120,
      .dose_completed_in_time = true,
      .tilt_count = 5,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &dummy_dose_event, sizeof(dose_event_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_EVENT_REPORT, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_event_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_event, rx_payload.payload, sizeof(dose_event_t)));

   // Ensure the dose event was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_temperature_log)
{
   temperature_log_t dummy_temperature_log = {
      .timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .temperature_deg_c = 25,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_TEMPERATURE_LOG, &dummy_temperature_log, sizeof(temperature_log_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_TEMPERATURE_LOG, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_TEMP_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(temperature_log_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_temperature_log, rx_payload.payload, sizeof(temperature_log_t)));

   // Ensure the temperature log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_TEMPERATURE_LOG, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_dock_weight_log)
{
   dock_weight_measurement_t dock_weight_measurement_dummy = {
      .weight_mg = 5000,
      .total_dispensed_mg = 1250,
      .std_dev = 100,
      .temperature_deg_c_div10 = 250,
      .timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
   };

   m_dock_data_manager_ifc->enqueue(m_dock_data_manager_ifc,
                                    DATA_ID_WEIGHT_MEASUREMENT,
                                    &dock_weight_measurement_dummy,
                                    sizeof(dock_weight_measurement_t),
                                    1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_WEIGHT_MEASUREMENT, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_WEIGHT_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(dock_weight_measurement_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dock_weight_measurement_dummy, rx_payload.payload, sizeof(dock_weight_measurement_t)));

   // Ensure the weight measurement log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_WEIGHT_MEASUREMENT, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_dock_charge_status)
{
   dock_charge_status_t dummy_charge_status = {
      .timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .charge_status = true,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOCK_CHARGE_STATUS, &dummy_charge_status, sizeof(dock_charge_status_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_CHARGE_STATUS, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_CHARGE_STATUS, rx_payload.ppi);
   ASSERT_EQ(sizeof(dock_charge_status_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_charge_status, rx_payload.payload, sizeof(dock_charge_status_t)));

   // Ensure the dock charge status log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_CHARGE_STATUS, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_ring_docked_status)
{
   ring_docked_status_t dummy_ring_docked_status = {
      .timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .docked_status = true,
      .ring_nfc_id = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33},
      .medication_nfc_id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99},
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_RING_DOCKED_STATUS, &dummy_ring_docked_status, sizeof(ring_docked_status_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_DOCKED_STATUS, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_RING_DOCKED_STATUS, rx_payload.ppi);
   ASSERT_EQ(sizeof(ring_docked_status_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_ring_docked_status, rx_payload.payload, sizeof(ring_docked_status_t)));

   // Ensure the ring docked status log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_DOCKED_STATUS, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_dock_batt_level)
{
   battery_level_t dummy_battery_level = {
      .timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .battery_level = 85,

   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOCK_BATTERY_LEVEL, &dummy_battery_level, sizeof(battery_level_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_BATTERY_LEVEL, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_BATT_LEVEL_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(battery_level_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_battery_level, rx_payload.payload, sizeof(battery_level_t)));

   // Ensure the dock battery level log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_BATTERY_LEVEL, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_ring_batt_level)
{
   battery_level_t dummy_battery_level = {
      .timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .battery_level = 85,

   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_RING_BATTERY_LEVEL, &dummy_battery_level, sizeof(battery_level_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_BATTERY_LEVEL, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_RING_BATT_LEVEL_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(battery_level_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_battery_level, rx_payload.payload, sizeof(battery_level_t)));

   // Ensure the ring battery level log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_BATTERY_LEVEL, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_dock_debug_log)
{
   raw_debug_log_t dummy_debug_log = {
      .level = 0,
      .timestamp = 1625097600,
      .module_id = 1,
      .line = 42,
      .arg_values = {100, 200, 300},
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &dummy_debug_log, sizeof(raw_debug_log_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_DEBUG_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(raw_debug_log_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_debug_log, rx_payload.payload, sizeof(raw_debug_log_t)));

   // Ensure the dock debug log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, push_ring_debug_log)
{
   raw_debug_log_t dummy_debug_log = {
      .level = 0,
      .timestamp = 1625097600,
      .module_id = 1,
      .line = 42,
      .arg_values = {100, 200, 300},
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_RING_DEBUG_LOG, &dummy_debug_log, sizeof(raw_debug_log_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_DEBUG_LOG, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_RING_DEBUG_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(raw_debug_log_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_debug_log, rx_payload.payload, sizeof(raw_debug_log_t)));

   // Ensure the ring debug log was popped from the queue after sending
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_RING_DEBUG_LOG, &count);
   ASSERT_EQ(0, count);
}

// Test outgoing data priority selection.
// Tx/RX queue gets serviced first, then dock data manager queues in order of data type priority.
TEST_F(AppManager, outgoing_data_priority)
{
   // First we test SENDING a message from Dock to App via the app manager's tx queue.
   dose_schedule_t dummy_dose_schedule = DUMMY_DOSE_SCHEDULE_DEFAULT;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = PPI_AD_DOSE_SCHEDULE;
   tx_payload.type = PPI_TYPE_PUSH;
   tx_payload.pkt_payload_len = sizeof(dose_schedule_t);
   memcpy(tx_payload.payload, &dummy_dose_schedule, sizeof(dose_schedule_t));

   // Queue TX Transaction
   m_app_manager_tx_queue_ifc->enqueue(m_app_manager_tx_queue_ifc, &tx_payload);

   // Queue Dock Debug Log
   raw_debug_log_t dummy_dock_debug_log = {
      .level = 0,
      .timestamp = 1625097600,
      .module_id = 1,
      .line = 42,
      .arg_values = {100, 200, 300},
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &dummy_dock_debug_log, sizeof(raw_debug_log_t), 1);

   // Queue Dose Event
   dose_event_t dummy_dose_event = {
      .event_id = 12345,
      .start_timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .duration_s = 120,
      .dose_completed_in_time = true,
      .tilt_count = 5,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &dummy_dose_event, sizeof(dose_event_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(1, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(1, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   // Now we check that we received the TX queue transaction first.
   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_SCHEDULE, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_schedule_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_schedule, rx_payload.payload, sizeof(dose_schedule_t)));

   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(1, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_EVENT_REPORT, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_event_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_event, rx_payload.payload, sizeof(dose_event_t)));

   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOCK_DEBUG_LOG, rx_payload.ppi);
   ASSERT_EQ(sizeof(raw_debug_log_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dock_debug_log, rx_payload.payload, sizeof(raw_debug_log_t)));

   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOCK_DEBUG_LOG, &count);
   ASSERT_EQ(0, count);
}

TEST_F(AppManager, outgoing_data_priority_duplicate)
{
   // First we test SENDING a message from Dock to App via the app manager's tx queue.
   dose_schedule_t dummy_dose_schedule = DUMMY_DOSE_SCHEDULE_DEFAULT;

   mp_packet_payload_t tx_payload = {0};
   tx_payload.ppi = PPI_AD_DOSE_SCHEDULE;
   tx_payload.type = PPI_TYPE_PUSH;
   tx_payload.pkt_payload_len = sizeof(dose_schedule_t);
   memcpy(tx_payload.payload, &dummy_dose_schedule, sizeof(dose_schedule_t));

   // Queue TX Transaction
   m_app_manager_tx_queue_ifc->enqueue(m_app_manager_tx_queue_ifc, &tx_payload);

   // Queue Dose Event 1
   dose_event_t dummy_dose_event = {
      .event_id = 12345,
      .start_timestamp_unix_s = 1625097600, // July 1, 2021 12:00:00 PM GMT
      .duration_s = 120,
      .dose_completed_in_time = true,
      .tilt_count = 5,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &dummy_dose_event, sizeof(dose_event_t), 1);

   // Queue Dose Event 2
   dose_event_t dummy_dose_event_2 = {
      .event_id = 12348,
      .start_timestamp_unix_s = 1625097605, // July 1, 2021 12:00:00 PM GMT
      .duration_s = 60,
      .dose_completed_in_time = false,
      .tilt_count = 3,
   };

   m_dock_data_manager_ifc->enqueue(
      m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &dummy_dose_event_2, sizeof(dose_event_t), 1);

   m_app_manager_ifc->set_comms_link_status(m_app_manager_ifc, true);

   size_t count = 0;
   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(1, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(2, count);

   process_till_new_msg_on_app(4);

   // Now we check that we received the TX queue transaction first.
   MSG_PROT_RX_PACKET_STATUS rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   mp_packet_payload_t rx_payload = {0};
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_SCHEDULE, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_schedule_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_schedule, rx_payload.payload, sizeof(dose_schedule_t)));

   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(2, count);

   process_till_new_msg_on_app(4);

   rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_EVENT_REPORT, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_event_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_event, rx_payload.payload, sizeof(dose_event_t)));

   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(1, count);

   process_till_new_msg_on_app(4);

   rx_status = MSG_PROT_RX_PACKET_STATUS_NONE;
   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet_status(m_mp_app_ifc, &rx_status)));
   ASSERT_EQ(MSG_PROT_RX_PACKET_STATUS_NEW, rx_status);

   ASSERT_TRUE(IS_OK(m_mp_app_ifc->get_rx_packet(m_mp_app_ifc, &rx_payload)));
   ASSERT_EQ(PPI_AD_DOSE_EVENT_REPORT, rx_payload.ppi);
   ASSERT_EQ(sizeof(dose_event_t), rx_payload.pkt_payload_len);
   ASSERT_EQ(0, memcmp(&dummy_dose_event_2, rx_payload.payload, sizeof(dose_event_t)));

   m_app_manager_tx_queue_ifc->get_count(m_app_manager_tx_queue_ifc, &count);
   ASSERT_EQ(0, count);
   m_dock_data_manager_ifc->get_element_count(m_dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &count);
   ASSERT_EQ(0, count);
}
