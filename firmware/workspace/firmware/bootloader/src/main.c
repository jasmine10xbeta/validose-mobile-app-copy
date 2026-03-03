#include <stdint.h>

#include "board.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "sbl.h"

static void on_error()
{
   NVIC_SystemReset();
}

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
   NRF_LOG_ERROR("%s:%d", p_file_name, line_num);
   on_error();
}

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
   NRF_LOG_ERROR("Received a fault! id: 0x%08x, pc: 0x%08x, info: 0x%08x", id, pc, info);
   on_error();
}

void app_error_handler_bare(uint32_t error_code)
{
   NRF_LOG_ERROR("Received an error: 0x%08x!", error_code);
   on_error();
}

static void logging_init()
{
   ret_code_t ret_val = NRF_LOG_INIT(NULL);
   APP_ERROR_CHECK(ret_val);
   NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for application main entry. */
int main()
{
   /* Set hardware to safe state. */
   board_init();
   /* Ensure that the MBR points to the bootloader. This must be done before
    * flash block protection is applied, since it edits a protected page. */
   sbl_mbr_configure();
   /* Check that the MBR correctly points to the bootloader. */
   APP_ERROR_CHECK_BOOL(sbl_mbr_is_configured());
   /* Protect the MBR and bootloader code from being overwritten. */
   sbl_mbr_protect();
   sbl_bootloader_protect();
   /* Initialize the logging module. */
   logging_init();
   /* Execute the bootloader. */
   sbl_execute();
}
