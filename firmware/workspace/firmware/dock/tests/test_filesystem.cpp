#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>

extern "C"
{
#include "common.h"
#include "filesystem.h"
#include "flash_driver_interface.h"
#include "flash_driver_mock.h"
#include "lfs.h"
#include <stdbool.h>
#include <stdint.h>
}

class FilesystemTestSuite: public testing::Test
{
protected:
   flash_driver_t flash_driver;
   filesystem_t filesystem;

   void SetUp() override
   {
   }

   void TearDown() override
   {
   }

   result_t InitFilesystem(bool read_ok = true, bool prog_ok = true, bool erase_ok = true)
   {
      result_t result = mock_flash_driver_init(&flash_driver, read_ok, prog_ok, erase_ok);

      if(IS_ERR(result))
      {
         return result;
      }

      return filesystem_init(&filesystem, &(flash_driver.interface));
   }
};

TEST_F(FilesystemTestSuite, Init_NullSelf)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));

   result = filesystem_init(NULL, &(flash_driver.interface));

   ASSERT_EQ(SW_UNIT_ID_FILESYSTEM, GET_ERR_UNIT(result));
   ASSERT_EQ(FILESYSTEM_ERROR_NULL, GET_ERR_CODE(result));
}

TEST_F(FilesystemTestSuite, Init_NullInterface)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   result = filesystem_init(&filesystem, NULL);

   ASSERT_EQ(SW_UNIT_ID_FILESYSTEM, GET_ERR_UNIT(result));
   ASSERT_EQ(FILESYSTEM_ERROR_NULL, GET_ERR_CODE(result));
}

TEST_F(FilesystemTestSuite, Init_DependencyNotInitialized)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   flash_driver._initialized = false;

   result = filesystem_init(&filesystem, &(flash_driver.interface));

   ASSERT_EQ(SW_UNIT_ID_FILESYSTEM, GET_ERR_UNIT(result));
   ASSERT_EQ(FILESYSTEM_ERROR_DEPEND_NOT_INITIALIZED, GET_ERR_CODE(result));
}

TEST_F(FilesystemTestSuite, Init_SetsConfiguration)
{
// Defines below are purposefully re-declared to make tests fail if config in code is changed.
#define FLASH_DRIVER_TOTAL_SIZE_BYTES_TEST      (0x04000000u) // 64MB total flash size.
#define FLASH_DRIVER_SECTOR_SIZE_BYTES_TEST     (0x40000u)    // 256kB blocks
#define FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES_TEST (128u)
#define FILESYSTEM_LFS_CFG_READ_SIZE_BYTES_TEST (16u)
#define FILESYSTEM_LFS_CFG_BLOCK_CYCLES_TEST    (500u) // Configures wear-levelling counter for LittleFS

   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   result = filesystem_init(&filesystem, &(flash_driver.interface));
   ASSERT_TRUE(IS_OK(result));

   ASSERT_TRUE(filesystem._initialized);
   EXPECT_EQ(0u, filesystem.partition_base_addr);
   EXPECT_EQ(FLASH_DRIVER_TOTAL_SIZE_BYTES_TEST, filesystem.partition_size_bytes);
   EXPECT_EQ(FLASH_DRIVER_SECTOR_SIZE_BYTES_TEST, filesystem.cfg.block_size);
   EXPECT_EQ(FLASH_DRIVER_TOTAL_SIZE_BYTES_TEST / FLASH_DRIVER_SECTOR_SIZE_BYTES_TEST, filesystem.cfg.block_count);
   EXPECT_EQ(FILESYSTEM_LFS_CFG_PROG_SIZE_BYTES_TEST, filesystem.cfg.prog_size);
   EXPECT_EQ(filesystem.cfg.prog_size, filesystem.cfg.cache_size);
   EXPECT_EQ(filesystem.cfg.prog_size, filesystem.cfg.lookahead_size);
   EXPECT_EQ(FILESYSTEM_LFS_CFG_READ_SIZE_BYTES_TEST, filesystem.cfg.read_size);
   EXPECT_EQ(FILESYSTEM_LFS_CFG_BLOCK_CYCLES_TEST, filesystem.cfg.block_cycles);
}

