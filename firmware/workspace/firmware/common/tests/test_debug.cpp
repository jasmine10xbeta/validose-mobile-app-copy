// test_foo.cpp
#include <gtest/gtest.h>

// #define ARRAY_LEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

extern "C"
{
#include "common.h"
#include "debug.h"
#include "queue.h"
#include "system_time.h"
#include <stdbool.h>
#include <stdint.h>
   extern hot_log_t m_hot_log_table[HOT_LOG_TABLE_SIZE];
   extern uint8_t m_summary_process_index;
   extern uint16_t allocate_hot_log_table_index(void);
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

// #define ARRAY_LEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#define QUEUE_LEN (4)

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static uint8_t queue_data[QUEUE_LEN * sizeof(debug_msg_t)];
queue_t rx_queue;
static system_time_t m_systick;
uint8_t mock_uart_tx_buf[128];
uint8_t mock_uart_tx_buf_len;
uint8_t mock_flash_tx_buf[128];
uint8_t mock_flash_tx_buf_len;
static uint32_t mock_rtc_counter_ticks;
static uint32_t mock_epoch_time_seconds;

static const uint16_t m_hot_log_min_intervals_ms[DEBUG_LEVEL_MAX] = {
   [DEBUG_LEVEL_TRACE] = 2000u,
   [DEBUG_LEVEL_DEBUG] = 2000u,
   [DEBUG_LEVEL_INFO] = 2000u,
   [DEBUG_LEVEL_WARN] = 2000u,
   [DEBUG_LEVEL_ERROR] = 2000u,
   [DEBUG_LEVEL_CRITICAL] = 2000u,
};

static uint32_t mock_system_time_get_rtc_counter(void)
{
   mock_rtc_counter_ticks += 1024u;
   return mock_rtc_counter_ticks;
}

/***********************************************************************************************************************
 * Static function definitions
 **********************************************************************************************************************/

void print_hex(const uint8_t *p_data, size_t length)
{
   for(size_t i = 0; i < length; i++)
   {
      printf("0x%02X, ", p_data[i]);
   }
   printf("\n");
}

result_t mock_debug_uart_tx_handler(const uint8_t *p_data, size_t length)
{
   memcpy(mock_uart_tx_buf, p_data, length);
   mock_uart_tx_buf_len = length;
   return RESULT_OK;
}

result_t mock_debug_flash_tx_handler(const uint8_t *p_data, size_t length)
{
   memcpy(mock_flash_tx_buf, p_data, length);
   mock_flash_tx_buf_len = length;
   return RESULT_OK;
}

result_t mock_debug_uart_rx_handler(const uint8_t *p_data, size_t length)
{
   (void)debug_on_data_rx(p_data, length);
   return RESULT_OK;
}

class DebugTestSuite: public testing::Test
{
protected:
   void SetUp() override
   {
      memset(mock_uart_tx_buf, 0, sizeof(mock_uart_tx_buf));
      mock_uart_tx_buf_len = 0;
      memset(mock_flash_tx_buf, 0, sizeof(mock_flash_tx_buf));
      mock_flash_tx_buf_len = 0;
      mock_rtc_counter_ticks = 0u;
      mock_epoch_time_seconds = 0u;

      result_t result
         = queue_init(&rx_queue, queue_data, (size_t)(QUEUE_LEN * sizeof(debug_msg_t)), (size_t)sizeof(debug_msg_t));
      ASSERT_TRUE(IS_OK(result));

      // Initialize the system time used for keeping track of time
      result = system_time_init(&m_systick);
      ASSERT_TRUE(IS_OK(result));
      ASSERT_EQ(m_systick._current_time_ms, 0);

      result = debug_init(&(rx_queue.interface), &(m_systick.interface), &(m_hot_log_min_intervals_ms[0]));
      ASSERT_TRUE(IS_OK(result));
   }

   void TearDown() override
   {
   }
};

TEST_F(DebugTestSuite, NoInterfaces)
{
   uint16_t THIS_UNIT_ID = 15;
   result_t result = RESULT_OK;

   for(int i = 0; i < 10; i++)
   {
      DEBUG_INFO("This is a test");
   }

   size_t count;
   result = rx_queue.interface.get_count(&(rx_queue.interface), &count);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(0, count);
}

TEST_F(DebugTestSuite, TwoInterfaces)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   endpoint_t mock_flash_endpoint;
   mock_flash_endpoint.id = ENDPOINT_USER_DEFINED_1;
   mock_flash_endpoint.handler = mock_debug_flash_tx_handler;
   mock_flash_endpoint.apply_encoding = true;
   mock_flash_endpoint.disable_auto_routing = false;
   mock_flash_endpoint.permit_bulk_data = true;
   result = debug_register_endpoint(&mock_flash_endpoint);
   ASSERT_TRUE(IS_OK(result));

   // Spacing line
   // Spacing line
   // Spacer Line
   DEBUG_INFO("This is a test"); // Update test_str_encoded if this line number changes

   const uint8_t test_str_encoded[] = {
      0x7E,
      0x02,
      0x00,
      0x00,
      0x00,
      0x00,
      0x0F,
      0x9E,
      0x00,
      0x98,
      0x73,
      0x7E,
   };

   ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);
   ASSERT_EQ(sizeof(test_str_encoded), mock_flash_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));
   EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_flash_tx_buf, sizeof(test_str_encoded)));

   size_t count;
   result = rx_queue.interface.get_count(&(rx_queue.interface), &count);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(0, count);
}

