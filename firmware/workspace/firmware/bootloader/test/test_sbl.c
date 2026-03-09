#include "nrf_bootloader_info.h"
#include "nrf_dfu_types.h"
#include "nrf_mbr.h"
#include "nrf_nvmc.h"
#include "sbl.h"
#include "unity.h"

/**
 * @brief Check that MBR defines have not been altered.
 */
static void test_sbl_mbr_defines()
{
   TEST_ASSERT_EQUAL_HEX32(0xFF8, MBR_BOOTLOADER_ADDR);
   TEST_ASSERT_EQUAL_HEX32(0xF1000, BOOTLOADER_START_ADDR);
   TEST_ASSERT_EQUAL_HEX32(0xFFC, MBR_PARAM_PAGE_ADDR);
   TEST_ASSERT_EQUAL_HEX32(0xFE000, NRF_MBR_PARAMS_PAGE_ADDRESS);
   TEST_ASSERT_EQUAL_HEX32(0x1000, MBR_SIZE);
}

static void test_sbl_mbr_configure_after_erase()
{
   /* Save a copy of the MBR. */
   uint32_t mbr_copy[CODE_PAGE_SIZE];
   memcpy(mbr_copy, 0, sizeof(mbr_copy));
   /* Erase the MBR.*/
   nrf_nvmc_page_erase(0);
   TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, *(const uint32_t *)MBR_BOOTLOADER_ADDR);
   TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, *(const uint32_t *)MBR_PARAM_PAGE_ADDR);
   TEST_ASSERT_FALSE(sbl_mbr_is_configured());
   sbl_mbr_configure();
   TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_START_ADDR, *(const uint32_t *)MBR_BOOTLOADER_ADDR);
   TEST_ASSERT_EQUAL_HEX32(NRF_MBR_PARAMS_PAGE_ADDRESS, *(const uint32_t *)MBR_PARAM_PAGE_ADDR);
   TEST_ASSERT_TRUE(sbl_mbr_is_configured());
   /* Restore the MBR. */
   nrf_nvmc_write_words(0, mbr_copy, CODE_PAGE_SIZE);
}

static void test_sbl_mbr_configure_repeated()
{
   sbl_mbr_configure();
   TEST_ASSERT_TRUE(sbl_mbr_is_configured());
   sbl_mbr_configure();
   TEST_ASSERT_TRUE(sbl_mbr_is_configured());
}

static bool is_protected(uint32_t address)
{
   for(uint32_t i = 0; i < ACL_REGIONS_COUNT; i++)
   {
      uint32_t from = NRF_ACL->ACL[i].ADDR;
      uint32_t to = from + NRF_ACL->ACL[i].SIZE;
      if((address >= from) && (address < to))
      {
         if(NRF_ACL->ACL[i].PERM == (ACL_ACL_PERM_WRITE_Disable << ACL_ACL_PERM_WRITE_Pos))
         {
            return true;
         }
      }
   }
   return false;
}

static void test_sbl_mbr_protect()
{
   sbl_mbr_configure();
   TEST_ASSERT_TRUE(sbl_mbr_is_configured());
   sbl_mbr_protect();
   for(uint32_t address = 0; address < MBR_SIZE; address += CODE_PAGE_SIZE)
   {
      TEST_ASSERT_TRUE(is_protected(address));
   }
}

static void test_sbl_bootloader_protect()
{
   sbl_mbr_configure();
   TEST_ASSERT_TRUE(sbl_mbr_is_configured());
   sbl_bootloader_protect();
   for(uint32_t address = BOOTLOADER_START_ADDR; address < NRF_MBR_PARAMS_PAGE_ADDRESS; address += CODE_PAGE_SIZE)
   {
      TEST_ASSERT_TRUE(is_protected(address));
   }
}

/**
 * @brief Check that security defines have not been altered.
 */
