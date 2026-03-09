#include <gtest/gtest.h>

extern "C"
{
#include "common.h"
#include "flash_driver_s25hl512t.h"
#include "spi_driver_interface.h"
#include "spi_driver_mock.h"
#include <stdbool.h>
#include <stdint.h>
}

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

class FlashDriverTestSuite: public testing::Test
{
protected:
   spi_driver_t spi_driver;
   flash_driver_t flash_driver;

   void SetUp() override
   {
   }

   void TearDown() override
   {
   }
};

TEST_F(FlashDriverTestSuite, ReadRaw_NullInterface)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   flash_driver_interface_t *flash_driver_interface_null_ptr = NULL;

   uint32_t read_address = 0x00000000u;
   uint8_t read_data[32u] = {0};
   uint8_t read_len = 32u;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      flash_driver_interface_null_ptr, read_address, read_data, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, ReadRaw_NullSpiInterface)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   flash_driver._spi_interface = NULL;

   uint32_t read_address = 0x00000000u;
   uint8_t read_data[32u] = {0};
   uint8_t read_len = 32u;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, ReadRaw_ReadDataNull)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t read_address = 0x00000000u;
   uint8_t *read_data_null_ptr = NULL;
   uint8_t read_len = 32u;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data_null_ptr, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, ReadRaw_ReadLengthZero)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t read_address = 0x00000000u;
   uint8_t read_data[32u] = {0};
   uint8_t read_len = 0u;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_DATA_LENGTH);
}

TEST_F(FlashDriverTestSuite, ReadRaw_ReadLengthOverflow)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t read_address = 0x00000000u;
   uint8_t read_data[32u] = {0};
   // lower level sdk limits SPI transfer. Setting the read_len to buffer length should cause overflow as command and
   // address are added.
   uint8_t read_len = FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_DATA_LENGTH);
}

TEST_F(FlashDriverTestSuite, ReadRaw_FailedSpiTransfer)
{
   result_t result = mock_spi_driver_init(&spi_driver, false);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t read_address = 0x00000000u;
   uint8_t read_data[32u] = {0};
   uint8_t read_len = 32u;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_READ_RAW);
}

TEST_F(FlashDriverTestSuite, ReadRaw_Success)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t read_address = 0x00000000u;
   uint8_t read_data[32u] = {0};
   uint8_t read_len = 32u;

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data, read_len);

   ASSERT_TRUE(IS_OK(result));
}

TEST_F(FlashDriverTestSuite, ProgPage_NullInterface)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   flash_driver_interface_t *flash_driver_interface_null_ptr = NULL;

   uint32_t write_address = 0x00000000u;
   uint8_t write_data[32u] = {0};
   uint8_t write_len = 32u;

   result = flash_driver.interface.prog_page_256b_blocking(
      flash_driver_interface_null_ptr, write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, ProgPage_NullSpiInterface)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   flash_driver._spi_interface = NULL;

   uint32_t write_address = 0x00000000u;
   uint8_t write_data[32u] = {0};
   uint8_t write_len = 32u;

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, ProgPage_WriteDataNull)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t write_address = 0x00000000u;
   uint8_t *write_data_null_ptr = NULL;
   uint8_t write_len = 32u;

   result = flash_driver.interface.prog_page_256b_blocking(
      &(flash_driver.interface), write_address, write_data_null_ptr, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, ProgPage_WriteLengthZero)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t write_address = 0x00000000u;
   uint8_t write_data[32u] = {0};
   uint8_t write_len = 0u;

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_DATA_LENGTH);
}

TEST_F(FlashDriverTestSuite, ProgPage_WriteLengthOverflow)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t write_address = 0x00000000u;
   uint8_t write_data[32u] = {0};
   uint8_t write_len = FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES;

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_DATA_LENGTH);
}

TEST_F(FlashDriverTestSuite, ProgPage_FailedSpiTransfer)
{
   result_t result = mock_spi_driver_init(&spi_driver, false);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t write_address = 0x00000000u;
   uint8_t write_data[32u] = {0};
   uint8_t write_len = 32u;

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_PROG_PAGE);
}

