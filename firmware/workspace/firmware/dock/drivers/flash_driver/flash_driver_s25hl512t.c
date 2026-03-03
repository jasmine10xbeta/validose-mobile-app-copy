/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

// Custom includes
#include "flash_driver_s25hl512t.h"
#include "custom_board.h"
#include "nrf_delay.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_FLASH_DRV;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define FLASH_SPI_TIMEOUT_US            (1000u)
#define FLASH_RESET_DELAY_MS            (5u)
#define FLASH_DRIVER_3_BYTE_ADDR_MAX    (0xFFFFFFu)
#define FLASH_DRIVER_NVM_TRANS_DELAY_MS (50u) // Typical nvm transaction time is 44 ms from datasheet.
#define FLASH_DRIVER_NVM_TRANS_DELAY_ITERATIONS                                                                        \
   (10u) // Max nvm transaction time is 357ms from datasheet. 10x iterations of 50ms is sufficient.
#define FLASH_DRIVER_256B_PAGE_PROG_TIMEOUT_MS     (5u)    // Datasheet specifies max of 1.7ms. Added some margin.
#define FLASH_DRIVER_256KB_SECTOR_ERASE_TIMEOUT_MS (6000u) // Datasheet specs max 2677ms. Add some margin.
#define FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES                                                                       \
   (FLASH_DRIVER_CMD_LENGTH_BYTES + FLASH_DRIVER_REG_4BYTE_ADDR_LENGTH_BYTES)
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t read_raw_linear_burst_blocking(const flash_driver_interface_t *const interface,
                                               uint32_t address,
                                               uint8_t *read_data,
                                               uint8_t read_len);
static result_t prog_page_256b_blocking(const flash_driver_interface_t *const interface,
                                        uint32_t address,
                                        const uint8_t *write_data,
                                        uint8_t write_len);
static result_t erase_sector_256kb_blocking(const flash_driver_interface_t *const interface, uint32_t start_address);
// Non-interface functions
static result_t
   read_any_register(const flash_driver_interface_t *const interface, const uint32_t address, uint8_t *read_value);
static result_t write_any_register(const flash_driver_interface_t *const interface,
                                   const uint32_t address,
                                   const uint8_t write_value);
static result_t write_registers_via_cmd(const flash_driver_interface_t *const interface,
                                        const uint8_t cmd,
                                        uint8_t *write_data,
                                        uint8_t write_len) __attribute__((unused));
static result_t read_registers_via_cmd(const flash_driver_interface_t *const interface,
                                       const uint8_t cmd,
                                       uint8_t *read_data,
                                       uint8_t read_len);
static result_t validate_manu_dev_id(const flash_driver_interface_t *const interface);
static result_t write_cmd(const flash_driver_interface_t *const interface, uint8_t cmd);
static result_t set_nvm_latency_cycles(const flash_driver_interface_t *const interface, uint8_t latency_selection);
static result_t enable_uniform_sector_architecture(const flash_driver_interface_t *const interface);
static void hw_reset_flash(void);
static result_t wait_while_busy(const flash_driver_interface_t *const interface, uint32_t timeout_ms);
static result_t
   populate_cmd_and_4byte_address(const flash_driver_interface_t *const interface, uint8_t cmd, uint32_t address);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
