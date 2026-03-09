#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "unity.h"

void test_board();
void test_sbl();

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
   NRF_LOG_ERROR("%s:%d", p_file_name, line_num);
   TEST_FAIL();
   NRF_BREAKPOINT_COND;
}

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
   NRF_LOG_ERROR("Received a fault! id: 0x%08x, pc: 0x%08x, info: 0x%08x", id, pc, info);
   TEST_FAIL();
   NRF_BREAKPOINT_COND;
}

void app_error_handler_bare(uint32_t error_code)
{
   NRF_LOG_ERROR("Received an error: 0x%08x!", error_code);
   TEST_FAIL();
   NRF_BREAKPOINT_COND;
}

static void logging_init()
{
   ret_code_t ret_val = NRF_LOG_INIT(NULL);
   APP_ERROR_CHECK(ret_val);
   NRF_LOG_DEFAULT_BACKENDS_INIT();
   NRF_LOG_DEFAULT_BACKENDS_INIT();
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main()
{
   logging_init();
   test_board();
   test_sbl();
   while(true)
   {
      NRF_LOG_FLUSH();
      nrf_delay_ms(1000);
   }
}
