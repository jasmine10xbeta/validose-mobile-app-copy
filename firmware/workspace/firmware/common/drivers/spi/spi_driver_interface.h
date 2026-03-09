/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file spi_driver_interface.h
 * @brief Interface for SPI driver
 *
 * Defines interface to SPI driver.
 */

#ifndef SPI_DRV_INTERFACE_H_
#define SPI_DRV_INTERFACE_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct spi_driver;                                          // Forward declaration
typedef struct spi_driver_interface spi_driver_interface_t; // Forward declaration

typedef struct spi_driver_interface
{
   struct spi_driver *parent; // Reference to the containing instance.

   /**
    * @brief Perform blocking SPI transfer.
    *
    * Timeout guideline: Absolute minimum timeout = 10/spi_frequency * transaction bytes.
    * For ample margin, a safety factor of up to x10 can be used without significantly
    * affecting performance.
    *
    * @param p_tx_buffer Buffer containing data to transmit.
    * @param tx_buffer_length Length of data in p_tx_buffer
    * @param p_rx_buffer Buffer to accept received data.
    * @param rx_buffer_length Length of data in rx_buffer_length
    * @param timeout_us If > 0, wait at most this number of microseconds before timing out. If zero, don't block.
    */
   result_t (*spi_driver_transfer_blocking)(const spi_driver_interface_t *const interface,
                                            uint8_t const *p_tx_buffer,
                                            uint8_t tx_buffer_length,
                                            uint8_t *p_rx_buffer,
                                            uint8_t rx_buffer_length,
                                            uint32_t timeout_us);
} spi_driver_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // SPI_DRV_INTERFACE_H_