static result_t
   read_any_register(const flash_driver_interface_t *const interface, const uint32_t address, uint8_t *read_value)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(read_value, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(address > FLASH_DRIVER_3_BYTE_ADDR_MAX, FLASH_DRV_ERROR_INVALID_ADDR);

   flash_driver_t *self = interface->parent;
   uint8_t tx_length = FLASH_DRIVER_CMD_LENGTH_BYTES + FLASH_DRIVER_REG_3BYTE_ADDR_LENGTH_BYTES;

   // We are only reading one register with this command. Limited by Flash HW.
   uint8_t rx_length = FLASH_DRIVER_CMD_LENGTH_BYTES + FLASH_DRIVER_REG_3BYTE_ADDR_LENGTH_BYTES + 1u;

   self->_tx_buf[0u] = FLASH_DRIVER_CMD_READ_ANY_REG;
   // The device uses 3-byte addressing as default. Use 4-byte addressing specific opcodes if required.
   self->_tx_buf[1u] = (uint8_t)((address >> 16u) & 0x000000FFu);
   self->_tx_buf[2u] = (uint8_t)((address >> 8u) & 0x000000FFu);
   self->_tx_buf[3u] = (uint8_t)(address & 0x000000FFu);

   memset(self->_rx_buf, 0u, rx_length);

   result_t result = self->_spi_interface->spi_driver_transfer_blocking(
      self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, rx_length, FLASH_SPI_TIMEOUT_US);
   UPDATE_ERR(result, FLASH_DRV_ERROR_READ_ANY_REGISTER);

   if(IS_OK(result))
   {
      *read_value = self->_rx_buf[FLASH_DRIVER_CMD_LENGTH_BYTES + FLASH_DRIVER_REG_3BYTE_ADDR_LENGTH_BYTES];
   }

   return result;
}

static result_t write_any_register(const flash_driver_interface_t *const interface,
                                   const uint32_t address,
                                   const uint8_t write_value)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(address > FLASH_DRIVER_3_BYTE_ADDR_MAX, FLASH_DRV_ERROR_INVALID_ADDR);

   flash_driver_t *self = interface->parent;

   result_t result = write_cmd(interface, FLASH_DRIVER_CMD_WRITE_ENABLE);

   if(IS_OK(result))
   {
      uint8_t tx_length = FLASH_DRIVER_CMD_LENGTH_BYTES + FLASH_DRIVER_REG_3BYTE_ADDR_LENGTH_BYTES + 1u;

      self->_tx_buf[0u] = FLASH_DRIVER_CMD_WRITE_ANY_REG;
      // The device uses 3-byte addressing as default. Use 4-byte addressing specific opcodes if required.
      self->_tx_buf[1u] = (uint8_t)((address >> 16u) & 0x000000FFu);
      self->_tx_buf[2u] = (uint8_t)((address >> 8u) & 0x000000FFu);
      self->_tx_buf[3u] = (uint8_t)(address & 0x000000FFu);
      self->_tx_buf[4u] = write_value;

      result = self->_spi_interface->spi_driver_transfer_blocking(
         self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, 0u, FLASH_SPI_TIMEOUT_US);
      UPDATE_ERR(result, FLASH_DRV_ERROR_WRITE_ANY_REGISTER);
   }

   uint8_t readback_reg = 0x00u;
   IF_OK_RUN_AND_UPDATE(result, read_any_register(interface, address, &readback_reg));

   if(IS_OK(result))
   {
      if(readback_reg != write_value)
      {
         SET_ERR(result, FLASH_DRV_ERROR_WRITE_ANY_REGISTER);
      }
   }

   return result;
}