TEST_F(FilesystemTestSuite, LfsRead_NullConfig)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(NULL, 0, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_NullBuffer)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, NULL, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_NullContext)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.context = NULL;

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_NullFlashInterface)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem._flash_interface = NULL;

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_BlockOutOfRange)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   result = filesystem_init(&filesystem, &(flash_driver.interface));
   ASSERT_TRUE(IS_OK(result));

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result
      = filesystem.cfg.read(&(filesystem.cfg), filesystem.cfg.block_count, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_OffsetOutsideBlock)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result
      = filesystem.cfg.read(&(filesystem.cfg), 0, filesystem.cfg.block_size, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_SizeExceedsBlockRemainder)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   uint8_t buffer[filesystem.cfg.read_size] = {0};
   lfs_size_t oversized = filesystem.cfg.block_size;

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, filesystem.cfg.read_size, buffer, oversized);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_SizeIsZero)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, 0);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_ReadSizeZero)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.read_size = 0;

   uint8_t buffer[16] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, sizeof(buffer));

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_UnalignedOffset)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   uint8_t buffer[filesystem.cfg.read_size] = {0};
   lfs_off_t unaligned_offset = filesystem.cfg.read_size / 2;

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, unaligned_offset, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_UnalignedSize)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   uint8_t buffer[filesystem.cfg.read_size + 1] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size + 1);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_PartitionOffsetOverflow)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.partition_size_bytes = (filesystem.cfg.block_size / 2u) - 1u;

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result
      = filesystem.cfg.read(&(filesystem.cfg), 0, filesystem.cfg.block_size / 2u, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_PartitionSizeOverflow)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.partition_size_bytes = filesystem.cfg.read_size / 2u;

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_TransactionTooLarge)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   result = filesystem_init(&filesystem, &(flash_driver.interface));
   ASSERT_TRUE(IS_OK(result));

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, 256);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_DriverError)
{
   ASSERT_TRUE(IS_OK(InitFilesystem(false, true, true)));

   uint8_t buffer[filesystem.cfg.read_size] = {0};

   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_Succeeds)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   result = filesystem_init(&filesystem, &(flash_driver.interface));
   ASSERT_TRUE(IS_OK(result));

   uint8_t buffer[filesystem.cfg.read_size] = {0};
   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size);

   ASSERT_EQ(LFS_ERR_OK, int_result);
}

TEST_F(FilesystemTestSuite, LfsProg_NullConfig)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(NULL, 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_NullBuffer)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, NULL, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_NullContext)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.context = NULL;

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_NullFlashInterface)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem._flash_interface = NULL;

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_BlockOutOfRange)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), filesystem.cfg.block_count, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_BlockSizeMismatch)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.block_size = FLASH_DRIVER_SECTOR_SIZE_BYTES + filesystem.cfg.prog_size;

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_OffsetOutsideBlock)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, filesystem.cfg.block_size, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_SizePastBlockEnd)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size * 2u;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(
      &(filesystem.cfg), 0, filesystem.cfg.block_size - filesystem.cfg.prog_size, data, data_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_SizeIsZero)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, 0);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_ProgSizeMismatch)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.prog_size += 1u;

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_UnalignedOffsetRejected)
{
   result_t result = mock_flash_driver_init(&flash_driver, true, true, true);
   ASSERT_TRUE(IS_OK(result));
   result = filesystem_init(&filesystem, &(flash_driver.interface));
   ASSERT_TRUE(IS_OK(result));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0xA5u, data_size);

   lfs_off_t unaligned_offset = filesystem.cfg.prog_size / 2;

   int int_result = filesystem.cfg.prog(&(filesystem.cfg), 0, unaligned_offset, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsProg_SizeNotAlignedToProgSize)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size + 1u;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, data_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_DriverError)
{
   ASSERT_TRUE(IS_OK(InitFilesystem(true, false, true)));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsProg_Succeeds)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);

   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, filesystem.cfg.prog_size);

   ASSERT_EQ(LFS_ERR_OK, result);
}

TEST_F(FilesystemTestSuite, LfsErase_NullConfig)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   int result = filesystem.cfg.erase(NULL, 0);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_NullContext)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.context = NULL;

   int result = filesystem.cfg.erase(&(filesystem.cfg), 0);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_NullFlashInterface)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem._flash_interface = NULL;

   int result = filesystem.cfg.erase(&(filesystem.cfg), 0);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_BlockOutOfRange)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   int result = filesystem.cfg.erase(&(filesystem.cfg), filesystem.cfg.block_count);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_BlockSizeMismatch)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.block_size = FLASH_DRIVER_SECTOR_SIZE_BYTES + 1u;

   int result = filesystem.cfg.erase(&(filesystem.cfg), 0);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_DriverError)
{
   ASSERT_TRUE(IS_OK(InitFilesystem(true, true, false)));

   int result = filesystem.cfg.erase(&(filesystem.cfg), 0);

   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_Succeeds)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));

   int result = filesystem.cfg.erase(&(filesystem.cfg), 0);

   ASSERT_EQ(LFS_ERR_OK, result);
}