TEST_F(DebugTestSuite, VarArgs)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   DEBUG_INFO("This is a test string with varargs such as %d and %u", (uint32_t)-25, 400);

   const uint8_t test_str_encoded[] = {
      0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xC7, 0x00, 0xE7,
      0xFF, 0xFF, 0xFF, 0x90, 0x01, 0x00, 0x00, 0xAC, 0x61, 0x7E,
   };
   ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);

   EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));

   // Spacer Line
   DEBUG_INFO("We can have up to six of them: %d %u %u %u %u %u", (uint32_t)-25, 400, 812, 44421, 812, 222);

   const uint8_t test_str_encoded2[] = {
      0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xD2, 0x00, 0xE7, 0xFF, 0xFF, 0xFF, 0x90, 0x01, 0x00, 0x00, 0x2C,
      0x03, 0x00, 0x00, 0x85, 0xAD, 0x00, 0x00, 0x2C, 0x03, 0x00, 0x00, 0xDE, 0x00, 0x00, 0x00, 0xBC, 0x1C, 0x7E,
   };

   ASSERT_EQ(sizeof(test_str_encoded2), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded2, mock_uart_tx_buf, sizeof(test_str_encoded2)));
}

TEST_F(DebugTestSuite, DisableLevels)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   DEBUG_INFO("This is a test");

   const uint8_t test_str_encoded[] = {
      0x7E,
      0x02,
      0x00,
      0x00,
      0x00,
      0x00,
      0x0F,
      0xEA,
      0x00,
      0x5C,
      0xEE,
      0x7E,
   };

   ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));

   // Mock a command to set debug level of unit 15 to "DEBUG_ERROR"
   const uint8_t set_unit_15_error_msg[] = {0x7E, 0x08, 0x0F, 0x04, 0xF9, 0x1B, 0x7E};
   mock_debug_uart_rx_handler(set_unit_15_error_msg, 7u);

   // Now an error should be logged, and nothing should happen for an info level message
   DEBUG_ERROR("This is an error test");

   // Spacer Line
   DEBUG_INFO("This info message should be ignored...");

   DEBUG_WARNING("...as should this warning. The only thing in the buffer should be that error.");

   const uint8_t test_str_encoded2[] = {
      0x7E,
      0x04,
      0x00,
      0x00,
      0x00,
      0x00,
      0x0F,
      0x03,
      0x01,
      0x66,
      0x2E,
      0x7E,
   }; // The encoded error message
   ASSERT_EQ(sizeof(test_str_encoded2), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded2, mock_uart_tx_buf, sizeof(test_str_encoded2)));
}

TEST_F(DebugTestSuite, DebugNumArgsMacro)
{
   ASSERT_EQ(0, NUMARGS());
   ASSERT_EQ(1, NUMARGS(55));
   ASSERT_EQ(2, NUMARGS(2, 8));
   ASSERT_EQ(3, NUMARGS(442, 12, (uint32_t)-5));
}