static result_t write_registers_via_cmd(const flash_driver_interface_t *const interface,
                                        const uint8_t cmd,
                                        uint8_t *write_data,
                                        uint8_t write_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(write_data, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE((FLASH_DRIVER_CMD_LENGTH_BYTES + write_len) > FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES,
                      FLASH_DRV_ERROR_DATA_LENGTH);

   flash_driver_t *self = interface->parent;
   uint8_t tx_length = FLASH_DRIVER_CMD_LENGTH_BYTES + write_len;

   self->_tx_buf[0u] = cmd;

   memcpy((self->_tx_buf) + 1u, write_data, write_len);

   result_t result = self->_spi_interface->spi_driver_transfer_blocking(
      self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, 0u, FLASH_SPI_TIMEOUT_US);
   UPDATE_ERR(result, FLASH_DRV_ERROR_WRITE_REGISTERS_VIA_CMD);

   return result;
}

static result_t read_registers_via_cmd(const flash_driver_interface_t *const interface,
                                       const uint8_t cmd,
                                       uint8_t *read_data,
                                       uint8_t read_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(read_data, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE((FLASH_DRIVER_CMD_LENGTH_BYTES + read_len) > FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES,
                      FLASH_DRV_ERROR_DATA_LENGTH);

   flash_driver_t *self = interface->parent;
   uint8_t tx_length = FLASH_DRIVER_CMD_LENGTH_BYTES;
   uint8_t rx_length = FLASH_DRIVER_CMD_LENGTH_BYTES + read_len;

   self->_tx_buf[0u] = cmd;

   memset(self->_rx_buf, 0u, rx_length);

   result_t result = self->_spi_interface->spi_driver_transfer_blocking(
      self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, rx_length, FLASH_SPI_TIMEOUT_US);
   UPDATE_ERR(result, FLASH_DRV_ERROR_READ_REGISTERS_VIA_CMD);

   if(IS_OK(result))
   {
      memcpy(read_data, (self->_rx_buf) + FLASH_DRIVER_CMD_LENGTH_BYTES, read_len);
   }

   return result;
}

static result_t validate_manu_dev_id(const flash_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);

   static const uint8_t expected_id_values[FLASH_DRIVER_MANU_DEV_ID_LENGTH_BYTES] = {
      FLASH_DRIVER_MANUFACTURER_ID,
      FLASH_DRIVER_DEVICE_ID_MSB,
      FLASH_DRIVER_DEVICE_ID_LSB,
      FLASH_DRIVER_ID_LENGTH,
      FLASH_DRVIER_PHY_SECTOR_ARCH_DEFAULT,
      FLASH_DRIVER_FAMILY_ID,
   };

   uint8_t read_data[FLASH_DRIVER_MANU_DEV_ID_LENGTH_BYTES] = {0u};

   result_t result = read_registers_via_cmd(
      interface, FLASH_DRIVER_CMD_READ_MANU_DEV_ID, read_data, FLASH_DRIVER_MANU_DEV_ID_LENGTH_BYTES);

   if(IS_OK(result))
   {
      if(memcmp(read_data, expected_id_values, FLASH_DRIVER_MANU_DEV_ID_LENGTH_BYTES) != 0)
      {
         SET_ERR(result, FLASH_DRV_ERROR_ID);
      }
   }

   return result;
}

static result_t write_cmd(const flash_driver_interface_t *const interface, uint8_t cmd)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);

   flash_driver_t *self = interface->parent;

   self->_tx_buf[0u] = cmd;

   uint8_t tx_length = FLASH_DRIVER_CMD_LENGTH_BYTES;

   result_t result = self->_spi_interface->spi_driver_transfer_blocking(
      self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, 0, FLASH_SPI_TIMEOUT_US);
   UPDATE_ERR(result, FLASH_DRV_ERROR_WRITE_CMD);

   return result;
}

static result_t set_nvm_latency_cycles(const flash_driver_interface_t *const interface, uint8_t latency_selection)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);

   uint8_t config_reg_2 = 0x00u;
   result_t result = read_any_register(interface, FLASH_DRIVER_REG_ADDR_CFR2V, &config_reg_2);

   config_reg_2 = config_reg_2 & (uint8_t)(~(FLASH_DRIVER_REG_CFR2_MEMLAT_MASK));
   config_reg_2 = config_reg_2 | latency_selection;

   IF_OK_RUN_AND_UPDATE(result, write_any_register(interface, FLASH_DRIVER_REG_ADDR_CFR2V, config_reg_2));

   return result;
}

