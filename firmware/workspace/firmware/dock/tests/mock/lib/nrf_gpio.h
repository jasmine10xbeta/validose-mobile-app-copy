
#ifndef NRF_GPIO_MOCK_H
#define NRF_GPIO_MOCK_H

#include <stdint.h>

uint16_t NRF_GPIO_PIN_MAP(uint8_t a, uint8_t b);

void nrf_gpio_pin_clear(uint32_t pin_number);
void nrf_gpio_pin_set(uint32_t pin_number);

#endif // NRF_DRV_GPIO_MOCK_H