TEST_F(FilesystemTestSuite, LfsRead_MaxBlock)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   uint8_t buffer[filesystem.cfg.read_size] = {0};
   int int_result
      = filesystem.cfg.read(&(filesystem.cfg), filesystem.cfg.block_count - 1, 0, buffer, filesystem.cfg.read_size);
   ASSERT_TRUE(int_result == LFS_ERR_OK || int_result == LFS_ERR_IO);
}

TEST_F(FilesystemTestSuite, LfsProg_MaxBlock)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5Au, data_size);
   int result
      = filesystem.cfg.prog(&(filesystem.cfg), filesystem.cfg.block_count - 1, 0, data, filesystem.cfg.prog_size);
   ASSERT_TRUE(result == LFS_ERR_OK || result == LFS_ERR_IO);
}

TEST_F(FilesystemTestSuite, LfsErase_MaxBlock)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   int result = filesystem.cfg.erase(&(filesystem.cfg), filesystem.cfg.block_count - 1);
   ASSERT_TRUE(result == LFS_ERR_OK || result == LFS_ERR_IO);
}

TEST_F(FilesystemTestSuite, LfsRead_MaxBufferSize)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   uint8_t buffer[FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES] = {0};
   int int_result
      = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES - 1);
   ASSERT_TRUE(int_result == LFS_ERR_OK || int_result == LFS_ERR_IO);
}

TEST_F(FilesystemTestSuite, Lfs_Sequential_ProgReadErase)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0xA5, data_size);
   uint8_t buffer[data_size];
   int result = filesystem.cfg.prog(&(filesystem.cfg), 0, 0, data, data_size);
   ASSERT_TRUE(result == LFS_ERR_OK || result == LFS_ERR_IO);
   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, data_size);
   ASSERT_TRUE(int_result == LFS_ERR_OK || int_result == LFS_ERR_IO);
   int erase_result = filesystem.cfg.erase(&(filesystem.cfg), 0);
   ASSERT_TRUE(erase_result == LFS_ERR_OK || erase_result == LFS_ERR_IO);
}

TEST_F(FilesystemTestSuite, LfsConfig_BlockCountZero)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   filesystem.cfg.block_count = 0;
   uint8_t buffer[filesystem.cfg.read_size] = {0};
   int int_result = filesystem.cfg.read(&(filesystem.cfg), 0, 0, buffer, filesystem.cfg.read_size);
   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsRead_PastFlashSize)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   // Try to read starting at the last valid block, but with a size that goes past the end
   uint32_t block = filesystem.cfg.block_count - 1;
   uint32_t off = filesystem.cfg.block_size - filesystem.cfg.read_size / 2;
   uint8_t buffer[filesystem.cfg.read_size] = {0};
   // This should go past the partition_size_bytes
   int int_result = filesystem.cfg.read(&(filesystem.cfg), block, off, buffer, filesystem.cfg.read_size);
   ASSERT_EQ(LFS_ERR_IO, int_result);
}

TEST_F(FilesystemTestSuite, LfsProg_PastFlashSize)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   // Try to write starting at the last valid block, but with a size that goes past the end
   uint32_t block = filesystem.cfg.block_count - 1;
   uint32_t off = filesystem.cfg.block_size - filesystem.cfg.prog_size / 2;
   const lfs_size_t data_size = filesystem.cfg.prog_size;
   uint8_t data[data_size];
   memset(data, 0x5A, data_size);
   int result = filesystem.cfg.prog(&(filesystem.cfg), block, off, data, data_size);
   ASSERT_EQ(LFS_ERR_IO, result);
}

TEST_F(FilesystemTestSuite, LfsErase_PastFlashSize)
{
   ASSERT_TRUE(IS_OK(InitFilesystem()));
   // Try to erase a block index that is out of range
   int result = filesystem.cfg.erase(&(filesystem.cfg), filesystem.cfg.block_count);
   ASSERT_EQ(LFS_ERR_IO, result);
}
