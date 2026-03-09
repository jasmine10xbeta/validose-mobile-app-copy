#ifndef BOARD_H
#define BOARD_H

#include "nrf_gpio.h"

#define ADS1235_5V_EN_PIN NRF_GPIO_PIN_MAP(0, 6) // Dock Pin

void board_init();

#endif // BOARD_H
