/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef FLASH_DRIVER_S25HL512T_H_
#define FLASH_DRIVER_S25HL512T_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <stdbool.h>

// Custom includes
#include "common.h"
#include "debug.h"
#include "flash_driver_interface.h"
#include "spi_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define FLASH_DRIVER_TOTAL_SIZE_BYTES (0x04000000u) // 64MB total flash size.

// Lengths
#define FLASH_DRIVER_REG_3BYTE_ADDR_LENGTH_BYTES (3u)
#define FLASH_DRIVER_REG_4BYTE_ADDR_LENGTH_BYTES (4u)
#define FLASH_DRIVER_ADDRESS_MAX_LENGTH_BYTES    (4u)
#define FLASH_DRIVER_CMD_LENGTH_BYTES            (1u)
#define FLASH_DRIVER_PAGE_WRITE_SIZE_BYTES       (256u)

// Default Identification Values at startup
#define FLASH_DRIVER_MANU_DEV_ID_LENGTH_BYTES (0x06u)
#define FLASH_DRIVER_MANUFACTURER_ID          (0x34u)
#define FLASH_DRIVER_DEVICE_ID_MSB            (0x2Au)
#define FLASH_DRIVER_DEVICE_ID_LSB            (0x1Au)
#define FLASH_DRIVER_ID_LENGTH                (0x0Fu)
#define FLASH_DRVIER_PHY_SECTOR_ARCH_DEFAULT  (0x03u)
#define FLASH_DRIVER_FAMILY_ID                (0x90u)

// Op Code Commands
#define FLASH_DRIVER_CMD_READ_MANU_DEV_ID             (0x9Fu)
#define FLASH_DRIVER_CMD_READ_JEDEC_FLASH_PARAM       (0x5Au)
#define FLASH_DRIVER_CMD_READ_UNIQUE_ID               (0x4Cu)
#define FLASH_DRIVER_CMD_READ_STATUS_REG_1            (0x05u)
#define FLASH_DRIVER_CMD_READ_STATUS_REG_2            (0x07u)
#define FLASH_DRIVER_CMD_READ_CONFIG_REG_1            (0x35u)
#define FLASH_DRIVER_CMD_READ_ANY_REG                 (0x65u)
#define FLASH_DRIVER_CMD_WRITE_ENABLE                 (0x06u)
#define FLASH_DRIVER_CMD_WRITE_ENABLE_VOLATILE        (0x50u)
#define FLASH_DRIVER_CMD_WRITE_REGISTER               (0x01u)
#define FLASH_DRIVER_CMD_WRITE_ANY_REG                (0x71u)
#define FLASH_DRIVER_CMD_PROG_PAGE_4_BYTE_ADDR        (0x12u)
#define FLASH_DRIVER_CMD_READ_SDR_4_BYTE_ADDR         (0x13u)
#define FLASH_DRIVER_CMD_ERASE_256_SECTOR_4_BYTE_ADDR (0xDCu)
#define FLASH_DRIVER_CMD_CLEAR_PROG_AND_ERASE_ERR     (0x30u)

// Volatile Register Address Map
#define FLASH_DRIVER_REG_ADDR_STR1V (0x00800000u)
#define FLASH_DRIVER_REG_ADDR_STR2V (0x00800001u)
#define FLASH_DRIVER_REG_ADDR_CFR1V (0x00800002u)
#define FLASH_DRIVER_REG_ADDR_CFR2V (0x00800003u)
#define FLASH_DRIVER_REG_ADDR_CFR3V (0x00800004u)
#define FLASH_DRIVER_REG_ADDR_CFR4V (0x00800005u)

// Nonvolatile Register Address Map
#define FLASH_DRIVER_REG_ADDR_STR1N (0x00000000u)

#define FLASH_DRIVER_REG_ADDR_CFR1N (0x00000002u)
#define FLASH_DRIVER_REG_ADDR_CFR2N (0x00000003u)
#define FLASH_DRIVER_REG_ADDR_CFR3N (0x00000004u)
#define FLASH_DRIVER_REG_ADDR_CFR4N (0x00000005u)

#define FLASH_DRIVER_REG_ADDR_ASPON_LSB (0x00000030)

// Register BitMasks
#define FLASH_DRIVER_REG_STR1_STCFWR_MASK (0x80u)
#define FLASH_DRIVER_REG_STR1_PRGERR_MASK (0x40u)
#define FLASH_DRIVER_REG_STR1_ERSERR_MASK (0x20u)
#define FLASH_DRIVER_REG_STR1_RDYBSY_MASK (0x01u)

#define FLASH_DRIVER_REG_CFR2_MEMLAT_MASK (0x0Fu)
#define FLASH_DRUVER_REG_CFR2_MEMLAT_0    (0x00u)

#define FLASH_DRIVER_REG_CFR3_PGMBUF_MASK (0x10u)
#define FLASH_DRIVER_REG_CFR3_UNHYSA_MASK (0x08u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

typedef struct flash_driver
{
   flash_driver_interface_t interface;
   const spi_driver_interface_t *_spi_interface;
   uint8_t _tx_buf[FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES];
   uint8_t _rx_buf[FLASH_DRIVER_TRANSACTION_BUFFER_MAX_LENGTH_BYTES];
   bool _initialized;
} flash_driver_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

result_t flash_driver_init(flash_driver_t *const self, const spi_driver_interface_t *spi_interface);

#endif // FLASH_DRIVER_S25HL512T_H_