TEST_F(FlashDriverTestSuite, ProgPage_Success)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t write_address = 0x00000000u;
   uint8_t write_data[32u] = {0};
   uint8_t write_len = 32u;

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_TRUE(IS_OK(result));
}

TEST_F(FlashDriverTestSuite, ProgPage_CrossPageWrite)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t write_address = (FLASH_DRIVER_PAGE_WRITE_SIZE_BYTES - 16u);
   uint8_t write_data[32u] = {0};
   uint8_t write_len = 32u;

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_CROSS_PAGE_WRITE);
}

TEST_F(FlashDriverTestSuite, EraseSector_NullInterface)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   flash_driver_interface_t *flash_driver_interface_null_ptr = NULL;
   uint32_t start_address = 0x00000000u;

   result = flash_driver.interface.erase_sector_256kb_blocking(flash_driver_interface_null_ptr, start_address);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, EraseSector_NullSpiInterface)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   flash_driver._spi_interface = NULL;
   uint32_t start_address = 0x00000000u;

   result = flash_driver.interface.erase_sector_256kb_blocking(&(flash_driver.interface), start_address);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_NULL);
}

TEST_F(FlashDriverTestSuite, EraseSector_StartAddressNotAligned)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t start_address = (FLASH_DRIVER_SECTOR_SIZE_BYTES - 1u);

   result = flash_driver.interface.erase_sector_256kb_blocking(&(flash_driver.interface), start_address);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_INVALID_ADDR);
}

TEST_F(FlashDriverTestSuite, EraseSector_StartAddressOutOfRange)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t start_address = FLASH_DRIVER_TOTAL_SIZE_BYTES;

   result = flash_driver.interface.erase_sector_256kb_blocking(&(flash_driver.interface), start_address);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_INVALID_ADDR);
}

TEST_F(FlashDriverTestSuite, EraseSector_FailedSpiTransfer)
{
   result_t result = mock_spi_driver_init(&spi_driver, false);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t start_address = 0x00000000u;

   result = flash_driver.interface.erase_sector_256kb_blocking(&(flash_driver.interface), start_address);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_SECTOR_ERASE);
}

TEST_F(FlashDriverTestSuite, EraseSector_Success)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   uint32_t start_address = 0x00000000u;

   result = flash_driver.interface.erase_sector_256kb_blocking(&(flash_driver.interface), start_address);

   ASSERT_TRUE(IS_OK(result));
}

TEST_F(FlashDriverTestSuite, ReadRaw_PastFlashSize)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   // Try to read starting at the last valid address, but with a length that goes past the end
   uint32_t read_address = FLASH_DRIVER_TOTAL_SIZE_BYTES - 16u;
   uint8_t read_data[32u] = {0};
   uint8_t read_len = 32u; // This should go past the end

   result = flash_driver.interface.read_raw_linear_burst_blocking(
      &(flash_driver.interface), read_address, read_data, read_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_INVALID_ADDR);
}

TEST_F(FlashDriverTestSuite, ProgPage_PastFlashSize)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   // Try to program starting at the last valid address, but with a length that goes past the end
   uint32_t write_address = FLASH_DRIVER_TOTAL_SIZE_BYTES - 16u;
   uint8_t write_data[32u] = {0xAA};
   uint8_t write_len = 32u; // This should go past the end

   result
      = flash_driver.interface.prog_page_256b_blocking(&(flash_driver.interface), write_address, write_data, write_len);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_INVALID_ADDR);
}

TEST_F(FlashDriverTestSuite, EraseSector_PastFlashSize)
{
   result_t result = mock_spi_driver_init(&spi_driver, true);
   ASSERT_TRUE(IS_OK(result));
   result = flash_driver_init(&flash_driver, &(spi_driver.interface));

   // Try to erase a sector starting at an address equal to the total size (out of range)
   uint32_t start_address = FLASH_DRIVER_TOTAL_SIZE_BYTES;

   result = flash_driver.interface.erase_sector_256kb_blocking(&(flash_driver.interface), start_address);

   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_FLASH_DRV);
   ASSERT_EQ(GET_ERR_CODE(result), FLASH_DRV_ERROR_INVALID_ADDR);
}
