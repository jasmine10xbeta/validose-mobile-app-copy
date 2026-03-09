
#ifndef NRF_DRV_SPI_MOCK_H
#define NRF_DRV_SPI_MOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct nrf_drv_spi
{
   uint8_t inst_idx;

   union
   {
   } u;

   bool use_easy_dma;
} nrf_drv_spi_t;

typedef struct nrf_drv_spi_config_
{
   uint8_t sck_pin;
   uint8_t mosi_pin;
   uint8_t miso_pin;
   uint8_t ss_pin;
   uint8_t irq_priority;
   uint8_t orc;

   uint16_t frequency;
   uint16_t mode;
   uint16_t bit_order;
} nrf_drv_spi_config_t;

#endif // NRF_DRV_SPI_MOCK_H
