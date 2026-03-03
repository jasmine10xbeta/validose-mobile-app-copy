/*
 * Mock flash driver for unit tests.
 */

#ifndef FLASH_DRIVER_MOCK_H_
#define FLASH_DRIVER_MOCK_H_

#include <stdbool.h>

#include "flash_driver_s25hl512t.h"

result_t mock_flash_driver_init(flash_driver_t *const self, bool read_ok, bool prog_ok, bool erase_ok);

#endif // FLASH_DRIVER_MOCK_H_