TEST_F(DebugTestSuite, ReceiveTestCommands)
{
   uint16_t THIS_UNIT_ID = 15;

   result_t result = RESULT_OK;

   // Mock some incoming tests commands
   const uint8_t test_command_id_zero[] = {0x7E, 0x06, 0x00, 0x01, 0xA2, 0x81, 0x7E};
   mock_debug_uart_rx_handler(test_command_id_zero, 7u);

   const uint8_t test_command_id_two[] = {0x7E, 0x06, 0x02, 0x86, 0x00, 0xF8, 0xC7, 0x7E};
   mock_debug_uart_rx_handler(test_command_id_two, 8u);

   size_t msg_count = 0;
   result = debug_get_num_rx_msgs(&msg_count);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(2, msg_count);

   debug_msg_t command_msg;

   // Check command with ID zero
   result = debug_dequeue_message(&command_msg);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(3, command_msg.packet_data_len);
   ASSERT_EQ(0, command_msg.packet_data[1]);

   // Check command with ID two
   result = debug_dequeue_message(&command_msg);
   ASSERT_TRUE(IS_OK(result));
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(4, command_msg.packet_data_len);
   ASSERT_EQ(2, command_msg.packet_data[1]);
}

TEST_F(DebugTestSuite, DifferentLineNoSuppressionTest)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   DEBUG_INFO("This is a the first test string");

   const uint8_t test_str_encoded[] = {0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x53, 0x01, 0xE8, 0x5A, 0x7E};
   ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);

   EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));

   DEBUG_INFO("This is a the second string, without a timestamp increase, proving that errors from different lines "
              "DON'T get suppressed.");

   const uint8_t test_str_encoded_2[] = {0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x5A, 0x01, 0x52, 0xC2, 0x7E};
   ASSERT_EQ(sizeof(test_str_encoded_2), mock_uart_tx_buf_len);

   EXPECT_TRUE(0 == memcmp(test_str_encoded_2, mock_uart_tx_buf, sizeof(test_str_encoded_2)));
}

TEST_F(DebugTestSuite, InfoSuppressionTest)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   for(uint8_t i = 0; i < 2; i++)
   {
      DEBUG_INFO("This is a the first test string");

      const uint8_t test_str_encoded[] = {0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x72, 0x01, 0xDD, 0x8D, 0x7E};
      ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);

      EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));

      m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   }
}

TEST_F(DebugTestSuite, InfoNoSuppressionTest)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   DEBUG_INFO("This is a the first test string");

   const uint8_t test_str_encoded[] = {0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x8A, 0x01, 0x47, 0xE5, 0x7E};
   ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);

   EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));

   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));

   DEBUG_INFO("Systick advanced passed suppresion time, so this should log again");
   const uint8_t test_str_encoded_2[] = {0x7E, 0x02, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x94, 0x01, 0x07, 0x7A, 0x7E};
   ASSERT_EQ(sizeof(test_str_encoded_2), mock_uart_tx_buf_len);

   EXPECT_TRUE(0 == memcmp(test_str_encoded_2, mock_uart_tx_buf, sizeof(test_str_encoded_2)));
}

TEST_F(DebugTestSuite, SummaryTest)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   for(uint8_t i = 0; i < 2; i++)
   {
      DEBUG_INFO("This is a the first test string");

      const uint8_t test_str_encoded[] = {0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xAA, 0x01, 0x41, 0x03, 0x7E};
      ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);

      EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));

      m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   }

   // Now force a summary log
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   result = debug_process();
   ASSERT_TRUE(IS_OK(result));

   const uint8_t test_str_encoded_summary[] = {
      0x7E, 0x03, 0x03, 0x00, 0x00, 0x00, 0x01, 0xCF, 0x03, 0x0F, 0x00, 0x00, 0x00, 0xAA,
      0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x85, 0x19, 0x7E,
   }; // The encoded summary message
   ASSERT_EQ(sizeof(test_str_encoded_summary), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded_summary, mock_uart_tx_buf, sizeof(test_str_encoded_summary)));
}