static result_t enable_uniform_sector_architecture(const flash_driver_interface_t *const interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   uint32_t wait_iterations = 0u;
   uint8_t config_reg_3 = 0x00u;
   result_t result = read_any_register(interface, FLASH_DRIVER_REG_ADDR_CFR3V, &config_reg_3);

   if(IS_OK(result) && (FLASH_DRIVER_REG_CFR3_UNHYSA_MASK != (config_reg_3 & FLASH_DRIVER_REG_CFR3_UNHYSA_MASK)))
   {
      config_reg_3 = (config_reg_3 | FLASH_DRIVER_REG_CFR3_UNHYSA_MASK);
      IF_OK_RUN_AND_UPDATE(result, write_any_register(interface, FLASH_DRIVER_REG_ADDR_CFR3N, config_reg_3));

      // Poll status and wait for NVM operation to finish.
      uint8_t str1v = FLASH_DRIVER_REG_STR1_RDYBSY_MASK;
      while(IS_OK(result) && (FLASH_DRIVER_REG_STR1_RDYBSY_MASK == (str1v & FLASH_DRIVER_REG_STR1_RDYBSY_MASK)))
      {
         result = read_registers_via_cmd(interface, FLASH_DRIVER_CMD_READ_STATUS_REG_1, &str1v, 1u);

         if(wait_iterations > FLASH_DRIVER_NVM_TRANS_DELAY_ITERATIONS)
         {
            SET_ERR(result, FLASH_DRV_ERROR_WAIT_BUSY_TIMEOUT);
            break;
         }

         nrf_delay_ms(FLASH_DRIVER_NVM_TRANS_DELAY_MS);
         wait_iterations++;
      }

      // Check for any other errors in the status byte
      if(0u != (str1v & (FLASH_DRIVER_REG_STR1_PRGERR_MASK | FLASH_DRIVER_REG_STR1_ERSERR_MASK)))
      {
         SET_ERR(result, FLASH_DRV_ERROR_ARCH_SETUP);
      }

      // Now we need to reset the flash for nvm to be loaded to volatile.
      if(IS_OK(result))
      {
         hw_reset_flash();
      }

      // Read Back volatile and confirm it updated correctly.
      uint8_t config_reg_3_readback = 0x00u;
      IF_OK_RUN_AND_UPDATE(result, read_any_register(interface, FLASH_DRIVER_REG_ADDR_CFR3V, &config_reg_3_readback));

      if(config_reg_3 != config_reg_3_readback)
      {
         SET_ERR(result, FLASH_DRV_ERROR_ARCH_SETUP);
      }
   }

   return result;
}

static void hw_reset_flash(void)
{
   nrf_gpio_pin_clear(FLASH_NRST_PIN);
   nrf_delay_ms(FLASH_RESET_DELAY_MS);
   nrf_gpio_pin_set(FLASH_NRST_PIN);
   nrf_delay_ms(FLASH_RESET_DELAY_MS);
}

static result_t wait_while_busy(const flash_driver_interface_t *const interface, uint32_t timeout_ms)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);

   result_t result = RESULT_OK;
   uint32_t wait_iterations = 0u;

   uint8_t str1 = FLASH_DRIVER_REG_STR1_RDYBSY_MASK;

   while(IS_OK(result) && (FLASH_DRIVER_REG_STR1_RDYBSY_MASK == (str1 & FLASH_DRIVER_REG_STR1_RDYBSY_MASK)))
   {
      result = read_registers_via_cmd(interface, FLASH_DRIVER_CMD_READ_STATUS_REG_1, &str1, 1u);

      if(wait_iterations > timeout_ms)
      {
         SET_ERR(result, FLASH_DRV_ERROR_WAIT_BUSY_TIMEOUT);
         break;
      }

      nrf_delay_ms(1u);
      wait_iterations++;
   }

   if(IS_OK(result))
   {
      // Check for programming or erase errors in the status byte
      uint8_t error_mask = FLASH_DRIVER_REG_STR1_PRGERR_MASK | FLASH_DRIVER_REG_STR1_ERSERR_MASK;
      if(str1 & error_mask)
      {
         if(str1 & FLASH_DRIVER_REG_STR1_PRGERR_MASK)
         {
            SET_ERR(result, FLASH_DRV_ERROR_PROG);
         }
         else if(str1 & FLASH_DRIVER_REG_STR1_ERSERR_MASK)
         {
            SET_ERR(result, FLASH_DRV_ERROR_ERASE);
         }
         // Clear errors on flash hardware, but propagate error up
         (void)write_cmd(interface, FLASH_DRIVER_CMD_CLEAR_PROG_AND_ERASE_ERR);
      }
   }

   return result;
}

