#include "board.h"
#include <stdint.h>

void board_init()
{
   nrf_gpio_pin_clear(ADS1235_5V_EN_PIN); // Turn off by default
   nrf_gpio_cfg_output(ADS1235_5V_EN_PIN);
}