TEST_F(DebugTestSuite, MultipleSummariesTest)
{
   uint16_t THIS_UNIT_ID = 15;

   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   for(uint8_t i = 0; i < 2; i++)
   {
      DEBUG_INFO("This is a the first test summary string");

      const uint8_t test_str_encoded[] = {0x7E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xD0, 0x01, 0xA6, 0x91, 0x7E};
      ASSERT_EQ(sizeof(test_str_encoded), mock_uart_tx_buf_len);

      EXPECT_TRUE(0 == memcmp(test_str_encoded, mock_uart_tx_buf, sizeof(test_str_encoded)));
      m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   }

   for(uint8_t i = 0; i < 2; i++)
   {
      DEBUG_INFO("This is a the second test summary string");

      const uint8_t test_str_encoded_2[] = {0x7E, 0x02, 0x02, 0x00, 0x00, 0x00, 0x0F, 0xDB, 0x01, 0x1A, 0x88, 0x7E};
      ASSERT_EQ(sizeof(test_str_encoded_2), mock_uart_tx_buf_len);

      EXPECT_TRUE(0 == memcmp(test_str_encoded_2, mock_uart_tx_buf, sizeof(test_str_encoded_2)));
      m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   }

   result = debug_process();
   ASSERT_TRUE(IS_OK(result));

   const uint8_t test_str_encoded_summary[] = {
      0x7E, 0x03, 0x04, 0x00, 0x00, 0x00, 0x01, 0xCF, 0x03, 0x0F, 0x00, 0x00, 0x00, 0xD0,
      0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0xFB, 0x7E,
   }; // The encoded summary message
   ASSERT_EQ(sizeof(test_str_encoded_summary), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded_summary, mock_uart_tx_buf, sizeof(test_str_encoded_summary)));

   // Process again, but next summary should not be shown as enough time has not passed.
   result = debug_process();
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(sizeof(test_str_encoded_summary), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded_summary, mock_uart_tx_buf, sizeof(test_str_encoded_summary)));

   // Advance time and process again, now the second summary should be shown.
   // Advacne with 2 seconds as WARN cool down (summary level) is 2 seconds.
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));

   result = debug_process();
   ASSERT_TRUE(IS_OK(result));

   const uint8_t test_str_encoded_summary_2[] = {
      0x7E, 0x03, 0x06, 0x00, 0x00, 0x00, 0x01, 0xCF, 0x03, 0x0F, 0x00, 0x00, 0x00, 0xDB,
      0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xE3, 0xDA, 0x7E,
   }; // The encoded summary message
   ASSERT_EQ(sizeof(test_str_encoded_summary_2), mock_uart_tx_buf_len);
   EXPECT_TRUE(0 == memcmp(test_str_encoded_summary_2, mock_uart_tx_buf, sizeof(test_str_encoded_summary_2)));
}