static result_t
   populate_cmd_and_4byte_address(const flash_driver_interface_t *const interface, uint8_t cmd, uint32_t address)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   flash_driver_t *self = interface->parent;
   self->_tx_buf[0u] = cmd;
   self->_tx_buf[1u] = (uint8_t)((address >> 24u) & 0x000000FFu);
   self->_tx_buf[2u] = (uint8_t)((address >> 16u) & 0x000000FFu);
   self->_tx_buf[3u] = (uint8_t)((address >> 8u) & 0x000000FFu);
   self->_tx_buf[4u] = (uint8_t)(address & 0x000000FFu);

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/
static result_t read_raw_linear_burst_blocking(const flash_driver_interface_t *const interface,
                                               uint32_t address,
                                               uint8_t *read_data,
                                               uint8_t read_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(read_data, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(0u == read_len, FLASH_DRV_ERROR_DATA_LENGTH);
   // This function is used with the hardware in linear burst mode, so can read the full memory in one go theoretically,
   // but limited by lower level SPI driver.
   RETURN_ERR_IF_TRUE((FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES + read_len)
                         > FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES,
                      FLASH_DRV_ERROR_DATA_LENGTH);
   RETURN_ERR_IF_TRUE((address + (uint32_t)read_len) > FLASH_DRIVER_TOTAL_SIZE_BYTES, FLASH_DRV_ERROR_INVALID_ADDR);

   flash_driver_t *self = interface->parent;
   uint8_t tx_length = FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES;
   uint8_t rx_length = FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES + read_len;

   // The device uses 3-byte addressing as default, but here we use a 4-byte address opcode
   result_t result = populate_cmd_and_4byte_address(interface, FLASH_DRIVER_CMD_READ_SDR_4_BYTE_ADDR, address);

   memset(self->_rx_buf, 0u, rx_length);

   IF_OK_RUN_AND_UPDATE(
      result,
      self->_spi_interface->spi_driver_transfer_blocking(
         self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, rx_length, FLASH_SPI_TIMEOUT_US));
   UPDATE_ERR(result, FLASH_DRV_ERROR_READ_RAW);

   if(IS_OK(result))
   {
      memcpy(read_data, (self->_rx_buf) + (FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES), read_len);
   }

   return result;
}

static result_t prog_page_256b_blocking(const flash_driver_interface_t *const interface,
                                        uint32_t address,
                                        const uint8_t *write_data,
                                        uint8_t write_len)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(write_data, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_TRUE(0u == write_len, FLASH_DRV_ERROR_DATA_LENGTH);
   RETURN_ERR_IF_TRUE((FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES + write_len)
                         > FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES,
                      FLASH_DRV_ERROR_DATA_LENGTH);
   // Guard clause to ensure address is within flash size, but should also be caught by cross-boundary check below.
   RETURN_ERR_IF_TRUE((address + (uint32_t)write_len) > FLASH_DRIVER_TOTAL_SIZE_BYTES, FLASH_DRV_ERROR_INVALID_ADDR);
   // Guard clause to ensure no cross-boundary programming
   const uint32_t page_offset = address % FLASH_DRIVER_PAGE_WRITE_SIZE_BYTES;
   RETURN_ERR_IF_TRUE((page_offset + (uint32_t)write_len) > FLASH_DRIVER_PAGE_WRITE_SIZE_BYTES,
                      FLASH_DRV_ERROR_CROSS_PAGE_WRITE);

   flash_driver_t *self = interface->parent;
   result_t result = write_cmd(interface, FLASH_DRIVER_CMD_WRITE_ENABLE);
   UPDATE_ERR(result, FLASH_DRV_ERROR_PROG_PAGE);

   if(IS_OK(result))
   {
      uint8_t tx_length = FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES + write_len;

      // The device uses 3-byte addressing as default, but here we use a 4-byte address opcode
      result = populate_cmd_and_4byte_address(interface, FLASH_DRIVER_CMD_PROG_PAGE_4_BYTE_ADDR, address);

      memcpy((self->_tx_buf) + (FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES), write_data, write_len);

      IF_OK_RUN_AND_UPDATE(result,
                           self->_spi_interface->spi_driver_transfer_blocking(
                              self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, 0u, FLASH_SPI_TIMEOUT_US));
      UPDATE_ERR(result, FLASH_DRV_ERROR_PROG_PAGE);

      IF_OK_RUN_AND_UPDATE(result, wait_while_busy(interface, FLASH_DRIVER_256B_PAGE_PROG_TIMEOUT_MS));
   }

   return result;
}

static result_t erase_sector_256kb_blocking(const flash_driver_interface_t *const interface, uint32_t start_address)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_NULL(interface->parent->_spi_interface, FLASH_DRV_ERROR_NULL);
   // Address has to be sector size aligned.
   RETURN_ERR_IF_TRUE(0u != (start_address % FLASH_DRIVER_SECTOR_SIZE_BYTES), FLASH_DRV_ERROR_INVALID_ADDR);
   RETURN_ERR_IF_TRUE(start_address >= (FLASH_DRIVER_TOTAL_SIZE_BYTES), FLASH_DRV_ERROR_INVALID_ADDR);

   flash_driver_t *self = interface->parent;
   result_t result = write_cmd(interface, FLASH_DRIVER_CMD_WRITE_ENABLE);
   UPDATE_ERR(result, FLASH_DRV_ERROR_SECTOR_ERASE);

   if(IS_OK(result))
   {
      uint8_t tx_length = FLASH_DRIVER_4BYTE_CMD_ADDR_LENGTH_BYTES;

      // The device uses 3-byte addressing as default, but here we use a 4-byte address opcode
      result = populate_cmd_and_4byte_address(interface, FLASH_DRIVER_CMD_ERASE_256_SECTOR_4_BYTE_ADDR, start_address);

      IF_OK_RUN_AND_UPDATE(result,
                           self->_spi_interface->spi_driver_transfer_blocking(
                              self->_spi_interface, self->_tx_buf, tx_length, self->_rx_buf, 0u, FLASH_SPI_TIMEOUT_US));
      UPDATE_ERR(result, FLASH_DRV_ERROR_SECTOR_ERASE);

      IF_OK_RUN_AND_UPDATE(result, wait_while_busy(interface, FLASH_DRIVER_256KB_SECTOR_ERASE_TIMEOUT_MS));
   }

   return result;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t flash_driver_init(flash_driver_t *const self, const spi_driver_interface_t *spi_interface)
{
   RETURN_ERR_IF_NULL(self, FLASH_DRV_ERROR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(spi_interface, FLASH_DRV_ERROR_NULL);

   self->_initialized = false;
   self->interface.parent = self;
   self->_spi_interface = spi_interface;
   self->interface.read_raw_linear_burst_blocking = read_raw_linear_burst_blocking;
   self->interface.prog_page_256b_blocking = prog_page_256b_blocking;
   self->interface.erase_sector_256kb_blocking = erase_sector_256kb_blocking;

   // Hardware RESET the Flash upon init as power is never cut.
   hw_reset_flash();

   // Validate Device identification.
   result_t result = validate_manu_dev_id(&(self->interface));

   // Note, this sets the NVM cycles to 0, which is suitable for up to 50MHz SPI clock.
   IF_OK_RUN_AND_UPDATE(result, set_nvm_latency_cycles(&(self->interface), FLASH_DRUVER_REG_CFR2_MEMLAT_0));

   // Set device to uniform 256kB blocks
   IF_OK_RUN_AND_UPDATE(result, enable_uniform_sector_architecture(&(self->interface)));

   if(IS_OK(result))
   {
      self->_initialized = true;
      DEBUG_INFO("Flash Driver Successfully Initialized.");
   }
   else
   {
      DEBUG_ERROR("Flash Driver Failed to Initialize.");
   }

   return result;
}
