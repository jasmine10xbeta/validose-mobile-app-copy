#include "app_error.h"
#include "boards.h"
#include "nrf_bootloader.h"
#include "nrf_bootloader_app_start.h"
#include "nrf_bootloader_dfu_timers.h"
#include "nrf_bootloader_info.h"
#include "nrf_dfu.h"
#include "nrf_mbr.h"
#include <stdint.h>

bool sbl_mbr_is_configured()
{
   return ((*(const uint32_t *)MBR_BOOTLOADER_ADDR == BOOTLOADER_START_ADDR)
           && (*(const uint32_t *)MBR_PARAM_PAGE_ADDR) == NRF_MBR_PARAMS_PAGE_ADDRESS);
}

void sbl_mbr_configure()
{
   nrf_bootloader_mbr_addrs_populate();
}

void sbl_mbr_protect()
{
   ret_code_t ret_val = nrf_bootloader_flash_protect(0, MBR_SIZE);
   APP_ERROR_CHECK(ret_val);
}

void sbl_bootloader_protect()
{
   ret_code_t ret_val = nrf_bootloader_flash_protect(BOOTLOADER_START_ADDR, BOOTLOADER_SIZE);
   APP_ERROR_CHECK(ret_val);
}

void sbl_execute()
{
   ret_code_t ret_val = nrf_bootloader_init(NULL);
   APP_ERROR_CHECK(ret_val);
}