TEST_F(DebugTestSuite, SummaryProcessIndexAdvancesCorrectly)
{
   // Register a mock endpoint to capture output
   endpoint_t mock_uart_endpoint;
   mock_uart_endpoint.id = ENDPOINT_UART;
   mock_uart_endpoint.handler = mock_debug_uart_tx_handler;
   mock_uart_endpoint.apply_encoding = true;
   mock_uart_endpoint.disable_auto_routing = false;
   mock_uart_endpoint.permit_bulk_data = true;
   result_t result = debug_register_endpoint(&mock_uart_endpoint);
   ASSERT_TRUE(IS_OK(result));

   // Simulate two different suppressed log entries in the hot log table
   m_hot_log_table[0].in_use = true;
   m_hot_log_table[0].updated_since_last_summary = true;
   m_hot_log_table[0].last_summary_time_ms = 0;
   m_hot_log_table[0].module_id = 1;
   m_hot_log_table[0].line = 100;
   m_hot_log_table[0].level = DEBUG_LEVEL_WARN;
   m_hot_log_table[0].suppress_count = 5;

   m_hot_log_table[1].in_use = true;
   m_hot_log_table[1].updated_since_last_summary = true;
   m_hot_log_table[1].last_summary_time_ms = 0;
   m_hot_log_table[1].module_id = 2;
   m_hot_log_table[1].line = 200;
   m_hot_log_table[1].level = DEBUG_LEVEL_WARN;
   m_hot_log_table[1].suppress_count = 3;

   // Advance time so both are eligible for summary
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));

   // First call: should process only the first entry (index 0)
   result = debug_process();
   ASSERT_TRUE(IS_OK(result));
   EXPECT_FALSE(m_hot_log_table[0].updated_since_last_summary);
   ASSERT_EQ(m_hot_log_table[0].last_summary_time_ms, 3000u);
   EXPECT_TRUE(m_hot_log_table[1].updated_since_last_summary);
   ASSERT_EQ(m_hot_log_table[1].last_summary_time_ms, 0u);

   // m_summary_process_index should now be 1 (next entry)
   ASSERT_EQ(m_summary_process_index, 1u);

   // Advance time so new summary can be printed.
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));

   // Reset this so we can test that it now starts processing at the second one.
   m_hot_log_table[0].in_use = true;
   m_hot_log_table[0].updated_since_last_summary = true;
   m_hot_log_table[0].last_summary_time_ms = 0;
   m_hot_log_table[0].module_id = 1;
   m_hot_log_table[0].line = 100;
   m_hot_log_table[0].level = DEBUG_LEVEL_WARN;
   m_hot_log_table[0].suppress_count = 5;

   // Second call: should process only the second entry (index 1)
   result = debug_process();
   ASSERT_TRUE(IS_OK(result));
   EXPECT_TRUE(m_hot_log_table[0].updated_since_last_summary);
   EXPECT_FALSE(m_hot_log_table[1].updated_since_last_summary);

   // m_summary_process_index should now be 2 (end of table or next entry)
   ASSERT_EQ(m_summary_process_index, 2u);

   // Advance time so new summary can be printed.
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));
   m_systick.interface.inc_time_by_1000_ms(&(m_systick.interface));

   // Third call: nothing left to process, index should reset to 0
   result = debug_process();
   ASSERT_TRUE(IS_OK(result));
   ASSERT_EQ(m_summary_process_index, 0u);
}

class AllocateHotLogTableIndexTestSuite: public testing::Test
{
protected:
   void SetUp() override
   {
      // Clear the hot log table before each test
      memset(m_hot_log_table, 0, sizeof(hot_log_t) * HOT_LOG_TABLE_SIZE);
   }
   void TearDown() override
   {
   }
};

// Test: When all entries are unused, the first index should be returned
TEST_F(AllocateHotLogTableIndexTestSuite, ReturnsFirstFreeIndexWhenAllUnused)
{
   uint16_t idx = allocate_hot_log_table_index();
   ASSERT_EQ(idx, 0u);
}

// Test: When some entries are in use, returns the first unused index
TEST_F(AllocateHotLogTableIndexTestSuite, ReturnsFirstFreeIndexWhenSomeUsed)
{
   m_hot_log_table[0].in_use = true;
   m_hot_log_table[1].in_use = true;
   m_hot_log_table[2].in_use = false;
   uint16_t idx = allocate_hot_log_table_index();
   ASSERT_EQ(idx, 2u);
}

// Test: When all entries are in use, returns the least recently used (oldest last_used_time_ms)
TEST_F(AllocateHotLogTableIndexTestSuite, ReturnsLRUIndexWhenAllUsed)
{
   for(uint16_t i = 0; i < HOT_LOG_TABLE_SIZE; ++i)
   {
      m_hot_log_table[i].in_use = true;
      m_hot_log_table[i].last_used_time_ms = 1000 + i; // increasing, so 0 is oldest
   }
   m_hot_log_table[5].last_used_time_ms = 500; // Make index 5 the oldest
   uint16_t idx = allocate_hot_log_table_index();
   ASSERT_EQ(idx, 5u);
}

// Test: If multiple entries have the same oldest last_used_time_ms, returns the first one
TEST_F(AllocateHotLogTableIndexTestSuite, ReturnsFirstOfOldestIfTie)
{
   for(uint16_t i = 0; i < HOT_LOG_TABLE_SIZE; ++i)
   {
      m_hot_log_table[i].in_use = true;
      m_hot_log_table[i].last_used_time_ms = 1000;
   }
   m_hot_log_table[3].last_used_time_ms = 500;
   m_hot_log_table[7].last_used_time_ms = 500;
   uint16_t idx = allocate_hot_log_table_index();
   // Should return 3, the first with the oldest time
   ASSERT_EQ(idx, 3u);
}