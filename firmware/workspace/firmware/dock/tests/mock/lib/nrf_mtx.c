#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "nrf_mtx.h"

void nrf_mtx_init(nrf_mtx_t *p_mtx)
{
   (void)p_mtx;
}

bool nrf_mtx_trylock(nrf_mtx_t *p_mtx)
{
   (void)p_mtx;
   return true;
}

void nrf_mtx_unlock(nrf_mtx_t *p_mtx)
{
   (void)p_mtx;
}