static void test_sbl_security_defines()
{
   TEST_ASSERT_EQUAL(0, NRF_DFU_APP_ACCEPT_SAME_VERSION);
   TEST_ASSERT_EQUAL(0, NRF_BL_APP_CRC_CHECK_SKIPPED_ON_GPREGRET2);
   TEST_ASSERT_EQUAL(0, NRF_BL_APP_CRC_CHECK_SKIPPED_ON_SYSTEMOFF_RESET);
   TEST_ASSERT_EQUAL(1, NRF_BL_APP_SIGNATURE_CHECK_REQUIRED);
   TEST_ASSERT_EQUAL(1, NRF_DFU_APP_DOWNGRADE_PREVENTION);
   TEST_ASSERT_EQUAL(4, NRF_DFU_HW_VERSION);
   TEST_ASSERT_EQUAL(0, NRF_DFU_IN_APP);
   TEST_ASSERT_EQUAL(1, NRF_DFU_REQUIRE_SIGNED_APP_UPDATE);
   TEST_ASSERT_EQUAL(0, NRF_DFU_SUPPORTS_EXTERNAL_APP);
}

/**
 * @brief Check that configuration defines have not been altered.
 */
static void test_sbl_configuration_defines()
{
   TEST_ASSERT_EQUAL(0, NRF_BL_DFU_ALLOW_UPDATE_FROM_APP);
   TEST_ASSERT_EQUAL(0, NRF_BL_DFU_ENTER_METHOD_BUTTON);
   TEST_ASSERT_EQUAL(0, NRF_BL_DFU_ENTER_METHOD_BUTTONLESS);
   TEST_ASSERT_EQUAL(1, NRF_BL_DFU_ENTER_METHOD_GPREGRET);
   TEST_ASSERT_EQUAL(0, NRF_BL_DFU_ENTER_METHOD_PINRESET);
   TEST_ASSERT_EQUAL(10000, NRF_BL_DFU_CONTINUATION_TIMEOUT_MS);
   TEST_ASSERT_EQUAL(30000, NRF_BL_DFU_INACTIVITY_TIMEOUT_MS);
   TEST_ASSERT_EQUAL(8, NRF_BL_FW_COPY_PROGRESS_STORE_STEP);
   TEST_ASSERT_EQUAL(0, NRF_BL_RESET_DELAY_MS);
   TEST_ASSERT_EQUAL(10000, NRF_BL_WDT_MAX_SCHEDULER_LATENCY_MS);
   TEST_ASSERT_EQUAL((1 * 4096), NRF_DFU_APP_DATA_AREA_SIZE);
   TEST_ASSERT_EQUAL(40, NRF_DFU_BLE_ADV_INTERVAL);
   TEST_ASSERT_EQUAL("VALIDOSE DFU", NRF_DFU_BLE_ADV_NAME);
   TEST_ASSERT_EQUAL(6000, NRF_DFU_BLE_CONN_SUP_TIMEOUT_MS);
   TEST_ASSERT_EQUAL(12, NRF_DFU_BLE_MAX_CONN_INTERVAL);
   TEST_ASSERT_EQUAL(12, NRF_DFU_BLE_MIN_CONN_INTERVAL);
   TEST_ASSERT_EQUAL(0, NRF_DFU_BLE_REQUIRES_BONDS);
   TEST_ASSERT_EQUAL(0, NRF_DFU_EXTERNAL_APP_VERSIONING);
   TEST_ASSERT_EQUAL(0, NRF_DFU_FORCE_DUAL_BANK_APP_UPDATES);
   TEST_ASSERT_EQUAL(0, NRF_DFU_PROTOCOL_FW_VERSION_MSG);
   TEST_ASSERT_EQUAL(1, NRF_DFU_PROTOCOL_REDUCED);
   TEST_ASSERT_EQUAL(0, NRF_DFU_PROTOCOL_VERSION_MSG);
   TEST_ASSERT_EQUAL(0, NRF_DFU_SAVE_PROGRESS_IN_FLASH);
   TEST_ASSERT_EQUAL(0, NRF_DFU_SINGLE_BANK_APP_UPDATES);
   TEST_ASSERT_EQUAL(1, NRF_DFU_TRANSPORT_BLE);
}

void test_sbl()
{
   UNITY_BEGIN();
   RUN_TEST(test_sbl_mbr_defines);
   RUN_TEST(test_sbl_mbr_configure_after_erase);
   RUN_TEST(test_sbl_mbr_configure_repeated);
   RUN_TEST(test_sbl_mbr_protect);
   RUN_TEST(test_sbl_bootloader_protect);
   RUN_TEST(test_sbl_security_defines);
   RUN_TEST(test_sbl_configuration_defines);
   UNITY_END();
}
