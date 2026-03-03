#ifndef NRF_MTX_COMMON_MOCK_H__
#define NRF_MTX_COMMON_MOCK_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t nrf_mtx_t;

void nrf_mtx_init(nrf_mtx_t *p_mtx);
bool nrf_mtx_trylock(nrf_mtx_t *p_mtx);
void nrf_mtx_unlock(nrf_mtx_t *p_mtx);

#endif // NRF_MTX_COMMON_MOCK_H__
