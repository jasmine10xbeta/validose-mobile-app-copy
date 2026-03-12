#include <cstdio>
#include <gtest/gtest.h>

extern "C"
{
#include "common.h"

#include "ring_data_manager.h"
#include "ring_sources.h"

#include "message_protocol.h"
#include "message_protocol_interface.h"

#include "ring_data_manager_interface.h"
#include "ring_sources_interface.h"

#include "../mock/fds_manager/fds_manager.h"

#include "dock_manager.h"
#include "dock_manager_interface.h"

#include "dock_data_manager.h"
#include "dock_data_manager_interface.h"

#include "ring_manager.h"
#include "ring_manager_interface.h"

#include "system_time.h"
#include "system_time_interface.h"

#include "rtc_system_time.h"
#include "rtc_system_time_interface.h"

#include "queue.h"
#include "queue_interface.h"

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
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static ring_data_manager_t mock_ring_data_manager = {0};
static ring_data_manager_interface_t *ring_data_manager_ifc;

static ring_sources_t mock_source_manager = {0};
static ring_sources_interface_t *source_manager_ifc;

static dock_manager_t dock_manager = {0};
static dock_manager_interface_t *dock_manager_ifc;

static ring_manager_t ring_manager = {0};
static ring_manager_interface_t *ring_manager_ifc;

static system_time_t systick = {0};
static system_time_interface_t *systick_ifc;

static dock_data_manager_t dock_data_manager = {0};
static dock_data_manager_interface_t *dock_data_manager_ifc;

static rtc_system_time_t rtc_mock = {0};
static rtc_system_time_interface_t *rtc_mock_ifc;

static test_link_endpoint_t m_link_dock = {0};
static test_link_endpoint_t m_link_ring = {0};

static message_protocol_t m_mp_dock = {0};
static message_protocol_interface_t *mp_dock_ifc;

static message_protocol_t m_mp_ring = {0};
static message_protocol_interface_t *mp_ring_ifc;

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
static result_t process_all(bool is_docked)
{
   result_t result = mp_ring_ifc->process(mp_ring_ifc);

   if(RESULT_OK != result)
   {
      printf("Error in ring ps process: %u\n", GET_ERR_CODE(result));
      return result;
   }

   IF_OK_RUN_AND_UPDATE(result, mp_dock_ifc->process(mp_dock_ifc));
   if(RESULT_OK != result)
   {
      printf("Error in dock mp process: %u\n", GET_ERR_CODE(result));
      return result;
   }

   IF_OK_RUN_AND_UPDATE(result, dock_manager_ifc->process(dock_manager_ifc, is_docked));
   if(RESULT_OK != result)
   {
      printf("Error in dock manager process: %u\n", GET_ERR_CODE(result));
      return result;
   }

   IF_OK_RUN_AND_UPDATE(result, ring_manager_ifc->process(ring_manager_ifc, is_docked));
   if(RESULT_OK != result)
   {
      printf("Error in ring manager process: %u\n", GET_ERR_CODE(result));
      return result;
   }
   return RESULT_OK;
}

static result_t run_process_loop_for_time(uint32_t duration_ms, uint32_t step_ms, bool is_docked, bool print_steps)
{
   result_t result = RESULT_OK;
   uint32_t elapsed_time = 0u;
   uint32_t rtc_start_value = rtc_mock_ifc->parent->_epoch;

   while(elapsed_time < duration_ms && IS_OK(result))
   {
      if(print_steps)
      {
         printf("Elapsed time: %lums\n", systick_ifc->parent->_current_time_ms);
      }

      systick_ifc->inc_time_by_set_val_ms(systick_ifc, step_ms);

      // Keep the dock RTC in sync with systick
      rtc_mock_ifc->set_time_unix(rtc_mock_ifc, rtc_start_value + (systick_ifc->parent->_current_time_ms / 1000u));

      elapsed_time += step_ms;

      IF_OK_RUN_AND_UPDATE(result, process_all(is_docked));
      if(RESULT_OK != result)
      {
         printf("Error during process loop: %u\n", GET_ERR_CODE(result));
         return result;
      }
   }

   return RESULT_OK;
}

void print_status(const ring_status_t *status)
{
   printf("\n");
   printf("\n");
   printf("Ring Status:\n");
   printf("  hardware_version: %u.%u.%u\n",
          status->hardware_version.major,
          status->hardware_version.minor,
          status->hardware_version.patch);
   printf("  firmware_version: %u.%u.%u\n",
          status->firmware_version.major,
          status->firmware_version.minor,
          status->firmware_version.patch);
   printf("  mac: ");
   for(int i = 0; i < 10; ++i)
   {
      printf("%02X", status->mac[i]);
      if(i < 9)
         printf(":");
   }
   printf("\n");
   printf("  timestamp_unix_s: %u\n", status->timestamp_unix_s);
   printf("  ship_mode_exit_timestamp_unix: %u\n", status->ship_mode_exit_timestamp_unix);
   printf("  battery_charge_status: %u\n", status->battery_charge_status);
   printf("  uptime_s: %u\n", status->uptime_s);
   printf("  temperature_celsius: %d\n", status->temperature_celsius);
   printf("  dose_fifo_used_percent: %u\n", status->dose_fifo_used_percent);
   printf("  battery_fifo_used_percent: %u\n", status->battery_fifo_used_percent);
   printf("  imu_fifo_used_percent: %u\n", status->imu_fifo_used_percent);
   printf("  error_fifo_used_percent: %u\n", status->error_fifo_used_percent);
   printf("  dose_fifo_used_percent_watermark: %u\n", status->dose_fifo_used_percent_watermark);
   printf("  battery_fifo_used_percent_watermark: %u\n", status->battery_fifo_used_percent_watermark);
   printf("  imu_fifo_used_percent_watermark: %u\n", status->imu_fifo_used_percent_watermark);
   printf("  error_fifo_used_percent_watermark: %u\n", status->error_fifo_used_percent_watermark);
   printf("  battery_sample_frequency_millihz: %u\n", status->battery_sample_frequency_millihz);
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/
class RingDockComms: public testing::Test
{
protected:
   void SetUp() override
   {
      result_t result = RESULT_OK;

      // Initialize the dock data manager
      result = dock_data_manager_init(&dock_data_manager);
      ASSERT_EQ(RESULT_OK, result);
      dock_data_manager_ifc = &dock_data_manager.interface;

      // Initialize systick
      result = mock_system_time_init(&systick);
      ASSERT_EQ(RESULT_OK, result);
      systick_ifc = &systick.interface;

      // Initialize mock RTC
      result = rtc_system_time_init(&rtc_mock);
      ASSERT_EQ(RESULT_OK, result);
      rtc_mock_ifc = &rtc_mock.interface;

      // Initialize link layer endpoints
      test_link_endpoint_init(&m_link_dock, MP_MAX_PACKET_LENGTH);
      test_link_endpoint_init(&m_link_ring, MP_MAX_PACKET_LENGTH);
      m_link_dock.peer = &m_link_ring;
      m_link_ring.peer = &m_link_dock;

      // Initialize mock message protocol interfaces with the link layer endpoints
      result = message_protocol_init(&m_mp_dock, systick_ifc, &m_link_dock.interface, true, 1u, NULL, NULL, NULL, NULL);
      ASSERT_EQ(RESULT_OK, result);
      mp_dock_ifc = &m_mp_dock.interface;
      result
         = message_protocol_init(&m_mp_ring, systick_ifc, &m_link_ring.interface, false, 1u, NULL, NULL, NULL, NULL);
      ASSERT_EQ(RESULT_OK, result);
      mp_ring_ifc = &m_mp_ring.interface;

      // Initialize mock ring data manager
      result = ring_data_manager_init(&mock_ring_data_manager);
      ASSERT_EQ(RESULT_OK, result);
      ring_data_manager_ifc = &mock_ring_data_manager.interface;

      // Initialize mock ring sources manager
      result = ring_sources_init(&mock_source_manager);
      ASSERT_EQ(RESULT_OK, result);
      source_manager_ifc = &mock_source_manager.interface;

      // Initialize dock manager
      result = dock_manager_init(&dock_manager, mp_dock_ifc, ring_data_manager_ifc, source_manager_ifc);
      ASSERT_EQ(RESULT_OK, result);
      dock_manager_ifc = &dock_manager.interface;

      // Initialize ring manager
      result = ring_manager_init(&ring_manager, systick_ifc, mp_ring_ifc, dock_data_manager_ifc, rtc_mock_ifc);
      ASSERT_EQ(RESULT_OK, result);
      ring_manager_ifc = &ring_manager.interface;

      // Set the Systick start time to the status poll interval - This will trigger an immediate status request
      systick_ifc->parent->_current_time_ms = 1000u;

      // Set the RTC start time to two seconds // This will trigger an immediate time update once the first ring status
      // is processed
      rtc_mock_ifc->parent->_epoch = 2u;
   }

   void TearDown() override
   {
   }
};

/**
 * @brief Check that the Ring manager automatically updates the time on the ring when a drift is detected between the
 * ring and dock times.
 *
 * This test simulates a scenario where the ring and dock times are initialized with a 2 second offset.
 * The test then simulates a process loop call time of 250ms.
 * This far greater than the expected loop time in the firmware and tests the functioning of the status and time
 * update algorithm for the "slow loop" edge case. The ring manager process function should detect that the ring time is
 * ahead of the dock time by more than the defined threshold and should trigger a time update command to be sent to the
 * ring.
 *
 * The test checks that after processing, the RTC time on the dock is updated to be within 2 seconds of the ring time,
 * indicating that the time update command was processed successfully.
 */
TEST_F(RingDockComms, dock_ring_comms_test_update_time_on_drift_250ms_loop)
{
   result_t result = RESULT_OK;

   // Simulate the passing of 5 seconds
   result = run_process_loop_for_time(5000, 250u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   // Check that the RTC time is within 2 seconds of the ring time
   ASSERT_NEAR(rtc_mock_ifc->parent->_epoch, mock_ring_data_manager._current_time_ms, 2000u);
}

/**
 * @brief Check that the Ring manager automatically updates the time on the ring when a drift is detected between the
 * ring and dock times.
 *
 * This test simulates a scenario where the ring and dock times are initialized with a 2 second offset.
 * The test then simulates the passing of time in 1ms increments, keeping the ring RTC 2 seconds ahead of the dock
 * systick. The ring manager process function should detect that the ring time is ahead of the dock time by more than
 * the defined threshold and should trigger a time update command to be sent to the ring.
 *
 * This test is tests the "fast loop" edge case for the time update algorithm
 *
 * The test checks that after processing, the RTC time on the dock is updated to be within 2 seconds of the ring time,
 * indicating that the time update command was processed successfully.
 */
TEST_F(RingDockComms, dock_ring_comms_test_update_time_on_drift_1ms_loop)
{
   result_t result = RESULT_OK;

   // Simulate the passing of 5 seconds
   result = run_process_loop_for_time(5000u, 1u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   // Check that the RTC time is within 2 seconds of the ring time
   ASSERT_NEAR(rtc_mock_ifc->parent->_epoch, mock_ring_data_manager._current_time_ms, 2000u);
}

/**
 * @brief Test that the dock can successfully send a battery sample rate update command to the ring and that the ring
 * processes the command and updates its battery sample rate accordingly.
 *
 */
TEST_F(RingDockComms, dock_ring_comms_test_update_battery_sample_rate)
{
   result_t result = RESULT_OK;

   // Send a battery sample rate update command from the dock to the ring with a sample rate of 500 millihz (2 second
   // sample period)
   ring_manager_ifc->update_battery_sample_freq(ring_manager_ifc, 500u);

   // Simulate the passing of 5 seconds
   result = run_process_loop_for_time(5000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   // Check that the battery sample frequency in the ring data manager was updated to 500 millihz
   ASSERT_EQ(mock_ring_data_manager._battery_sample_rate_millihz, 500u);
}

/**
 * @brief Test that the dock can successfully can requests a status ipdate from the ring and that the ring responds
 with
 * a status update that is correct and has not been corrupted in transmission.
 *
 * Also check that the status is posted to the Dock data manager since there are sufficient changes in the status to
 * trigger a dock data manager update.
 *
 */
TEST_F(RingDockComms, dock_ring_comms_test_check_status_transmission_fidelity)
{
   result_t result = RESULT_OK;
   ring_status_t test_ring_status = {0};

   // Send a battery sample rate update command from the dock to the ring with a sample rate of 500 millihz (2 second
   // sample period)
   ring_manager_ifc->update_battery_sample_freq(ring_manager_ifc, 500u);

   // Set the status in the ring sources manager which will be read by the ring manager when processing the status
   // update and posted to the dock data manager if there are sufficient changes in the status to trigger an update.
   mock_source_manager._status.hardware_version = {1, 2, 3};
   mock_source_manager._status.firmware_version = {4, 5, 6};
   for(uint8_t idx = 0; idx < 10; idx++)
   {
      mock_source_manager._status.mac[idx] = idx;
   }
   mock_source_manager._status.timestamp_unix_s = 1234567890u;
   mock_source_manager._status.ship_mode_exit_timestamp_unix = 1234560000u;
   mock_source_manager._status.battery_charge_status = 85;
   mock_source_manager._status.uptime_s = 98765u;
   mock_source_manager._status.temperature_celsius = 36;
   mock_source_manager._status.dose_fifo_used_percent = 0; // Set fifos to zero to avoid triggering data requests
   mock_source_manager._status.battery_fifo_used_percent = 0;
   mock_source_manager._status.imu_fifo_used_percent = 0;
   mock_source_manager._status.error_fifo_used_percent = 0;
   mock_source_manager._status.dose_fifo_used_percent_watermark = 15;
   mock_source_manager._status.battery_fifo_used_percent_watermark = 25;
   mock_source_manager._status.imu_fifo_used_percent_watermark = 35;
   mock_source_manager._status.error_fifo_used_percent_watermark = 55;
   mock_source_manager._status.battery_sample_frequency_millihz = 1000u;

   // print_status(&mock_source_manager._status);

   // Simulate the passing of 15 seconds
   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   // print_status(&test_ring_status);

   // Check that the status received in the dock data manager matches the original status sent by the ring
   // (i.e. that the status was not corrupted in transmission and that it was posted to the dock data manager since
   // there were sufficient changes in the status to trigger an update).
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
}

/**
 * @brief Check that received status updates are only stored if they are different enough from the last stored status to
 * warrant an update.
 */
TEST_F(RingDockComms, dock_ring_comms_test_check_status_update_only_on_change)
{
   result_t result = RESULT_OK;
   ring_status_t test_ring_status = {0};

   // Send a battery sample rate update command from the dock to the ring with a sample rate of 500 millihz (2 second
   // sample period)
   ring_manager_ifc->update_battery_sample_freq(ring_manager_ifc, 500u);

   // Set the status in the ring sources manager which will be read by the ring manager when processing the status
   // update and posted to the dock data manager if there are sufficient changes in the status to trigger an update.
   mock_source_manager._status.hardware_version = {1, 2, 3};
   mock_source_manager._status.firmware_version = {4, 5, 6};
   for(uint8_t idx = 0; idx < 10; idx++)
   {
      mock_source_manager._status.mac[idx] = idx;
   }
   mock_source_manager._status.timestamp_unix_s = 1234567890u;
   mock_source_manager._status.ship_mode_exit_timestamp_unix = 1234560000u;
   mock_source_manager._status.battery_charge_status = 85;
   mock_source_manager._status.uptime_s = 98765u;
   mock_source_manager._status.temperature_celsius = 36;
   mock_source_manager._status.dose_fifo_used_percent = 0; // Set fifos to zero to avoid triggering data requests
   mock_source_manager._status.battery_fifo_used_percent = 0;
   mock_source_manager._status.imu_fifo_used_percent = 0;
   mock_source_manager._status.error_fifo_used_percent = 0;
   mock_source_manager._status.dose_fifo_used_percent_watermark = 15;
   mock_source_manager._status.battery_fifo_used_percent_watermark = 25;
   mock_source_manager._status.imu_fifo_used_percent_watermark = 35;
   mock_source_manager._status.error_fifo_used_percent_watermark = 55;
   mock_source_manager._status.battery_sample_frequency_millihz = 1000u;

   // Simulate the passing of 100 seconds
   result = run_process_loop_for_time(100000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   size_t free_elements = 0;
   dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);

   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   ASSERT_EQ(free_elements, 49u); // Check that the only status that was posted to the dock data manager was the
   // initial one since there were no sufficient changes after that to warrant an update
}

/**
 * @brief Check that received status updates are only stored if they are different enough from the last stored status to
 * warrant an update.
 *
 * Specifically check that changes are detected when they should be
 */
TEST_F(RingDockComms, dock_ring_comms_test_check_status_update_check_change_detection)
{
   result_t result = RESULT_OK;
   size_t free_elements = 0;
   ring_status_t test_ring_status = {0};

   // Blank status to initialize the memory with

   mock_source_manager._status.hardware_version = {1, 2, 3};
   mock_source_manager._status.firmware_version = {4, 5, 6};
   for(uint8_t idx = 0; idx < 10; idx++)
   {
      mock_source_manager._status.mac[idx] = idx;
   }
   mock_source_manager._status.timestamp_unix_s = 0u;
   mock_source_manager._status.ship_mode_exit_timestamp_unix = 0u;
   mock_source_manager._status.battery_charge_status = 0u;
   mock_source_manager._status.uptime_s = 0u;
   mock_source_manager._status.temperature_celsius = 36;
   mock_source_manager._status.dose_fifo_used_percent = 0; // Set fifos to zero to avoid triggering data requests
   mock_source_manager._status.battery_fifo_used_percent = 0;
   mock_source_manager._status.imu_fifo_used_percent = 0;
   mock_source_manager._status.error_fifo_used_percent = 0;
   mock_source_manager._status.dose_fifo_used_percent_watermark = 15;
   mock_source_manager._status.battery_fifo_used_percent_watermark = 25;
   mock_source_manager._status.imu_fifo_used_percent_watermark = 35;
   mock_source_manager._status.error_fifo_used_percent_watermark = 55;
   mock_source_manager._status.battery_sample_frequency_millihz = 1000u;

   // Simulate the passing of 15 seconds with for the status to be detected as changed and be stored
   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);

   // Simulate the passing of another 15 seconds with no significant changes to the status - the status should not be
   // stored again since there are no changes
   mock_source_manager._status.dose_fifo_used_percent = 0;
   mock_source_manager._status.battery_fifo_used_percent = 0;
   mock_source_manager._status.imu_fifo_used_percent = 0;
   mock_source_manager._status.error_fifo_used_percent = 0;
   mock_source_manager._status.dose_fifo_used_percent_watermark = 19;
   mock_source_manager._status.battery_fifo_used_percent_watermark = 29;
   mock_source_manager._status.imu_fifo_used_percent_watermark = 39;
   mock_source_manager._status.error_fifo_used_percent_watermark = 59;

   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);

   // Simulate the passing of another 15 seconds with a significant changes to the status - the status should be
   // stored again
   mock_source_manager._status.dose_fifo_used_percent = 0;
   mock_source_manager._status.battery_fifo_used_percent = 0;
   mock_source_manager._status.imu_fifo_used_percent = 0;
   mock_source_manager._status.error_fifo_used_percent = 0;
   mock_source_manager._status.dose_fifo_used_percent_watermark
      = 20; // Significant change since it crosses a 10% step threshold even though only a single value
   mock_source_manager._status.battery_fifo_used_percent_watermark = 29;
   mock_source_manager._status.imu_fifo_used_percent_watermark = 39;
   mock_source_manager._status.error_fifo_used_percent_watermark = 59;

   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);

   // Simulate the passing of another 15 seconds with a significant changes to the status - the status should be
   // stored again
   mock_source_manager._status.dose_fifo_used_percent = 0;
   mock_source_manager._status.battery_fifo_used_percent = 0;
   mock_source_manager._status.imu_fifo_used_percent = 0;
   mock_source_manager._status.error_fifo_used_percent = 0;
   mock_source_manager._status.dose_fifo_used_percent_watermark
      = 19; // Significant change since it crosses a 10% step threshold even though only a single value
   mock_source_manager._status.battery_fifo_used_percent_watermark = 29;
   mock_source_manager._status.imu_fifo_used_percent_watermark = 39;
   mock_source_manager._status.error_fifo_used_percent_watermark = 59;

   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);

   // Simulate the passing of another 15 seconds with a significant changes to the status - the status should be
   // stored again
   mock_source_manager._status.temperature_celsius
      = 37; // Significant change since it crosses the 5 degree step threshold

   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);

   // Simulate the passing of another 15 seconds with a significant changes to the status - the status should be
   // stored again
   mock_source_manager._status.battery_sample_frequency_millihz = 1001u; // Significant change

   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);

   // Test that if there are multiple changes but they are all within the update time threshold that the status is
   // only updated once and not multiple times
   mock_source_manager._status.temperature_celsius = 38;

   result = run_process_loop_for_time(200u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   mock_source_manager._status.battery_sample_frequency_millihz = 10u;

   result = run_process_loop_for_time(200u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   mock_source_manager._status.error_fifo_used_percent_watermark = 70;

   result = run_process_loop_for_time(200u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   result = run_process_loop_for_time(15000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);
   result = dock_data_manager_ifc->dequeue(
      dock_data_manager_ifc, DATA_ID_RING_STATUS, &test_ring_status, sizeof(ring_status_t), 1);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_TRUE(memcmp(&mock_source_manager._status, &test_ring_status, sizeof(ring_status_t)) == 0);
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_RING_STATUS, &free_elements);
   ASSERT_EQ(RESULT_OK, result);
   ASSERT_EQ(free_elements, 50u);
}

/**
 * @brief Check data transmission
 */
TEST_F(RingDockComms, dock_ring_comms_test_check_data_transmission)
{
   const uint16_t DOSE_EVENT_COUNT = 50;
   const uint16_t DOCKED_STATUS_COUNT = 20;

   result_t result = RESULT_OK;
   ring_status_t test_ring_status = {0};

   // Simulate the passing of 10 seconds
   result = run_process_loop_for_time(10000u, 10u, true, false);
   ASSERT_EQ(RESULT_OK, result);

   // Queue some data
   dose_event_t dose_event = {0};
   dose_event.dose_completed_in_time = true;
   dose_event.event_id = event_id_t{.days_since_epoch = 12345u, .event_ctr = 1u};
   dose_event.start_timestamp_unix_s = 1234567890u;
   dose_event.duration_s = 1234u;

   // Enqueue multiple dose events
   for(int i = 0; i < DOSE_EVENT_COUNT; i++)
   {
      mock_source_manager._dose_event_queue_ifc->enqueue(mock_source_manager._dose_event_queue_ifc, &dose_event);
   }

   ring_docked_status_t ring_docked_status = {0};
   ring_docked_status.docked_status = true;
   ring_docked_status.timestamp_unix_s = 1234567890u;

   size_t free_elements = 0;
   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &free_elements);
   ASSERT_EQ(result, RESULT_OK);

   // Simulate the passing of 60 seconds
   result = run_process_loop_for_time(60000u, 10u, true, false);
   ASSERT_EQ(result, RESULT_OK);

   result = dock_data_manager_ifc->get_free_element_count(dock_data_manager_ifc, DATA_ID_DOSE_EVENT, &free_elements);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(free_elements, 100u - DOSE_EVENT_COUNT); // Check that all dose events were transmitted and stored in the
                                                      // dock data manager
}
